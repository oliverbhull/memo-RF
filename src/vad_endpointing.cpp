#include "vad_endpointing.h"
#include "common.h"
#include "logger.h"
#include <algorithm>
#include <cmath>
#include <deque>
#include <sstream>

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
        start_threshold_ = config_.threshold;
        end_threshold_ = config_.threshold * 0.5f;  // Hysteresis: only count as "speech" if rms above this; lowers chance of getting stuck
        start_frames_required_ = 2;  // Debounce: require 2 consecutive hot frames (~40ms) before SpeechStart
    }
    
    VADEvent process(const AudioFrame& frame) {
        float rms = compute_energy(frame);
        // Start: use strict threshold; once in speech, use lower threshold (hysteresis)
        bool above_start = rms > start_threshold_;
        bool above_end = rms > end_threshold_;
        
        VADEvent event = VADEvent::None;
        
        if (config_.debug_log_rms_each_frame) {
            const char* state_str = (state_ == VADState::Silence) ? "Silence" :
                (state_ == VADState::Speech) ? "Speech" : "Hangover";
            std::ostringstream oss;
            oss << "[VAD] rms=" << rms << " start_thr=" << start_threshold_
                << " end_thr=" << end_threshold_ << " state=" << state_str
                << " above_start=" << (above_start ? 1 : 0) << " above_end=" << (above_end ? 1 : 0);
            Logger::info(oss.str());
        }
        
        switch (state_) {
            case VADState::Silence: {
                // Start debounce: require N consecutive frames above start threshold
                if (above_start) {
                    consecutive_speech_frames_++;
                    if (consecutive_speech_frames_ >= start_frames_required_) {
                        state_ = VADState::Speech;
                        speech_samples_ = frame.size();
                        silence_samples_ = 0;
                        consecutive_speech_frames_ = 0;
                        debug_samples_since_log_ = 0;
                        current_segment_.clear();
                        current_segment_.insert(current_segment_.end(), frame.begin(), frame.end());
                        event = VADEvent::SpeechStart;
                        std::ostringstream oss;
                        oss << "[VAD] SpeechStart rms=" << rms << " threshold=" << start_threshold_;
                        Logger::info(oss.str());
                    }
                } else {
                    consecutive_speech_frames_ = 0;
                }
                break;
            }
                
            case VADState::Speech: {
                // Hysteresis: during speech, treat as "speech" if above end_threshold (lower)
                bool is_speech = above_end;
                current_segment_.insert(current_segment_.end(), frame.begin(), frame.end());
                debug_samples_since_log_ += static_cast<int64_t>(frame.size());
                
                if (is_speech) {
                    speech_samples_ += frame.size();
                    silence_samples_ = 0;
                } else {
                    silence_samples_ += frame.size();
                    
                    if (silence_samples_ >= pause_tolerance_samples_ && 
                        silence_samples_ < end_silence_samples_) {
                        // within pause tolerance - keep accumulating
                    } else if (silence_samples_ >= end_silence_samples_) {
                        if (speech_samples_ >= min_speech_samples_) {
                            state_ = VADState::Hangover;
                            current_hangover_samples_ = 0;
                            event = VADEvent::SpeechEnd;
                            std::ostringstream oss;
                            oss << "[VAD] SpeechEnd rms=" << rms << " threshold=" << end_threshold_
                                << " silence_ms=" << (silence_samples_ * 1000 / DEFAULT_SAMPLE_RATE);
                            Logger::info(oss.str());
                        } else {
                            state_ = VADState::Silence;
                            current_segment_.clear();
                            speech_samples_ = 0;
                            silence_samples_ = 0;
                        }
                    }
                }
                if (debug_samples_since_log_ >= DEFAULT_SAMPLE_RATE / 2) {
                    debug_samples_since_log_ = 0;
                    std::ostringstream oss;
                    oss << "[VAD] state=Speech rms=" << rms << " end_thr=" << end_threshold_
                        << " is_speech=" << (is_speech ? 1 : 0)
                        << " silence_ms=" << (silence_samples_ * 1000 / DEFAULT_SAMPLE_RATE)
                        << " speech_ms=" << (speech_samples_ * 1000 / DEFAULT_SAMPLE_RATE);
                    Logger::info(oss.str());
                }
                break;
            }
                
            case VADState::Hangover:
                current_hangover_samples_ += frame.size();
                if (above_end) {
                    state_ = VADState::Speech;
                    current_segment_.insert(current_segment_.end(), frame.begin(), frame.end());
                    speech_samples_ += frame.size();
                    silence_samples_ = 0;
                    current_hangover_samples_ = 0;
                } else if (current_hangover_samples_ >= max_hangover_samples_) {
                    state_ = VADState::Silence;
                    speech_samples_ = 0;
                    silence_samples_ = 0;
                    debug_samples_since_log_ = 0;
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
        consecutive_speech_frames_ = 0;
        debug_samples_since_log_ = 0;
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
    float start_threshold_;
    float end_threshold_;
    int start_frames_required_;
    int consecutive_speech_frames_ = 0;
    int64_t speech_samples_;
    int64_t silence_samples_;
    int64_t current_hangover_samples_;
    int64_t min_speech_samples_;
    int64_t end_silence_samples_;
    int64_t max_hangover_samples_;
    int64_t pause_tolerance_samples_;
    int64_t debug_samples_since_log_ = 0;
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
