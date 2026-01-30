/**
 * @file types.cpp
 * @brief Implementation of core type helper functions
 */

#include "core/types.h"

namespace memo_rf {

Message Message::system(const std::string& content) {
    Message msg;
    msg.role = MessageRole::System;
    msg.content = content;
    msg.timestamp_ms = now_ms();
    return msg;
}

Message Message::user(const std::string& content) {
    Message msg;
    msg.role = MessageRole::User;
    msg.content = content;
    msg.timestamp_ms = now_ms();
    return msg;
}

Message Message::assistant(const std::string& content) {
    Message msg;
    msg.role = MessageRole::Assistant;
    msg.content = content;
    msg.timestamp_ms = now_ms();
    return msg;
}

Message Message::tool(const std::string& tool_call_id, const std::string& content) {
    Message msg;
    msg.role = MessageRole::Tool;
    msg.content = content;
    msg.tool_call_id = tool_call_id;
    msg.timestamp_ms = now_ms();
    return msg;
}

} // namespace memo_rf
