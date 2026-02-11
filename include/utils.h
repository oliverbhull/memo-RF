#pragma once

#include <string>
#include <algorithm>
#include <cctype>
#include <vector>

namespace memo_rf {

/**
 * @brief String utility functions
 */
namespace utils {

/**
 * @brief Trim whitespace from both ends of a string
 * @param str String to trim (modified in place)
 * @return Reference to the trimmed string
 */
inline std::string& trim(std::string& str) {
    str.erase(0, str.find_first_not_of(" \t\n\r"));
    str.erase(str.find_last_not_of(" \t\n\r") + 1);
    return str;
}

/**
 * @brief Trim whitespace from both ends of a string (returns copy)
 * @param str String to trim
 * @return Trimmed copy of the string
 */
inline std::string trim_copy(const std::string& str) {
    std::string result = str;
    trim(result);
    return result;
}

/**
 * @brief Normalize string to lowercase
 * @param str String to normalize (modified in place)
 * @return Reference to the normalized string
 */
inline std::string& normalize(std::string& str) {
    std::transform(str.begin(), str.end(), str.begin(),
                  [](unsigned char c) { return std::tolower(c); });
    return str;
}

/**
 * @brief Normalize string to lowercase (returns copy)
 * @param str String to normalize
 * @return Normalized copy of the string
 */
inline std::string normalize_copy(const std::string& str) {
    std::string result = str;
    normalize(result);
    return result;
}

/**
 * @brief Check if string is empty or contains only whitespace
 * @param str String to check
 * @return True if string is empty or whitespace-only
 */
inline bool is_empty_or_whitespace(const std::string& str) {
    return str.find_first_not_of(" \t\n\r") == std::string::npos;
}

/**
 * @brief Check if transcript text is blank (empty/whitespace or equals blank sentinel)
 * @param text Raw transcript text
 * @param blank_sentinel String to treat as blank (e.g. "[BLANK_AUDIO]"); compared after trim
 * @return True if text is empty, whitespace-only, or equals blank_sentinel after trim
 */
inline bool is_blank_transcript(const std::string& text, const std::string& blank_sentinel = "[BLANK_AUDIO]") {
    std::string t = trim_copy(text);
    if (t.empty()) return true;
    if (!blank_sentinel.empty() && t == blank_sentinel) return true;

    // Filter common noise patterns that STT transcribes from static/silence
    std::string lower = t;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c) { return std::tolower(c); });

    // Remove parentheses and other punctuation for pattern matching
    std::string cleaned;
    for (char c : lower) {
        if (std::isalnum(c) || std::isspace(c)) {
            cleaned += c;
        }
    }
    cleaned = trim_copy(cleaned);

    // Common noise patterns
    static const std::vector<std::string> noise_patterns = {
        "static", "silence", "noise", "inaudible", "unclear",
        "background noise", "radio static", "interference",
        "nothing", "blank", "mute", "hiss", "hissing",
        "clicking", "beeping", "buzzing", "crackling", "humming",
        "whooshing", "popping", "rustling", "crackle", "buzz",
        "beep", "click", "pop", "hum", "whoosh", "rustle"
    };

    for (const auto& pattern : noise_patterns) {
        if (cleaned == pattern || cleaned.find(pattern) != std::string::npos) {
            return true;
        }
    }

    // Filter very short transcripts (likely noise)
    if (cleaned.length() < 3) {
        return true;
    }

    return false;
}

/**
 * @brief Remove trailing " over." or " over" (case-insensitive) for TTS when using end tone instead.
 * @param str Text that may end with "over" / "over."
 * @return Copy of str with trailing over/over. removed (trimmed)
 */
inline std::string strip_trailing_over(const std::string& str) {
    std::string t = trim_copy(str);
    if (t.empty()) return t;
    std::string n = normalize_copy(t);
    size_t len = n.size();
    if (len >= 5 && n.compare(len - 5, 5, "over.") == 0)
        return trim_copy(t.substr(0, t.size() - 5));
    if (len >= 4 && n.compare(len - 4, 4, "over") == 0)
        return trim_copy(t.substr(0, t.size() - 4));
    return t;
}

/**
 * @brief Ensure transmission text ends with " over." (radio protocol)
 * Replaces trailing "over and out" with "over." so we never say both or double "over."
 * @param str Text that will be spoken over the air
 * @return Copy of str, with " over." appended if it does not already end with "over" or "over."
 */
inline std::string ensure_ends_with_over(const std::string& str) {
    std::string t = trim_copy(str);
    if (t.empty()) return " over.";
    std::string n = normalize_copy(t);
    size_t len = n.size();
    // Replace "over and out" / "over and out." (and "... over." suffix) with "over." so we never double up.
    if (len >= 18 && n.compare(len - 18, 18, "over and out. over.") == 0) {
        t = t.substr(0, t.size() - 18) + " over.";
        n = normalize_copy(t);
        len = n.size();
    } else if (len >= 13 && n.compare(len - 13, 13, "over and out.") == 0) {
        t = t.substr(0, t.size() - 13) + " over.";
        n = normalize_copy(t);
        len = n.size();
    } else if (len >= 12 && n.compare(len - 12, 12, "over and out") == 0) {
        t = t.substr(0, t.size() - 12) + " over.";
        n = normalize_copy(t);
        len = n.size();
    }
    if (len >= 5 && n.compare(len - 5, 5, "over.") == 0) return t;
    if (len >= 4 && n.compare(len - 4, 4, "over") == 0) return t;
    return t + " over.";
}

} // namespace utils

} // namespace memo_rf
