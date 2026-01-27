#pragma once

#include "tool.h"
#include <string>
#include <memory>

namespace memo_rf {

/**
 * @brief Tool for logging/memoing user information
 * 
 * Allows the LLM to save information the user mentioned for later reference.
 * Writes to session log or a separate memo file.
 */
class LogMemoTool : public Tool {
public:
    /**
     * @brief Construct log memo tool
     * @param session_log_dir Directory for session logs
     */
    explicit LogMemoTool(const std::string& session_log_dir);
    
    std::string name() const override { return "log_memo"; }
    
    std::string description() const override {
        return "Log or memo something the user said for later reference. "
               "Use this when the user wants to remember something or make a note.";
    }
    
    std::string parameter_schema() const override;
    
    ToolResult execute(const std::string& params_json) override;

private:
    std::string session_log_dir_;
    
    std::string get_memo_file_path() const;
};

} // namespace memo_rf
