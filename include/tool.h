#pragma once

#include <string>
#include <map>

namespace memo_rf {

/**
 * @brief Result structure for tool execution
 */
struct ToolResult {
    bool success = false;
    std::string content;  // Result text for LLM
    std::string error;    // Error message if failed
    std::map<std::string, std::string> metadata;  // Optional metadata
    
    static ToolResult success_result(const std::string& content) {
        ToolResult result;
        result.success = true;
        result.content = content;
        return result;
    }
    
    static ToolResult error_result(const std::string& error_msg) {
        ToolResult result;
        result.success = false;
        result.error = error_msg;
        return result;
    }
};

/**
 * @brief Abstract base class for all tools
 * 
 * Tools are callable functions that the LLM can invoke to perform actions.
 * Each tool must provide:
 * - A unique name
 * - A clear description (for the LLM)
 * - A JSON schema for parameters
 * - An execute method that performs the tool's action
 */
class Tool {
public:
    virtual ~Tool() = default;
    
    /**
     * @brief Get the tool's unique name
     * @return Tool name (e.g., "log_memo", "external_research")
     */
    virtual std::string name() const = 0;
    
    /**
     * @brief Get the tool's description for the LLM
     * @return Human-readable description explaining when and how to use this tool
     */
    virtual std::string description() const = 0;
    
    /**
     * @brief Get the JSON schema for tool parameters
     * @return JSON schema string describing the tool's parameters
     */
    virtual std::string parameter_schema() const = 0;
    
    /**
     * @brief Execute the tool with given parameters
     * @param params_json JSON string containing tool parameters
     * @return ToolResult with success status and result content or error
     */
    virtual ToolResult execute(const std::string& params_json) = 0;
    
    /**
     * @brief Check if this tool executes asynchronously
     * @return true if tool is async, false if synchronous
     */
    virtual bool is_async() const { return false; }
};

} // namespace memo_rf
