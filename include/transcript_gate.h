#pragma once

#include "common.h"
#include "config.h"
#include "utils.h"

namespace memo_rf {

/**
 * @brief Check if transcript is low-signal (blank, too short, or low confidence).
 * Used to block router, clarifier, and memory when transcript should not drive LLM.
 * @param t Transcript from STT
 * @param gate Gate thresholds
 * @param blank_sentinel String to treat as blank (e.g. "[BLANK_AUDIO]")
 * @return True if transcript fails the gate (do not call router/LLM/memory)
 */
inline bool is_low_signal_transcript(const Transcript& t,
                                    const TranscriptGateConfig& gate,
                                    const std::string& blank_sentinel = "[BLANK_AUDIO]") {
    if (utils::is_blank_transcript(t.text, blank_sentinel))
        return true;
    if (gate.min_transcript_tokens > 0 && t.token_count < gate.min_transcript_tokens)
        return true;
    if (gate.min_confidence > 0.0f && t.confidence < gate.min_confidence)
        return true;
    std::string trimmed = utils::trim_copy(t.text);
    if (gate.min_transcript_chars > 0 && static_cast<int>(trimmed.size()) < gate.min_transcript_chars)
        return true;
    return false;
}

} // namespace memo_rf
