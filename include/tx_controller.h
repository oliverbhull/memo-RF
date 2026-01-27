#pragma once

#include "common.h"
#include "config.h"
#include <memory>

namespace memo_rf {

class AudioIO; // Forward declaration

class TXController {
public:
    explicit TXController(const TXConfig& config);
    ~TXController();
    
    // Set audio IO (must be called before use)
    void set_audio_io(AudioIO* audio_io);
    
    // Transmit audio buffer (triggers VOX)
    void transmit(const AudioBuffer& buffer);
    
    // Check if currently transmitting
    bool is_transmitting() const;
    
    // Stop transmission immediately
    void stop();

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace memo_rf
