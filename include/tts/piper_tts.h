#pragma once

/**
 * @file piper_tts.h
 * @brief Piper TTS implementation with optimizations
 *
 * Features:
 * - Path caching (find piper once)
 * - Phrase caching (LRU cache)
 * - Pre-roll generation
 * - Warmup support
 * - Configurable output gain
 */

#include "tts_interface.h"
#include "core/constants.h"
#include <string>
#include <memory>

namespace memo_rf {
namespace tts {

/**
 * @brief Configuration for Piper TTS
 */
struct PiperConfig {
    /// Path to Piper voice model (.onnx file)
    std::string voice_path;

    /// Path to espeak-ng data directory
    std::string espeak_data_path = "/opt/homebrew/share/espeak-ng-data";

    /// Custom piper binary path (empty = auto-detect)
    std::string piper_path;

    /// VOX pre-roll duration (ms)
    int preroll_ms = constants::tts::VOX_PREROLL_MS;

    /// Pre-roll tone frequency (Hz)
    float preroll_freq = constants::tts::PREROLL_FREQ_HZ;

    /// Pre-roll amplitude (0-1)
    float preroll_amplitude = constants::tts::PREROLL_AMPLITUDE;

    /// Output gain multiplier
    float output_gain = 1.0f;

    /// Maximum phrases to cache
    size_t max_cache_entries = constants::tts::MAX_CACHE_ENTRIES;

    /// Maximum text length to cache
    size_t max_cache_text_length = constants::tts::MAX_CACHE_TEXT_LENGTH;

    /// Phrases to preload on warmup
    std::vector<std::string> preload_phrases = {
        "roger.",
        "affirmative.",
        "negative.",
        "stand by.",
        "copy.",
        "over."
    };
};

/**
 * @brief Piper TTS implementation
 */
class PiperTTS : public ITTS {
public:
    explicit PiperTTS(const PiperConfig& config);
    ~PiperTTS() override;

    // Non-copyable
    PiperTTS(const PiperTTS&) = delete;
    PiperTTS& operator=(const PiperTTS&) = delete;

    // ITTS interface
    SynthResult synth(const std::string& text) override;
    SynthResult synth_with_preroll(const std::string& text) override;
    void preload(const std::string& text) override;
    void preload_batch(const std::vector<std::string>& phrases) override;
    void clear_cache() override;
    bool is_ready() const override;
    Stats get_stats() const override;
    VoidResult warmup() override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace tts
} // namespace memo_rf
