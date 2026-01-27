#include "agent.h"
#include "audio_io.h"
#include "config.h"
#include "logger.h"
#include <signal.h>
#include <csignal>

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
    
    // Load config
    std::string config_path = "config/config.json";
    if (argc > 1) {
        config_path = argv[1];
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
