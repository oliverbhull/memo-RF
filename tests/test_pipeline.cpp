/**
 * Deterministic test harness for transcript gate and router pipeline.
 * Asserts:
 * - Silence/blank never passes gate (no LLM path).
 * - Low confidence triggers repair plan from router.
 * - Only valid speech (non-blank, sufficient tokens/confidence) passes gate and reaches LLM path.
 *
 * Run from build dir: ./test_pipeline
 * No whisper/PortAudio required.
 */

#include "common.h"
#include "config.h"
#include "router.h"
#include "transcript_gate.h"
#include "utils.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <string>

using namespace memo_rf;

static int failed = 0;

#define ASSERT(cond) do { if (!(cond)) { std::cerr << "FAIL: " << #cond << " (line " << __LINE__ << ")\n"; failed++; } } while(0)

int main() {
    // --- is_blank_transcript ---
    ASSERT(utils::is_blank_transcript("", "[BLANK_AUDIO]"));
    ASSERT(utils::is_blank_transcript("   ", "[BLANK_AUDIO]"));
    ASSERT(utils::is_blank_transcript("\t\n", "[BLANK_AUDIO]"));
    ASSERT(utils::is_blank_transcript("[BLANK_AUDIO]", "[BLANK_AUDIO]"));
    ASSERT(utils::is_blank_transcript("  [BLANK_AUDIO]  ", "[BLANK_AUDIO]")); // trim gives "[BLANK_AUDIO]" equals sentinel
    ASSERT(utils::is_blank_transcript("  [BLANK_AUDIO]", "[BLANK_AUDIO]"));   // trim gives "[BLANK_AUDIO]" equals sentinel
    ASSERT(utils::is_blank_transcript("hello", "[BLANK_AUDIO]") == false);
    ASSERT(utils::is_blank_transcript("go", "[BLANK_AUDIO]") == false);

    // --- is_low_signal_transcript (gate) ---
    TranscriptGateConfig gate_strict;
    gate_strict.min_transcript_chars = 2;
    gate_strict.min_transcript_tokens = 1;
    gate_strict.min_confidence = 0.3f;

    Transcript t_blank;
    t_blank.text = "";
    t_blank.confidence = 0.9f;
    t_blank.token_count = 0;
    ASSERT(is_low_signal_transcript(t_blank, gate_strict, "[BLANK_AUDIO]"));

    Transcript t_blank_sentinel;
    t_blank_sentinel.text = "[BLANK_AUDIO]";
    t_blank_sentinel.confidence = 0.5f;
    t_blank_sentinel.token_count = 1;
    ASSERT(is_low_signal_transcript(t_blank_sentinel, gate_strict, "[BLANK_AUDIO]"));

    Transcript t_low_tokens;
    t_low_tokens.text = "go";
    t_low_tokens.confidence = 0.9f;
    t_low_tokens.token_count = 0;
    ASSERT(is_low_signal_transcript(t_low_tokens, gate_strict, "[BLANK_AUDIO]"));

    Transcript t_low_confidence;
    t_low_confidence.text = "my problem";
    t_low_confidence.confidence = 0.1f;
    t_low_confidence.token_count = 5;
    ASSERT(is_low_signal_transcript(t_low_confidence, gate_strict, "[BLANK_AUDIO]"));

    Transcript t_valid;
    t_valid.text = "what is the weather";
    t_valid.confidence = 0.8f;
    t_valid.token_count = 5;
    ASSERT(!is_low_signal_transcript(t_valid, gate_strict, "[BLANK_AUDIO]"));

    // --- Router: low confidence triggers repair plan (no LLM) ---
    Router router;
    float repair_threshold = 0.5f;
    std::string repair_phrase = "Say again, over";

    Transcript t_repair;
    t_repair.text = "something unclear";
    t_repair.confidence = 0.2f;
    t_repair.token_count = 3;
    Plan plan_repair = router.decide(t_repair, "", repair_threshold, repair_phrase);
    ASSERT(plan_repair.type == PlanType::Speak);
    ASSERT(plan_repair.answer_text == repair_phrase);
    ASSERT(!plan_repair.needs_llm);

    // --- Router: valid confidence goes to LLM path ---
    Transcript t_llm;
    t_llm.text = "what is the status";
    t_llm.confidence = 0.9f;
    t_llm.token_count = 4;
    Plan plan_llm = router.decide(t_llm, "", repair_threshold, repair_phrase);
    ASSERT(plan_llm.type == PlanType::SpeakAckThenAnswer);
    ASSERT(plan_llm.needs_llm);

    // --- Gate with defaults: minimal bar (silence/blank still fails via is_blank) ---
    TranscriptGateConfig gate_default;
    gate_default.min_transcript_chars = 1;
    gate_default.min_transcript_tokens = 1;
    gate_default.min_confidence = 0.0f;
    Transcript t_silence;
    t_silence.text = "";
    t_silence.confidence = 0.0f;
    t_silence.token_count = 0;
    ASSERT(is_low_signal_transcript(t_silence, gate_default, "[BLANK_AUDIO]"));

    if (failed) {
        std::cerr << failed << " assertion(s) failed.\n";
        return 1;
    }
    std::cout << "All pipeline tests passed.\n";
    return 0;
}
