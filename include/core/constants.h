#pragma once

/**
 * @file constants.h
 * @brief System-wide constants and tuning parameters
 *
 * All magic numbers should be defined here with clear documentation.
 * This makes tuning the system behavior straightforward.
 */

namespace memo_rf {
namespace constants {

// =============================================================================
// VAD (Voice Activity Detection) Constants
// =============================================================================

namespace vad {
    /// Default RMS threshold for speech detection (normalized 0-1)
    constexpr float DEFAULT_THRESHOLD = 0.05f;

    /// Minimum speech duration to be considered valid (ms)
    constexpr int MIN_SPEECH_MS = 200;

    /// Maximum speech duration before force-ending (ms) - prevents runaway
    constexpr int MAX_SPEECH_MS = 30000;

    /// Silence duration to end utterance (ms)
    constexpr int END_SILENCE_MS = 350;

    /// Grace period after tentative end (ms) - catches trailing words
    constexpr int HANGOVER_MS = 50;

    /// Allow pauses this long during speech (ms)
    constexpr int PAUSE_TOLERANCE_MS = 150;

    /// Pre-speech buffer duration (ms) - captures word beginnings
    constexpr int PRE_SPEECH_BUFFER_MS = 300;

    /// Number of consecutive frames required before triggering speech start
    constexpr int DEBOUNCE_FRAMES = 2;

    /// Hysteresis ratio (end threshold = start threshold * this)
    constexpr float HYSTERESIS_RATIO = 0.5f;

    /// Adaptive threshold: rolling window size in frames
    constexpr int NOISE_FLOOR_WINDOW_FRAMES = 50;

    /// Adaptive threshold: multiplier above noise floor
    constexpr float ADAPTIVE_THRESHOLD_MULTIPLIER = 3.0f;

    /// Minimum threshold even in quiet environments
    constexpr float MIN_ADAPTIVE_THRESHOLD = 0.01f;

    /// Maximum threshold even in noisy environments
    constexpr float MAX_ADAPTIVE_THRESHOLD = 0.3f;
}

// =============================================================================
// Latency Targets
// =============================================================================

namespace latency {
    /// Target time from end-of-speech to acknowledgment (ms)
    constexpr int TARGET_ACK_MS = 500;

    /// Target time from end-of-speech to full response (ms)
    constexpr int TARGET_ANSWER_MS = 2000;

    /// Guard period after transmission before re-enabling VAD (ms)
    /// Prevents feedback loops from echo/reverb
    constexpr int VAD_GUARD_PERIOD_MS = 1500;
}

// =============================================================================
// TTS (Text-to-Speech) Constants
// =============================================================================

namespace tts {
    /// VOX pre-roll tone duration (ms)
    constexpr int VOX_PREROLL_MS = 200;

    /// Pre-roll tone frequency (Hz)
    constexpr float PREROLL_FREQ_HZ = 440.0f;

    /// Pre-roll tone amplitude (0-1)
    constexpr float PREROLL_AMPLITUDE = 0.3f;

    /// Maximum text length to cache
    constexpr size_t MAX_CACHE_TEXT_LENGTH = 50;

    /// Maximum number of cached phrases
    constexpr size_t MAX_CACHE_ENTRIES = 100;
}

// =============================================================================
// LLM (Language Model) Constants
// =============================================================================

namespace llm {
    /// Default timeout for LLM requests (ms)
    constexpr int DEFAULT_TIMEOUT_MS = 30000;

    /// Connection timeout (ms)
    constexpr int CONNECT_TIMEOUT_MS = 1000;

    /// Default max tokens in response
    constexpr int DEFAULT_MAX_TOKENS = 100;

    /// Default temperature
    constexpr float DEFAULT_TEMPERATURE = 0.3f;

    /// Maximum tool execution iterations
    constexpr int MAX_TOOL_ITERATIONS = 5;
}

// =============================================================================
// Conversation Memory Constants
// =============================================================================

namespace memory {
    /// Maximum number of messages to keep in history
    constexpr size_t MAX_HISTORY_MESSAGES = 20;

    /// Maximum total tokens in history before summarization
    constexpr size_t MAX_HISTORY_TOKENS = 2000;

    /// Approximate tokens per character (for estimation)
    constexpr float TOKENS_PER_CHAR = 0.25f;
}

// =============================================================================
// Transmission Constants
// =============================================================================

namespace tx {
    /// Maximum transmission duration (ms) - safety limit
    constexpr int MAX_TRANSMIT_MS = 20000;
}

// =============================================================================
// Logging Constants
// =============================================================================

namespace logging {
    /// RMS level logging interval (frames)
    constexpr int RMS_LOG_INTERVAL_FRAMES = 50;

    /// Speech progress logging interval (ms)
    constexpr int SPEECH_PROGRESS_LOG_INTERVAL_MS = 1000;
}

} // namespace constants
} // namespace memo_rf
