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
        
        // Read WAV header to get sample rate
        char header[44];
        file.read(header, 44);
        if (file.gcount() < 44) {
            LOG_TTS("WAV file too short or invalid header");
            return AudioBuffer();
        }
        
        // Extract sample rate from WAV header (offset 24-27, little-endian)
        int wav_sample_rate = static_cast<unsigned char>(header[24]) |
                              (static_cast<unsigned char>(header[25]) << 8) |
                              (static_cast<unsigned char>(header[26]) << 16) |
                              (static_cast<unsigned char>(header[27]) << 24);
        
        // Extract number of channels (offset 22-23, little-endian)
        int channels = static_cast<unsigned char>(header[22]) |
                       (static_cast<unsigned char>(header[23]) << 8);
        
        // Extract data size (offset 40-43, little-endian)
        int data_size = static_cast<unsigned char>(header[40]) |
                        (static_cast<unsigned char>(header[41]) << 8) |
                        (static_cast<unsigned char>(header[42]) << 16) |
                        (static_cast<unsigned char>(header[43]) << 24);
        
        std::ostringstream info_oss;
        info_oss << "WAV file: sample_rate=" << wav_sample_rate 
                 << "Hz, channels=" << channels << ", data_size=" << data_size;
        LOG_TTS(info_oss.str());
        
        // Read PCM16 samples
        AudioBuffer raw_audio;
        Sample sample;
        int samples_to_read = data_size / sizeof(Sample);
        raw_audio.reserve(samples_to_read);
        
        while (file.read(reinterpret_cast<char*>(&sample), sizeof(Sample))) {
            raw_audio.push_back(sample);
        }
        
        // If stereo, convert to mono (take left channel only)
        AudioBuffer mono_audio;
        if (channels == 2) {
            mono_audio.reserve(raw_audio.size() / 2);
            for (size_t i = 0; i < raw_audio.size(); i += 2) {
                mono_audio.push_back(raw_audio[i]);
            }
        } else {
            mono_audio = raw_audio;
        }
        
        // Resample to 16kHz if needed
        if (wav_sample_rate == DEFAULT_SAMPLE_RATE) {
            return mono_audio;
        }
        
        // Resample using linear interpolation
        // Calculate how many output samples we need
        float ratio = static_cast<float>(wav_sample_rate) / static_cast<float>(DEFAULT_SAMPLE_RATE);
        size_t output_samples = static_cast<size_t>(mono_audio.size() / ratio);
        AudioBuffer resampled;
        resampled.reserve(output_samples);
        
        // Generate output samples at target rate
        for (size_t out_idx = 0; out_idx < output_samples; out_idx++) {
            // Calculate position in input buffer
            float input_pos = static_cast<float>(out_idx) * ratio;
            size_t idx0 = static_cast<size_t>(input_pos);
            size_t idx1 = std::min(idx0 + 1, mono_audio.size() - 1);
            
            if (idx0 >= mono_audio.size()) break;
            
            // Linear interpolation
            float t = input_pos - idx0;
            float sample0 = static_cast<float>(mono_audio[idx0]);
            float sample1 = static_cast<float>(mono_audio[idx1]);
            float interpolated = sample0 * (1.0f - t) + sample1 * t;
            
            resampled.push_back(static_cast<Sample>(interpolated));
        }
        
        std::ostringstream resample_oss;
        resample_oss << "Resampled from " << wav_sample_rate << "Hz to " 
                     << DEFAULT_SAMPLE_RATE << "Hz: " << mono_audio.size() 
                     << " -> " << resampled.size() << " samples";
        LOG_TTS(resample_oss.str());
        
        return resampled;
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
