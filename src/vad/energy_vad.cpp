/**
 * @file energy_vad.cpp
 * @brief Energy-based VAD implementation
 */

#include "vad/energy_vad.h"
#include "logger.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <sstream>

namespace memo_rf {
namespace vad {

/**
 * @brief Implementation details for EnergyVAD
 */
class EnergyVAD::Impl {
public:
    explicit Impl(const EnergyVADConfig& config)
        : config_(config)
        , state_(State::Silence)
        , pre_speech_buffer_(audio::ms_to_samples(config.pre_speech_buffer_ms))
        , speech_samples_(0)
        , silence_samples_(0)
        , hangover_samples_(0)
        , pause_samples_(0)
        , consecutive_speech_frames_(0)
        , noise_floor_(config.threshold)
        , current_rms_(0.0f)
    {
        // Pre-calculate sample counts
        min_speech_samples_ = audio::ms_to_samples(config.min_speech_ms);
        end_silence_samples_ = audio::ms_to_samples(config.end_silence_ms);
        hangover_samples_max_ = audio::ms_to_samples(config.hangover_ms);
        pause_tolerance_samples_ = audio::ms_to_samples(config.pause_tolerance_ms);

        // Calculate thresholds
        start_threshold_ = config.threshold;
        end_threshold_ = config.threshold * constants::vad::HYSTERESIS_RATIO;

        LOG_VAD("EnergyVAD initialized:");
        std::ostringstream oss;
        oss << "  threshold=" << config.threshold
            << ", min_speech=" << config.min_speech_ms << "ms"
            << ", end_silence=" << config.end_silence_ms << "ms"
            << ", hangover=" << config.hangover_ms << "ms"
            << ", pause_tolerance=" << config.pause_tolerance_ms << "ms"
            << ", pre_buffer=" << config.pre_speech_buffer_ms << "ms"
            << ", adaptive=" << (config.adaptive_threshold ? "on" : "off");
        LOG_VAD(oss.str());
    }

    Event process(const AudioFrame& frame) {
        // Compute RMS energy
        current_rms_ = compute_rms(frame);

        // Update noise floor estimate (only during silence/low energy)
        if (config_.adaptive_threshold) {
            update_noise_floor(current_rms_);
        }

        // Get effective thresholds
        float effective_start = get_effective_threshold();
        float effective_end = effective_start * constants::vad::HYSTERESIS_RATIO;

        // Check against thresholds
        bool above_start = current_rms_ > effective_start;
        bool above_end = current_rms_ > effective_end;

        // Debug logging
        if (config_.debug_log_frames) {
            log_frame_debug(above_start, above_end, effective_start);
        }

        // Always add to pre-speech buffer (ring buffer automatically discards old)
        pre_speech_buffer_.write(frame);

        // State machine
        Event event = Event::None;

        switch (state_) {
            case State::Silence:
                event = process_silence_state(frame, above_start);
                break;

            case State::Speech:
                event = process_speech_state(frame, above_end);
                break;

            case State::Hangover:
                event = process_hangover_state(frame, above_end);
                break;
        }

        return event;
    }

    AudioBuffer get_speech_buffer() const {
        return speech_buffer_;
    }

    AudioBuffer finalize_segment() {
        AudioBuffer result = std::move(speech_buffer_);
        speech_buffer_.clear();
        return result;
    }

    void reset() {
        state_ = State::Silence;
        speech_samples_ = 0;
        silence_samples_ = 0;
        hangover_samples_ = 0;
        pause_samples_ = 0;
        consecutive_speech_frames_ = 0;
        speech_buffer_.clear();
        // Don't clear pre_speech_buffer_ - we want to keep recent audio
        // Don't reset noise_floor_ - adaptive threshold should persist
    }

    Stats get_stats() const {
        Stats stats;
        stats.state = state_;
        stats.current_rms = current_rms_;
        stats.noise_floor = noise_floor_;
        stats.threshold = get_effective_threshold();
        stats.speech_duration_ms = audio::samples_to_ms(speech_samples_);
        stats.silence_duration_ms = audio::samples_to_ms(silence_samples_);
        stats.pre_buffer_samples = pre_speech_buffer_.size();
        return stats;
    }

