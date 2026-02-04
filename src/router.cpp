#include "router.h"

namespace memo_rf {

class Router::Impl {
public:
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
        (void)transcript;
        (void)context;
        Plan plan;
        plan.type = PlanType::SpeakAckThenAnswer;
        plan.ack_text = "";
        plan.answer_text = "";
        plan.needs_llm = true;
        return plan;
    }
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

} // namespace memo_rf
