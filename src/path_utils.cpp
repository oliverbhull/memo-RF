#include "path_utils.h"
#include <cstdlib>
#include <fstream>
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

static bool espeak_data_exists(const std::string& base) {
    std::ifstream f(base + "/phontab");
    return f.good();
}

std::string default_espeak_data_path() {
#if defined(__linux__)
    const char* candidates[] = {
        "/usr/share/espeak-ng-data",
        "/usr/lib/aarch64-linux-gnu/espeak-ng-data",
        "/usr/lib/x86_64-linux-gnu/espeak-ng-data",
    };
    for (const char* p : candidates) {
        if (espeak_data_exists(p)) return p;
    }
    return "/usr/share/espeak-ng-data";  // Piper may still use PATH
#elif defined(__APPLE__)
    return "/opt/homebrew/share/espeak-ng-data";
#else
    return "";
#endif
}

} // namespace memo_rf
