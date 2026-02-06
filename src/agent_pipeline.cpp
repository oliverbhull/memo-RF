#include "agent_pipeline.h"
#include "audio_io.h"
#include "vad_endpointing.h"
#include "stt_engine.h"
#include "router.h"
#include "llm_client.h"
#include "tts_engine.h"
#include "tx_controller.h"
#include "state_machine.h"
#include "session_recorder.h"
#include "transcript_gate.h"
#include "utils.h"
#include "logger.h"
#include "path_utils.h"
#include <thread>
#include <chrono>
#include <cmath>
#include <sstream>
#include <vector>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace memo_rf {

AgentPipeline::AgentPipeline(
    const Config* config,
    AudioIO* audio_io,
    VADEndpointing* vad,
    STTEngine* stt,
    Router* router,
    LLMClient* llm,
    TTSEngine* tts,
    TXController* tx,
    StateMachine* state_machine,
    SessionRecorder* recorder,
    std::atomic<bool>* running,
    std::chrono::steady_clock::time_point* transmission_end_time)
    : config_(config)
    , audio_io_(audio_io)
    , vad_(vad)
    , stt_(stt)
    , router_(router)
    , llm_(llm)
    , tts_(tts)
    , tx_(tx)
    , state_machine_(state_machine)
    , recorder_(recorder)
    , running_(running)
    , transmission_end_time_(transmission_end_time)
{
    // Initialize runtime persona state from config
    current_persona_ = config_->llm.agent_persona;
    current_system_prompt_ = config_->llm.system_prompt;
    current_persona_name_ = config_->llm.persona_name;

    // Initialize translation target language from build flag or config
#ifdef TRANSLATE_LANGUAGE
    target_language_ = TRANSLATE_LANGUAGE;
    Logger::info("Translation target language (build): " + target_language_);
#else
    // Map response_language codes to full names for translation
    if (!config_->llm.response_language.empty()) {
        std::string lang = config_->llm.response_language;
        if (lang == "es") target_language_ = "Spanish";
        else if (lang == "fr") target_language_ = "French";
        else if (lang == "de") target_language_ = "German";
        else target_language_ = lang;
    } else {
        target_language_ = "Spanish";  // Default for translator persona
    }
#endif
}

bool AgentPipeline::load_persona(const std::string& persona_id, std::string& out_system_prompt, std::string& out_persona_name) {
    // Find personas.json in config directory
    std::string personas_path = "config/personas.json";
    std::ifstream pf(personas_path);
    if (!pf.is_open()) {
        Logger::warn("Could not open " + personas_path + " for persona change");
        return false;
    }

    json personas_json;
    try {
        pf >> personas_json;
    } catch (const json::exception& e) {
        Logger::warn("Failed to parse " + personas_path + ": " + std::string(e.what()));
        return false;
    }

    if (!personas_json.contains(persona_id) || !personas_json[persona_id].is_object()) {
        Logger::warn("Persona \"" + persona_id + "\" not found in " + personas_path);
        return false;
    }

    const auto& persona = personas_json[persona_id];
    if (persona.contains("system_prompt") && persona["system_prompt"].is_string()) {
        out_system_prompt = persona["system_prompt"].get<std::string>();
    } else {
        Logger::warn("Persona \"" + persona_id + "\" missing system_prompt");
        return false;
    }

    if (persona.contains("name") && persona["name"].is_string()) {
        out_persona_name = persona["name"].get<std::string>();
    } else {
        out_persona_name = persona_id;
    }

    return true;
}

