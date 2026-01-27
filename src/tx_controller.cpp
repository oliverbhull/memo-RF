#include "tx_controller.h"
#include "audio_io.h"
#include "common.h"
#include "logger.h"
#include <sstream>

namespace memo_rf {

class TXController::Impl {
public:
    Impl(const TXConfig& config) : config_(config), transmitting_(false), audio_io_(nullptr) {}
    
    void set_audio_io(AudioIO* audio_io) {
        audio_io_ = audio_io;
    }
    
    void transmit(const AudioBuffer& buffer) {
        if (!audio_io_) {
            LOG_TX("AudioIO not set");
            return;
        }
        
        transmitting_ = true;
        
        // Limit buffer size to max transmit time (0 = no limit)
        AudioBuffer limited_buffer = buffer;
        if (config_.max_transmit_ms > 0) {
            int max_samples = (config_.max_transmit_ms * DEFAULT_SAMPLE_RATE) / 1000;
            if (limited_buffer.size() > static_cast<size_t>(max_samples)) {
                LOG_TX("Audio truncated: " + std::to_string(buffer.size()) + 
                      " samples -> " + std::to_string(max_samples) + " samples " +
                      "(" + std::to_string(config_.max_transmit_ms) + "ms limit)");
                limited_buffer.resize(max_samples);
            }
        }
        
        // Play audio (triggers VOX)
        audio_io_->play(limited_buffer);
    }
    
    bool is_transmitting() const {
        return transmitting_ && audio_io_ && !audio_io_->is_playback_complete();
    }
    
    void stop() {
        if (audio_io_) {
            audio_io_->stop_playback();
        }
        transmitting_ = false;
    }

private:
    TXConfig config_;
    bool transmitting_;
    AudioIO* audio_io_;
};

TXController::TXController(const TXConfig& config) 
    : pimpl_(std::make_unique<Impl>(config)) {}

TXController::~TXController() = default;

void TXController::transmit(const AudioBuffer& buffer) {
    pimpl_->transmit(buffer);
}

bool TXController::is_transmitting() const {
    return pimpl_->is_transmitting();
}

void TXController::stop() {
    pimpl_->stop();
}

void TXController::set_audio_io(AudioIO* audio_io) {
    pimpl_->set_audio_io(audio_io);
}

} // namespace memo_rf
