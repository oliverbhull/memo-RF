#include "path_utils.h"
#include <cstdlib>
#include <string>

namespace memo_rf {

std::string expand_path(const std::string& path) {
    if (path.empty()) return path;
    if (path.size() == 1 && path[0] == '~') {
        const char* home = std::getenv("HOME");
        if (home) return std::string(home);
        return path;
    }
    if (path.size() >= 2 && path[0] == '~' && (path[1] == '/' || path[1] == '\\')) {
        const char* home = std::getenv("HOME");
        if (home) return std::string(home) + path.substr(1);
        return path;
    }
    return path;
}

std::string default_espeak_data_path() {
#if defined(__linux__)
    return "/usr/share/espeak-ng-data";
#elif defined(__APPLE__)
    return "/opt/homebrew/share/espeak-ng-data";
#else
    return "";
#endif
}

} // namespace memo_rf