bool AgentPipeline::check_and_handle_persona_change(const std::string& transcript, AudioBuffer& response_audio, int utterance_id) {
    // Normalize transcript for matching
    std::string lower = transcript;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c) { return std::tolower(c); });

    // Check for "memo change persona to X" or "memo change persona X"
    const std::string trigger = "memo change persona";
    size_t pos = lower.find(trigger);
    if (pos == std::string::npos) {
        return false;
    }

    // Extract persona name after trigger
    size_t after_trigger = pos + trigger.length();
    while (after_trigger < lower.length() && std::isspace(lower[after_trigger])) {
        after_trigger++;
    }

    // Skip optional "to"
    if (lower.substr(after_trigger, 3) == "to ") {
        after_trigger += 3;
        while (after_trigger < lower.length() && std::isspace(lower[after_trigger])) {
            after_trigger++;
        }
    }

    if (after_trigger >= lower.length()) {
        Logger::warn("Persona change command detected but no persona specified");
        std::string error_text = "No persona specified. Over.";
        response_audio = tts_->synth_vox(error_text);
        state_machine_->on_response_ready(response_audio);
        vad_->reset();
        tx_->transmit(response_audio);
        return true;
    }

    // Extract persona ID (until end or punctuation)
    std::string persona_id;
    for (size_t i = after_trigger; i < lower.length(); i++) {
        char c = lower[i];
        if (std::isalnum(c) || c == '_') {
            persona_id += c;
        } else if (std::isspace(c) || c == '.' || c == ',') {
            break;
        }
    }

    if (persona_id.empty()) {
        Logger::warn("Persona change command detected but could not parse persona ID");
        std::string error_text = "Could not parse persona. Over.";
        response_audio = tts_->synth_vox(error_text);
        state_machine_->on_response_ready(response_audio);
        vad_->reset();
        tx_->transmit(response_audio);
        return true;
    }

    // Load the persona
    std::string new_system_prompt;
    std::string new_persona_name;
    if (!load_persona(persona_id, new_system_prompt, new_persona_name)) {
        Logger::warn("Failed to load persona: " + persona_id);
        std::string error_text = "Persona not found: " + persona_id + ". Over.";
        response_audio = tts_->synth_vox(error_text);
        state_machine_->on_response_ready(response_audio);
        vad_->reset();
        tx_->transmit(response_audio);
        return true;
    }

    // Update runtime state
    current_persona_ = persona_id;
    current_system_prompt_ = new_system_prompt;
    current_persona_name_ = new_persona_name;

    Logger::info("Persona changed to: " + current_persona_name_ + " (" + persona_id + ")");

    // Acknowledge the change
    std::string ack_text = "Persona changed to " + current_persona_name_ + ". Over.";
    response_audio = tts_->synth_vox(ack_text);
    recorder_->record_tts_output(response_audio, utterance_id);
    state_machine_->on_response_ready(response_audio);
    vad_->reset();
    tx_->transmit(response_audio);

    return true;
}

void AgentPipeline::handle_blank_behavior() {
    const std::string& behavior = config_->transcript_blank_behavior.behavior;
    if (behavior == "none") {
        LOG_ROUTER("Transcript blank/low-signal - re-listening");
        vad_->reset();
        *transmission_end_time_ = std::chrono::steady_clock::now();
        state_machine_->reset();
        return;
    }
    if (behavior == "say_again") {
        std::string phrase = utils::ensure_ends_with_over(config_->transcript_blank_behavior.say_again_phrase);
        AudioBuffer audio = tts_->synth_vox(phrase);
        state_machine_->on_response_ready(audio);
        vad_->reset();
        tx_->transmit(audio);
        return;
    }
    if (behavior == "beep") {
        AudioBuffer beep = tts_->get_preroll_buffer();
        state_machine_->on_response_ready(beep);
        vad_->reset();
        tx_->transmit(beep);
        return;
    }
    LOG_ROUTER("Transcript blank/low-signal - re-listening (unknown behavior: " + behavior + ")");
    vad_->reset();
    *transmission_end_time_ = std::chrono::steady_clock::now();
    state_machine_->reset();
}

