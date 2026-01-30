#pragma once

#include "common.h"
#include "config.h"
#include <string>
#include <memory>
#include <vector>

namespace memo_rf {

/**
 * @brief Tool call structure from LLM response
 */
struct ToolCall {
    std::string id;           // Tool call ID from LLM
    std::string name;         // Tool name
    std::string arguments;    // JSON string of arguments
};

/**
 * @brief LLM response that may contain text or tool calls
 */
struct LLMResponse {
    std::string content;                    // Text content (if any)
    std::vector<ToolCall> tool_calls;       // Tool calls (if any)
    bool has_tool_calls() const { return !tool_calls.empty(); }
    bool has_content() const { return !content.empty(); }
};

class LLMClient {
public:
    explicit LLMClient(const LLMConfig& config);
    ~LLMClient();
    
    // Non-copyable
    LLMClient(const LLMClient&) = delete;
    LLMClient& operator=(const LLMClient&) = delete;
    
    /**
     * @brief Generate response with tool support
     * @param prompt User prompt
     * @param context Optional context
     * @param tool_definitions_json JSON array of tool definitions (empty = no tools)
     * @param conversation_history Previous messages in conversation (for multi-turn)
     * @param timeout_ms Timeout in ms (0 = use config default)
     * @param max_tokens Max tokens (0 = use config default)
     * @return LLMResponse with content and/or tool calls
     */
    LLMResponse generate_with_tools(
        const std::string& prompt,
        const std::string& tool_definitions_json = "",
        const std::vector<std::string>& conversation_history = {},
        int timeout_ms = 0,
        int max_tokens = 0);
    
    /**
     * @brief Generate response (legacy method, for backward compatibility)
     * @param prompt User prompt
     * @param context Optional context
     * @param timeout_ms Timeout in ms (0 = use config default)
     * @param max_tokens Max tokens (0 = use config default)
     * @return Text response
     */
    std::string generate(const std::string& prompt, 
                        const std::string& context = "",
                        int timeout_ms = 0,
                        int max_tokens = 0);
    
    /**
     * @brief Add tool result to conversation for follow-up
     * @param tool_call_id ID of the tool call
     * @param result_content Result content from tool execution
     * @return Formatted message for conversation history
     */
    static std::string format_tool_result(const std::string& tool_call_id, 
                                         const std::string& result_content);

    /**
     * @brief Resolve user message using conversation context (follow-ups, STT errors).
     * When conversation_history has prior turns, uses context to produce a single clarified
     * user message. Call this before the main LLM when session has history.
     * @param raw_user_message Latest STT output (may contain recognition errors)
     * @param conversation_history Session history (e.g. from ConversationMemory::to_json_strings())
     * @param timeout_ms Timeout in ms (0 = use config default)
     * @return Clarified user message, or raw_user_message if clarification fails or no history
     */
    std::string clarify_user_message(
        const std::string& raw_user_message,
        const std::vector<std::string>& conversation_history,
        int timeout_ms = 0);

    /**
     * @brief Summarize conversation (for background context compression).
     * Uses a fixed system prompt and low max_tokens. Thread-safe when using a
     * dedicated LLMClient instance per thread.
     */
    std::string summarize_conversation(
        const std::string& conversation_text,
        int timeout_ms = 0);
    
    // Check if client is ready
    bool is_ready() const;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace memo_rf
