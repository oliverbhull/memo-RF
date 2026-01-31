#include "config.h"
#include "logger.h"
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {

/// Resolve agent_persona from personas.json in the same directory as the config file.
/// On success: sets cfg.llm.system_prompt and cfg.llm.persona_name. On failure: logs warning, leaves system_prompt unchanged.
void resolve_persona(memo_rf::Config& cfg, const std::string& config_path) {
    const std::string& id = cfg.llm.agent_persona;
    if (id.empty()) return;

    std::string config_dir;
    std::string::size_type pos = config_path.find_last_of("/\\");
    if (pos != std::string::npos) {
        config_dir = config_path.substr(0, pos + 1);
    }
    std::string personas_path = config_dir + "personas.json";

    std::ifstream pf(personas_path);
    if (!pf.is_open()) {
        memo_rf::Logger::warn("agent_persona \"" + id + "\" set but could not open " + personas_path + "; using existing system_prompt.");
        return;
    }
    json personas_json;
    try {
        pf >> personas_json;
    } catch (const json::exception& e) {
        memo_rf::Logger::warn("agent_persona \"" + id + "\" set but failed to parse " + personas_path + ": " + e.what());
        return;
    }
    if (!personas_json.contains(id) || !personas_json[id].is_object()) {
        memo_rf::Logger::warn("agent_persona \"" + id + "\" not found in " + personas_path + "; using existing system_prompt.");
        return;
    }
    const auto& persona = personas_json[id];
    if (persona.contains("system_prompt") && persona["system_prompt"].is_string()) {
        cfg.llm.system_prompt = persona["system_prompt"].get<std::string>();
    }
    if (persona.contains("name") && persona["name"].is_string()) {
        cfg.llm.persona_name = persona["name"].get<std::string>();
    }
    memo_rf::Logger::info("Agent persona: " + (cfg.llm.persona_name.empty() ? id : cfg.llm.persona_name));
}

} // namespace

