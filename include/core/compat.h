#pragma once

/**
 * @file compat.h
 * @brief Backward compatibility layer
 *
 * This file provides aliases and adapters to maintain compatibility
 * with existing code while transitioning to the new architecture.
 */

#include "types.h"
#include "constants.h"
#include "config.h"

namespace memo_rf {

// =============================================================================
// Type Aliases (from old common.h)
// =============================================================================

// These are already defined in types.h, just re-export for compatibility
using Sample = Sample;
using AudioFrame = AudioFrame;
using AudioBuffer = AudioBuffer;
using TimePoint = TimePoint;
using Duration = Duration;

// =============================================================================
// Constants (from old common.h)
// =============================================================================

// Audio format
constexpr int DEFAULT_SAMPLE_RATE = audio::SAMPLE_RATE;
constexpr int FRAME_SIZE_MS = audio::FRAME_DURATION_MS;
constexpr int SAMPLES_PER_FRAME = audio::SAMPLES_PER_FRAME;

// VAD/Endpointing
constexpr int MIN_SPEECH_MS = constants::vad::MIN_SPEECH_MS;
constexpr int END_OF_UTTERANCE_SILENCE_MS_MIN = 200;
constexpr int END_OF_UTTERANCE_SILENCE_MS_MAX = constants::vad::END_SILENCE_MS;
constexpr int HANGOVER_MS = constants::vad::HANGOVER_MS;

// Latency targets
constexpr int TARGET_ACK_LATENCY_MS = constants::latency::TARGET_ACK_MS;
constexpr int TARGET_ANSWER_LATENCY_MS = constants::latency::TARGET_ANSWER_MS;
constexpr int MAX_TRANSMIT_MS = constants::tx::MAX_TRANSMIT_MS;

// VOX pre-roll
constexpr int VOX_PREROLL_MS_MIN = 150;
constexpr int VOX_PREROLL_MS_MAX = 250;

// VAD guard period
constexpr int VAD_GUARD_PERIOD_MS = constants::latency::VAD_GUARD_PERIOD_MS;

// =============================================================================
// Transcript (from old common.h)
// =============================================================================

// Already defined in types.h

// =============================================================================
// VAD Event Compatibility
// =============================================================================

// Forward declaration from new VAD
namespace vad {
    enum class Event;
}

// Legacy VADEvent enum for backward compatibility
enum class VADEvent {
    None = 0,
    SpeechStart = 1,
    SpeechEnd = 2
};

// Conversion helper
inline VADEvent to_legacy_vad_event(vad::Event event) {
    switch (static_cast<int>(event)) {
        case 1: return VADEvent::SpeechStart;
        case 2: return VADEvent::SpeechEnd;
        default: return VADEvent::None;
    }
}

} // namespace memo_rf