void AgentPipeline::handle_speech_end(
    AudioBuffer& current_utterance,
    Transcript& current_transcript,
    Plan& current_plan,
    AudioBuffer& response_audio,
    int& utterance_id,
    std::chrono::steady_clock::time_point& last_speech_end_time,
    AudioBuffer& pending_response_audio) {
    last_speech_end_time = std::chrono::steady_clock::now();

    if (!pending_response_audio.empty()) {
        state_machine_->on_vad_event(VADEvent::SpeechEnd);
        return;
    }

    state_machine_->on_vad_event(VADEvent::SpeechEnd);
    current_utterance = vad_->finalize_segment();

    size_t min_samples = (config_->vad.min_speech_ms * config_->audio.sample_rate) / 1000;
    if (current_utterance.size() < min_samples) {
        return;
    }

    utterance_id++;

    recorder_->record_utterance(current_utterance, utterance_id);
    int duration_ms = (current_utterance.size() * 1000) / config_->audio.sample_rate;
    recorder_->record_event("speech_end", "duration_ms=" + std::to_string(duration_ms));
    float avg_energy = 0.0f;
    if (!current_utterance.empty()) {
        float sum_sq = 0.0f;
        for (auto s : current_utterance) {
            float n = static_cast<float>(s) / 32768.0f;
            sum_sq += n * n;
        }
        avg_energy = static_cast<float>(std::sqrt(sum_sq / current_utterance.size()));
    }
    {
        std::ostringstream toss;
        toss << "duration_ms=" << duration_ms << " avg_energy=" << avg_energy;
        LOG_TRACE(utterance_id, "vad_end", toss.str());
    }

    auto transcribe_start = std::chrono::steady_clock::now();
    current_transcript = stt_->transcribe(current_utterance);
    auto transcribe_end = std::chrono::steady_clock::now();
    int64_t transcribe_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        transcribe_end - transcribe_start).count();
    std::ostringstream oss;
    oss << "(" << transcribe_ms << "ms) " << current_transcript.text;
    LOG_STT(oss.str());
    recorder_->record_transcript(current_transcript, utterance_id);
    {
        std::string text_snippet = current_transcript.text.size() > 40
            ? current_transcript.text.substr(0, 37) + "..."
            : current_transcript.text;
        std::ostringstream toss;
        toss << "text=\"" << text_snippet << "\" token_count=" << current_transcript.token_count
             << " confidence=" << current_transcript.confidence;
        LOG_TRACE(utterance_id, "stt", toss.str());
    }

    bool gate_passed = !is_low_signal_transcript(current_transcript, config_->transcript_gate, config_->stt.blank_sentinel);
    {
        std::ostringstream toss;
        toss << (gate_passed ? "passed" : "failed reason=low_signal");
        LOG_TRACE(utterance_id, "gate", toss.str());
    }
    if (!gate_passed) {
        handle_blank_behavior();
        return;
    }

    // Check for persona change command before routing
    if (check_and_handle_persona_change(current_transcript.text, response_audio, utterance_id)) {
        return;
    }

    if (config_->wake_word.enabled) {
        std::string lower = utils::normalize_copy(current_transcript.text);
        const std::string wake_phrase = "hey memo";
        if (lower.find(wake_phrase) == std::string::npos) {
            return;
        }
        std::string text = current_transcript.text;
        size_t pos = lower.find(wake_phrase);
        text = (pos + wake_phrase.size() < text.size())
            ? text.substr(pos + wake_phrase.size()) : "";
        current_transcript.text = utils::trim_copy(text);
        if (utils::is_blank_transcript(current_transcript.text, config_->stt.blank_sentinel)) {
            handle_blank_behavior();
            return;
        }
        LOG_ROUTER(std::string("Deciding on plan for: \"") + current_transcript.text + "\"");
        current_plan = router_->decide(current_transcript, "", config_->router.repair_confidence_threshold, config_->router.repair_phrase);
        {
            std::ostringstream toss;
            toss << "plan_type=" << static_cast<int>(current_plan.type);
            LOG_TRACE(utterance_id, "router", toss.str());
        }
        state_machine_->on_transcript_ready(current_transcript);
        vad_->reset();
        execute_plan(current_plan, current_transcript, response_audio, utterance_id, true, pending_response_audio);
        if (response_audio.empty()) {
            return;
        }
        AudioBuffer preroll = tts_->get_preroll_buffer();
        pending_response_audio.clear();
        pending_response_audio.reserve(preroll.size() + response_audio.size());
        pending_response_audio.insert(pending_response_audio.end(), preroll.begin(), preroll.end());
        pending_response_audio.insert(pending_response_audio.end(), response_audio.begin(), response_audio.end());
        state_machine_->on_response_ready(pending_response_audio);
        return;
    }

    LOG_ROUTER(std::string("Deciding on plan for: \"") + current_transcript.text + "\"");
    current_plan = router_->decide(current_transcript, "", config_->router.repair_confidence_threshold, config_->router.repair_phrase);
    {
        std::ostringstream toss;
        toss << "plan_type=" << static_cast<int>(current_plan.type);
        LOG_TRACE(utterance_id, "router", toss.str());
    }
    std::ostringstream plan_oss;
    plan_oss << "Plan type: " << (int)current_plan.type
             << ", needs_llm: " << current_plan.needs_llm;
    LOG_ROUTER(plan_oss.str());
    state_machine_->on_transcript_ready(current_transcript);
    execute_plan(current_plan, current_transcript, response_audio, utterance_id, false, pending_response_audio);
}

