/**
 * @file config.cpp
 * @brief Configuration loading and validation
 */

#include "core/config.h"
#include "logger.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>

using json = nlohmann::json;

namespace memo_rf {
namespace config {

// =============================================================================
// JSON Serialization Helpers
// =============================================================================

namespace {

template<typename T>
T get_or_default(const json& j, const std::string& key, const T& default_val) {
    if (j.contains(key) && !j[key].is_null()) {
        return j[key].get<T>();
    }
    return default_val;
}

template<typename T>
std::vector<T> get_array_or_default(const json& j, const std::string& key,
                                    const std::vector<T>& default_val) {
    if (j.contains(key) && j[key].is_array()) {
        return j[key].get<std::vector<T>>();
    }
    return default_val;
}

AudioConfig parse_audio_config(const json& j) {
    AudioConfig config;
    if (!j.contains("audio")) return config;

    const auto& audio = j["audio"];
    config.input_device = get_or_default(audio, "input_device", config.input_device);
    config.output_device = get_or_default(audio, "output_device", config.output_device);
    config.sample_rate = get_or_default(audio, "sample_rate", config.sample_rate);
    return config;
}

VADConfig parse_vad_config(const json& j) {
    VADConfig config;
    if (!j.contains("vad")) return config;

    const auto& vad = j["vad"];
    config.threshold = get_or_default(vad, "threshold", config.threshold);
    config.min_speech_ms = get_or_default(vad, "min_speech_ms", config.min_speech_ms);
    config.end_silence_ms = get_or_default(vad, "end_of_utterance_silence_ms", config.end_silence_ms);
    config.hangover_ms = get_or_default(vad, "hangover_ms", config.hangover_ms);
    config.pause_tolerance_ms = get_or_default(vad, "pause_tolerance_ms", config.pause_tolerance_ms);
    config.pre_speech_buffer_ms = get_or_default(vad, "pre_speech_buffer_ms", config.pre_speech_buffer_ms);
    config.adaptive_threshold = get_or_default(vad, "adaptive_threshold", config.adaptive_threshold);
    config.debug_log_frames = get_or_default(vad, "debug_log_rms_each_frame", config.debug_log_frames);
    return config;
}

STTConfig parse_stt_config(const json& j) {
    STTConfig config;
    if (!j.contains("stt")) return config;

    const auto& stt = j["stt"];
    config.model_path = get_or_default(stt, "model_path", config.model_path);
    config.language = get_or_default(stt, "language", config.language);
    return config;
}

LLMConfig parse_llm_config(const json& j) {
    LLMConfig config;
    if (!j.contains("llm")) return config;

    const auto& llm = j["llm"];
    config.endpoint = get_or_default(llm, "endpoint", config.endpoint);
    config.model_name = get_or_default(llm, "model_name", config.model_name);
    config.timeout_ms = get_or_default(llm, "timeout_ms", config.timeout_ms);
    config.max_tokens = get_or_default(llm, "max_tokens", config.max_tokens);
    config.temperature = get_or_default(llm, "temperature", config.temperature);
    config.stop_sequences = get_array_or_default(llm, "stop_sequences", config.stop_sequences);
    return config;
}

TTSConfig parse_tts_config(const json& j) {
    TTSConfig config;
    if (!j.contains("tts")) return config;

    const auto& tts = j["tts"];
    config.voice_path = get_or_default(tts, "voice_path", config.voice_path);
    config.espeak_data_path = get_or_default(tts, "espeak_data_path", config.espeak_data_path);
    config.piper_path = get_or_default(tts, "piper_path", config.piper_path);
    config.preroll_ms = get_or_default(tts, "vox_preroll_ms", config.preroll_ms);
    config.output_gain = get_or_default(tts, "output_gain", config.output_gain);
    config.preload_phrases = get_array_or_default(tts, "preload_phrases", config.preload_phrases);
    return config;
}

TXConfig parse_tx_config(const json& j) {
    TXConfig config;
    if (!j.contains("tx")) return config;

    const auto& tx = j["tx"];
    config.max_transmit_ms = get_or_default(tx, "max_transmit_ms", config.max_transmit_ms);
    config.enable_start_chirp = get_or_default(tx, "enable_start_chirp", config.enable_start_chirp);
    config.enable_end_chirp = get_or_default(tx, "enable_end_chirp", config.enable_end_chirp);
    return config;
}

ToolsConfig parse_tools_config(const json& j) {
    ToolsConfig config;
    if (!j.contains("tools")) return config;

    const auto& tools = j["tools"];
    config.enabled = get_array_or_default<std::string>(tools, "enabled", config.enabled);
    config.timeout_ms = get_or_default(tools, "timeout_ms", config.timeout_ms);
    config.max_concurrent = get_or_default(tools, "max_concurrent", config.max_concurrent);
    return config;
}

MemoryConfig parse_memory_config(const json& j) {
    MemoryConfig config;
    if (!j.contains("memory")) return config;

    const auto& memory = j["memory"];
    config.enabled = get_or_default(memory, "enabled", config.enabled);
    config.max_messages = get_or_default(memory, "max_messages", config.max_messages);
    config.max_tokens = get_or_default(memory, "max_tokens", config.max_tokens);
    config.system_prompt = get_or_default(memory, "system_prompt", config.system_prompt);
    config.persistence_path = get_or_default(memory, "persistence_path", config.persistence_path);
    config.auto_save = get_or_default(memory, "auto_save", config.auto_save);
    return config;
}

json audio_config_to_json(const AudioConfig& config) {
    return {
        {"input_device", config.input_device},
        {"output_device", config.output_device},
        {"sample_rate", config.sample_rate}
    };
}

json vad_config_to_json(const VADConfig& config) {
    return {
        {"threshold", config.threshold},
        {"min_speech_ms", config.min_speech_ms},
        {"end_of_utterance_silence_ms", config.end_silence_ms},
        {"hangover_ms", config.hangover_ms},
        {"pause_tolerance_ms", config.pause_tolerance_ms},
        {"pre_speech_buffer_ms", config.pre_speech_buffer_ms},
        {"adaptive_threshold", config.adaptive_threshold},
        {"debug_log_rms_each_frame", config.debug_log_frames}
    };
}

json stt_config_to_json(const STTConfig& config) {
    return {
        {"model_path", config.model_path},
        {"language", config.language}
    };
}

json llm_config_to_json(const LLMConfig& config) {
    return {
        {"endpoint", config.endpoint},
        {"model_name", config.model_name},
        {"timeout_ms", config.timeout_ms},
        {"max_tokens", config.max_tokens},
        {"temperature", config.temperature},
        {"stop_sequences", config.stop_sequences}
    };
}

json tts_config_to_json(const TTSConfig& config) {
    return {
        {"voice_path", config.voice_path},
        {"espeak_data_path", config.espeak_data_path},
        {"piper_path", config.piper_path},
        {"vox_preroll_ms", config.preroll_ms},
        {"output_gain", config.output_gain},
        {"preload_phrases", config.preload_phrases}
    };
}

json tx_config_to_json(const TXConfig& config) {
    return {
        {"max_transmit_ms", config.max_transmit_ms},
        {"enable_start_chirp", config.enable_start_chirp},
        {"enable_end_chirp", config.enable_end_chirp}
    };
}

json tools_config_to_json(const ToolsConfig& config) {
    return {
        {"enabled", config.enabled},
        {"timeout_ms", config.timeout_ms},
        {"max_concurrent", config.max_concurrent}
    };
}

json memory_config_to_json(const MemoryConfig& config) {
    return {
        {"enabled", config.enabled},
        {"max_messages", config.max_messages},
        {"max_tokens", config.max_tokens},
        {"system_prompt", config.system_prompt},
        {"persistence_path", config.persistence_path},
        {"auto_save", config.auto_save}
    };
}

} // anonymous namespace

// =============================================================================
// AgentConfig Implementation
// =============================================================================

Result<AgentConfig> AgentConfig::load(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            return Result<AgentConfig>::failure("Failed to open config file: " + path);
        }