namespace memo_rf {

Config Config::load_from_file(const std::string& path) {
    Config cfg;
    
    std::ifstream file(path);
    if (!file.is_open()) {
        Logger::warn("Could not open config file: " + path + ". Using defaults.");
        return cfg;
    }
    
    json j;
    try {
        file >> j;
    } catch (const json::exception& e) {
        Logger::error("Error parsing config JSON: " + std::string(e.what()));
        return cfg;
    }
    
    // Audio config
    if (j.contains("audio")) {
        auto& a = j["audio"];
        if (a.contains("input_device")) cfg.audio.input_device = a["input_device"];
        if (a.contains("output_device")) cfg.audio.output_device = a["output_device"];
        if (a.contains("sample_rate")) cfg.audio.sample_rate = a["sample_rate"];
    }
    
    // VAD config
    if (j.contains("vad")) {
        auto& v = j["vad"];
        if (v.contains("threshold")) cfg.vad.threshold = v["threshold"];
        if (v.contains("end_of_utterance_silence_ms")) 
            cfg.vad.end_of_utterance_silence_ms = v["end_of_utterance_silence_ms"];
        if (v.contains("min_speech_ms")) cfg.vad.min_speech_ms = v["min_speech_ms"];
        if (v.contains("hangover_ms")) cfg.vad.hangover_ms = v["hangover_ms"];
        if (v.contains("pause_tolerance_ms")) cfg.vad.pause_tolerance_ms = v["pause_tolerance_ms"];
        if (v.contains("debug_log_rms_each_frame")) cfg.vad.debug_log_rms_each_frame = v["debug_log_rms_each_frame"];
    }
    
    // STT config
    if (j.contains("stt")) {
        auto& s = j["stt"];
        if (s.contains("model_path")) cfg.stt.model_path = s["model_path"];
        if (s.contains("language")) cfg.stt.language = s["language"];
    }
    
    // LLM config
    if (j.contains("llm")) {
        auto& l = j["llm"];
        if (l.contains("endpoint")) cfg.llm.endpoint = l["endpoint"];
        if (l.contains("timeout_ms")) cfg.llm.timeout_ms = l["timeout_ms"];
        if (l.contains("max_tokens")) cfg.llm.max_tokens = l["max_tokens"];
        if (l.contains("context_max_turns_to_send")) cfg.llm.context_max_turns_to_send = l["context_max_turns_to_send"];
        if (l.contains("model_name")) cfg.llm.model_name = l["model_name"];
        if (l.contains("temperature")) cfg.llm.temperature = l["temperature"];
        if (l.contains("system_prompt")) cfg.llm.system_prompt = l["system_prompt"];
        if (l.contains("agent_persona") && l["agent_persona"].is_string()) {
            cfg.llm.agent_persona = l["agent_persona"].get<std::string>();
        }
        if (l.contains("stop_sequences") && l["stop_sequences"].is_array()) {
            cfg.llm.stop_sequences.clear();
            for (const auto& seq : l["stop_sequences"]) {
                cfg.llm.stop_sequences.push_back(seq.get<std::string>());
            }
        }
    }
    
    // TTS config
    if (j.contains("tts")) {
        auto& t = j["tts"];
        if (t.contains("voice_path")) cfg.tts.voice_path = t["voice_path"];
        if (t.contains("vox_preroll_ms")) cfg.tts.vox_preroll_ms = t["vox_preroll_ms"];
        if (t.contains("vox_preroll_amplitude")) cfg.tts.vox_preroll_amplitude = t["vox_preroll_amplitude"];
        if (t.contains("output_gain")) cfg.tts.output_gain = t["output_gain"];
    }
    
    // TX config
    if (j.contains("tx")) {
        auto& tx = j["tx"];
        if (tx.contains("max_transmit_ms")) cfg.tx.max_transmit_ms = tx["max_transmit_ms"];
        if (tx.contains("standby_delay_ms")) cfg.tx.standby_delay_ms = tx["standby_delay_ms"];
        if (tx.contains("channel_clear_silence_ms")) cfg.tx.channel_clear_silence_ms = tx["channel_clear_silence_ms"];
        if (tx.contains("enable_start_chirp")) cfg.tx.enable_start_chirp = tx["enable_start_chirp"];
        if (tx.contains("enable_end_chirp")) cfg.tx.enable_end_chirp = tx["enable_end_chirp"];
    }

    // Wake word config (respond only on "hey memo" when enabled)
    if (j.contains("wake_word")) {
        auto& w = j["wake_word"];
        if (w.contains("enabled")) cfg.wake_word.enabled = w["enabled"];
    }
    
    // Tools config
    if (j.contains("tools")) {
        auto& tools = j["tools"];
        if (tools.contains("timeout_ms")) cfg.tools.timeout_ms = tools["timeout_ms"];
        if (tools.contains("max_concurrent")) cfg.tools.max_concurrent = tools["max_concurrent"];
        if (tools.contains("enabled") && tools["enabled"].is_array()) {
            cfg.tools.enabled.clear();
            for (const auto& tool_name : tools["enabled"]) {
                if (tool_name.is_string()) {
                    cfg.tools.enabled.push_back(tool_name.get<std::string>());
                }
            }
        }
    }
    
    // Session/replay
    if (j.contains("session_log_dir")) cfg.session_log_dir = j["session_log_dir"];
    if (j.contains("enable_replay_mode")) cfg.enable_replay_mode = j["enable_replay_mode"];
    if (j.contains("replay_wav_path")) cfg.replay_wav_path = j["replay_wav_path"];

    // Resolve agent persona from library (persona wins over inline system_prompt)
    resolve_persona(cfg, path);

    return cfg;
}

void Config::save_to_file(const std::string& path) const {
    json j;
    
    j["audio"]["input_device"] = audio.input_device;
    j["audio"]["output_device"] = audio.output_device;
    j["audio"]["sample_rate"] = audio.sample_rate;
    
    j["vad"]["threshold"] = vad.threshold;
    j["vad"]["end_of_utterance_silence_ms"] = vad.end_of_utterance_silence_ms;
    j["vad"]["min_speech_ms"] = vad.min_speech_ms;
    j["vad"]["hangover_ms"] = vad.hangover_ms;
    j["vad"]["pause_tolerance_ms"] = vad.pause_tolerance_ms;
    j["vad"]["debug_log_rms_each_frame"] = vad.debug_log_rms_each_frame;
    
    j["stt"]["model_path"] = stt.model_path;
    j["stt"]["language"] = stt.language;
    
    j["llm"]["endpoint"] = llm.endpoint;
    j["llm"]["timeout_ms"] = llm.timeout_ms;
    j["llm"]["max_tokens"] = llm.max_tokens;
    j["llm"]["context_max_turns_to_send"] = llm.context_max_turns_to_send;
    j["llm"]["model_name"] = llm.model_name;
    j["llm"]["temperature"] = llm.temperature;
    if (!llm.agent_persona.empty()) {
        j["llm"]["agent_persona"] = llm.agent_persona;
        // When using a persona, system_prompt is derived from personas.json at load time; do not persist it
    } else {
        j["llm"]["system_prompt"] = llm.system_prompt;
    }
    j["llm"]["stop_sequences"] = llm.stop_sequences;
    
    j["tts"]["voice_path"] = tts.voice_path;
    j["tts"]["vox_preroll_ms"] = tts.vox_preroll_ms;
    j["tts"]["vox_preroll_amplitude"] = tts.vox_preroll_amplitude;
    j["tts"]["output_gain"] = tts.output_gain;
    
    j["tx"]["max_transmit_ms"] = tx.max_transmit_ms;
    j["tx"]["standby_delay_ms"] = tx.standby_delay_ms;
    j["tx"]["channel_clear_silence_ms"] = tx.channel_clear_silence_ms;
    j["tx"]["enable_start_chirp"] = tx.enable_start_chirp;
    j["tx"]["enable_end_chirp"] = tx.enable_end_chirp;

    j["wake_word"]["enabled"] = wake_word.enabled;
    
    j["tools"]["enabled"] = tools.enabled;
    j["tools"]["timeout_ms"] = tools.timeout_ms;
    j["tools"]["max_concurrent"] = tools.max_concurrent;
    
    j["session_log_dir"] = session_log_dir;
    j["enable_replay_mode"] = enable_replay_mode;
    j["replay_wav_path"] = replay_wav_path;
    
    std::ofstream file(path);
    if (file.is_open()) {
        file << j.dump(2);
    }
}

} // namespace memo_rf
