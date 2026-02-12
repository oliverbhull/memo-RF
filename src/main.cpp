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
    
    // Load config: pass directory (e.g. config) for identity loading (active.json + defaults + robot/agent file),
    // or pass a file path (e.g. config/config.json) for legacy single-file config.
    std::string config_path = "config";
    if (argc > 1) {
        config_path = argv[1];
    } else {
        // Try config directory relative to executable (e.g. build/../config)
        char buf[1024];
        ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
        if (len != -1) {
            buf[len] = '\0';
            std::string exe_dir(buf);
            size_t pos = exe_dir.find_last_of('/');
            if (pos != std::string::npos) {
                exe_dir = exe_dir.substr(0, pos);
                std::string config_dir = exe_dir + "/../config";
                std::ifstream test(config_dir + "/config.json");
                if (test.good()) {
                    config_path = config_dir;
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
