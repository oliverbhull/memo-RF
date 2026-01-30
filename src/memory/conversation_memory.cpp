/**
 * @file conversation_memory.cpp
 * @brief Conversation memory implementation
 */

#include "memory/conversation_memory.h"
#include "logger.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>

using json = nlohmann::json;

namespace memo_rf {
namespace memory {

// =============================================================================
// ConversationMessage Implementation
// =============================================================================

size_t ConversationMessage::estimated_tokens() const {
    // Rough estimation: ~4 characters per token for English
    size_t char_count = content.size() + tool_call_id.size() + tool_calls_json.size();
    return static_cast<size_t>(char_count * constants::memory::TOKENS_PER_CHAR) + 4; // +4 for role tokens
}

ConversationMessage ConversationMessage::system(const std::string& content) {
    ConversationMessage msg;
    msg.role = MessageRole::System;
    msg.content = content;
    msg.timestamp_ms = now_ms();
    return msg;
}

ConversationMessage ConversationMessage::user(const std::string& content) {
    ConversationMessage msg;
    msg.role = MessageRole::User;
    msg.content = content;
    msg.timestamp_ms = now_ms();
    return msg;
}

ConversationMessage ConversationMessage::assistant(const std::string& content) {
    ConversationMessage msg;
    msg.role = MessageRole::Assistant;
    msg.content = content;
    msg.timestamp_ms = now_ms();
    return msg;
}

ConversationMessage ConversationMessage::assistant_with_tools(
    const std::string& content,
    const std::string& tool_calls_json)
{
    ConversationMessage msg;
    msg.role = MessageRole::Assistant;
    msg.content = content;
    msg.tool_calls_json = tool_calls_json;
    msg.timestamp_ms = now_ms();
    return msg;
}

ConversationMessage ConversationMessage::tool(
    const std::string& tool_call_id,
    const std::string& content)
{
    ConversationMessage msg;
    msg.role = MessageRole::Tool;
    msg.content = content;
    msg.tool_call_id = tool_call_id;
    msg.timestamp_ms = now_ms();
    return msg;
}

// =============================================================================
// ConversationMemory Implementation
// =============================================================================

class ConversationMemory::Impl {
public:
    explicit Impl(const ConversationConfig& config)
        : config_(config)
    {
        // Initialize with system message
        system_message_ = ConversationMessage::system(config.system_prompt);

        LOG_LLM("ConversationMemory initialized:");
        std::ostringstream oss;
        oss << "  max_messages=" << config.max_messages
            << ", max_tokens=" << config.max_tokens
            << ", persistence=" << (config.persistence_path.empty() ? "disabled" : config.persistence_path);
        LOG_LLM(oss.str());
    }

    void add_user_message(const std::string& content) {
        add_message(ConversationMessage::user(content));
    }

    void add_assistant_message(const std::string& content) {
        add_message(ConversationMessage::assistant(content));
    }

    void add_assistant_message_with_tools(const std::string& content,
                                          const std::string& tool_calls_json) {
        add_message(ConversationMessage::assistant_with_tools(content, tool_calls_json));
    }

    void add_tool_result(const std::string& tool_call_id, const std::string& content) {
        add_message(ConversationMessage::tool(tool_call_id, content));
    }

    void clear() {
        messages_.clear();
        LOG_LLM("Conversation history cleared");
    }

    std::vector<ConversationMessage> get_messages() const {
        std::vector<ConversationMessage> result;
        result.reserve(messages_.size() + 1);
        result.push_back(system_message_);
        result.insert(result.end(), messages_.begin(), messages_.end());
        return result;
    }

    std::vector<ConversationMessage> get_recent_messages(size_t n) const {
        std::vector<ConversationMessage> result;
        result.push_back(system_message_);

        size_t start = messages_.size() > n ? messages_.size() - n : 0;
        result.insert(result.end(), messages_.begin() + start, messages_.end());

        return result;
    }

    size_t message_count() const {
        return messages_.size();
    }

