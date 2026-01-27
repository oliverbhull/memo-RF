#pragma once

#include <string>
#include <algorithm>
#include <cctype>

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

} // namespace utils

} // namespace memo_rf
