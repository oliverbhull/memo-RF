#include "agent.h"
#include "audio_io.h"
#include "config.h"
#include "logger.h"
#include <signal.h>
#include <csignal>
#include <unistd.h>
#include <fstream>

namespace memo_rf {

static VoiceAgent* g_agent = nullptr;

void signal_handler(int signal) {
    Logger::info("\nShutting down...");
    if (g_agent) {
        g_agent->shutdown();
    }
}

} // namespace memo_rf

int main(int argc, char* argv[]) {
    // Initialize logger (default to INFO level, console output)
    memo_rf::Logger::initialize(memo_rf::LogLevel::INFO);
    
    // List devices if requested (check before loading config)
    if (argc > 1 && std::string(argv[1]) == "--list-devices") {
        memo_rf::AudioIO::list_devices();
        memo_rf::Logger::shutdown();
        return 0;
    }
    
    // Load config (default: config/config.json relative to binary location)
    std::string config_path = "config/config.json";
    if (argc > 1) {
        config_path = argv[1];
    } else {
        // Try to find config relative to executable location
        std::string exe_dir;
        char buf[1024];
        ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
        if (len != -1) {
            buf[len] = '\0';
            exe_dir = std::string(buf);
            size_t pos = exe_dir.find_last_of('/');
            if (pos != std::string::npos) {
                exe_dir = exe_dir.substr(0, pos);
                // Check parent directory (build/../config)
                std::string parent_config = exe_dir + "/../config/config.json";
                std::ifstream test(parent_config);
                if (test.good()) {
                    config_path = parent_config;
                }
            }
        }
    }
    
    memo_rf::Config config = memo_rf::Config::load_from_file(config_path);
    
    // Create and run agent
    memo_rf::VoiceAgent agent(config);
    memo_rf::g_agent = &agent;
    
    // Set up signal handlers
    std::signal(SIGINT, memo_rf::signal_handler);
    std::signal(SIGTERM, memo_rf::signal_handler);
    
    // Run agent
    int result = agent.run();
    
    memo_rf::g_agent = nullptr;
    
    // Shutdown logger
    memo_rf::Logger::shutdown();
    
    return result;
}