    size_t estimated_tokens() const {
        size_t total = system_message_.estimated_tokens();
        for (const auto& msg : messages_) {
            total += msg.estimated_tokens();
        }
        return total;
    }

    bool is_empty() const {
        return messages_.empty();
    }

    std::string to_json() const {
        json messages_json = json::array();

        // Add system message
        messages_json.push_back(message_to_json(system_message_));

        // Add conversation messages
        for (const auto& msg : messages_) {
            messages_json.push_back(message_to_json(msg));
        }

        return messages_json.dump();
    }

    std::vector<std::string> to_json_strings() const {
        std::vector<std::string> result;
        result.reserve(messages_.size() + 1);

        // Add system message
        result.push_back(message_to_json(system_message_).dump());

        // Add conversation messages
        for (const auto& msg : messages_) {
            result.push_back(message_to_json(msg).dump());
        }

        return result;
    }

    std::vector<std::string> to_json_strings_recent(size_t n) const {
        std::vector<std::string> result;
        std::vector<ConversationMessage> recent = get_recent_messages(n);
        result.reserve(recent.size());
        for (const auto& msg : recent) {
            result.push_back(message_to_json(msg).dump());
        }
        return result;
    }

    VoidResult save(const std::string& path) const {
        std::string save_path = path.empty() ? config_.persistence_path : path;
        if (save_path.empty()) {
            return VoidResult::failure("No persistence path specified");
        }

        try {
            json data;
            data["system_prompt"] = system_message_.content;
            data["messages"] = json::array();

            for (const auto& msg : messages_) {
                data["messages"].push_back(message_to_json(msg));
            }

            std::ofstream file(save_path);
            if (!file.is_open()) {
                return VoidResult::failure("Failed to open file for writing: " + save_path);
            }

            file << data.dump(2);
            LOG_LLM("Conversation saved to: " + save_path);
            return VoidResult::ok_result();
        } catch (const std::exception& e) {
            return VoidResult::failure(std::string("Save failed: ") + e.what());
        }
    }

    VoidResult load(const std::string& path) {
        std::string load_path = path.empty() ? config_.persistence_path : path;
        if (load_path.empty()) {
            return VoidResult::failure("No persistence path specified");
        }

        try {
            std::ifstream file(load_path);
            if (!file.is_open()) {
                return VoidResult::failure("Failed to open file: " + load_path);
            }

            json data = json::parse(file);

            // Load system prompt
            if (data.contains("system_prompt")) {
                system_message_.content = data["system_prompt"].get<std::string>();
            }

            // Load messages
            messages_.clear();
            if (data.contains("messages") && data["messages"].is_array()) {
                for (const auto& msg_json : data["messages"]) {
                    messages_.push_back(json_to_message(msg_json));
                }
            }

            LOG_LLM("Conversation loaded from: " + load_path +
                   " (" + std::to_string(messages_.size()) + " messages)");
            return VoidResult::ok_result();
        } catch (const std::exception& e) {
            return VoidResult::failure(std::string("Load failed: ") + e.what());
        }
    }

    void set_system_prompt(const std::string& prompt) {
        system_message_.content = prompt;
    }

    std::string get_system_prompt() const {
        return system_message_.content;
    }

private:
    void add_message(ConversationMessage msg) {
        messages_.push_back(std::move(msg));
        prune_if_needed();

        if (config_.auto_save && !config_.persistence_path.empty()) {
            save("");
        }
    }

    void prune_if_needed() {
        // Prune by message count
        while (messages_.size() > config_.max_messages) {
            LOG_LLM("Pruning oldest message (max messages exceeded)");
            messages_.erase(messages_.begin());
        }

        // Prune by token count
        while (estimated_tokens() > config_.max_tokens && messages_.size() > 1) {
            LOG_LLM("Pruning oldest message (max tokens exceeded)");
            messages_.erase(messages_.begin());
        }
    }

