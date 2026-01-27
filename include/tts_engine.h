#pragma once

#include "common.h"
#include "config.h"
#include <string>
#include <memory>

namespace memo_rf {

class TTSEngine {
public:
    explicit TTSEngine(const TTSConfig& config);
    ~TTSEngine();
    
    // Non-copyable
    TTSEngine(const TTSEngine&) = delete;
    TTSEngine& operator=(const TTSEngine&) = delete;
    
    // Synthesize text to audio
    AudioBuffer synth(const std::string& text);
    
    // Synthesize with VOX pre-roll
    AudioBuffer synth_vox(const std::string& text);
    
    // Pre-load common phrases for caching
    void preload_phrase(const std::string& text);

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace memo_rf
