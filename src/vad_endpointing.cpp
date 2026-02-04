#include "vad_endpointing.h"
#include "common.h"
#include "logger.h"
#include <cmath>
#include <sstream>

namespace memo_rf {

class VADEndpointing::Impl {
public:
    Impl(const VADConfig& config)
        : config_(config),
          state_(VADState::Silence),
          threshold_(0.0f),
          start_frames_required_(config.start_frames_required > 0 ? config.start_frames_required : 2),
          consecutive_speech_frames_(0),
          silence_samples_(0),
          current_hangover_samples_(0),
          end_silence_samples_(0),
          max_hangover_samples_(0),
          current_segment_() {
        int sample_rate = DEFAULT_SAMPLE_RATE;
        int end_silence_ms = config.end_of_utterance_silence_ms;
        end_silence_samples_ = (static_cast<int64_t>(end_silence_ms) * sample_rate) / 1000;
        max_hangover_samples_ = (config.hangover_ms * sample_rate) / 1000;
        threshold_ = config_.threshold;
    }

    VADEvent process(const AudioFrame& frame) {
        float rms = compute_rms(frame);
        bool is_speech = rms > threshold_;

        VADEvent event = VADEvent::None;

        switch (state_) {
            case VADState::Silence:
                if (is_speech) {
                    consecutive_speech_frames_++;
                    if (consecutive_speech_frames_ >= start_frames_required_) {
                        state_ = VADState::Speech;
                        silence_samples_ = 0;
                        consecutive_speech_frames_ = 0;
                        current_segment_.clear();
                        current_segment_.insert(current_segment_.end(), frame.begin(), frame.end());
                        event = VADEvent::SpeechStart;
                        Logger::info("[VAD] SpeechStart rms=" + std::to_string(rms) + " threshold=" + std::to_string(threshold_));
                    }
                } else {
                    consecutive_speech_frames_ = 0;
                }
                break;

            case VADState::Speech:
                current_segment_.insert(current_segment_.end(), frame.begin(), frame.end());
                // Only accumulate silence when RMS is below silence_threshold (very quiet).
                // Frames with rms in [silence_threshold, threshold_] are speech dips and reset silence.
                if (rms >= config_.silence_threshold) {
                    silence_samples_ = 0;
                } else {
                    silence_samples_ += static_cast<int64_t>(frame.size());
                    if (silence_samples_ >= end_silence_samples_) {
                        state_ = VADState::Hangover;
                        current_hangover_samples_ = 0;
                        event = VADEvent::SpeechEnd;
                        int silence_ms = static_cast<int>(silence_samples_ * 1000 / DEFAULT_SAMPLE_RATE);
                        Logger::info("[VAD] SpeechEnd rms=" + std::to_string(rms) + " silence_ms=" + std::to_string(silence_ms));
                    }
                }
                break;

            case VADState::Hangover:
                current_hangover_samples_ += frame.size();
                if (is_speech) {
                    state_ = VADState::Speech;
                    current_segment_.insert(current_segment_.end(), frame.begin(), frame.end());
                    silence_samples_ = 0;
                    current_hangover_samples_ = 0;
                } else if (current_hangover_samples_ >= max_hangover_samples_) {
                    state_ = VADState::Silence;
                    silence_samples_ = 0;
                }
                break;
        }

        return event;
    }

    AudioBuffer get_current_segment() const {
        return current_segment_;
    }

    AudioBuffer finalize_segment() {
        AudioBuffer result = current_segment_;
        current_segment_.clear();
        return result;
    }

    void reset() {
        state_ = VADState::Silence;
        silence_samples_ = 0;
        current_hangover_samples_ = 0;
        consecutive_speech_frames_ = 0;
        current_segment_.clear();
    }

private:
    enum class VADState { Silence, Speech, Hangover };

    static float compute_rms(const AudioFrame& frame) {
        if (frame.empty()) return 0.0f;
        float sum_sq = 0.0f;
        for (Sample s : frame) {
            float x = static_cast<float>(s) / 32768.0f;
            sum_sq += x * x;
        }
        return std::sqrt(sum_sq / static_cast<float>(frame.size()));
    }

    VADConfig config_;
    VADState state_;
    float threshold_;
    int start_frames_required_;
    int consecutive_speech_frames_;
    int64_t silence_samples_;
    int64_t current_hangover_samples_;
    int64_t end_silence_samples_;
    int64_t max_hangover_samples_;
    AudioBuffer current_segment_;
};

VADEndpointing::VADEndpointing(const VADConfig& config)
    : pimpl_(std::make_unique<Impl>(config)) {}

VADEndpointing::~VADEndpointing() = default;

VADEvent VADEndpointing::process(const AudioFrame& frame) {
    return pimpl_->process(frame);
}

AudioBuffer VADEndpointing::get_current_segment() const {
    return pimpl_->get_current_segment();
}

AudioBuffer VADEndpointing::finalize_segment() {
    return pimpl_->finalize_segment();
}

void VADEndpointing::reset() {
    pimpl_->reset();
}

} // namespace memo_rf
