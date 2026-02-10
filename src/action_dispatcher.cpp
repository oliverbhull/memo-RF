#include "action_dispatcher.h"
#include "logger.h"
#include <algorithm>

namespace memo_rf {

void ActionDispatcher::register_plugin(std::shared_ptr<ActionPlugin> plugin) {
    plugins_.push_back(std::move(plugin));
    // Keep sorted by priority (lower = first)
    std::sort(plugins_.begin(), plugins_.end(),
        [](const std::shared_ptr<ActionPlugin>& a, const std::shared_ptr<ActionPlugin>& b) {
            return a->priority() < b->priority();
        });
}

bool ActionDispatcher::dispatch(const std::string& transcript, ActionResult& result) {
    for (auto& plugin : plugins_) {
        if (plugin->try_handle(transcript, result)) {
            Logger::info("[ActionDispatcher] Plugin \"" + plugin->name() + "\" handled transcript");
            return true;
        }
    }
    return false;
}

} // namespace memo_rf
