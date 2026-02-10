/**
 * Simple test program for the JsonCommandPlugin system
 */

#include "plugins/json_command_plugin.h"
#include "action_dispatcher.h"
#include "logger.h"
#include <iostream>
#include <iomanip>

using namespace memo_rf;

void print_separator() {
    std::cout << std::string(70, '=') << "\n";
}

void test_command(ActionDispatcher& dispatcher, const std::string& transcript) {
    print_separator();
    std::cout << "Testing: \"" << transcript << "\"\n";
    print_separator();

    ActionResult result;
    bool matched = dispatcher.dispatch(transcript, result);

    if (matched) {
        std::cout << "✓ MATCHED\n";
        std::cout << "  Success: " << (result.success ? "YES" : "NO") << "\n";
        std::cout << "  Response: " << result.response_text << "\n";
        if (!result.error.empty()) {
            std::cout << "  Error: " << result.error << "\n";
        }
    } else {
        std::cout << "✗ NO MATCH (would fall through to LLM)\n";
    }
    std::cout << "\n";
}

int main(int argc, char** argv) {
    // Initialize logger
    Logger::initialize(LogLevel::INFO);

    std::string plugin_path = "config/plugins/muni_test.json";
    if (argc > 1) {
        plugin_path = argv[1];
    }

    std::cout << "\n";
    print_separator();
    std::cout << "Memo-RF Plugin System Test\n";
    print_separator();
    std::cout << "Plugin file: " << plugin_path << "\n\n";

    // Load plugin
    ActionDispatcher dispatcher;
    try {
        auto plugin = std::make_shared<JsonCommandPlugin>(plugin_path);
        dispatcher.register_plugin(plugin);

        std::cout << "✓ Plugin loaded successfully\n";
        std::cout << "  Name: " << plugin->name() << "\n";
        std::cout << "  Priority: " << plugin->priority() << "\n";
        std::cout << "  Vocab words: " << plugin->vocab().size() << "\n";
        std::cout << "\n";

        // Show some vocab
        auto vocab = plugin->vocab();
        if (!vocab.empty()) {
            std::cout << "Sample vocab (first 10): ";
            for (size_t i = 0; i < std::min(size_t(10), vocab.size()); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << vocab[i];
            }
            std::cout << "\n\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "✗ Failed to load plugin: " << e.what() << "\n";
        return 1;
    }

    // Test cases
    std::cout << "Running command matching tests...\n\n";

    // E-stop tests
    test_command(dispatcher, "stop the robot");
    test_command(dispatcher, "emergency stop");
    test_command(dispatcher, "halt");

    // E-stop release tests
    test_command(dispatcher, "release");
    test_command(dispatcher, "resume");

    // Navigation tests
    test_command(dispatcher, "go to position 5 3");
    test_command(dispatcher, "navigate to 10 20");
    test_command(dispatcher, "go to five three");
    test_command(dispatcher, "move to position 0 0");

    // Mode change tests
    test_command(dispatcher, "set mode to autonomous");
    test_command(dispatcher, "put the robot in sleep");
    test_command(dispatcher, "change to dance");
    test_command(dispatcher, "set mode to idle");

    // Non-matching tests
    test_command(dispatcher, "what is the weather today");
    test_command(dispatcher, "tell me a joke");

    print_separator();
    std::cout << "Test complete!\n";
    std::cout << "Note: API calls will fail unless mock server is running on localhost:4890\n";
    print_separator();
    std::cout << "\n";

    return 0;
}