void AgentPipeline::execute_plan(const Plan& plan, const Transcript& transcript,
                                 AudioBuffer& response_audio, int utterance_id, bool wait_for_channel_clear,
                                 AudioBuffer& pending_response_audio) {
    (void)pending_response_audio;
    if (plan.type == PlanType::NoOp) {
        LOG_ROUTER("NoOp - returning to IdleListening");
        vad_->reset();
        state_machine_->reset();
        return;
    }
    if (plan.type == PlanType::Speak) {
        execute_fast_path(plan, response_audio, utterance_id, wait_for_channel_clear);
    } else if (plan.type == PlanType::SpeakAckThenAnswer) {
        execute_llm_path(plan, transcript, response_audio, utterance_id, wait_for_channel_clear);
    } else if (plan.type == PlanType::Fallback) {
        execute_fallback(plan, response_audio, utterance_id, wait_for_channel_clear);
    }
}

void AgentPipeline::execute_fast_path(const Plan& plan, AudioBuffer& response_audio, int utterance_id,
                                       bool wait_for_channel_clear) {
    std::string text = utils::ensure_ends_with_over(plan.answer_text);
    LOG_ROUTER(std::string("Fast path - speaking: \"") + text + "\"");
    response_audio = tts_->synth_vox(text);
    recorder_->record_tts_output(response_audio, utterance_id);
    if (wait_for_channel_clear) {
        return;
    }
    state_machine_->on_response_ready(response_audio);
    vad_->reset();
    tx_->transmit(response_audio);
}

