#pragma once

#ifdef USE_AGENTS_SDK

#include "config.h"
#include <memory>
#include <string>

namespace memo_rf {

/**
 * Bridge to RunEdgeAI agents-sdk: Context + Ollama LLM + ActorAgent.
 * Used when memo-RF is built with -DUSE_AGENTS_SDK=ON.
 */
class AgentsSdkBridge {
public:
    AgentsSdkBridge();
    ~AgentsSdkBridge();

    AgentsSdkBridge(const AgentsSdkBridge&) = delete;
    AgentsSdkBridge& operator=(const AgentsSdkBridge&) = delete;

    /**
     * Initialize SDK context, Ollama LLM, and agent from memo-RF config.
     * Registers tools when config.tools.enabled is non-empty.
     */
    bool init(const Config& config);

    /**
     * Run the agent on the given transcript; returns response text.
     * Returns "Stand by." on error or empty result.
     */
    std::string run(const std::string& transcript);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace memo_rf

#endif // USE_AGENTS_SDK
