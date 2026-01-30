/**
 * @file piper_tts.cpp
 * @brief Piper TTS implementation
 */

#include "tts/piper_tts.h"
#include "logger.h"
#include <fstream>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <chrono>
#include <sstream>
#include <unordered_map>
#include <list>
#include <mutex>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace memo_rf {
namespace tts {

/**
 * @brief LRU Cache for synthesized phrases
 */
template<typename K, typename V>
class LRUCache {
public:
    explicit LRUCache(size_t capacity) : capacity_(capacity) {}

    std::optional<V> get(const K& key) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = map_.find(key);
        if (it == map_.end()) {
            return std::nullopt;
        }

        // Move to front (most recently used)
        list_.splice(list_.begin(), list_, it->second);
        return it->second->second;
    }

    void put(const K& key, const V& value) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = map_.find(key);
        if (it != map_.end()) {
            // Update existing
            it->second->second = value;
            list_.splice(list_.begin(), list_, it->second);
            return;
        }

        // Evict if at capacity
        if (list_.size() >= capacity_) {
            auto last = list_.back();
            map_.erase(last.first);
            list_.pop_back();
        }

        // Insert new
        list_.emplace_front(key, value);
        map_[key] = list_.begin();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return list_.size();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        list_.clear();
        map_.clear();
    }

private:
    size_t capacity_;
    std::list<std::pair<K, V>> list_;
    std::unordered_map<K, typename std::list<std::pair<K, V>>::iterator> map_;
    mutable std::mutex mutex_;
};

/**
 * @brief Implementation details for PiperTTS
 */
class PiperTTS::Impl {
public:
    explicit Impl(const PiperConfig& config)
        : config_(config)
        , cache_(config.max_cache_entries)
        , piper_path_cached_(false)
        , ready_(false)
        , cache_hits_(0)
        , cache_misses_(0)
        , total_synthesis_ms_(0)
        , synthesis_count_(0)
    {
        // Pre-calculate preroll samples
        preroll_samples_ = audio::ms_to_samples(config.preroll_ms);

        LOG_TTS("PiperTTS initialized:");
        std::ostringstream oss;
        oss << "  voice=" << config.voice_path
            << ", preroll=" << config.preroll_ms << "ms"
            << ", gain=" << config.output_gain
            << ", cache_max=" << config.max_cache_entries;
        LOG_TTS(oss.str());
    }

    VoidResult warmup() {
        LOG_TTS("Warming up TTS engine...");

        // Find piper binary
        auto result = find_piper();
        if (result.failed()) {
            return result;
        }

        // Verify voice model exists
        std::ifstream voice_file(config_.voice_path);
        if (!voice_file.good()) {
            return VoidResult::failure("Voice model not found: " + config_.voice_path);
        }

        // Preload common phrases
        LOG_TTS("Preloading " + std::to_string(config_.preload_phrases.size()) + " phrases...");
        for (const auto& phrase : config_.preload_phrases) {
            auto synth_result = synthesize_uncached(phrase);
            if (synth_result.ok()) {
                cache_.put(phrase, synth_result.audio);
                LOG_TTS("  Preloaded: \"" + phrase + "\"");
            } else {
                LOG_TTS("  Failed to preload: \"" + phrase + "\" - " + synth_result.error);
            }
        }

        ready_ = true;
        LOG_TTS("TTS warmup complete");
        return VoidResult::ok_result();
    }

    SynthResult synth(const std::string& text) {
        // Check cache first
        auto cached = cache_.get(text);
        if (cached) {
            cache_hits_++;
            SynthResult result;
            result.audio = *cached;
            result.synthesis_ms = 0;  // Instant from cache
            return result;
        }

        cache_misses_++;

        // Synthesize
        auto result = synthesize_uncached(text);

        // Cache if successful and short enough
        if (result.ok() && text.length() <= config_.max_cache_text_length) {
            cache_.put(text, result.audio);
        }

        return result;
    }

    SynthResult synth_with_preroll(const std::string& text) {
        SynthResult result = synth(text);

        if (!result.ok()) {
            return result;
        }

        // Generate preroll
        AudioBuffer preroll = generate_preroll();

        // Combine preroll + audio
        AudioBuffer combined;
        combined.reserve(preroll.size() + result.audio.size());
        combined.insert(combined.end(), preroll.begin(), preroll.end());
        combined.insert(combined.end(), result.audio.begin(), result.audio.end());

        result.audio = std::move(combined);
        return result;
    }

    void preload(const std::string& text) {
        auto result = synth(text);  // Will cache automatically
        if (!result.ok()) {
            LOG_TTS("Failed to preload: \"" + text + "\" - " + result.error);
        }
    }

    void preload_batch(const std::vector<std::string>& phrases) {
        for (const auto& phrase : phrases) {
            preload(phrase);
        }
    }

    void clear_cache() {
        cache_.clear();
        cache_hits_ = 0;
        cache_misses_ = 0;
    }

    bool is_ready() const {
        return ready_;
    }

