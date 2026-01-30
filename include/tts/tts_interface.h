#pragma once

/**
 * @file tts_interface.h
 * @brief Text-to-Speech interface
 *
 * Defines the abstract interface for TTS implementations.
 * This allows swapping TTS backends (Piper, Coqui, cloud APIs, etc.)
 */

#include "core/types.h"
#include <string>
#include <memory>
#include <vector>

namespace memo_rf {
namespace tts {

/**
 * @brief TTS synthesis result
 */
struct SynthResult {
    AudioBuffer audio;
    int64_t synthesis_ms = 0;
    std::string error;

    bool ok() const { return error.empty() && !audio.empty(); }
};

/**
 * @brief TTS engine statistics
 */
struct Stats {
    size_t cache_size = 0;
    size_t cache_hits = 0;
    size_t cache_misses = 0;
    int64_t avg_synthesis_ms = 0;
    bool engine_ready = false;
};

/**
 * @brief Abstract TTS interface
 */
class ITTS {
public:
    virtual ~ITTS() = default;

    /**
     * @brief Synthesize text to audio
     * @param text Text to synthesize
     * @return Audio buffer or error
     */
    virtual SynthResult synth(const std::string& text) = 0;

    /**
     * @brief Synthesize with VOX pre-roll tone
     * @param text Text to synthesize
     * @return Audio buffer with pre-roll prepended
     */
    virtual SynthResult synth_with_preroll(const std::string& text) = 0;

    /**
     * @brief Pre-load a phrase into cache
     * @param text Text to pre-load
     */
    virtual void preload(const std::string& text) = 0;

    /**
     * @brief Pre-load multiple phrases
     * @param phrases List of phrases to pre-load
     */
    virtual void preload_batch(const std::vector<std::string>& phrases) = 0;

    /**
     * @brief Clear the phrase cache
     */
    virtual void clear_cache() = 0;

    /**
     * @brief Check if engine is ready
     */
    virtual bool is_ready() const = 0;

    /**
     * @brief Get engine statistics
     */
    virtual Stats get_stats() const = 0;

    /**
     * @brief Warm up the engine (load models, etc.)
     *
     * Call during initialization to avoid first-call latency.
     */
    virtual VoidResult warmup() = 0;
};

} // namespace tts
} // namespace memo_rf