    json message_to_json(const ConversationMessage& msg) const {
        json j;

        // Set role
        switch (msg.role) {
            case MessageRole::System:
                j["role"] = "system";
                break;
            case MessageRole::User:
                j["role"] = "user";
                break;
            case MessageRole::Assistant:
                j["role"] = "assistant";
                break;
            case MessageRole::Tool:
                j["role"] = "tool";
                break;
        }

        // Set content
        if (!msg.content.empty()) {
            j["content"] = msg.content;
        }

        // Set tool-specific fields
        if (!msg.tool_call_id.empty()) {
            j["tool_call_id"] = msg.tool_call_id;
        }

        if (!msg.tool_calls_json.empty()) {
            try {
                j["tool_calls"] = json::parse(msg.tool_calls_json);
            } catch (...) {
                // If parsing fails, store as string
                j["tool_calls_raw"] = msg.tool_calls_json;
            }
        }

        return j;
    }

    ConversationMessage json_to_message(const json& j) const {
        ConversationMessage msg;

        // Parse role
        std::string role_str = j.value("role", "user");
        if (role_str == "system") {
            msg.role = MessageRole::System;
        } else if (role_str == "user") {
            msg.role = MessageRole::User;
        } else if (role_str == "assistant") {
            msg.role = MessageRole::Assistant;
        } else if (role_str == "tool") {
            msg.role = MessageRole::Tool;
        }

        // Parse content
        msg.content = j.value("content", "");

        // Parse tool fields
        msg.tool_call_id = j.value("tool_call_id", "");

        if (j.contains("tool_calls")) {
            msg.tool_calls_json = j["tool_calls"].dump();
        } else if (j.contains("tool_calls_raw")) {
            msg.tool_calls_json = j["tool_calls_raw"].get<std::string>();
        }

        return msg;
    }

    ConversationConfig config_;
    ConversationMessage system_message_;
    std::vector<ConversationMessage> messages_;
};

// =============================================================================
// Public Interface
// =============================================================================

ConversationMemory::ConversationMemory(const ConversationConfig& config)
    : impl_(std::make_unique<Impl>(config)) {}

ConversationMemory::~ConversationMemory() = default;

void ConversationMemory::add_user_message(const std::string& content) {
    impl_->add_user_message(content);
}

void ConversationMemory::add_assistant_message(const std::string& content) {
    impl_->add_assistant_message(content);
}

void ConversationMemory::add_assistant_message_with_tools(
    const std::string& content,
    const std::string& tool_calls_json)
{
    impl_->add_assistant_message_with_tools(content, tool_calls_json);
}

void ConversationMemory::add_tool_result(const std::string& tool_call_id,
                                         const std::string& content) {
    impl_->add_tool_result(tool_call_id, content);
}

void ConversationMemory::clear() {
    impl_->clear();
}

std::vector<ConversationMessage> ConversationMemory::get_messages() const {
    return impl_->get_messages();
}

std::vector<ConversationMessage> ConversationMemory::get_recent_messages(size_t n) const {
    return impl_->get_recent_messages(n);
}

size_t ConversationMemory::message_count() const {
    return impl_->message_count();
}

size_t ConversationMemory::estimated_tokens() const {
    return impl_->estimated_tokens();
}

bool ConversationMemory::is_empty() const {
    return impl_->is_empty();
}

std::string ConversationMemory::to_json() const {
    return impl_->to_json();
}

std::vector<std::string> ConversationMemory::to_json_strings() const {
    return impl_->to_json_strings();
}

std::vector<std::string> ConversationMemory::to_json_strings_recent(size_t n) const {
    return impl_->to_json_strings_recent(n);
}

VoidResult ConversationMemory::save(const std::string& path) const {
    return impl_->save(path);
}

VoidResult ConversationMemory::load(const std::string& path) {
    return impl_->load(path);
}

void ConversationMemory::set_system_prompt(const std::string& prompt) {
    impl_->set_system_prompt(prompt);
}

std::string ConversationMemory::get_system_prompt() const {
    return impl_->get_system_prompt();
}

} // namespace memory
} // namespace memo_rf