    Stats get_stats() const {
        Stats stats;
        stats.cache_size = cache_.size();
        stats.cache_hits = cache_hits_;
        stats.cache_misses = cache_misses_;
        stats.avg_synthesis_ms = synthesis_count_ > 0
            ? static_cast<int64_t>(total_synthesis_ms_ / synthesis_count_)
            : 0;
        stats.engine_ready = ready_;
        return stats;
    }

private:
    // =========================================================================
    // Piper Interaction
    // =========================================================================

    VoidResult find_piper() {
        if (piper_path_cached_) {
            return VoidResult::ok_result();
        }

        // If custom path specified, use it
        if (!config_.piper_path.empty()) {
            std::ifstream test(config_.piper_path);
            if (test.good()) {
                cached_piper_path_ = config_.piper_path;
                piper_path_cached_ = true;
                LOG_TTS("Using custom piper path: " + cached_piper_path_);
                return VoidResult::ok_result();
            }
            return VoidResult::failure("Custom piper path not found: " + config_.piper_path);
        }

        // Try common locations
        std::vector<std::string> search_paths = {
            "/Users/oliverhull/dev/piper/build/piper",
            "/usr/local/bin/piper",
            "/opt/homebrew/bin/piper",
            "/usr/bin/piper"
        };

        for (const auto& path : search_paths) {
            std::ifstream test(path);
            if (test.good()) {
                cached_piper_path_ = path;
                piper_path_cached_ = true;
                LOG_TTS("Found piper at: " + cached_piper_path_);
                return VoidResult::ok_result();
            }
        }

        // Try PATH as last resort (but cache the result)
        int ret = system("which piper > /tmp/memo_rf_piper_path 2>/dev/null");
        if (ret == 0) {
            std::ifstream path_file("/tmp/memo_rf_piper_path");
            if (path_file.good()) {
                std::getline(path_file, cached_piper_path_);
                // Trim whitespace
                while (!cached_piper_path_.empty() && std::isspace(cached_piper_path_.back())) {
                    cached_piper_path_.pop_back();
                }
                if (!cached_piper_path_.empty()) {
                    piper_path_cached_ = true;
                    LOG_TTS("Found piper in PATH: " + cached_piper_path_);
                    return VoidResult::ok_result();
                }
            }
        }

        return VoidResult::failure("Piper binary not found. Install piper or set piper_path in config.");
    }

    SynthResult synthesize_uncached(const std::string& text) {
        SynthResult result;

        // Ensure piper path is cached
        auto find_result = find_piper();
        if (find_result.failed()) {
            result.error = find_result.error;
            return result;
        }

        auto start_time = Clock::now();

        // Generate unique temp file name
        std::string temp_wav = "/tmp/memo_rf_tts_" +
            std::to_string(Clock::now().time_since_epoch().count()) + ".wav";

        // Escape text for shell
        std::string escaped_text = escape_for_shell(text);

        // Build command
        std::ostringstream cmd;
        cmd << "echo \"" << escaped_text << "\" | "
            << cached_piper_path_
            << " --model " << config_.voice_path
            << " --espeak_data " << config_.espeak_data_path
            << " --output_file " << temp_wav
            << " 2>/dev/null";  // Suppress stderr

        LOG_TTS("Synthesizing: \"" + text + "\"");

        int ret = system(cmd.str().c_str());

        if (ret != 0) {
            result.error = "Piper command failed with code: " + std::to_string(ret);
            LOG_TTS(result.error);
            return result;
        }

        // Read WAV file
        result.audio = read_wav(temp_wav);

        // Clean up temp file
        remove(temp_wav.c_str());

        if (result.audio.empty()) {
            result.error = "Failed to read synthesized audio";
            return result;
        }

        // Apply gain
        apply_gain(result.audio);

        // Record timing
        result.synthesis_ms = ms_since(start_time);
        total_synthesis_ms_ += result.synthesis_ms;
        synthesis_count_++;

        std::ostringstream timing_oss;
        timing_oss << "Synthesized " << result.audio.size() << " samples in "
                   << result.synthesis_ms << "ms";
        LOG_TTS(timing_oss.str());

        return result;
    }

    std::string escape_for_shell(const std::string& text) const {
        std::string escaped = text;

        // Escape characters that could cause shell issues
        const std::vector<std::pair<std::string, std::string>> replacements = {
            {"\"", "\\\""},
            {"$", "\\$"},
            {"`", "\\`"},
            {"\\", "\\\\"}
        };

        for (const auto& [from, to] : replacements) {
            size_t pos = 0;
            while ((pos = escaped.find(from, pos)) != std::string::npos) {
                escaped.replace(pos, from.length(), to);
                pos += to.length();
            }
        }

        return escaped;
    }

    // =========================================================================
    // Audio Processing
    // =========================================================================

    AudioBuffer read_wav(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            LOG_TTS("Failed to open WAV file: " + path);
            return {};
        }

        // Read WAV header
        char header[44];
        file.read(header, 44);
        if (file.gcount() < 44) {
            LOG_TTS("WAV file too short or invalid header");
            return {};
        }

