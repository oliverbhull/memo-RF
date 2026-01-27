#pragma once

#include "common.h"
#include "config.h"
#include <string>
#include <memory>

namespace memo_rf {

class STTEngine {
public:
    explicit STTEngine(const STTConfig& config);
    ~STTEngine();
    
    // Non-copyable
    STTEngine(const STTEngine&) = delete;
    STTEngine& operator=(const STTEngine&) = delete;
    
    // Transcribe audio segment
    Transcript transcribe(const AudioBuffer& segment);
    
    // Check if engine is ready
    bool is_ready() const;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace memo_rf
