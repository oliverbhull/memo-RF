#include "vad_endpointing.h"
#include "common.h"
#include <algorithm>
#include <cmath>
#include <deque>

namespace memo_rf {

class VADEndpointing::Impl {
public:
    Impl(const VADConfig& config) 
        : config_(config), 
          state_(VADState::Silence),
          speech_samples_(0),
          silence_samples_(0),
          current_hangover_samples_(0),
          current_segment_() {
        int sample_rate = DEFAULT_SAMPLE_RATE;
        min_speech_samples_ = (config.min_speech_ms * sample_rate) / 1000;
        end_silence_samples_ = (config.end_of_utterance_silence_ms * sample_rate) / 1000;
        max_hangover_samples_ = (config.hangover_ms * sample_rate) / 1000;
        pause_tolerance_samples_ = (config.pause_tolerance_ms * sample_rate) / 1000;
    }
    
    VADEvent process(const AudioFrame& frame) {
        // Simple energy-based VAD
        float energy = compute_energy(frame);
        bool is_speech = energy > config_.threshold;
        
        VADEvent event = VADEvent::None;
        
        switch (state_) {
            case VADState::Silence:
                if (is_speech) {
                    state_ = VADState::Speech;
                    speech_samples_ = frame.size();
                    silence_samples_ = 0;
                    current_segment_.clear();
                    current_segment_.insert(current_segment_.end(), frame.begin(), frame.end());
                    event = VADEvent::SpeechStart;
                }
                break;
                
            case VADState::Speech:
                // Always accumulate frames during speech (even during pauses)
                current_segment_.insert(current_segment_.end(), frame.begin(), frame.end());
                
                if (is_speech) {
                    // Speech detected - reset silence counter
                    speech_samples_ += frame.size();
                    silence_samples_ = 0;
                } else {
                    // Silence detected - accumulate silence
                    silence_samples_ += frame.size();
                    
                    // Check if silence exceeds pause tolerance (brief pause during speech)
                    if (silence_samples_ >= pause_tolerance_samples_ && 
                        silence_samples_ < end_silence_samples_) {
                        // Still within pause tolerance - continue accumulating but don't count as speech
                        // This allows user to pause and continue speaking
                        // Do nothing, just keep accumulating
                    } else if (silence_samples_ >= end_silence_samples_) {
                        // Silence exceeded end threshold - end the utterance
                        // Check if we have enough speech
                        if (speech_samples_ >= min_speech_samples_) {
                            state_ = VADState::Hangover;
                            current_hangover_samples_ = 0;
                            event = VADEvent::SpeechEnd;
                        } else {
                            // Too short, discard
                            state_ = VADState::Silence;
                            current_segment_.clear();
                            speech_samples_ = 0;
                            silence_samples_ = 0;
                        }
                    }
                }
                break;
                
            case VADState::Hangover:
                current_hangover_samples_ += frame.size();
                if (is_speech) {
                    // Speech resumed during hangover - continue the utterance
                    state_ = VADState::Speech;
                    current_segment_.insert(current_segment_.end(), frame.begin(), frame.end());
                    speech_samples_ += frame.size();
                    silence_samples_ = 0;
                    current_hangover_samples_ = 0;
                } else if (current_hangover_samples_ >= max_hangover_samples_) {
                    // Hangover period expired - finalize
                    state_ = VADState::Silence;
                    speech_samples_ = 0;
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
        speech_samples_ = 0;
        silence_samples_ = 0;
        current_hangover_samples_ = 0;
        current_segment_.clear();
    }

private:
    enum class VADState {
        Silence,
        Speech,
        Hangover
    };
    
    float compute_energy(const AudioFrame& frame) const {
        if (frame.empty()) return 0.0f;
        
        float sum_sq = 0.0f;
        for (Sample s : frame) {
            float normalized = static_cast<float>(s) / 32768.0f;
            sum_sq += normalized * normalized;
        }
        
        return std::sqrt(sum_sq / frame.size());
    }
    
    VADConfig config_;
    VADState state_;
    int64_t speech_samples_;
    int64_t silence_samples_;
    int64_t current_hangover_samples_;
    int64_t min_speech_samples_;
    int64_t end_silence_samples_;
    int64_t max_hangover_samples_;
    int64_t pause_tolerance_samples_;
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
