#include "agent.h"
#include "audio_io.h"
#include "vad_endpointing.h"
#include "stt_engine.h"
#include "router.h"
#include "llm_client.h"
#include "tts_engine.h"
#include "tx_controller.h"
#include "state_machine.h"
#include "session_recorder.h"
#include "logger.h"
#include "utils.h"
#include "transcript_gate.h"
#include "common.h"
#include "tool_registry.h"
#include "tool_executor.h"
#define MEMO_RF_USE_COMMON_TYPES 1  // Avoid redefinition of Transcript/ms_since from core/types.h
#include "memory/conversation_memory.h"
#include "core/constants.h"
#include "tools/log_memo_tool.h"
#include "tools/external_research_tool.h"
#include "tools/internal_search_tool.h"
#include "twitter_flow.h"
#include <thread>
#include <atomic>
#include <chrono>
#include <cmath>
#include <sstream>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace memo_rf {

class VoiceAgent::Impl {
public:
    Impl(const Config& config) 
        : config_(config), running_(true), initialized_(false),
          transmission_end_time_(std::chrono::steady_clock::now()),
          previous_state_(State::IdleListening),
          speech_frame_count_(0),
          speech_start_time_(std::chrono::steady_clock::now()),
          last_speech_log_time_(std::chrono::steady_clock::now()),
          last_speech_end_time_(std::chrono::steady_clock::now()) {}
    
    ~Impl() {
        shutdown();
    }
    
    bool initialize() {
        if (initialized_) {
            return true;
        }
        
        // Initialize components
        audio_io_ = std::make_unique<AudioIO>();
        vad_ = std::make_unique<VADEndpointing>(config_.vad);
        stt_ = std::make_unique<STTEngine>(config_.stt);
        router_ = std::make_unique<Router>();
        llm_ = std::make_unique<LLMClient>(config_.llm);
        tts_ = std::make_unique<TTSEngine>(config_.tts);
        tx_ = std::make_unique<TXController>(config_.tx);
        state_machine_ = std::make_unique<StateMachine>(config_.wake_word.enabled);
        recorder_ = std::make_unique<SessionRecorder>(config_.session_log_dir);
        
        // Set up TX controller
        tx_->set_audio_io(audio_io_.get());
        
        // Start audio I/O
        Logger::info("Initializing audio I/O...");
        Logger::info("  Input device: " + config_.audio.input_device);
        Logger::info("  Output device: " + config_.audio.output_device);
        
        if (!audio_io_->start(config_.audio.input_device, 
                              config_.audio.output_device,
                              config_.audio.sample_rate)) {
            Logger::error("Failed to start audio I/O");
            return false;
        }
        
        Logger::info("Audio I/O started successfully");
        
        // Start session recording
        Logger::info("Starting session recording...");
        recorder_->start_session();
        Logger::info("Session recording started");
        
        // Initialize tool system
        Logger::info("Initializing tool system...");
        tool_registry_ = std::make_unique<ToolRegistry>();
        tool_executor_ = std::make_unique<ToolExecutor>(
            tool_registry_.get(), 
            config_.tools.max_concurrent
        );
        
        // Register tools based on config
        for (const auto& tool_name : config_.tools.enabled) {
            if (tool_name == "log_memo") {
                auto tool = std::make_shared<LogMemoTool>(config_.session_log_dir);
                tool_registry_->register_tool(tool);
            } else if (tool_name == "external_research") {
                auto tool = std::make_shared<ExternalResearchTool>();
                tool_registry_->register_tool(tool);
            } else if (tool_name == "internal_search") {
                auto tool = std::make_shared<InternalSearchTool>(config_.session_log_dir);
                tool_registry_->register_tool(tool);
            } else {
                Logger::warn("Unknown tool name in config: " + tool_name);
            }
        }
        Logger::info("Tool system initialized with " + std::to_string(tool_registry_->size()) + " tools");

        // Session conversation memory for multi-turn context (optional)
        if (config_.memory.enabled) {
            memory::ConversationConfig mem_config;
            mem_config.system_prompt = config_.llm.system_prompt;
            mem_config.max_messages = config_.memory.max_messages;
            mem_config.max_tokens = config_.memory.max_tokens;
            session_memory_ = std::make_unique<memory::ConversationMemory>(mem_config);
            Logger::info("Session memory enabled (max " + std::to_string(mem_config.max_messages) + " messages)");
        } else {
            Logger::info("Session memory disabled");
        }

        // Dedicated LLM client for background summarization (same config, separate instance for thread safety)
        summarizer_llm_ = std::make_unique<LLMClient>(config_.llm);
        worker_shutdown_ = false;
        worker_thread_ = std::thread(&Impl::summarizer_worker_loop, this);
        Logger::info("Background context summarizer started");

        // Optional: warmup translation model so first user request avoids load_duration (Ollama only)
        if (config_.llm.warmup_translation_model && !config_.llm.translation_model.empty()
            && config_.llm.endpoint.find("/api/chat") != std::string::npos) {
            std::string lang = config_.llm.response_language.empty() ? "Spanish" : config_.llm.response_language;
            if (lang == "es") lang = "Spanish";
            else if (lang == "fr") lang = "French";
            else if (lang == "de") lang = "German";
            std::string warmup_prompt = "You are a professional English to " + lang + " translator. "
                "Output only the " + lang + " translation, no explanations. End transmissions with \"over\".";
            std::vector<std::string> empty_history;
            LLMResponse warmup_r = llm_->generate_with_tools("Hi", "", empty_history,
                config_.llm.timeout_ms, 5, config_.llm.translation_model, warmup_prompt);
            (void)warmup_r;
            Logger::info("Translation model warmup complete");
        }

        initialized_ = true;
        return true;
    }
    
