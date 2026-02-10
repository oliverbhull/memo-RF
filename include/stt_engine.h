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

    // Set initial_prompt for vocabulary boosting (merged from plugins).
    // This biases Whisper toward recognizing domain-specific words.
    void set_initial_prompt(const std::string& prompt);

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace memo_rf