        json j = json::parse(file);
        AgentConfig config;

        config.audio = parse_audio_config(j);
        config.vad = parse_vad_config(j);
        config.stt = parse_stt_config(j);
        config.llm = parse_llm_config(j);
        config.tts = parse_tts_config(j);
        config.tx = parse_tx_config(j);
        config.tools = parse_tools_config(j);
        config.memory = parse_memory_config(j);

        config.session_log_dir = get_or_default(j, "session_log_dir", config.session_log_dir);
        config.enable_replay_mode = get_or_default(j, "enable_replay_mode", config.enable_replay_mode);
        config.replay_wav_path = get_or_default(j, "replay_wav_path", config.replay_wav_path);

        // Validate
        std::string validation_error = config.validate();
        if (!validation_error.empty()) {
            return Result<AgentConfig>::failure("Config validation failed: " + validation_error);
        }

        Logger::info("Configuration loaded from: " + path);
        return Result<AgentConfig>::success(std::move(config));

    } catch (const json::exception& e) {
        return Result<AgentConfig>::failure(std::string("JSON parse error: ") + e.what());
    } catch (const std::exception& e) {
        return Result<AgentConfig>::failure(std::string("Error loading config: ") + e.what());
    }
}

VoidResult AgentConfig::save(const std::string& path) const {
    try {
        json j;

        j["audio"] = audio_config_to_json(audio);
        j["vad"] = vad_config_to_json(vad);
        j["stt"] = stt_config_to_json(stt);
        j["llm"] = llm_config_to_json(llm);
        j["tts"] = tts_config_to_json(tts);
        j["tx"] = tx_config_to_json(tx);
        j["tools"] = tools_config_to_json(tools);
        j["memory"] = memory_config_to_json(memory);

        j["session_log_dir"] = session_log_dir;
        j["enable_replay_mode"] = enable_replay_mode;
        j["replay_wav_path"] = replay_wav_path;

        std::ofstream file(path);
        if (!file.is_open()) {
            return VoidResult::failure("Failed to open file for writing: " + path);
        }

        file << j.dump(2);
        Logger::info("Configuration saved to: " + path);
        return VoidResult::ok_result();

    } catch (const std::exception& e) {
        return VoidResult::failure(std::string("Error saving config: ") + e.what());
    }
}

AgentConfig AgentConfig::defaults() {
    return AgentConfig{};  // All defaults are set in struct definitions
}

std::string AgentConfig::validate() const {
    std::ostringstream errors;

    // Validate STT
    if (stt.model_path.empty()) {
        errors << "stt.model_path is required; ";
    }

    // Validate TTS
    if (tts.voice_path.empty()) {
        errors << "tts.voice_path is required; ";
    }

    // Validate VAD
    if (vad.threshold <= 0 || vad.threshold > 1.0) {
        errors << "vad.threshold must be between 0 and 1; ";
    }

    // Validate LLM
    if (llm.endpoint.empty()) {
        errors << "llm.endpoint is required; ";
    }

    return errors.str();
}

} // namespace config
} // namespace memo_rf