    int run() {
        if (!initialized_ && !initialize()) {
            return 1;
        }
        
        Logger::info("=== Memo-RF Voice Agent Started ===");
        if (!config_.llm.persona_name.empty()) {
            Logger::info("Agent persona: " + config_.llm.persona_name);
        } else if (!config_.llm.agent_persona.empty()) {
            Logger::info("Agent persona: " + config_.llm.agent_persona);
        }
        Logger::info("Listening for speech...");
        
        // State tracking
        int utterance_id = 0;
        AudioBuffer current_utterance;
        Transcript current_transcript;
        Plan current_plan;
        AudioBuffer response_audio;
        
        // Main loop
        AudioFrame frame;
        int frame_count = 0;
        constexpr int RMS_LOG_INTERVAL = 50; // Log RMS every 50 frames (~1 second)
        
        while (running_) {
            // Read audio frame
            if (!audio_io_->read_frame(frame)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            
            frame_count++;
            
            // Debug: print audio level periodically
            if (frame_count % RMS_LOG_INTERVAL == 0) {
                log_rms_level(frame, frame_count);
            }
            
            // Record raw input (always record, even during guard period)
            recorder_->record_input_frame(frame);
            
            // Detect state transitions for guard period timing
            State current_state = state_machine_->get_state();
            
            // Echo probe: log RMS 1-3 s after TX end to measure whether we capture our own TTS in the mic
            if (current_state == State::IdleListening && (frame_count % 50) == 0) {
                auto now = std::chrono::steady_clock::now();
                int64_t elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - transmission_end_time_).count();
                if (elapsed_ms >= 1000 && elapsed_ms <= 3000) {
                    float sum_sq = 0.0f;
                    for (auto s : frame) {
                        float normalized = static_cast<float>(s) / 32768.0f;
                        sum_sq += normalized * normalized;
                    }
                    float rms = frame.empty() ? 0.0f : std::sqrt(sum_sq / frame.size());
                    std::ostringstream eoss;
                    eoss << "[echo_probe] post_tx_ms=" << elapsed_ms << " rms=" << rms;
                    LOG_AUDIO(eoss.str());
                }
            }
            
            // Check if playback completed while in Transmitting state
            // This must be checked every frame to catch the transition
            if (current_state == State::Transmitting) {
                bool playback_done = audio_io_->is_playback_complete();
                // Log periodically to debug stuck state (every 100 frames = ~6 seconds at 16kHz)
                if (frame_count % 100 == 0) {
                    std::ostringstream oss;
                    oss << "Transmitting state: playback_complete=" << (playback_done ? "true" : "false");
                    LOG_TX(oss.str());
                }
                if (playback_done) {
                    // Playback finished - transition to IdleListening
                    LOG_TX("Playback complete, transitioning to IdleListening");
                    state_machine_->on_playback_complete();
                    transmission_end_time_ = std::chrono::steady_clock::now();
                    vad_->reset(); // Clear any accumulated audio
                    // Flush input queue so we do not process stale audio that accumulated
                    // while the main loop was blocked in handle_speech_end (STT/LLM/TTS)
                    audio_io_->flush_input_queue();
                    current_state = state_machine_->get_state(); // Update current_state after transition
                }
            }
            
            previous_state_ = current_state;
            
            // Process frame through state machine and VAD
            // (process_frame will return early if in guard period or transmitting)
            process_frame(frame, current_utterance, current_transcript, 
                        current_plan, response_audio, utterance_id);
            
            // Small sleep to prevent busy-waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        // Cleanup
        cleanup();
        
        return 0;
    }
    
    void shutdown() {
        running_ = false;
        // Signal summarizer worker to exit and join (so destructor/cleanup don't leave thread running)
        {
            std::lock_guard<std::mutex> lock(job_mutex_);
            worker_shutdown_ = true;
        }
        job_cv_.notify_one();
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }

private:
    void log_rms_level(const AudioFrame& frame, int frame_count) {
        float sum_sq = 0.0f;
        for (auto s : frame) {
            float normalized = static_cast<float>(s) / 32768.0f;
            sum_sq += normalized * normalized;
        }
        float rms = std::sqrt(sum_sq / frame.size());
        std::ostringstream oss;
        oss << "Frame " << frame_count << ", RMS level: " << rms 
            << " (threshold: " << config_.vad.threshold << ")";
        LOG_AUDIO(oss.str());
    }
    
    void process_frame(const AudioFrame& frame, AudioBuffer& current_utterance,
                      Transcript& current_transcript, Plan& current_plan,
                      AudioBuffer& response_audio, int& utterance_id) {
        State current_state = state_machine_->get_state();
        
        // CRITICAL: Never process VAD during transmission or thinking
        // This is the primary defense against feedback loops
        // We do process VAD in WaitingForChannelClear so we can detect SpeechStart (interrupt on channel)
        if (current_state == State::Transmitting || current_state == State::Thinking) {
            return;
        }

        // When WaitingForChannelClear: check if channel is clear; if so, transmit pending response
        if (current_state == State::WaitingForChannelClear && !pending_response_audio_.empty()) {
            auto now = std::chrono::steady_clock::now();
            auto silence_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_speech_end_time_).count();
            if (silence_ms >= config_.tx.channel_clear_silence_ms) {
                state_machine_->on_channel_clear();
                vad_->reset();
                tx_->transmit(pending_response_audio_);
                pending_response_audio_.clear();
                return;
            }
        }
        
        // Check guard period when in IdleListening (not when WaitingForChannelClear)
        if (current_state == State::IdleListening) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - transmission_end_time_).count();
            
