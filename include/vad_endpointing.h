#pragma once

#include "common.h"
#include "config.h"
#include <vector>
#include <memory>

namespace memo_rf {

enum class VADEvent {
    None,
    SpeechStart,
    SpeechEnd
};

class VADEndpointing {
public:
    explicit VADEndpointing(const VADConfig& config);
    ~VADEndpointing();
    
    // Process a frame and return events
    VADEvent process(const AudioFrame& frame);
    
    // Get current segment buffer (since SpeechStart)
    AudioBuffer get_current_segment() const;
    
    // Finalize and return segment (on SpeechEnd)
    AudioBuffer finalize_segment();
    
    // Reset state
    void reset();

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace memo_rf
