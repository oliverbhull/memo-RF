#pragma once

/**
 * @file energy_vad.h
 * @brief Energy-based Voice Activity Detection with improvements
 *
 * Features:
 * - Pre-speech buffer to capture word beginnings
 * - Adaptive threshold based on noise floor
 * - Hysteresis to prevent oscillation
 * - Pause tolerance for natural speech pauses
 * - Configurable debouncing
 */

#include "vad_interface.h"
#include "core/ring_buffer.h"
#include "core/constants.h"
#include <deque>
#include <memory>

namespace memo_rf {
namespace vad {

/**
 * @brief Configuration for energy-based VAD
 */
struct EnergyVADConfig {
    /// Base threshold (used if adaptive disabled, or as minimum)
    float threshold = constants::vad::DEFAULT_THRESHOLD;

    /// Minimum speech duration to be valid (ms)
    int min_speech_ms = constants::vad::MIN_SPEECH_MS;

    /// Silence duration to end utterance (ms)
    int end_silence_ms = constants::vad::END_SILENCE_MS;

    /// Grace period after speech ends (ms)
    int hangover_ms = constants::vad::HANGOVER_MS;

    /// Allow pauses this long during speech (ms)
    int pause_tolerance_ms = constants::vad::PAUSE_TOLERANCE_MS;

    /// Pre-speech buffer duration (ms)
    int pre_speech_buffer_ms = constants::vad::PRE_SPEECH_BUFFER_MS;

    /// Enable adaptive threshold based on noise floor
    bool adaptive_threshold = true;

    /// Log verbose debug info every frame
    bool debug_log_frames = false;
};

/**
 * @brief Energy-based VAD implementation
 *
 * Uses RMS energy with adaptive thresholding and pre-speech buffering.
 */
class EnergyVAD : public IVAD {
public:
    explicit EnergyVAD(const EnergyVADConfig& config = {});
    ~EnergyVAD() override;

    // IVAD interface
    Event process(const AudioFrame& frame) override;
    AudioBuffer get_speech_buffer() const override;
    AudioBuffer finalize_segment() override;
    void reset() override;
    Stats get_stats() const override;
    bool is_speech() const override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace vad

// =============================================================================
// Backward Compatibility
// =============================================================================

// Alias for existing code that uses VADEvent
using VADEvent = vad::Event;

// Re-export for backward compatibility
inline constexpr VADEvent VADEvent_None = vad::Event::None;
inline constexpr VADEvent VADEvent_SpeechStart = vad::Event::SpeechStart;
inline constexpr VADEvent VADEvent_SpeechEnd = vad::Event::SpeechEnd;

} // namespace memo_rf
