#pragma once

#include <string>

namespace memo_rf {

/**
 * @brief State for the Twitter draft-tweet flow (regex-driven, no LLM autonomy).
 */
enum class TwitterFlowState {
    Idle,         ///< Not in Twitter flow
    DraftOpened,  ///< Safari at x.com, new tweet opened; waiting for tweet content
    Confirming    ///< Tweet content captured; waiting for yes/yeah/yup to post
};

/**
 * @brief Result of Twitter flow handling.
 */
struct TwitterFlowResult {
    bool handled = false;       ///< True if this turn was consumed by the Twitter flow
    std::string response_text;  ///< Scripted phrase to speak (no LLM); empty if not handled
};

/**
 * @brief Handle one user utterance in the Twitter flow.
 *
 * Runs only when persona is "twitter". Uses regex/keyword matching and state;
 * no tool calling, no model autonomy. If handled, performs AppleScript side
 * effects (open Safari, clipboard, Cmd+Return) and returns scripted response.
 *
 * @param transcript User's transcribed utterance
 * @param state Current Twitter flow state (updated in place when handled)
 * @return Result with handled flag and response text to speak
 */
TwitterFlowResult twitter_flow_handle(const std::string& transcript,
                                      TwitterFlowState& state);

} // namespace memo_rf
