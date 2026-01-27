#pragma once

#include "tool.h"
#include <string>
#include <memory>

namespace memo_rf {

/**
 * @brief Tool for internal database/knowledge base search
 * 
 * Allows the LLM to search previously stored memos, notes, or information.
 * This is a placeholder implementation - will connect to database later.
 */
class InternalSearchTool : public Tool {
public:
    /**
     * @brief Construct internal search tool
     * @param session_log_dir Directory where memos/logs are stored
     */
    explicit InternalSearchTool(const std::string& session_log_dir);
    
    std::string name() const override { return "internal_search"; }
    
    std::string description() const override {
        return "Search the internal database or knowledge base for previously stored memos, "
               "notes, or information. Use this when the user asks about something they mentioned before.";
    }
    
    std::string parameter_schema() const override;
    
    ToolResult execute(const std::string& params_json) override;

private:
    std::string session_log_dir_;
    
    std::string get_memo_file_path() const;
};

} // namespace memo_rf
