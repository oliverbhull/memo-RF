#pragma once

#include "common.h"
#include <string>
#include <memory>

namespace memo_rf {

/**
 * @brief Plan types for response generation
 */
enum class PlanType {
    NoOp,              ///< Do nothing
    Speak,             ///< Direct response without LLM
    SpeakAckThenAnswer,///< Acknowledge then call LLM for answer
    Fallback           ///< Use fallback response
};

/**
 * @brief Response plan structure
 */
struct Plan {
    PlanType type = PlanType::NoOp;
    std::string ack_text;      ///< Acknowledgment text (for SpeakAckThenAnswer)
    std::string answer_text;   ///< Answer text (for SpeakAckThenAnswer or Speak)
    std::string fallback_text; ///< Fallback text (for Fallback)
    bool needs_llm = false;    ///< Whether LLM is required
};

/**
 * @brief Router for deciding response strategy
 * 
 * Routes transcripts to either fast-path responses (keyword matching)
 * or LLM-based responses. Supports configurable fast-path rules.
 */
class Router {
public:
    Router();
    ~Router();
    
    /**
     * @brief Decide on response plan based on transcript (with confidence/repair)
     * @param transcript Full transcript (text, confidence, token_count)
     * @param context Optional context (reserved for future use)
     * @param repair_confidence_threshold Below this: return repair plan (0 = disabled)
     * @param repair_phrase Text to speak when returning repair plan (e.g. "Say again, over")
     * @return Plan object describing response strategy
     */
    Plan decide(const Transcript& transcript, const std::string& context = "",
               float repair_confidence_threshold = 0.0f, const std::string& repair_phrase = "");

    /**
     * @brief Decide on response plan based on transcript text only (backward compatibility)
     */
    Plan decide(const std::string& transcript_text, const std::string& context = "");
    
    /**
     * @brief Add fast-path rule for keyword matching
     * @param pattern Keyword pattern to match (case-insensitive)
     * @param response Response text to return if pattern matches
     */
    void add_fast_path_rule(const std::string& pattern, const std::string& response);

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace memo_rf
