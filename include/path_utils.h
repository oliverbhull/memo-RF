#pragma once

/**
 * @file path_utils.h
 * @brief Path resolution: ~ expansion and platform defaults
 */

#include <string>

namespace memo_rf {

/**
 * Expands leading ~ to $HOME (getenv("HOME")). ~user not supported.
 * Returns path unchanged if path is empty or ~ expansion not applicable.
 */
std::string expand_path(const std::string& path);

/**
 * Returns platform-specific default espeak-ng data path when config leaves it empty.
 * macOS: /opt/homebrew/share/espeak-ng-data
 * Linux: /usr/share/espeak-ng-data
 * Other: empty string
 */
std::string default_espeak_data_path();

} // namespace memo_rf