        // Extract format info from header (little-endian)
        auto read_u16 = [&](int offset) -> uint16_t {
            return static_cast<uint8_t>(header[offset]) |
                   (static_cast<uint8_t>(header[offset + 1]) << 8);
        };
        auto read_u32 = [&](int offset) -> uint32_t {
            return static_cast<uint8_t>(header[offset]) |
                   (static_cast<uint8_t>(header[offset + 1]) << 8) |
                   (static_cast<uint8_t>(header[offset + 2]) << 16) |
                   (static_cast<uint8_t>(header[offset + 3]) << 24);
        };

        int wav_sample_rate = read_u32(24);
        int channels = read_u16(22);
        int data_size = read_u32(40);

        std::ostringstream info;
        info << "WAV: " << wav_sample_rate << "Hz, " << channels << "ch, "
             << data_size << " bytes";
        LOG_TTS(info.str());

        // Read PCM samples
        AudioBuffer raw_audio;
        raw_audio.reserve(data_size / sizeof(Sample));

        Sample sample;
        while (file.read(reinterpret_cast<char*>(&sample), sizeof(Sample))) {
            raw_audio.push_back(sample);
        }

        // Convert stereo to mono if needed
        AudioBuffer mono_audio;
        if (channels == 2) {
            mono_audio.reserve(raw_audio.size() / 2);
            for (size_t i = 0; i < raw_audio.size(); i += 2) {
                mono_audio.push_back(raw_audio[i]);
            }
        } else {
            mono_audio = std::move(raw_audio);
        }

        // Resample if needed
        if (wav_sample_rate != audio::SAMPLE_RATE) {
            mono_audio = resample(mono_audio, wav_sample_rate, audio::SAMPLE_RATE);
        }

        return mono_audio;
    }

    AudioBuffer resample(const AudioBuffer& input, int from_rate, int to_rate) {
        if (from_rate == to_rate) return input;

        float ratio = static_cast<float>(from_rate) / static_cast<float>(to_rate);
        size_t output_samples = static_cast<size_t>(input.size() / ratio);

        AudioBuffer output;
        output.reserve(output_samples);

        for (size_t i = 0; i < output_samples; i++) {
            float input_pos = static_cast<float>(i) * ratio;
            size_t idx0 = static_cast<size_t>(input_pos);
            size_t idx1 = std::min(idx0 + 1, input.size() - 1);

            if (idx0 >= input.size()) break;

            // Linear interpolation
            float t = input_pos - idx0;
            float sample0 = static_cast<float>(input[idx0]);
            float sample1 = static_cast<float>(input[idx1]);
            float interpolated = sample0 * (1.0f - t) + sample1 * t;

            output.push_back(static_cast<Sample>(interpolated));
        }

        std::ostringstream oss;
        oss << "Resampled " << from_rate << "Hz -> " << to_rate << "Hz: "
            << input.size() << " -> " << output.size() << " samples";
        LOG_TTS(oss.str());

        return output;
    }

    void apply_gain(AudioBuffer& audio) {
        if (std::abs(config_.output_gain - 1.0f) < 0.001f) return;

        for (auto& sample : audio) {
            float scaled = static_cast<float>(sample) * config_.output_gain;
            sample = static_cast<Sample>(std::clamp(scaled, -32768.0f, 32767.0f));
        }
    }

    AudioBuffer generate_preroll() {
        AudioBuffer preroll(preroll_samples_);

        float sample_rate = static_cast<float>(audio::SAMPLE_RATE);

        for (size_t i = 0; i < preroll_samples_; i++) {
            float t = static_cast<float>(i) / sample_rate;
            float value = config_.preroll_amplitude *
                         std::sin(2.0f * static_cast<float>(M_PI) * config_.preroll_freq * t);
            preroll[i] = static_cast<Sample>(value * 32767.0f);
        }

        return preroll;
    }

    // =========================================================================
    // Member Variables
    // =========================================================================

    PiperConfig config_;
    LRUCache<std::string, AudioBuffer> cache_;

    // Cached piper path
    std::string cached_piper_path_;
    bool piper_path_cached_;
    bool ready_;

    // Preroll
    size_t preroll_samples_;

    // Statistics
    size_t cache_hits_;
    size_t cache_misses_;
    int64_t total_synthesis_ms_;
    size_t synthesis_count_;
};

// =============================================================================
// Public Interface Implementation
// =============================================================================

PiperTTS::PiperTTS(const PiperConfig& config)
    : impl_(std::make_unique<Impl>(config)) {}

PiperTTS::~PiperTTS() = default;

SynthResult PiperTTS::synth(const std::string& text) {
    return impl_->synth(text);
}

SynthResult PiperTTS::synth_with_preroll(const std::string& text) {
    return impl_->synth_with_preroll(text);
}

void PiperTTS::preload(const std::string& text) {
    impl_->preload(text);
}

void PiperTTS::preload_batch(const std::vector<std::string>& phrases) {
    impl_->preload_batch(phrases);
}

void PiperTTS::clear_cache() {
    impl_->clear_cache();
}

bool PiperTTS::is_ready() const {
    return impl_->is_ready();
}

Stats PiperTTS::get_stats() const {
    return impl_->get_stats();
}

VoidResult PiperTTS::warmup() {
    return impl_->warmup();
}

} // namespace tts
} // namespace memo_rf
