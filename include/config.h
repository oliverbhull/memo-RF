#pragma once

#include <string>
#include <cstdint>
#include <vector>

namespace memo_rf {

struct AudioConfig {
    std::string input_device;
    std::string output_device;
    int sample_rate = 16000;
};

struct VADConfig {
    float threshold = 0.5f;
    int end_of_utterance_silence_ms = 1000;  // Silence duration before ending utterance
    int min_speech_ms = 200;                  // Minimum speech duration to be valid
    int hangover_ms = 200;                    // Grace period after speech end
    int pause_tolerance_ms = 500;             // Allow pauses this long during speech without ending
    bool debug_log_rms_each_frame = false;    // When true, log RMS and state every frame (verbose)
};

struct STTConfig {
    std::string model_path;
    std::string language = "en";
};

struct LLMConfig {
    std::string endpoint = "http://localhost:8080/completion";
    int timeout_ms = 2000;
    int max_tokens = 100;
    int context_max_turns_to_send = 6;  ///< Only send last N turns to LLM (bounded context)
    std::string model_name = "qwen";
    float temperature = 0.7f;
    std::vector<std::string> stop_sequences = {"</s>", "\n\n", "User:", "Human:"};
    std::string system_prompt = "You are a helpful radio operator supporting field operators. "
                               "Use clear, concise comms. Be succinct: one short sentence, under 15 words when possible. "
                               "No preamble. Answer in standard radio procedure.";
};

struct TTSConfig {
    std::string voice_path;
    int vox_preroll_ms = 350;       ///< Tone duration before speech so VOX opens (ms)
    float vox_preroll_amplitude = 0.55f;  ///< Tone amplitude 0–1 so VOX reliably triggers
    float output_gain = 1.0f;
};

struct TXConfig {
    int max_transmit_ms = 20000;  // Maximum transmission time in ms (0 = no limit)
    int standby_delay_ms = 200;   // Delay after user speech ends before sending "Standby, over" (ms)
    bool enable_start_chirp = false;
    bool enable_end_chirp = false;
};

struct ToolsConfig {
    std::vector<std::string> enabled;  // List of enabled tool names
    int timeout_ms = 5000;              // Default timeout for tool execution
    size_t max_concurrent = 1;          // Maximum concurrent tool executions
};

struct Config {
    AudioConfig audio;
    VADConfig vad;
    STTConfig stt;
    LLMConfig llm;
    TTSConfig tts;
    TXConfig tx;
    ToolsConfig tools;
    
    std::string session_log_dir = "sessions";
    bool enable_replay_mode = false;
    std::string replay_wav_path;
    
    static Config load_from_file(const std::string& path);
    void save_to_file(const std::string& path) const;
};

} // namespace memo_rf