void AgentPipeline::execute_llm_path(const Plan& plan, const Transcript& transcript,
                                     AudioBuffer& response_audio, int utterance_id,
                                     bool wait_for_channel_clear) {
    if (!wait_for_channel_clear && !plan.ack_text.empty()) {
        std::string ack_text = utils::ensure_ends_with_over(plan.ack_text);
        LOG_ROUTER(std::string("LLM path - acknowledging first: \"") + ack_text + "\"");
        vad_->reset();
        AudioBuffer ack_audio = tts_->synth_vox(ack_text);
        tx_->transmit(ack_audio);
        LOG_ROUTER("Waiting for ack playback to complete...");
        while (!audio_io_->is_playback_complete() && running_->load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        LOG_ROUTER("Ack playback complete, calling LLM...");
        *transmission_end_time_ = std::chrono::steady_clock::now();
    } else if (!wait_for_channel_clear) {
        LOG_ROUTER("LLM path - skipping acknowledgment, going straight to response");
    }

    auto llm_start = std::chrono::steady_clock::now();
    std::string llm_prompt = transcript.text;
    recorder_->record_llm_prompt(llm_prompt, utterance_id);

    std::vector<std::string> empty_history;
    std::string model_override;
    std::string system_prompt_override;

    // Use runtime persona state for system prompt
    if (current_persona_ == "translator") {
        // Translation mode: tight prompt for verbatim translation
        // Note: Not using translation_model anymore - using Qwen for translation
        system_prompt_override =
            "Translate this English radio transmission to " + target_language_ + " verbatim. "
            "Output ONLY the " + target_language_ + " translation. "
            "Do not add explanations, preamble, or commentary. "
            "Preserve the exact meaning and radio terminology. "
            "End with \"over\".";
        LOG_LLM("Translation mode - target language: " + target_language_);
    } else {
        // Use current runtime system prompt (may have been changed dynamically)
        system_prompt_override = current_system_prompt_;
    }

    LOG_LLM(std::string("Calling LLM with prompt: \"") + llm_prompt + "\"");
    LLMResponse response = llm_->generate_with_tools(
        llm_prompt,
        "",
        empty_history,
        config_->llm.timeout_ms,
        config_->llm.max_tokens,
        model_override,
        system_prompt_override
    );
    std::string llm_response = response.content;
    std::string llm_stop_reason = response.stop_reason;
    if (response.stop_reason == "length") {
        LOG_LLM("Truncated response (done_reason=length), using fallback");
        llm_response = (config_->llm.response_language == "es")
            ? "Un momento."
            : config_->llm.truncation.fallback_phrase;
    }

    auto llm_end = std::chrono::steady_clock::now();
    int64_t llm_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        llm_end - llm_start).count();
    {
        std::ostringstream toss;
        toss << "done_reason=" << (llm_stop_reason.empty() ? "unknown" : llm_stop_reason)
             << " latency_ms=" << llm_ms;
        LOG_TRACE(utterance_id, "llm", toss.str());
    }
    std::ostringstream llm_oss;
    llm_oss << "(" << llm_ms << "ms) " << llm_response;
    LOG_LLM(llm_oss.str());
    recorder_->record_llm_response(llm_response, utterance_id);

    std::string trimmed_response = utils::trim_copy(llm_response);
    if (trimmed_response.empty() || utils::is_empty_or_whitespace(trimmed_response)) {
        LOG_LLM("Empty response, using fallback");
        llm_response = (config_->llm.response_language == "es")
            ? "Un momento."
            : config_->llm.truncation.fallback_phrase;
    }

    std::string response_text = utils::trim_copy(llm_response);
    if (response_text.empty()) response_text = "Stand by.";
    LOG_TTS("Synthesizing response...");
    auto tts_start = std::chrono::steady_clock::now();
    response_audio = tts_->synth_vox(response_text);
    AudioBuffer end_tone = tts_->get_end_tone_buffer();
    if (!end_tone.empty()) {
        response_audio.insert(response_audio.end(), end_tone.begin(), end_tone.end());
    }
    auto tts_end = std::chrono::steady_clock::now();
    int64_t tts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        tts_end - tts_start).count();
    {
        std::ostringstream toss;
        toss << "samples=" << response_audio.size() << " latency_ms=" << tts_ms;
        LOG_TRACE(utterance_id, "tts", toss.str());
    }
    recorder_->record_tts_output(response_audio, utterance_id);
    if (wait_for_channel_clear) return;
    state_machine_->on_response_ready(response_audio);
    vad_->reset();
    LOG_TX("Transmitting response (" + std::to_string(response_audio.size()) + " samples)...");
    tx_->transmit(response_audio);
}

void AgentPipeline::execute_fallback(const Plan& plan, AudioBuffer& response_audio, int utterance_id,
                                     bool wait_for_channel_clear) {
    std::string text = utils::trim_copy(plan.fallback_text);
    if (text.empty()) text = "Stand by.";
    LOG_ROUTER(std::string("Fallback - speaking: \"") + text + "\"");
    response_audio = tts_->synth_vox(text);
    AudioBuffer end_tone = tts_->get_end_tone_buffer();
    if (!end_tone.empty()) {
        response_audio.insert(response_audio.end(), end_tone.begin(), end_tone.end());
    }
    recorder_->record_tts_output(response_audio, utterance_id);
    if (wait_for_channel_clear) return;
    state_machine_->on_response_ready(response_audio);
    vad_->reset();
    tx_->transmit(response_audio);
}

} // namespace memo_rf
