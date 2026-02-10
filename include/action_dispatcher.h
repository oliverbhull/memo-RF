#pragma once

#include "action_plugin.h"
#include <memory>
#include <vector>

namespace memo_rf {

/// Owns registered plugins and dispatches transcripts to the first matching one.
class ActionDispatcher {
public:
    /// Register a plugin. Plugins are sorted by priority (lower = checked first).
    void register_plugin(std::shared_ptr<ActionPlugin> plugin);

    /// Try all plugins in priority order. Returns true if any handled the transcript.
    bool dispatch(const std::string& transcript, ActionResult& result);

    /// Access registered plugins (e.g. to collect vocab).
    const std::vector<std::shared_ptr<ActionPlugin>>& plugins() const { return plugins_; }

    /// Number of registered plugins.
    size_t size() const { return plugins_.size(); }

private:
    std::vector<std::shared_ptr<ActionPlugin>> plugins_;
};

} // namespace memo_rf