            if (elapsed_ms < VAD_GUARD_PERIOD_MS) {
                // Still in guard period - completely skip VAD processing
                // This prevents feedback loops from agent's own voice (echo/reverb)
                return;
            }
        }
        
        // Process VAD when in IdleListening (past guard period), ReceivingSpeech, or WaitingForChannelClear (to detect interrupt)
        VADEvent vad_event = VADEvent::None;
        if (current_state == State::IdleListening || 
            current_state == State::ReceivingSpeech ||
            current_state == State::WaitingForChannelClear) {
            vad_event = vad_->process(frame);
            
            // Debug VAD events
            if (vad_event == VADEvent::SpeechStart) {
                LOG_VAD("Speech detected - START");
            } else if (vad_event == VADEvent::SpeechEnd) {
                LOG_VAD("Speech detected - END");
            }
        }
        
        if (vad_event == VADEvent::SpeechStart) {
            handle_speech_start(current_state);
            speech_frame_count_ = 0; // Reset speech frame counter
            speech_start_time_ = std::chrono::steady_clock::now(); // Track when speech started
            last_speech_log_time_ = speech_start_time_; // Initialize last log time
            current_utterance.clear();
            current_utterance = vad_->get_current_segment();
        } else if (vad_event == VADEvent::SpeechEnd) {
            handle_speech_end(current_utterance, current_transcript, 
                            current_plan, response_audio, utterance_id);
        } else {
            // Continue accumulating if in ReceivingSpeech
            if (current_state == State::ReceivingSpeech) {
                current_utterance = vad_->get_current_segment();
                speech_frame_count_++;
                
                // Log periodically to show speech is happening (~every 1 second)
                auto now = std::chrono::steady_clock::now();
                auto elapsed_since_last_log = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - last_speech_log_time_).count();
                
                if (elapsed_since_last_log >= 1000) { // Log every 1 second
                    auto speech_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - speech_start_time_).count();
                    std::ostringstream oss;
                    oss << "Receiving speech... (" << speech_duration << "ms)";
                    Logger::info(oss.str());
                    last_speech_log_time_ = now;
                }
            } else {
                // Reset counter when not in ReceivingSpeech
                speech_frame_count_ = 0;
            }
        }
    }
    
    void handle_speech_start(State current_state) {
        if (current_state == State::Transmitting) {
            // Interrupt transmission (shouldn't happen now, but keep as safety)
            tx_->stop();
            audio_io_->stop_playback();
            state_machine_->on_playback_complete();
        }
        state_machine_->on_vad_event(VADEvent::SpeechStart);
    }

    void handle_blank_behavior() {
        const std::string& behavior = config_.transcript_blank_behavior.behavior;
        if (behavior == "none") {
            LOG_ROUTER("Transcript blank/low-signal - re-listening");
            vad_->reset();
            return;
        }
        if (behavior == "say_again") {
            std::string phrase = utils::ensure_ends_with_over(config_.transcript_blank_behavior.say_again_phrase);
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
    }
    
    void handle_speech_end(AudioBuffer& current_utterance, Transcript& current_transcript,
                          Plan& current_plan, AudioBuffer& response_audio, int& utterance_id) {
        last_speech_end_time_ = std::chrono::steady_clock::now();

        // Half-duplex: we were waiting for channel clear and someone was talking; now they stopped
        if (!pending_response_audio_.empty()) {
            state_machine_->on_vad_event(VADEvent::SpeechEnd);
            return;
        }

        state_machine_->on_vad_event(VADEvent::SpeechEnd);
        current_utterance = vad_->finalize_segment();

        size_t min_samples = (config_.vad.min_speech_ms * config_.audio.sample_rate) / 1000;
        if (current_utterance.size() < min_samples) {
            return;
        }

        utterance_id++;

        // Continual STT: always transcribe
        recorder_->record_utterance(current_utterance, utterance_id);
        int duration_ms = (current_utterance.size() * 1000) / config_.audio.sample_rate;
        recorder_->record_event("speech_end", "duration_ms=" + std::to_string(duration_ms));
        float avg_energy = 0.0f;
        if (!current_utterance.empty()) {
            float sum_sq = 0.0f;
            for (auto s : current_utterance) {
                float n = static_cast<float>(s) / 32768.0f;
                sum_sq += n * n;
            }
            avg_energy = std::sqrt(sum_sq / current_utterance.size());
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

        // Transcript gate: block router, clarifier, memory when low-signal
        bool gate_passed = !is_low_signal_transcript(current_transcript, config_.transcript_gate, config_.stt.blank_sentinel);
        {
            std::ostringstream toss;
            toss << (gate_passed ? "passed" : "failed reason=low_signal");
            LOG_TRACE(utterance_id, "gate", toss.str());
        }
        if (!gate_passed) {
            handle_blank_behavior();
            return;
        }

        // Wake word: respond only when transcript contains "hey memo"
        if (config_.wake_word.enabled) {
            std::string lower = utils::normalize_copy(current_transcript.text);
            const std::string wake_phrase = "hey memo";
            if (lower.find(wake_phrase) == std::string::npos) {
                return;  // No response; agent stays silent
            }
            // Strip "hey memo" (case-insensitive) and use remainder as command
            std::string text = current_transcript.text;
            size_t pos = lower.find(wake_phrase);
            text = (pos + wake_phrase.size() < text.size())
                ? text.substr(pos + wake_phrase.size()) : "";
            current_transcript.text = utils::trim_copy(text);
            // Remainder blank: do not send space to LLM; run blank behavior and return
            if (utils::is_blank_transcript(current_transcript.text, config_.stt.blank_sentinel)) {
                handle_blank_behavior();
                return;
            }
            bool twitter_handled_ww = false;
            {
                auto tf_result = twitter_flow_handle(current_transcript.text, twitter_flow_state_);
                if (tf_result.handled) {
                    current_plan.type = PlanType::Speak;
                    current_plan.answer_text = tf_result.response_text;
                    current_plan.needs_llm = false;
                    twitter_handled_ww = true;
                    LOG_ROUTER(std::string("Twitter flow - speaking: \"") + tf_result.response_text + "\"");
                }
            }
            if (!twitter_handled_ww) {
                LOG_ROUTER(std::string("Deciding on plan for: \"") + current_transcript.text + "\"");
                current_plan = router_->decide(current_transcript, "", config_.router.repair_confidence_threshold, config_.router.repair_phrase);
            }
            {
                std::ostringstream toss;
                toss << "plan_type=" << static_cast<int>(current_plan.type);
                LOG_TRACE(utterance_id, "router", toss.str());
            }
            state_machine_->on_transcript_ready(current_transcript);
            vad_->reset();
            execute_plan(current_plan, current_transcript, response_audio, utterance_id, true);
            if (response_audio.empty()) {
                return;
            }
            AudioBuffer preroll = tts_->get_preroll_buffer();
            pending_response_audio_.clear();
            pending_response_audio_.reserve(preroll.size() + response_audio.size());
            pending_response_audio_.insert(pending_response_audio_.end(), preroll.begin(), preroll.end());
            pending_response_audio_.insert(pending_response_audio_.end(), response_audio.begin(), response_audio.end());
            state_machine_->on_response_ready(pending_response_audio_);
            return;
        }

        bool twitter_handled = false;
        {
            auto tf_result = twitter_flow_handle(current_transcript.text, twitter_flow_state_);
            if (tf_result.handled) {
                current_plan.type = PlanType::Speak;
                current_plan.answer_text = tf_result.response_text;
                current_plan.needs_llm = false;
                twitter_handled = true;
                LOG_ROUTER(std::string("Twitter flow - speaking: \"") + tf_result.response_text + "\"");
            }
        }
        if (!twitter_handled) {
            LOG_ROUTER(std::string("Deciding on plan for: \"") + current_transcript.text + "\"");
            current_plan = router_->decide(current_transcript, "", config_.router.repair_confidence_threshold, config_.router.repair_phrase);
        }
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
        execute_plan(current_plan, current_transcript, response_audio, utterance_id, false);
    }
    
    void execute_plan(const Plan& plan, const Transcript& transcript,
                     AudioBuffer& response_audio, int utterance_id, bool wait_for_channel_clear = false) {
        if (plan.type == PlanType::NoOp) {
            LOG_ROUTER("NoOp - doing nothing");
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
    
    void execute_fast_path(const Plan& plan, AudioBuffer& response_audio, int utterance_id,
                           bool wait_for_channel_clear = false) {
        std::string text = utils::ensure_ends_with_over(plan.answer_text);
        LOG_ROUTER(std::string("Fast path - speaking: \"") + text + "\"");
        response_audio = tts_->synth_vox(text);
        recorder_->record_tts_output(response_audio, utterance_id);
        if (wait_for_channel_clear) {
            return;  // Caller will store as pending and call on_response_ready after channel clear
        }
        state_machine_->on_response_ready(response_audio);
        vad_->reset();
        tx_->transmit(response_audio);
    }
    
    void execute_llm_path(const Plan& plan, const Transcript& transcript,
                         AudioBuffer& response_audio, int utterance_id,
                         bool wait_for_channel_clear = false) {
        // When waiting for channel clear, skip ack transmit (we will send standby+response when clear)
        if (!wait_for_channel_clear && !plan.ack_text.empty()) {
            std::string ack_text = utils::ensure_ends_with_over(plan.ack_text);
            LOG_ROUTER(std::string("LLM path - acknowledging first: \"") + ack_text + "\"");
            vad_->reset();
            AudioBuffer ack_audio = tts_->synth_vox(ack_text);
            tx_->transmit(ack_audio);
            LOG_ROUTER("Waiting for ack playback to complete...");
            while (!audio_io_->is_playback_complete() && running_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            LOG_ROUTER("Ack playback complete, calling LLM...");
            transmission_end_time_ = std::chrono::steady_clock::now();
        } else if (!wait_for_channel_clear) {
            LOG_ROUTER("LLM path - skipping acknowledgment, going straight to response");
        }

        // Generate LLM response with tool support
        auto llm_start = std::chrono::steady_clock::now();
        std::string raw_prompt = transcript.text;
        recorder_->record_llm_prompt(raw_prompt, utterance_id);
        
        // Bounded context: only send last N turns to avoid "lost in the middle" (Mistral)
        size_t max_turns = static_cast<size_t>(std::max(1, config_.llm.context_max_turns_to_send));
        std::vector<std::string> session_history = session_memory_ ? session_memory_->to_json_strings_recent(max_turns) : std::vector<std::string>{};
        // Prepend conversation summary from background summarizer (primacy for Mistral)
        {
            std::lock_guard<std::mutex> lock(summary_mutex_);
            if (!context_summary_.empty()) {
                json sum_msg;
                sum_msg["role"] = "user";
                sum_msg["content"] = "Conversation summary: " + context_summary_;
                session_history.insert(session_history.begin() + 1, sum_msg.dump());
            }
        }
        
        // Contextual clarification: when we have prior turns, resolve references and STT errors
        // so the main LLM sees what the user likely meant (e.g. "that fan" -> "that frequency").
        // Never run clarifier on empty/low-signal input.
        std::string llm_prompt = raw_prompt;
        bool skip_clarifier = utils::is_blank_transcript(raw_prompt, config_.stt.blank_sentinel)
            || static_cast<int>(utils::trim_copy(raw_prompt).size()) < config_.clarifier.min_chars
            || transcript.confidence < config_.clarifier.min_confidence;
        if (session_memory_ && session_memory_->message_count() >= 2 && !skip_clarifier) {
            std::string clarified = llm_->clarify_user_message(raw_prompt, session_history,
                                                              config_.llm.timeout_ms,
                                                              config_.clarifier.min_chars);
            {
                std::ostringstream toss;
                toss << "ran output=\"" << (clarified.size() > 30 ? clarified.substr(0, 27) + "..." : clarified) << "\"";
                LOG_TRACE(utterance_id, "clarifier", toss.str());
            }
            if (!clarified.empty() && clarified != raw_prompt) {
                std::string clarified_trimmed = utils::trim_copy(clarified);
                if (clarified_trimmed == config_.clarifier.unknown_sentinel) {
                    // Resolver said unknown: do not call main LLM; speak fallback and return without adding to memory
                    std::string fallback = utils::ensure_ends_with_over(config_.llm.truncation.fallback_phrase);
                    response_audio = tts_->synth_vox(fallback);
                    recorder_->record_tts_output(response_audio, utterance_id);
                    if (wait_for_channel_clear) return;
                    state_machine_->on_response_ready(response_audio);
                    vad_->reset();
                    tx_->transmit(response_audio);
                    return;
                }
                LOG_LLM(std::string("Context clarified: \"") + raw_prompt + "\" -> \"" + clarified + "\"");
                llm_prompt = clarified;
            }
        } else if (session_memory_ && session_memory_->message_count() >= 2 && skip_clarifier) {
            LOG_TRACE(utterance_id, "clarifier", "skipped");
        }
        
        // Get tool definitions for LLM (only if tools are enabled)
        std::string tool_definitions;
        if (tool_registry_ && tool_registry_->size() > 0) {
            tool_definitions = tool_registry_->get_tool_definitions_json();
        }
        
        LOG_LLM(std::string("Calling LLM with prompt: \"") + llm_prompt + "\"");
        
        // Translator path: use dedicated translation model when configured
        bool use_translation_model = (config_.llm.agent_persona == "translator"
                                      && !config_.llm.translation_model.empty());
        std::string translation_system_prompt;
        std::vector<std::string> llm_history = session_history;
        std::string model_override;
        std::string system_prompt_override;
        if (use_translation_model) {
            model_override = config_.llm.translation_model;
            std::string lang = config_.llm.response_language.empty() ? "Spanish" : config_.llm.response_language;
            if (lang == "es") lang = "Spanish";
            else if (lang == "fr") lang = "French";
            else if (lang == "de") lang = "German";
            translation_system_prompt = "You are a professional English to " + lang + " translator. "
                "Output only the " + lang + " translation, no explanations. End transmissions with \"over\".";
            system_prompt_override = translation_system_prompt;
            llm_history.clear();  // Translation is stateless: single user message only
        }
        
        // Main LLM loop with tool execution (only if tools are enabled)
        std::string llm_response;
        std::string llm_stop_reason;
        
        if (tool_definitions.empty()) {
            // Streaming path: Ollama only, no tools, not waiting for channel clear
            bool use_streaming = false;  // Disabled: full response then single TTS (smoother playback)
            if (use_streaming) {
                std::string stream_buffer;
                bool first_sentence_sent = false;
                AudioBuffer full_response_audio;
                auto on_delta = [this, &stream_buffer, &first_sentence_sent, &full_response_audio](
                                    const std::string& delta) {
                    stream_buffer += delta;
                    for (;;) {
                        // For first chunk only: try short phrase (comma / 5 words / 20 chars) for lower latency
                        std::string chunk = first_sentence_sent
                            ? extract_first_sentence(stream_buffer)
                            : extract_first_phrase(stream_buffer);
                        if (chunk.empty())
                            chunk = extract_first_sentence(stream_buffer);
                        if (chunk.empty()) break;
                        std::string chunk_text = utils::trim_copy(chunk);
                        if (chunk_text.empty()) continue;
                        if (!first_sentence_sent) {
                            first_sentence_sent = true;
                            AudioBuffer first_audio = tts_->synth_vox(chunk_text);
                            full_response_audio = first_audio;
                            state_machine_->on_response_ready(first_audio);
                            vad_->reset();
                            tx_->transmit(first_audio);
                        } else {
                            AudioBuffer chunk_audio = tts_->synth(chunk_text);
                            full_response_audio.insert(full_response_audio.end(),
                                                       chunk_audio.begin(), chunk_audio.end());
                            tx_->transmit_append(chunk_audio);
                        }
                    }
                };
                llm_response = llm_->generate_ollama_chat_stream(
                    llm_prompt, llm_history,
                    config_.llm.timeout_ms, config_.llm.max_tokens,
                    model_override, system_prompt_override, on_delta);
                llm_stop_reason = "stop";
                // Flush remainder
                std::string remainder = utils::trim_copy(stream_buffer);
                if (!remainder.empty()) {
                    if (!first_sentence_sent) {
                        AudioBuffer first_audio = tts_->synth_vox(remainder);
                        full_response_audio = first_audio;
                        state_machine_->on_response_ready(first_audio);
                        vad_->reset();
                        tx_->transmit(first_audio);
                    } else {
                        AudioBuffer chunk_audio = tts_->synth(remainder);
                        full_response_audio.insert(full_response_audio.end(),
                                                   chunk_audio.begin(), chunk_audio.end());
                        tx_->transmit_append(chunk_audio);
                    }
                }
                if (llm_response.empty()) {
                    llm_response = (config_.llm.response_language == "es")
                        ? "Un momento."
                        : config_.llm.truncation.fallback_phrase;
                }
                if (session_memory_) {
                    session_memory_->add_user_message(llm_prompt);
                    session_memory_->add_assistant_message(llm_response);
                }
                recorder_->record_llm_response(llm_response, utterance_id);
                recorder_->record_tts_output(full_response_audio, utterance_id);
                if (session_memory_ && session_memory_->message_count() >= 4) {
                    std::vector<std::string> snapshot = session_memory_->to_json_strings();
                    { std::lock_guard<std::mutex> lock(job_mutex_); pending_snapshot_ = std::move(snapshot); }
                    job_cv_.notify_one();
                }
                return;
            }
            // No tools enabled - use simple direct LLM call with session history (or translation path)
            LLMResponse response = llm_->generate_with_tools(
                llm_prompt,
                "",  // No tool definitions
                llm_history,
                config_.llm.timeout_ms,
                config_.llm.max_tokens,
                model_override,
                system_prompt_override
            );
            llm_stop_reason = response.stop_reason;
            llm_response = response.content;
            if (response.stop_reason == "length") {
                LOG_LLM("Truncated response (done_reason=length), using fallback");
                llm_response = (config_.llm.response_language == "es")
                    ? "Un momento."
                    : config_.llm.truncation.fallback_phrase;
            }
            if (session_memory_) {
                session_memory_->add_user_message(llm_prompt);
                session_memory_->add_assistant_message(llm_response);
            }
        } else {
            // Tools enabled - use tool execution loop with session history + current turn
            std::vector<std::string> conversation_history = session_history;
            json user_msg;
            user_msg["role"] = "user";
            user_msg["content"] = llm_prompt;
            conversation_history.push_back(user_msg.dump());
            
            int max_iterations = 5;  // Prevent infinite loops
            int iteration = 0;
            
            while (iteration < max_iterations) {
                iteration++;
                
                LLMResponse response = llm_->generate_with_tools(
                    "",  // Don't send prompt again - it's in conversation_history
                    tool_definitions,
                    conversation_history,
                    config_.llm.timeout_ms,
                    config_.llm.max_tokens,
                    "",  // No model override when tools enabled
                    ""
                );
                
                // Add assistant message to conversation
                json assistant_msg;
                assistant_msg["role"] = "assistant";
                if (response.has_content()) {
                    assistant_msg["content"] = response.content;
                }
                if (response.has_tool_calls()) {
                    assistant_msg["tool_calls"] = json::array();
                    for (const auto& tc : response.tool_calls) {
                        json tool_call;
                        tool_call["id"] = tc.id;
                        tool_call["type"] = "function";
                        tool_call["function"]["name"] = tc.name;
                        tool_call["function"]["arguments"] = tc.arguments;
                        assistant_msg["tool_calls"].push_back(tool_call);
                    }
                }
                conversation_history.push_back(assistant_msg.dump());
                
                // If no tool calls, we're done (LLM responded directly)
                if (!response.has_tool_calls()) {
                    llm_stop_reason = response.stop_reason;
                    if (response.has_content()) {
                        llm_response = response.content;
                        if (response.stop_reason == "length") {
                            LOG_LLM("Truncated response (done_reason=length), using fallback");
                            llm_response = (config_.llm.response_language == "es")
                                ? "Un momento."
                                : config_.llm.truncation.fallback_phrase;
                        }
                    } else {
                        // Empty response, use fallback
                        llm_response = (config_.llm.response_language == "es")
                            ? "Un momento."
                            : config_.llm.truncation.fallback_phrase;
                    }
                    break;
                }
                
                // Execute tools
                LOG_LLM("Tool calls detected: " + std::to_string(response.tool_calls.size()));
                std::vector<ToolExecutionResult> tool_results;
                
                for (const auto& tool_call : response.tool_calls) {
                    ToolExecutionRequest call;
                    call.tool_name = tool_call.name;
                    call.tool_call_id = tool_call.id;
                    call.params_json = tool_call.arguments;
                    
                    LOG_LLM("Executing tool: " + call.tool_name);
                    
                    // Execute synchronously for now (can be made async later)
                    auto result = tool_executor_->execute_sync(call, config_.tools.timeout_ms);
                    tool_results.push_back(result);
                    
                    // Add tool result to conversation
                    std::string result_msg = LLMClient::format_tool_result(
                        result.tool_call_id,
                        result.result.success ? result.result.content : ("Error: " + result.result.error)
                    );
                    conversation_history.push_back(result_msg);
                }
                
                // Continue loop to get LLM response with tool results
            }
            
            // If we hit max iterations, use last response or fallback
            if (iteration >= max_iterations && llm_response.empty()) {
                LOG_LLM("Max tool execution iterations reached");
                llm_response = (config_.llm.response_language == "es")
                    ? "Un momento."
                    : config_.llm.truncation.fallback_phrase;
            }
            if (session_memory_) {
                session_memory_->add_user_message(llm_prompt);
                session_memory_->add_assistant_message(llm_response);
            }
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
        
        // Check if response is empty or just whitespace
        std::string trimmed_response = utils::trim_copy(llm_response);
        if (trimmed_response.empty() || utils::is_empty_or_whitespace(trimmed_response)) {
            LOG_LLM("Empty response, using fallback");
            llm_response = (config_.llm.response_language == "es")
                ? "Un momento."
                : config_.llm.truncation.fallback_phrase;
        }
        
        // Synthesize response (ensure transmission ends with " over.")
        std::string response_text = utils::ensure_ends_with_over(llm_response);
        LOG_TTS("Synthesizing response...");
        auto tts_start = std::chrono::steady_clock::now();
        response_audio = tts_->synth_vox(response_text);
        auto tts_end = std::chrono::steady_clock::now();
        int64_t tts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            tts_end - tts_start).count();
        {
            std::ostringstream toss;
            toss << "samples=" << response_audio.size() << " latency_ms=" << tts_ms;
            LOG_TRACE(utterance_id, "tts", toss.str());
        }
        recorder_->record_tts_output(response_audio, utterance_id);
        if (wait_for_channel_clear) {
            if (session_memory_ && session_memory_->message_count() >= 4) {
                std::vector<std::string> snapshot = session_memory_->to_json_strings();
                {
                    std::lock_guard<std::mutex> lock(job_mutex_);
                    pending_snapshot_ = std::move(snapshot);
                }
                job_cv_.notify_one();
            }
            return;
        }
        state_machine_->on_response_ready(response_audio);
        vad_->reset();
        LOG_TX("Transmitting response (" + std::to_string(response_audio.size()) + " samples)...");
        tx_->transmit(response_audio);
        if (session_memory_ && session_memory_->message_count() >= 4) {
            std::vector<std::string> snapshot = session_memory_->to_json_strings();
            {
                std::lock_guard<std::mutex> lock(job_mutex_);
                pending_snapshot_ = std::move(snapshot);
            }
            job_cv_.notify_one();
        }
    }

    void execute_fallback(const Plan& plan, AudioBuffer& response_audio, int utterance_id,
                         bool wait_for_channel_clear = false) {
        std::string text = utils::ensure_ends_with_over(plan.fallback_text);
        LOG_ROUTER(std::string("Fallback - speaking: \"") + text + "\"");
        response_audio = tts_->synth_vox(text);
        recorder_->record_tts_output(response_audio, utterance_id);
        if (wait_for_channel_clear) return;
        state_machine_->on_response_ready(response_audio);
        vad_->reset();
        tx_->transmit(response_audio);
    }
    
    void cleanup() {
        // Worker thread is joined in shutdown() (called from destructor or by user)
        if (tx_) {
            tx_->stop();
        }
        if (audio_io_) {
            audio_io_->stop();
        }
        if (recorder_) {
            recorder_->finalize_session();
            Logger::info("Session saved: " + recorder_->get_session_id());
        }
    }

    static bool last_user_turn_is_low_signal(const std::vector<std::string>& snapshot) {
        std::string last_user_content;
        for (auto it = snapshot.rbegin(); it != snapshot.rend(); ++it) {
            try {
                json msg = json::parse(*it);
                std::string role = msg.value("role", "");
                if (role == "user") {
                    last_user_content = msg.value("content", "");
                    break;
                }
            } catch (...) { continue; }
        }
        std::string trimmed = utils::trim_copy(last_user_content);
        return trimmed.empty() || static_cast<int>(trimmed.size()) < 2;
    }

    void summarizer_worker_loop() {
        while (true) {
            std::vector<std::string> job;
            {
                std::unique_lock<std::mutex> lock(job_mutex_);
                job_cv_.wait(lock, [this] {
                    return worker_shutdown_ || !pending_snapshot_.empty();
                });
                if (worker_shutdown_) break;
                job = std::move(pending_snapshot_);
                pending_snapshot_.clear();
            }
            if (last_user_turn_is_low_signal(job)) continue;
            std::string formatted = format_snapshot_for_summary(job);
            if (formatted.empty()) continue;
            std::string s = summarizer_llm_->summarize_conversation(formatted, config_.llm.timeout_ms);
            if (!s.empty()) {
                std::lock_guard<std::mutex> lock(summary_mutex_);
                context_summary_ = s;
                LOG_LLM("Context summary updated");
            }
        }
    }

    // Extract first short phrase for low-latency first chunk only (comma, 5 words, or 20 chars).
    // Returns empty if we should wait for more.
    static std::string extract_first_phrase(std::string& buffer) {
        const size_t MIN_PHRASE_CHARS = 20;
        const size_t MIN_PHRASE_WORDS = 5;
        std::string trimmed = utils::trim_copy(buffer);
        if (trimmed.empty()) return "";
        // Count words (space-separated)
        size_t word_count = 0;
        size_t i = 0;
        while (i < trimmed.size()) {
            while (i < trimmed.size() && (trimmed[i] == ' ' || trimmed[i] == '\n')) ++i;
            if (i >= trimmed.size()) break;
            ++word_count;
            while (i < trimmed.size() && trimmed[i] != ' ' && trimmed[i] != '\n') ++i;
        }
        // First comma with at least 2 words before it -> emit up to comma
        size_t comma = buffer.find(',');
        if (comma != std::string::npos) {
            std::string before = buffer.substr(0, comma);
            size_t spaces = 0;
            for (char c : before) { if (c == ' ' || c == '\n') ++spaces; }
            if (spaces >= 1) {  // at least 2 words
                std::string phrase = buffer.substr(0, comma + 1);
                buffer.erase(0, comma + 1);
                return phrase;
            }
        }
        // At least 5 words -> emit first 5 words
        if (word_count >= MIN_PHRASE_WORDS) {
            size_t pos = 0;
            int w = 0;
            while (pos < buffer.size() && w < static_cast<int>(MIN_PHRASE_WORDS)) {
                while (pos < buffer.size() && (buffer[pos] == ' ' || buffer[pos] == '\n')) ++pos;
                if (pos >= buffer.size()) break;
                ++w;
                while (pos < buffer.size() && buffer[pos] != ' ' && buffer[pos] != '\n') ++pos;
            }
            if (w == static_cast<int>(MIN_PHRASE_WORDS)) {
                std::string phrase = buffer.substr(0, pos);
                buffer.erase(0, pos);
                return phrase;
            }
        }
        // At least 20 chars -> emit first 20 (to last space)
        if (buffer.size() >= MIN_PHRASE_CHARS) {
            size_t last_space = std::string::npos;
            for (size_t j = 0; j < MIN_PHRASE_CHARS && j < buffer.size(); ++j) {
                if (buffer[j] == ' ' || buffer[j] == '\n') last_space = j;
            }
            size_t end = (last_space != std::string::npos) ? last_space : (MIN_PHRASE_CHARS - 1);
            std::string phrase = buffer.substr(0, end + 1);
            buffer.erase(0, end + 1);
            return phrase;
        }
        return "";
    }

    // Extract first complete sentence from stream buffer; remove it from buffer.
    // Sentence = first . ! ? (followed by space/newline/end), or first newline, or first ~40 chars.
    static std::string extract_first_sentence(std::string& buffer) {
        const size_t MIN_SENTENCE_CHARS = 40;
        size_t end_pos = std::string::npos;
        for (size_t i = 0; i < buffer.size(); ++i) {
            char c = buffer[i];
            if (c == '.' || c == '!' || c == '?') {
                if (i + 1 >= buffer.size() || buffer[i + 1] == ' ' || buffer[i + 1] == '\n' || buffer[i + 1] == '\r') {
                    end_pos = i;
                    break;
                }
            }
            if (c == '\n') {
                if (end_pos == std::string::npos || i < end_pos) end_pos = i;
                break;
            }
        }
        if (end_pos == std::string::npos && buffer.size() >= MIN_SENTENCE_CHARS) {
            size_t last_space = std::string::npos;
            for (size_t i = 0; i < MIN_SENTENCE_CHARS && i < buffer.size(); ++i) {
                if (buffer[i] == ' ' || buffer[i] == '\n') last_space = i;
            }
            end_pos = (last_space != std::string::npos) ? last_space : (MIN_SENTENCE_CHARS - 1);
        }
        if (end_pos == std::string::npos) return "";
        std::string sentence = buffer.substr(0, end_pos + 1);
        buffer.erase(0, end_pos + 1);
        return sentence;
    }

    static std::string format_snapshot_for_summary(const std::vector<std::string>& snapshot) {
        std::ostringstream out;
        for (const auto& msg_str : snapshot) {
            try {
                json msg = json::parse(msg_str);
                std::string role = msg.value("role", "");
                if (role == "system") continue;  // Skip system prompt so summarizer sees only dialogue
                std::string content = msg.value("content", "");
                if (content.empty()) continue;
                out << role << ": " << content << "\n";
            } catch (...) { continue; }
        }
        return out.str();
    }
    
    Config config_;
    std::atomic<bool> running_;
    bool initialized_;
    
    // Twitter flow state (regex-driven draft-tweet flow when persona is "twitter")
    TwitterFlowState twitter_flow_state_ = TwitterFlowState::Idle;
    
    // Track transmission end time for guard period
    std::chrono::steady_clock::time_point transmission_end_time_;
    
    // Track previous state for transition detection
    State previous_state_;
    
    // Track speech frame count for logging
    int speech_frame_count_;
    
        // Track speech timing for periodic logging
    std::chrono::steady_clock::time_point speech_start_time_;
    std::chrono::steady_clock::time_point last_speech_log_time_;

    // Half-duplex: channel clear before TX
    std::chrono::steady_clock::time_point last_speech_end_time_;
    AudioBuffer pending_response_audio_;
    
    std::unique_ptr<AudioIO> audio_io_;
    std::unique_ptr<VADEndpointing> vad_;
    std::unique_ptr<STTEngine> stt_;
    std::unique_ptr<Router> router_;
    std::unique_ptr<LLMClient> llm_;
    std::unique_ptr<TTSEngine> tts_;
        std::unique_ptr<TXController> tx_;
        std::unique_ptr<StateMachine> state_machine_;
        std::unique_ptr<SessionRecorder> recorder_;
        
        // Tool system
        std::unique_ptr<ToolRegistry> tool_registry_;
        std::unique_ptr<ToolExecutor> tool_executor_;

        // Session conversation memory (past user/assistant turns)
        std::unique_ptr<memory::ConversationMemory> session_memory_;

        // Background context summarization (non-blocking)
        std::mutex summary_mutex_;
        std::string context_summary_;
        std::unique_ptr<LLMClient> summarizer_llm_;
        std::mutex job_mutex_;
        std::condition_variable job_cv_;
        std::vector<std::string> pending_snapshot_;
        std::atomic<bool> worker_shutdown_{false};
        std::thread worker_thread_;
};

VoiceAgent::VoiceAgent(const Config& config)
    : pimpl_(std::make_unique<Impl>(config)) {}

VoiceAgent::~VoiceAgent() = default;

bool VoiceAgent::initialize() {
    return pimpl_->initialize();
}

int VoiceAgent::run() {
    return pimpl_->run();
}

void VoiceAgent::shutdown() {
    pimpl_->shutdown();
}

} // namespace memo_rf