    bool is_speech() const {
        return state_ == State::Speech || state_ == State::Hangover;
    }

private:
    // =========================================================================
    // State Processing
    // =========================================================================

    Event process_silence_state(const AudioFrame& frame, bool above_start) {
        if (above_start) {
            consecutive_speech_frames_++;

            if (consecutive_speech_frames_ >= constants::vad::DEBOUNCE_FRAMES) {
                // Transition to speech!
                state_ = State::Speech;
                speech_samples_ = frame.size();
                silence_samples_ = 0;
                pause_samples_ = 0;
                consecutive_speech_frames_ = 0;

                // Initialize speech buffer with pre-speech audio
                speech_buffer_.clear();
                AudioBuffer pre_audio = pre_speech_buffer_.peek_all();
                speech_buffer_.insert(speech_buffer_.end(), pre_audio.begin(), pre_audio.end());

                // Add current frame
                speech_buffer_.insert(speech_buffer_.end(), frame.begin(), frame.end());

                log_state_transition("SpeechStart", current_rms_, get_effective_threshold());
                return Event::SpeechStart;
            }
        } else {
            consecutive_speech_frames_ = 0;
        }

        return Event::None;
    }

    Event process_speech_state(const AudioFrame& frame, bool above_end) {
        // Always accumulate audio during speech
        speech_buffer_.insert(speech_buffer_.end(), frame.begin(), frame.end());

        if (above_end) {
            // Active speech
            speech_samples_ += frame.size();
            silence_samples_ = 0;
            pause_samples_ = 0;
        } else {
            // Silence/low energy during speech
            silence_samples_ += frame.size();

            // Check pause tolerance - THIS IS THE FIXED LOGIC
            if (silence_samples_ < pause_tolerance_samples_) {
                // Within pause tolerance - count as a pause, keep accumulating
                pause_samples_ += frame.size();
                // Don't transition yet, this is a normal speech pause
            } else if (silence_samples_ >= end_silence_samples_) {
                // Exceeded end-of-utterance silence threshold
                if (speech_samples_ >= min_speech_samples_) {
                    // Valid speech segment - go to hangover
                    state_ = State::Hangover;
                    hangover_samples_ = 0;

                    log_state_transition("SpeechEnd (-> Hangover)", current_rms_, get_effective_threshold());
                    return Event::SpeechEnd;
                } else {
                    // Too short - discard and return to silence
                    state_ = State::Silence;
                    speech_buffer_.clear();
                    speech_samples_ = 0;
                    silence_samples_ = 0;

                    LOG_VAD("Speech too short, discarding");
                    return Event::None;
                }
            }
            // else: between pause_tolerance and end_silence - keep waiting
        }

        return Event::None;
    }

    Event process_hangover_state(const AudioFrame& frame, bool above_end) {
        hangover_samples_ += frame.size();

        if (above_end) {
            // Speech resumed during hangover - go back to speech state
            state_ = State::Speech;
            speech_buffer_.insert(speech_buffer_.end(), frame.begin(), frame.end());
            speech_samples_ += frame.size();
            silence_samples_ = 0;
            hangover_samples_ = 0;

            LOG_VAD("Speech resumed during hangover");
            return Event::None;
        }

        if (hangover_samples_ >= hangover_samples_max_) {
            // Hangover complete - return to silence
            state_ = State::Silence;
            speech_samples_ = 0;
            silence_samples_ = 0;
            hangover_samples_ = 0;

            LOG_VAD("Hangover complete, returning to silence");
        }

        return Event::None;
    }

    // =========================================================================
    // Signal Processing
    // =========================================================================

    float compute_rms(const AudioFrame& frame) const {
        if (frame.empty()) return 0.0f;

        double sum_sq = 0.0;
        for (Sample s : frame) {
            double normalized = static_cast<double>(s) / 32768.0;
            sum_sq += normalized * normalized;
        }

        return static_cast<float>(std::sqrt(sum_sq / frame.size()));
    }

