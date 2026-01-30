#pragma once

/**
 * @file types.h
 * @brief Core type definitions for the Memo-RF audio agent framework
 *
 * This file contains all fundamental types used throughout the system.
 * Keeping types centralized ensures consistency and makes refactoring easier.
 */

#include <cstdint>
#include <vector>
#include <string>
#include <chrono>
#include <memory>
#include <functional>
#include <optional>

namespace memo_rf {

// =============================================================================
// Audio Types
// =============================================================================

/// Raw audio sample (16-bit signed PCM)
using Sample = int16_t;

/// Single frame of audio (typically 20ms worth of samples)
using AudioFrame = std::vector<Sample>;

/// Variable-length audio buffer
using AudioBuffer = std::vector<Sample>;

// =============================================================================
// Timing Types
// =============================================================================

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;
using Duration = std::chrono::milliseconds;
using Microseconds = std::chrono::microseconds;

/// Get milliseconds elapsed since a time point (omit if common.h already defines it)
#ifndef MEMO_RF_USE_COMMON_TYPES
inline int64_t ms_since(TimePoint start) {
    return std::chrono::duration_cast<Duration>(Clock::now() - start).count();
}
#endif

/// Get current timestamp in milliseconds (for logging)
inline int64_t now_ms() {
    return std::chrono::duration_cast<Duration>(
        Clock::now().time_since_epoch()).count();
}

// =============================================================================
// Audio Format Constants
// =============================================================================

namespace audio {
    constexpr int SAMPLE_RATE = 16000;           // Hz
    constexpr int FRAME_DURATION_MS = 20;        // ms per frame
    constexpr int SAMPLES_PER_FRAME = (SAMPLE_RATE * FRAME_DURATION_MS) / 1000; // 320
    constexpr int BYTES_PER_SAMPLE = sizeof(Sample);
    constexpr int BYTES_PER_FRAME = SAMPLES_PER_FRAME * BYTES_PER_SAMPLE;

    /// Convert milliseconds to samples
    constexpr size_t ms_to_samples(int ms) {
        return (static_cast<size_t>(ms) * SAMPLE_RATE) / 1000;
    }

    /// Convert samples to milliseconds
    constexpr int samples_to_ms(size_t samples) {
        return static_cast<int>((samples * 1000) / SAMPLE_RATE);
    }
}

// =============================================================================
// Result Types (for error handling without exceptions)
// =============================================================================

/// Generic result type for operations that can fail
template<typename T>
struct Result {
    std::optional<T> value;
    std::string error;

    bool ok() const { return value.has_value(); }
    bool failed() const { return !ok(); }

    static Result success(T val) { return {std::move(val), ""}; }
    static Result failure(std::string err) { return {std::nullopt, std::move(err)}; }

    /// Get value or throw if failed
    T& unwrap() {
        if (!ok()) throw std::runtime_error(error);
        return *value;
    }

    /// Get value or return default
    T value_or(T default_val) const {
        return ok() ? *value : default_val;
    }
};

/// Void result for operations that don't return a value
struct VoidResult {
    bool success;
    std::string error;

    bool ok() const { return success; }
    bool failed() const { return !ok(); }

    static VoidResult ok_result() { return {true, ""}; }
    static VoidResult failure(std::string err) { return {false, std::move(err)}; }
};

// =============================================================================
// Speech/Transcript Types (omit Transcript if common.h already defines it)
// =============================================================================

#ifndef MEMO_RF_USE_COMMON_TYPES
/// Result of speech-to-text transcription
struct Transcript {
    std::string text;
    float confidence = 0.0f;
    int64_t processing_ms = 0;
    int64_t audio_duration_ms = 0;

    bool empty() const { return text.empty(); }
};
#endif

/// Conversation message roles
enum class MessageRole {
    System,
    User,
    Assistant,
    Tool
};

/// Single message in conversation history
struct Message {
    MessageRole role;
    std::string content;
    std::string tool_call_id;  // For tool messages
    int64_t timestamp_ms = 0;

    static Message system(const std::string& content);
    static Message user(const std::string& content);
    static Message assistant(const std::string& content);
    static Message tool(const std::string& tool_call_id, const std::string& content);
};

// =============================================================================
// Callback Types
// =============================================================================

/// Audio frame callback
using AudioFrameCallback = std::function<void(const AudioFrame&)>;

/// Transcript ready callback
using TranscriptCallback = std::function<void(const Transcript&)>;

/// Response ready callback
using ResponseCallback = std::function<void(const std::string&)>;

/// Error callback
using ErrorCallback = std::function<void(const std::string&)>;

} // namespace memo_rf
