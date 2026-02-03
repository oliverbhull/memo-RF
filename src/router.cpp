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
    
    Plan decide(const Transcript& transcript, const std::string& context,
                float repair_confidence_threshold, const std::string& repair_phrase) {
        if (repair_confidence_threshold > 0.0f && transcript.confidence < repair_confidence_threshold) {
            Plan plan;
            plan.type = PlanType::Speak;
            plan.answer_text = repair_phrase.empty() ? "Say again, over" : repair_phrase;
            plan.needs_llm = false;
            return plan;
        }
        return decide(transcript.text, context);
    }

    Plan decide(const std::string& transcript, const std::string& context) {
        Plan plan;
        
        // Normalize transcript (reserve space to avoid reallocation)
        std::string normalized;
        normalized.reserve(transcript.size());
        normalized = utils::normalize_copy(transcript);
        
        // Check fast path rules (whole word matching to avoid false positives)
        for (const auto& [pattern, response] : fast_path_rules_) {
            // Check for whole word match (word boundaries)
            size_t pos = normalized.find(pattern);
            while (pos != std::string::npos) {
                // Check if it's at start or preceded by space/punctuation
                bool start_ok = (pos == 0 || !std::isalnum(normalized[pos - 1]));
                // Check if it's at end or followed by space/punctuation
                size_t end_pos = pos + pattern.length();
                bool end_ok = (end_pos >= normalized.length() || !std::isalnum(normalized[end_pos]));
                
                if (start_ok && end_ok) {
                    plan.type = PlanType::Speak;
                    plan.answer_text = response;
                    plan.needs_llm = false;
                    return plan;
                }
                // Continue searching
                pos = normalized.find(pattern, pos + 1);
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

Plan Router::decide(const Transcript& transcript, const std::string& context,
                    float repair_confidence_threshold, const std::string& repair_phrase) {
    return pimpl_->decide(transcript, context, repair_confidence_threshold, repair_phrase);
}

Plan Router::decide(const std::string& transcript_text, const std::string& context) {
    Transcript t;
    t.text = transcript_text;
    t.confidence = 1.0f;
    t.token_count = 0;
    return pimpl_->decide(t.text, context);
}

void Router::add_fast_path_rule(const std::string& pattern, const std::string& response) {
    pimpl_->add_fast_path_rule(pattern, response);
}

} // namespace memo_rf
