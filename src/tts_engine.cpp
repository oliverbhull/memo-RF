#include "tts_engine.h"
#include "logger.h"
#include <fstream>
#include <cmath>
#include <cstring>
#include <map>
#include <cstdlib>
#include <algorithm>
#include <vector>
#include <chrono>
#include <sstream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace memo_rf {

class TTSEngine::Impl {
public:
    Impl(const TTSConfig& config) : config_(config) {
        // For v1, we'll use a simple approach:
        // - Call piper via command line (external process)
        // - Cache common phrases
        // - Generate VOX pre-roll
        
        preroll_samples_ = (config_.vox_preroll_ms * DEFAULT_SAMPLE_RATE) / 1000;
        
        // Pre-load common phrases disabled for now (TTS has espeak-ng dependency issue)
        // These will be generated on-demand when TTS is fixed
        // TODO: Re-enable once espeak-ng is installed
    }
    
    AudioBuffer synth(const std::string& text) {
        // Check cache first
        auto it = cache_.find(text);
        if (it != cache_.end()) {
            return it->second;
        }
        
        // Generate using piper (external process)
        AudioBuffer audio = call_piper(text);
        
        // Apply gain
        for (auto& sample : audio) {
            sample = static_cast<Sample>(std::clamp(
                static_cast<float>(sample) * config_.output_gain,
                -32768.0f, 32767.0f));
        }
        
        // Cache if short enough
        if (text.length() < 50) {
            cache_[text] = audio;
        }
        
        return audio;
    }
    
    AudioBuffer synth_vox(const std::string& text) {
        AudioBuffer audio = synth(text);
        
        // Generate pre-roll (tone burst)
        AudioBuffer preroll = generate_preroll();
        
        // Combine (pre-allocate to avoid reallocation)
        AudioBuffer result;
        result.reserve(preroll.size() + audio.size());
        result.insert(result.end(), preroll.begin(), preroll.end());
        result.insert(result.end(), audio.begin(), audio.end());
        
        return result;
    }
    
    void preload_phrase(const std::string& text) {
        // Try to preload, but don't throw on failure
        AudioBuffer audio = call_piper(text);
        if (!audio.empty() && text.length() < 50) {
            cache_[text] = audio;
        }
    }

private:
    AudioBuffer call_piper(const std::string& text) {
        // For v1: call piper via command line
        // Try to find piper in common locations or use full path
        
        std::string piper_cmd = "piper";
        
        // Try common locations
        std::vector<std::string> possible_paths = {
            "/Users/oliverhull/dev/piper/build/piper",
            "/usr/local/bin/piper",
            "/opt/homebrew/bin/piper"
        };
        
        // Check if piper is in PATH first
        int path_check = system("which piper > /dev/null 2>&1");
        if (path_check != 0) {
            // Not in PATH, try full paths
            bool found = false;
            for (const auto& path : possible_paths) {
                std::ifstream test(path);
                if (test.good()) {
                    piper_cmd = path;
                    found = true;
                    break;
                }
            }
            if (!found) {
                // Don't print error during preload - will fail silently
                return AudioBuffer();
            }
        }
        
        std::string temp_wav = "/tmp/memo_rf_tts_temp_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".wav";
        
        // Use piper's --espeak_data flag (more reliable than environment variable)
        std::string espeak_path = "/opt/homebrew/share/espeak-ng-data";
        
        // Escape text for shell (replace quotes and special chars)
        std::string escaped_text = text;
        // Replace " with \"
        size_t pos = 0;
        while ((pos = escaped_text.find("\"", pos)) != std::string::npos) {
            escaped_text.replace(pos, 1, "\\\"");
            pos += 2;
        }
        // Replace $ with \$ to prevent variable expansion
        pos = 0;
        while ((pos = escaped_text.find("$", pos)) != std::string::npos) {
            escaped_text.replace(pos, 1, "\\$");
            pos += 2;
        }
        // Replace ` with \` to prevent command substitution
        pos = 0;
        while ((pos = escaped_text.find("`", pos)) != std::string::npos) {
            escaped_text.replace(pos, 1, "\\`");
            pos += 2;
        }
        
        // Use echo to pipe text to piper with --espeak_data flag
        std::string cmd = "echo \"" + escaped_text + "\" | " + piper_cmd + 
                        " --model " + config_.voice_path + 
                        " --espeak_data " + espeak_path +
                        " --output_file " + temp_wav;
        
        LOG_TTS("Calling piper: " + piper_cmd);
        LOG_TTS("Text: \"" + text + "\"");
        LOG_TTS("Command: " + cmd);
        
        int ret = system(cmd.c_str());
        std::ostringstream ret_oss;
        ret_oss << "Piper returned: " << ret;
        LOG_TTS(ret_oss.str());
        
        if (ret != 0) {
            std::ostringstream err_oss;
            err_oss << "Piper command failed with code: " << ret;
            LOG_TTS(err_oss.str());
            LOG_TTS("Command was: " + cmd);
            return AudioBuffer();
        }
        
        LOG_TTS("Checking for output file: " + temp_wav);
        
        // Read WAV file
        LOG_TTS("Reading WAV file...");
        AudioBuffer audio = read_wav(temp_wav);
        std::ostringstream read_oss;
        read_oss << "Read " << audio.size() << " samples";
        LOG_TTS(read_oss.str());
        
        // Clean up
        remove(temp_wav.c_str());
        
        if (audio.empty()) {
            LOG_TTS("Failed to generate audio for: \"" + text + "\"");
            LOG_TTS("WAV file may be empty or missing");
        }
        
        return audio;
    }
    
    AudioBuffer read_wav(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            LOG_TTS("Failed to open WAV file: " + path);
            return AudioBuffer();
        }
        
        // Skip WAV header (44 bytes for standard PCM)
        file.seekg(44);
        
        // Read PCM16 samples
        AudioBuffer audio;
        Sample sample;
        while (file.read(reinterpret_cast<char*>(&sample), sizeof(Sample))) {
            audio.push_back(sample);
        }
        
        return audio;
    }
    
    AudioBuffer generate_preroll() {
        AudioBuffer preroll(preroll_samples_);
        
        // Generate tone burst (440 Hz sine wave)
        float freq = 440.0f;
        float sample_rate = static_cast<float>(DEFAULT_SAMPLE_RATE);
        
        for (size_t i = 0; i < preroll_samples_; i++) {
            float t = static_cast<float>(i) / sample_rate;
            float amplitude = 0.3f; // Moderate level to trigger VOX
            float value = amplitude * std::sin(2.0f * M_PI * freq * t);
            preroll[i] = static_cast<Sample>(value * 32767.0f);
        }
        
        return preroll;
    }
    
    TTSConfig config_;
    std::map<std::string, AudioBuffer> cache_;
    size_t preroll_samples_;
};

TTSEngine::TTSEngine(const TTSConfig& config) 
    : pimpl_(std::make_unique<Impl>(config)) {}

TTSEngine::~TTSEngine() = default;

AudioBuffer TTSEngine::synth(const std::string& text) {
    return pimpl_->synth(text);
}

AudioBuffer TTSEngine::synth_vox(const std::string& text) {
    return pimpl_->synth_vox(text);
}

void TTSEngine::preload_phrase(const std::string& text) {
    pimpl_->preload_phrase(text);
}

} // namespace memo_rf
