#pragma once

/**
 * @file vad_interface.h
 * @brief Voice Activity Detection interface
 *
 * Defines the abstract interface for VAD implementations.
 * This allows swapping VAD backends (energy-based, WebRTC, Silero, etc.)
 */

#include "core/types.h"
#include <memory>

namespace memo_rf {
namespace vad {

/**
 * @brief VAD events emitted during processing
 */
enum class Event {
    None,           ///< No significant event
    SpeechStart,    ///< Speech detected (transition from silence)
    SpeechEnd,      ///< Speech ended (transition to silence)
    SpeechContinue  ///< Speech continuing (informational)
};

/**
 * @brief VAD state for debugging/monitoring
 */
enum class State {
    Silence,    ///< No speech detected
    Speech,     ///< Currently in speech
    Hangover    ///< Grace period after speech
};

/**
 * @brief VAD statistics for debugging
 */
struct Stats {
    State state = State::Silence;
    float current_rms = 0.0f;
    float noise_floor = 0.0f;
    float threshold = 0.0f;
    int64_t speech_duration_ms = 0;
    int64_t silence_duration_ms = 0;
    size_t pre_buffer_samples = 0;
};

/**
 * @brief Abstract VAD interface
 *
 * All VAD implementations must implement this interface.
 */
class IVAD {
public:
    virtual ~IVAD() = default;

    /**
     * @brief Process a single audio frame
     * @param frame Audio frame to process
     * @return Event indicating state change (if any)
     */
    virtual Event process(const AudioFrame& frame) = 0;

    /**
     * @brief Get accumulated audio since speech start
     *
     * Includes pre-speech buffer to capture word beginnings.
     */
    virtual AudioBuffer get_speech_buffer() const = 0;

    /**
     * @brief Finalize and return speech segment
     *
     * Called after SpeechEnd to get the complete utterance.
     * Clears internal buffers.
     */
    virtual AudioBuffer finalize_segment() = 0;

    /**
     * @brief Reset VAD state
     *
     * Call after processing is complete or to abort current detection.
     */
    virtual void reset() = 0;

    /**
     * @brief Get current VAD statistics
     */
    virtual Stats get_stats() const = 0;

    /**
     * @brief Check if currently detecting speech
     */
    virtual bool is_speech() const = 0;
};

/**
 * @brief Convert Event to string for logging
 */
inline const char* event_to_string(Event event) {
    switch (event) {
        case Event::None: return "None";
        case Event::SpeechStart: return "SpeechStart";
        case Event::SpeechEnd: return "SpeechEnd";
        case Event::SpeechContinue: return "SpeechContinue";
    }
    return "Unknown";
}

/**
 * @brief Convert State to string for logging
 */
inline const char* state_to_string(State state) {
    switch (state) {
        case State::Silence: return "Silence";
        case State::Speech: return "Speech";
        case State::Hangover: return "Hangover";
    }
    return "Unknown";
}

} // namespace vad
} // namespace memo_rf
