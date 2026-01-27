#include "router.h"
#include "utils.h"
#include <map>
#include <cctype>

namespace memo_rf {

class Router::Impl {
public:
    Impl() {
        // Initialize fast path rules (removed "copy" - no acknowledgment needed)
        add_fast_path_rule("roger", "roger.");
        add_fast_path_rule("affirmative", "affirmative.");
        add_fast_path_rule("negative", "negative.");
        add_fast_path_rule("stand by", "stand by.");
        add_fast_path_rule("over", "over.");
    }
    
    Plan decide(const std::string& transcript, const std::string& context) {
        Plan plan;
        
        // Normalize transcript (reserve space to avoid reallocation)
        std::string normalized;
        normalized.reserve(transcript.size());
        normalized = utils::normalize_copy(transcript);
        
        // Check fast path rules
        for (const auto& [pattern, response] : fast_path_rules_) {
            if (normalized.find(pattern) != std::string::npos) {
                plan.type = PlanType::Speak;
                plan.answer_text = response;
                plan.needs_llm = false;
                return plan;
            }
        }
        
        // Default: use LLM (no acknowledgment, just beep and answer)
        plan.type = PlanType::SpeakAckThenAnswer;
        plan.ack_text = "";  // Empty - no verbal acknowledgment, just VOX beep
        plan.answer_text = ""; // Will be filled by LLM
        plan.needs_llm = true;
        
        return plan;
    }
    
    void add_fast_path_rule(const std::string& pattern, const std::string& response) {
        std::string normalized_pattern = utils::normalize_copy(pattern);
        fast_path_rules_[normalized_pattern] = response;
    }

private:
    std::map<std::string, std::string> fast_path_rules_;
};

Router::Router() : pimpl_(std::make_unique<Impl>()) {}
Router::~Router() = default;

Plan Router::decide(const std::string& transcript, const std::string& context) {
    return pimpl_->decide(transcript, context);
}

void Router::add_fast_path_rule(const std::string& pattern, const std::string& response) {
    pimpl_->add_fast_path_rule(pattern, response);
}

} // namespace memo_rf
