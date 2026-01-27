#include "tool_registry.h"
#include "logger.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace memo_rf {

bool ToolRegistry::register_tool(std::shared_ptr<Tool> tool) {
    if (!tool) {
        Logger::error("Attempted to register null tool");
        return false;
    }
    
    std::string name = tool->name();
    if (tools_.find(name) != tools_.end()) {
        Logger::warn("Tool '" + name + "' is already registered. Skipping.");
        return false;
    }
    
    tools_[name] = tool;
    Logger::info("Registered tool: " + name);
    return true;
}

std::shared_ptr<Tool> ToolRegistry::get_tool(const std::string& name) const {
    auto it = tools_.find(name);
    if (it != tools_.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::string> ToolRegistry::get_tool_names() const {
    std::vector<std::string> names;
    names.reserve(tools_.size());
    for (const auto& [name, tool] : tools_) {
        names.push_back(name);
    }
    return names;
}

std::vector<std::shared_ptr<Tool>> ToolRegistry::get_all_tools() const {
    std::vector<std::shared_ptr<Tool>> result;
    result.reserve(tools_.size());
    for (const auto& [name, tool] : tools_) {
        result.push_back(tool);
    }
    return result;
}

std::string ToolRegistry::get_tool_definitions_json() const {
    json tools_array = json::array();
    
    for (const auto& [name, tool] : tools_) {
        json tool_def;
        tool_def["type"] = "function";
        
        json function_def;
        function_def["name"] = tool->name();
        function_def["description"] = tool->description();
        
        // Parse parameter schema JSON string
        try {
            json params_schema = json::parse(tool->parameter_schema());
            function_def["parameters"] = params_schema;
        } catch (const json::exception& e) {
            Logger::error("Failed to parse parameter schema for tool '" + name + "': " + e.what());
            // Create empty schema as fallback
            function_def["parameters"] = json::object();
        }
        
        tool_def["function"] = function_def;
        tools_array.push_back(tool_def);
    }
    
    return tools_array.dump();
}

bool ToolRegistry::has_tool(const std::string& name) const {
    return tools_.find(name) != tools_.end();
}

void ToolRegistry::clear() {
    tools_.clear();
}

} // namespace memo_rf
