#include "twitter_flow.h"
#include "utils.h"
#include "logger.h"
#include <cctype>
#include <algorithm>

#ifdef __APPLE__
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#endif

namespace memo_rf {

namespace {

std::string normalize(const std::string& s) {
    std::string t = utils::trim_copy(s);
    return utils::normalize_copy(t);
}

/// Word-boundary match: transcript equals or is exactly one of the words (yes, yeah, yup).
bool is_confirmation_word(const std::string& normalized) {
    if (normalized.empty()) return false;
    auto is_word_char = [](char c) { return std::isalnum(static_cast<unsigned char>(c)); };
    auto match_word = [&](const std::string& word) {
        size_t pos = normalized.find(word);
        if (pos == std::string::npos) return false;
        bool start_ok = (pos == 0 || !is_word_char(normalized[pos - 1]));
        size_t end_pos = pos + word.size();
        bool end_ok = (end_pos >= normalized.size() || !is_word_char(normalized[end_pos]));
        return start_ok && end_ok;
    };
    return match_word("yes") || match_word("yeah") || match_word("yup");
}

/// Word-boundary match: "no" or "edit" (cancel/edit and re-enter tweet text).
bool is_cancel_or_edit_word(const std::string& normalized) {
    if (normalized.empty()) return false;
    auto is_word_char = [](char c) { return std::isalnum(static_cast<unsigned char>(c)); };
    auto match_word = [&](const std::string& word) {
        size_t pos = normalized.find(word);
        if (pos == std::string::npos) return false;
        bool start_ok = (pos == 0 || !is_word_char(normalized[pos - 1]));
        size_t end_pos = pos + word.size();
        bool end_ok = (end_pos >= normalized.size() || !is_word_char(normalized[end_pos]));
        return start_ok && end_ok;
    };
    return match_word("no") || match_word("edit");
}

/// Match initial flow: "open twitter" + draft/post/new tweet, or "new tweet" on its own.
bool matches_open_twitter_draft(const std::string& normalized) {
    bool has_open_twitter = normalized.find("open twitter") != std::string::npos;
    bool has_draft = normalized.find("draft") != std::string::npos;
    bool has_post = normalized.find("post") != std::string::npos;
    bool has_new_tweet = normalized.find("new tweet") != std::string::npos;
    return (has_open_twitter && (has_draft || has_post || has_new_tweet)) || has_new_tweet;
}

#ifdef __APPLE__
void run_osascript(const std::string& script) {
    std::string cmd = "osascript -e '" + script + "'";
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        Logger::warn("Twitter flow: osascript returned " + std::to_string(ret));
    }
}

/// Escape double quotes for AppleScript string (double them).
std::string escape_applescript_string(const std::string& s) {
    std::string r;
    for (char c : s) {
        if (c == '"') r += "\"\"";
        else r += c;
    }
    return r;
}

/// Escape single quotes for shell: ' -> '\''
std::string escape_shell_single_quotes(const std::string& s) {
    std::string r;
    for (char c : s) {
        if (c == '\'') r += "'\\''";
        else r += c;
    }
    return r;
}

void apple_open_twitter_draft() {
    run_osascript("tell application \"Safari\" to activate");
    run_osascript("tell application \"Safari\" to open location \"https://x.com\"");
    run_osascript("delay 3");
    run_osascript("tell application \"System Events\" to keystroke \"n\"");
}

void apple_set_clipboard(const std::string& content) {
    run_osascript("set the clipboard to \"\"");  // clear first
    std::string escaped = escape_applescript_string(content);
    std::string script = "set the clipboard to \"" + escaped + "\"";
    std::string safe = escape_shell_single_quotes(script);
    std::string cmd = "osascript -e '" + safe + "'";
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        Logger::warn("Twitter flow: set clipboard failed");
    }
}

void apple_paste() {
    run_osascript("tell application \"System Events\" to keystroke \"v\" using command down");
}

void apple_cmd_return() {
    run_osascript("tell application \"System Events\" to key code 36 using command down");
}

/// Clear compose box (Cmd+A, Delete) and clipboard; use when user says "no" or "edit".
void apple_clear_compose_and_clipboard() {
    run_osascript("tell application \"System Events\" to keystroke \"a\" using command down");
    run_osascript("delay 0.2");
    run_osascript("tell application \"System Events\" to key code 51");  // Delete/Backspace
    run_osascript("set the clipboard to \"\"");  // clear clipboard
}
#endif

} // namespace

TwitterFlowResult twitter_flow_handle(const std::string& transcript,
                                      TwitterFlowState& state) {
    TwitterFlowResult result;
    std::string normalized = normalize(transcript);

#ifdef __APPLE__
    switch (state) {
    case TwitterFlowState::Idle:
        if (matches_open_twitter_draft(normalized)) {
            apple_open_twitter_draft();
            state = TwitterFlowState::DraftOpened;
            result.handled = true;
            result.response_text = "What do you want to tweet?";
        }
        break;

    case TwitterFlowState::DraftOpened:
        if (!normalized.empty()) {
            apple_set_clipboard(transcript);
            apple_paste();
            state = TwitterFlowState::Confirming;
            result.handled = true;
            result.response_text = "Confirming you want to post: " + utils::trim_copy(transcript) + "?";
        }
        break;

    case TwitterFlowState::Confirming:
        if (is_confirmation_word(normalized)) {
            apple_cmd_return();
            state = TwitterFlowState::Idle;
            result.handled = true;
            result.response_text = "Tweet posted.";
        } else if (is_cancel_or_edit_word(normalized)) {
            apple_clear_compose_and_clipboard();
            state = TwitterFlowState::DraftOpened;
            result.handled = true;
            result.response_text = "What do you want to tweet?";
        }
        break;
    }
#else
    (void)state;
    (void)normalized;
    (void)transcript;
#endif

    return result;
}

} // namespace memo_rf
