#pragma once

#include "tool.h"
#include <string>
#include <memory>

namespace memo_rf {

/**
 * @brief Tool for external internet research
 * 
 * Allows the LLM to search the internet for current information, news, or facts.
 * This is a placeholder implementation - will call web search API later.
 */
class ExternalResearchTool : public Tool {
public:
    ExternalResearchTool() = default;
    
    std::string name() const override { return "external_research"; }
    
    std::string description() const override {
        return "Search the internet for current information, news, or facts. "
               "Use this when you need up-to-date information that isn't in your training data.";
    }
    
    std::string parameter_schema() const override;
    
    ToolResult execute(const std::string& params_json) override;
    
    bool is_async() const override { return true; }  // Internet search is async
};

} // namespace memo_rf
