#include "config.h"
#include "logger.h"
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

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
        if (l.contains("model_name")) cfg.llm.model_name = l["model_name"];
        if (l.contains("temperature")) cfg.llm.temperature = l["temperature"];
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
        if (t.contains("output_gain")) cfg.tts.output_gain = t["output_gain"];
    }
    
    // TX config
    if (j.contains("tx")) {
        auto& tx = j["tx"];
        if (tx.contains("max_transmit_ms")) cfg.tx.max_transmit_ms = tx["max_transmit_ms"];
        if (tx.contains("enable_start_chirp")) cfg.tx.enable_start_chirp = tx["enable_start_chirp"];
        if (tx.contains("enable_end_chirp")) cfg.tx.enable_end_chirp = tx["enable_end_chirp"];
    }
    
    // Session/replay
    if (j.contains("session_log_dir")) cfg.session_log_dir = j["session_log_dir"];
    if (j.contains("enable_replay_mode")) cfg.enable_replay_mode = j["enable_replay_mode"];
    if (j.contains("replay_wav_path")) cfg.replay_wav_path = j["replay_wav_path"];
    
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
    
    j["stt"]["model_path"] = stt.model_path;
    j["stt"]["language"] = stt.language;
    
    j["llm"]["endpoint"] = llm.endpoint;
    j["llm"]["timeout_ms"] = llm.timeout_ms;
    j["llm"]["max_tokens"] = llm.max_tokens;
    j["llm"]["model_name"] = llm.model_name;
    j["llm"]["temperature"] = llm.temperature;
    j["llm"]["stop_sequences"] = llm.stop_sequences;
    
    j["tts"]["voice_path"] = tts.voice_path;
    j["tts"]["vox_preroll_ms"] = tts.vox_preroll_ms;
    j["tts"]["output_gain"] = tts.output_gain;
    
    j["tx"]["max_transmit_ms"] = tx.max_transmit_ms;
    j["tx"]["enable_start_chirp"] = tx.enable_start_chirp;
    j["tx"]["enable_end_chirp"] = tx.enable_end_chirp;
    
    j["session_log_dir"] = session_log_dir;
    j["enable_replay_mode"] = enable_replay_mode;
    j["replay_wav_path"] = replay_wav_path;
    
    std::ofstream file(path);
    if (file.is_open()) {
        file << j.dump(2);
    }
}

} // namespace memo_rf
