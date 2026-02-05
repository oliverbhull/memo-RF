#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <chrono>
#include <memory>

namespace memo_rf {

// Audio types
using Sample = int16_t;
using AudioFrame = std::vector<Sample>;
using AudioBuffer = std::vector<Sample>;

// Timing
using TimePoint = std::chrono::steady_clock::time_point;
using Duration = std::chrono::milliseconds;

inline int64_t ms_since(TimePoint start) {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<Duration>(now - start).count();
}

// Audio format constants
constexpr int DEFAULT_SAMPLE_RATE = 16000;
constexpr int FRAME_SIZE_MS = 20;
constexpr int SAMPLES_PER_FRAME = (DEFAULT_SAMPLE_RATE * FRAME_SIZE_MS) / 1000; // 320 samples @ 16kHz

// VAD/Endpointing constants
constexpr int MIN_SPEECH_MS = 200;
constexpr int END_OF_UTTERANCE_SILENCE_MS_MIN = 200;
constexpr int END_OF_UTTERANCE_SILENCE_MS_MAX = 350;
constexpr int HANGOVER_MS = 50;

// Latency targets
constexpr int TARGET_ACK_LATENCY_MS = 500;
constexpr int TARGET_ANSWER_LATENCY_MS = 2000;
constexpr int MAX_TRANSMIT_MS = 20000;  // Default max transmit time (20 seconds)

// VOX pre-roll
constexpr int VOX_PREROLL_MS_MIN = 150;
constexpr int VOX_PREROLL_MS_MAX = 250;

// VAD guard period after transmission (prevents feedback)
constexpr int VAD_GUARD_PERIOD_MS = 1500; // Wait 1500ms after playback completes before re-enabling VAD

// Delay after playback_complete before transitioning to IdleListening (lets DAC/mic settle)
constexpr int POST_PLAYBACK_DELAY_MS = 1000;

// Transcript result
struct Transcript {
    std::string text;
    float confidence;
    int64_t processing_ms;
    int token_count = 0;  ///< Number of tokens from STT (0 if not set)
};

} // namespace memo_rf
