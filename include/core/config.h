#pragma once

/**
 * @file config.h
 * @brief Unified configuration system
 *
 * This file defines the configuration structure for the entire agent.
 * It supports:
 * - JSON file loading
 * - Environment variable overrides
 * - Default values
 * - Validation
 */

#include "types.h"
#include "constants.h"
#include <string>
#include <vector>
#include <optional>

namespace memo_rf {
namespace config {

// =============================================================================
// Component Configurations
// =============================================================================

/**
 * @brief Audio I/O configuration
 */
struct AudioConfig {
    std::string input_device = "default";
    std::string output_device = "default";
    int sample_rate = audio::SAMPLE_RATE;
};

/**
 * @brief VAD configuration
 */
struct VADConfig {
    float threshold = constants::vad::DEFAULT_THRESHOLD;
    int min_speech_ms = constants::vad::MIN_SPEECH_MS;
    int end_silence_ms = constants::vad::END_SILENCE_MS;
    int hangover_ms = constants::vad::HANGOVER_MS;
    int pause_tolerance_ms = constants::vad::PAUSE_TOLERANCE_MS;
    int pre_speech_buffer_ms = constants::vad::PRE_SPEECH_BUFFER_MS;
    bool adaptive_threshold = true;
    bool debug_log_frames = false;
};

/**
 * @brief STT (Speech-to-Text) configuration
 */
struct STTConfig {
    std::string model_path;
    std::string language = "en";
};

/**
 * @brief LLM configuration
 */
struct LLMConfig {
    std::string endpoint = "http://localhost:11434/api/chat";
    std::string model_name = "qwen2.5:7b";
    int timeout_ms = constants::llm::DEFAULT_TIMEOUT_MS;
    int max_tokens = constants::llm::DEFAULT_MAX_TOKENS;
    float temperature = constants::llm::DEFAULT_TEMPERATURE;
    std::vector<std::string> stop_sequences = {"</s>", "\n\n", "User:", "Human:"};
};

/**
 * @brief TTS configuration
 */
struct TTSConfig {
    std::string voice_path;
    std::string espeak_data_path = "/opt/homebrew/share/espeak-ng-data";
    std::string piper_path;  // Empty = auto-detect
    int preroll_ms = constants::tts::VOX_PREROLL_MS;
    float output_gain = 1.0f;
    std::vector<std::string> preload_phrases = {
        "roger.",
        "affirmative.",
        "negative.",
        "stand by.",
        "copy.",
        "over."
    };
};

/**
 * @brief Transmission configuration
 */
struct TXConfig {
    int max_transmit_ms = constants::tx::MAX_TRANSMIT_MS;
    bool enable_start_chirp = false;
    bool enable_end_chirp = false;
};

/**
 * @brief Tool system configuration
 */
struct ToolsConfig {
    std::vector<std::string> enabled;
    int timeout_ms = 5000;
    size_t max_concurrent = 1;
};

/**
 * @brief Conversation memory configuration
 */
struct MemoryConfig {
    size_t max_messages = constants::memory::MAX_HISTORY_MESSAGES;
    size_t max_tokens = constants::memory::MAX_HISTORY_TOKENS;
    std::string system_prompt = "You are a military field operator on tactical radio. "
                                "Use clear, concise comms. Be succinct: one short sentence, "
                                "under 15 words when possible. No preamble. "
                                "Answer in standard radio procedure.";
    std::string persistence_path;  // Empty = no persistence
    bool auto_save = false;
};

// =============================================================================
// Main Configuration
// =============================================================================

/**
 * @brief Complete agent configuration
 */
struct AgentConfig {
    AudioConfig audio;
    VADConfig vad;
    STTConfig stt;
    LLMConfig llm;
    TTSConfig tts;
    TXConfig tx;
    ToolsConfig tools;
    MemoryConfig memory;

    std::string session_log_dir = "sessions";
    bool enable_replay_mode = false;
    std::string replay_wav_path;

    /**
     * @brief Load configuration from JSON file
     * @param path Path to JSON config file
     * @return Loaded config or error
     */
    static Result<AgentConfig> load(const std::string& path);

    /**
     * @brief Save configuration to JSON file
     * @param path Path to save to
     * @return Success or error
     */
    VoidResult save(const std::string& path) const;

    /**
     * @brief Create with default values
     */
    static AgentConfig defaults();

    /**
     * @brief Validate configuration
     * @return Error message if invalid, empty if valid
     */
    std::string validate() const;
};

} // namespace config

// Convenience alias
using Config = config::AgentConfig;

} // namespace memo_rf