    void update_noise_floor(float rms) {
        // Only update noise floor when in silence state and energy is low
        // This prevents speech from contaminating the noise estimate
        if (state_ != State::Silence) return;

        // Use exponential moving average with slow adaptation
        constexpr float NOISE_FLOOR_ALPHA = 0.01f;  // Slow adaptation

        // Only update if this looks like noise (not much higher than current floor)
        if (rms < noise_floor_ * 2.0f) {
            noise_floor_ = noise_floor_ * (1.0f - NOISE_FLOOR_ALPHA) + rms * NOISE_FLOOR_ALPHA;

            // Clamp to reasonable range
            noise_floor_ = std::clamp(
                noise_floor_,
                constants::vad::MIN_ADAPTIVE_THRESHOLD / constants::vad::ADAPTIVE_THRESHOLD_MULTIPLIER,
                constants::vad::MAX_ADAPTIVE_THRESHOLD / constants::vad::ADAPTIVE_THRESHOLD_MULTIPLIER
            );
        }
    }

    float get_effective_threshold() const {
        if (!config_.adaptive_threshold) {
            return start_threshold_;
        }

        // Adaptive threshold = noise floor * multiplier, clamped to config range
        float adaptive = noise_floor_ * constants::vad::ADAPTIVE_THRESHOLD_MULTIPLIER;

        return std::clamp(
            adaptive,
            constants::vad::MIN_ADAPTIVE_THRESHOLD,
            std::max(start_threshold_, constants::vad::MAX_ADAPTIVE_THRESHOLD)
        );
    }

    // =========================================================================
    // Logging
    // =========================================================================

    void log_frame_debug(bool above_start, bool above_end, float threshold) const {
        std::ostringstream oss;
        oss << "[VAD] rms=" << current_rms_
            << " thr=" << threshold
            << " noise=" << noise_floor_
            << " state=" << state_to_string(state_)
            << " above_start=" << above_start
            << " above_end=" << above_end;
        Logger::info(oss.str());
    }

    void log_state_transition(const char* event, float rms, float threshold) const {
        std::ostringstream oss;
        oss << "[VAD] " << event
            << " rms=" << rms
            << " threshold=" << threshold
            << " speech_ms=" << audio::samples_to_ms(speech_samples_)
            << " silence_ms=" << audio::samples_to_ms(silence_samples_);
        Logger::info(oss.str());
    }

    // =========================================================================
    // Member Variables
    // =========================================================================

    EnergyVADConfig config_;
    State state_;

    // Buffers
    AudioRingBuffer pre_speech_buffer_;  // Rolling buffer of recent audio
    AudioBuffer speech_buffer_;          // Accumulated speech since SpeechStart

    // Sample counters
    size_t speech_samples_;
    size_t silence_samples_;
    size_t hangover_samples_;
    size_t pause_samples_;
    int consecutive_speech_frames_;

    // Pre-calculated sample thresholds
    size_t min_speech_samples_;
    size_t end_silence_samples_;
    size_t hangover_samples_max_;
    size_t pause_tolerance_samples_;

    // Threshold values
    float start_threshold_;
    float end_threshold_;
    float noise_floor_;
    float current_rms_;
};

// =============================================================================
// Public Interface Implementation
// =============================================================================

EnergyVAD::EnergyVAD(const EnergyVADConfig& config)
    : impl_(std::make_unique<Impl>(config)) {}

EnergyVAD::~EnergyVAD() = default;

Event EnergyVAD::process(const AudioFrame& frame) {
    return impl_->process(frame);
}

AudioBuffer EnergyVAD::get_speech_buffer() const {
    return impl_->get_speech_buffer();
}

AudioBuffer EnergyVAD::finalize_segment() {
    return impl_->finalize_segment();
}

void EnergyVAD::reset() {
    impl_->reset();
}

Stats EnergyVAD::get_stats() const {
    return impl_->get_stats();
}

bool EnergyVAD::is_speech() const {
    return impl_->is_speech();
}

} // namespace vad
} // namespace memo_rf
