#pragma once

#include "common.h"
#include "config.h"
#include <string>
#include <memory>

namespace memo_rf {

class LLMClient {
public:
    explicit LLMClient(const LLMConfig& config);
    ~LLMClient();
    
    // Non-copyable
    LLMClient(const LLMClient&) = delete;
    LLMClient& operator=(const LLMClient&) = delete;
    
    // Generate response with timeout
    std::string generate(const std::string& prompt, 
                        const std::string& context = "",
                        int timeout_ms = 0,  // 0 = use config default
                        int max_tokens = 0); // 0 = use config default
    
    // Check if client is ready
    bool is_ready() const;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace memo_rf
