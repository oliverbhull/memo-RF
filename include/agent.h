#pragma once

#include "config.h"
#include "common.h"
#include <memory>

namespace memo_rf {

/**
 * @brief Voice agent orchestrator
 * 
 * Manages the complete voice agent lifecycle including:
 * - Component initialization and cleanup
 * - Main event loop processing
 * - State transitions and coordination
 * - Audio processing pipeline
 */
class VoiceAgent {
public:
    /**
     * @brief Construct voice agent with configuration
     * @param config Configuration object
     */
    explicit VoiceAgent(const Config& config);
    
    /**
     * @brief Destructor - ensures proper cleanup
     */
    ~VoiceAgent();
    
    // Non-copyable
    VoiceAgent(const VoiceAgent&) = delete;
    VoiceAgent& operator=(const VoiceAgent&) = delete;
    
    /**
     * @brief Initialize all components
     * @return True if initialization successful, false otherwise
     */
    bool initialize();
    
    /**
     * @brief Run the main event loop
     * @return Exit code (0 for success, non-zero for error)
     */
    int run();
    
    /**
     * @brief Request shutdown (thread-safe)
     */
    void shutdown();

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace memo_rf
