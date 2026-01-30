#pragma once

/**
 * @file conversation_memory.h
 * @brief Conversation history management
 *
 * Features:
 * - Bounded history (max messages)
 * - Token estimation and limiting
 * - Persistence (save/load)
 * - Export to LLM-compatible formats
 */

#include "core/types.h"
#include "core/constants.h"
#include <string>
#include <vector>
#include <memory>
#include <optional>

namespace memo_rf {
namespace memory {

/**
 * @brief Configuration for conversation memory
 */
struct ConversationConfig {
    /// Maximum messages to keep in history
    size_t max_messages = constants::memory::MAX_HISTORY_MESSAGES;

    /// Maximum estimated tokens before pruning old messages
    size_t max_tokens = constants::memory::MAX_HISTORY_TOKENS;

    /// System prompt (always first message)
    std::string system_prompt = "You are a helpful assistant.";

    /// Path for persistence (empty = no persistence)
    std::string persistence_path;

    /// Auto-save after each message
    bool auto_save = false;
};

/**
 * @brief Single message in conversation
 */
struct ConversationMessage {
    MessageRole role;
    std::string content;
    std::string tool_call_id;     // For tool results
    std::string tool_calls_json;  // For assistant messages with tool calls
    int64_t timestamp_ms = 0;

    /// Estimate token count for this message
    size_t estimated_tokens() const;

    /// Create from role and content
    static ConversationMessage system(const std::string& content);
    static ConversationMessage user(const std::string& content);
    static ConversationMessage assistant(const std::string& content);
    static ConversationMessage assistant_with_tools(const std::string& content,
                                                     const std::string& tool_calls_json);
    static ConversationMessage tool(const std::string& tool_call_id,
                                    const std::string& content);
};

/**
 * @brief Conversation memory manager
 */
class ConversationMemory {
public:
    explicit ConversationMemory(const ConversationConfig& config = {});
    ~ConversationMemory();

    // Non-copyable
    ConversationMemory(const ConversationMemory&) = delete;
    ConversationMemory& operator=(const ConversationMemory&) = delete;

    // =========================================================================
    // Message Management
    // =========================================================================

    /// Add a user message
    void add_user_message(const std::string& content);

    /// Add an assistant message
    void add_assistant_message(const std::string& content);

    /// Add an assistant message with tool calls
    void add_assistant_message_with_tools(const std::string& content,
                                          const std::string& tool_calls_json);

    /// Add a tool result
    void add_tool_result(const std::string& tool_call_id,
                         const std::string& content);

    /// Clear all messages (except system prompt)
    void clear();

    // =========================================================================
    // Query
    // =========================================================================

    /// Get all messages (including system prompt)
    std::vector<ConversationMessage> get_messages() const;

    /// Get recent N messages (including system prompt)
    std::vector<ConversationMessage> get_recent_messages(size_t n) const;

    /// Get message count (excluding system prompt)
    size_t message_count() const;

    /// Get total estimated tokens
    size_t estimated_tokens() const;

    /// Check if empty (only system prompt)
    bool is_empty() const;

    // =========================================================================
    // Export Formats
    // =========================================================================

    /// Export to JSON array (for Ollama/OpenAI API)
    std::string to_json() const;

    /// Export to JSON array of message strings (for existing LLM client)
    std::vector<std::string> to_json_strings() const;

    // =========================================================================
    // Persistence
    // =========================================================================

    /// Save to file
    VoidResult save(const std::string& path = "") const;

    /// Load from file
    VoidResult load(const std::string& path = "");

    // =========================================================================
    // Configuration
    // =========================================================================

    /// Update system prompt
    void set_system_prompt(const std::string& prompt);

    /// Get current system prompt
    std::string get_system_prompt() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace memory
} // namespace memo_rf
