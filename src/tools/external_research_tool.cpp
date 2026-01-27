#include "tools/external_research_tool.h"
#include "logger.h"
#include <nlohmann/json.hpp>
#include <sstream>

using json = nlohmann::json;

namespace memo_rf {

std::string ExternalResearchTool::parameter_schema() const {
    json schema;
    schema["type"] = "object";
    schema["properties"]["query"] = json::object({
        {"type", "string"},
        {"description", "Search query string"}
    });
    schema["properties"]["max_results"] = json::object({
        {"type", "integer"},
        {"description", "Maximum number of results to return"},
        {"default", 5}
    });
    schema["required"] = json::array({"query"});
    return schema.dump();
}

ToolResult ExternalResearchTool::execute(const std::string& params_json) {
    try {
        json params = json::parse(params_json);
        
        if (!params.contains("query") || !params["query"].is_string()) {
            return ToolResult::error_result("Missing or invalid 'query' parameter");
        }
        
        std::string query = params["query"].get<std::string>();
        int max_results = 5;
        
        if (params.contains("max_results") && params["max_results"].is_number_integer()) {
            max_results = params["max_results"].get<int>();
            if (max_results < 1) max_results = 1;
            if (max_results > 20) max_results = 20;  // Limit results
        }
        
        Logger::info("ExternalResearchTool: Searching for: " + query + " (max_results: " + std::to_string(max_results) + ")");
        
        // TODO: Implement actual web search API integration
        // For now, return placeholder result
        std::ostringstream result;
        result << "External research placeholder for query: \"" << query << "\"\n";
        result << "This tool will be implemented to call a web search API.\n";
        result << "Requested max_results: " << max_results << "\n";
        result << "Example results would appear here once the web search API is integrated.";
        
        return ToolResult::success_result(result.str());
        
    } catch (const json::exception& e) {
        Logger::error("ExternalResearchTool: JSON parse error: " + std::string(e.what()));
        return ToolResult::error_result("Invalid JSON parameters: " + std::string(e.what()));
    } catch (const std::exception& e) {
        Logger::error("ExternalResearchTool: Execution error: " + std::string(e.what()));
        return ToolResult::error_result("Tool execution error: " + std::string(e.what()));
    }
}

} // namespace memo_rf
