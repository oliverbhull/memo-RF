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
};

struct STTConfig {
    std::string model_path;
    std::string language = "en";
};

struct LLMConfig {
    std::string endpoint = "http://localhost:8080/completion";
    int timeout_ms = 2000;
    int max_tokens = 100;
    std::string model_name = "llama";
    float temperature = 0.7f;
    std::vector<std::string> stop_sequences = {"</s>", "\n\n", "User:", "Human:"};
};

struct TTSConfig {
    std::string voice_path;
    int vox_preroll_ms = 200;
    float output_gain = 1.0f;
};

struct TXConfig {
    int max_transmit_ms = 20000;  // Maximum transmission time in ms (0 = no limit)
    bool enable_start_chirp = false;
    bool enable_end_chirp = false;
};

struct Config {
    AudioConfig audio;
    VADConfig vad;
    STTConfig stt;
    LLMConfig llm;
    TTSConfig tts;
    TXConfig tx;
    
    std::string session_log_dir = "sessions";
    bool enable_replay_mode = false;
    std::string replay_wav_path;
    
    static Config load_from_file(const std::string& path);
    void save_to_file(const std::string& path) const;
};

} // namespace memo_rf
