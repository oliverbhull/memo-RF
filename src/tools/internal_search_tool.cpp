#include "tools/internal_search_tool.h"
#include "logger.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

using json = nlohmann::json;

namespace memo_rf {

InternalSearchTool::InternalSearchTool(const std::string& session_log_dir)
    : session_log_dir_(session_log_dir) {
}

std::string InternalSearchTool::parameter_schema() const {
    json schema;
    schema["type"] = "object";
    schema["properties"]["query"] = json::object({
        {"type", "string"},
        {"description", "Search query string"}
    });
    schema["properties"]["limit"] = json::object({
        {"type", "integer"},
        {"description", "Maximum number of results to return"},
        {"default", 10}
    });
    schema["required"] = json::array({"query"});
    return schema.dump();
}

ToolResult InternalSearchTool::execute(const std::string& params_json) {
    try {
        json params = json::parse(params_json);
        
        if (!params.contains("query") || !params["query"].is_string()) {
            return ToolResult::error_result("Missing or invalid 'query' parameter");
        }
        
        std::string query = params["query"].get<std::string>();
        int limit = 10;
        
        if (params.contains("limit") && params["limit"].is_number_integer()) {
            limit = params["limit"].get<int>();
            if (limit < 1) limit = 1;
            if (limit > 50) limit = 50;  // Limit results
        }
        
        Logger::info("InternalSearchTool: Searching for: \"" + query + "\" (limit: " + std::to_string(limit) + ")");
        
        // Convert query to lowercase for case-insensitive search
        std::string query_lower = query;
        std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(),
                      [](unsigned char c) { return std::tolower(c); });
        
        // Search in memo file
        std::string memo_path = get_memo_file_path();
        std::ifstream file(memo_path);
        
        std::vector<std::string> matches;
        
        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line) && matches.size() < static_cast<size_t>(limit)) {
                // Convert line to lowercase for comparison
                std::string line_lower = line;
                std::transform(line_lower.begin(), line_lower.end(), line_lower.begin(),
                              [](unsigned char c) { return std::tolower(c); });
                
                // Simple substring search
                if (line_lower.find(query_lower) != std::string::npos) {
                    matches.push_back(line);
                }
            }
            file.close();
        } else {
            // File doesn't exist yet - that's okay, just return empty results
            Logger::info("InternalSearchTool: Memo file not found: " + memo_path + " (no memos stored yet)");
        }
        
        // Format results
        std::ostringstream result;
        if (matches.empty()) {
            result << "No results found for query: \"" << query << "\"\n";
            result << "No matching memos or notes were found in the internal database.";
        } else {
            result << "Found " << matches.size() << " result(s) for query: \"" << query << "\"\n\n";
            for (size_t i = 0; i < matches.size(); ++i) {
                result << "Result " << (i + 1) << ": " << matches[i] << "\n";
            }
        }
        
        return ToolResult::success_result(result.str());
        
    } catch (const json::exception& e) {
        Logger::error("InternalSearchTool: JSON parse error: " + std::string(e.what()));
        return ToolResult::error_result("Invalid JSON parameters: " + std::string(e.what()));
    } catch (const std::exception& e) {
        Logger::error("InternalSearchTool: Execution error: " + std::string(e.what()));
        return ToolResult::error_result("Tool execution error: " + std::string(e.what()));
    }
}

std::string InternalSearchTool::get_memo_file_path() const {
    // Same as LogMemoTool - search in the memos.txt file
    return session_log_dir_ + "/memos.txt";
}

} // namespace memo_rf
