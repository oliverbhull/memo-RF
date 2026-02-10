#pragma once

#include <string>
#include <vector>

namespace memo_rf {

/// Result of a plugin handling a voice command.
struct ActionResult {
    bool success = false;
    std::string response_text;  ///< What to speak back via TTS
    std::string error;
};

/// Generic plugin interface for voice-driven external integrations.
/// Each plugin can match transcripts, execute actions, and contribute
/// domain-specific vocabulary for STT boosting.
class ActionPlugin {
public:
    virtual ~ActionPlugin() = default;

    /// Plugin name for logging/config (e.g. "muni", "home_assistant")
    virtual std::string name() const = 0;

    /// Test if this plugin handles the given transcript.
    /// Returns true if matched; fills out `result` with the action outcome.
    /// Called with transcript text (post wake-word strip, lowercased).
    virtual bool try_handle(const std::string& transcript, ActionResult& result) = 0;

    /// Priority (lower = checked first). Safety-critical plugins should use low values.
    virtual int priority() const { return 100; }

    /// Domain-specific vocabulary for Whisper STT initial_prompt boosting.
    /// Return words/phrases the user might say that are domain-specific.
    virtual std::vector<std::string> vocab() const { return {}; }
};

} // namespace memo_rf
