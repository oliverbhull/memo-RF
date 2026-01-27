#include "tools/log_memo_tool.h"
#include "logger.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>

using json = nlohmann::json;

namespace memo_rf {

LogMemoTool::LogMemoTool(const std::string& session_log_dir)
    : session_log_dir_(session_log_dir) {
}

std::string LogMemoTool::parameter_schema() const {
    json schema;
    schema["type"] = "object";
    schema["properties"]["content"] = json::object({
        {"type", "string"},
        {"description", "The content or information to log/memo"}
    });
    schema["properties"]["tags"] = json::object({
        {"type", "array"},
        {"items", json::object({{"type", "string"}})},
        {"description", "Optional tags to categorize the memo"}
    });
    schema["required"] = json::array({"content"});
    return schema.dump();
}

ToolResult LogMemoTool::execute(const std::string& params_json) {
    try {
        json params = json::parse(params_json);
        
        if (!params.contains("content") || !params["content"].is_string()) {
            return ToolResult::error_result("Missing or invalid 'content' parameter");
        }
        
        std::string content = params["content"].get<std::string>();
        std::vector<std::string> tags;
        
        if (params.contains("tags") && params["tags"].is_array()) {
            for (const auto& tag : params["tags"]) {
                if (tag.is_string()) {
                    tags.push_back(tag.get<std::string>());
                }
            }
        }
        
        // Get current timestamp
        auto now = std::time(nullptr);
        auto tm = *std::localtime(&now);
        std::ostringstream timestamp;
        timestamp << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        
        // Write to memo file
        std::string memo_path = get_memo_file_path();
        std::ofstream file(memo_path, std::ios::app);
        if (!file.is_open()) {
            Logger::error("Failed to open memo file: " + memo_path);
            return ToolResult::error_result("Failed to write memo file");
        }
        
        file << "[" << timestamp.str() << "] ";
        if (!tags.empty()) {
            file << "[";
            for (size_t i = 0; i < tags.size(); ++i) {
                if (i > 0) file << ", ";
                file << tags[i];
            }
            file << "] ";
        }
        file << content << "\n";
        file.close();
        
        std::string result_msg = "Memo logged successfully";
        if (!tags.empty()) {
            result_msg += " with tags: ";
            for (size_t i = 0; i < tags.size(); ++i) {
                if (i > 0) result_msg += ", ";
                result_msg += tags[i];
            }
        }
        
        Logger::info("LogMemoTool: " + result_msg);
        return ToolResult::success_result(result_msg);
        
    } catch (const json::exception& e) {
        Logger::error("LogMemoTool: JSON parse error: " + std::string(e.what()));
        return ToolResult::error_result("Invalid JSON parameters: " + std::string(e.what()));
    } catch (const std::exception& e) {
        Logger::error("LogMemoTool: Execution error: " + std::string(e.what()));
        return ToolResult::error_result("Tool execution error: " + std::string(e.what()));
    }
}

std::string LogMemoTool::get_memo_file_path() const {
    // Create memos directory if it doesn't exist
    std::string memos_dir = session_log_dir_ + "/memos";
    
    // For now, use a simple memos.txt file in session log dir
    // TODO: Could organize by date or use database
    return session_log_dir_ + "/memos.txt";
}

} // namespace memo_rf
