#pragma once

#include "tool.h"
#include <string>
#include <vector>
#include <memory>
#include <map>

namespace memo_rf {

/**
 * @brief Central registry for all available tools
 * 
 * Manages tool registration, lookup, and provides tool definitions
 * in formats needed for LLM integration (JSON schema).
 */
class ToolRegistry {
public:
    /**
     * @brief Register a tool with the registry
     * @param tool Shared pointer to tool instance
     * @return true if registration successful, false if tool with same name already exists
     */
    bool register_tool(std::shared_ptr<Tool> tool);
    
    /**
     * @brief Get a tool by name
     * @param name Tool name
     * @return Shared pointer to tool, or nullptr if not found
     */
    std::shared_ptr<Tool> get_tool(const std::string& name) const;
    
    /**
     * @brief Get all registered tool names
     * @return Vector of tool names
     */
    std::vector<std::string> get_tool_names() const;
    
    /**
     * @brief Get all registered tools
     * @return Vector of shared pointers to all tools
     */
    std::vector<std::shared_ptr<Tool>> get_all_tools() const;
    
    /**
     * @brief Get tool definitions in Ollama/OpenAI format for LLM
     * @return JSON array of tool definitions
     */
    std::string get_tool_definitions_json() const;
    
    /**
     * @brief Check if a tool is registered
     * @param name Tool name
     * @return true if tool exists, false otherwise
     */
    bool has_tool(const std::string& name) const;
    
    /**
     * @brief Get number of registered tools
     * @return Count of registered tools
     */
    size_t size() const { return tools_.size(); }
    
    /**
     * @brief Clear all registered tools
     */
    void clear();

private:
    std::map<std::string, std::shared_ptr<Tool>> tools_;
};

} // namespace memo_rf
