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
    
    // Synthesize with VOX pre-roll (tone + speech)
    AudioBuffer synth_vox(const std::string& text);
    
    // Return just the VOX pre-roll tone buffer (same as prepended in synth_vox)
    AudioBuffer get_preroll_buffer();
    
    // Return end-of-transmission tone buffer (append after speech when vox_end_tone_ms > 0)
    AudioBuffer get_end_tone_buffer();
    
    // Pre-load common phrases for caching
    void preload_phrase(const std::string& text);

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace memo_rf
