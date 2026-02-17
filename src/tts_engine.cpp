#include "tts_engine.h"
#include "logger.h"
#include "path_utils.h"
#include <cstdio>
#include <fstream>
#include <cmath>
#include <cstring>
#include <map>
#include <cstdlib>
#include <algorithm>
#include <vector>
#include <chrono>
#include <sstream>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <mutex>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace memo_rf {

class TTSEngine::Impl {
public:
    Impl(const TTSConfig& config)
        : config_(config)
        , piper_pid_(-1)
        , pipe_to_piper_{-1, -1}
        , pipe_from_piper_{-1, -1}
        , piper_initialized_(false)
    {
        preroll_samples_ = (config_.vox_preroll_ms * DEFAULT_SAMPLE_RATE) / 1000;
        end_tone_samples_ = (config_.vox_end_tone_ms > 0)
            ? (config_.vox_end_tone_ms * DEFAULT_SAMPLE_RATE) / 1000
            : 0;

        // Find piper binary path once
        find_piper_path();

        // Start persistent piper process
        if (!piper_path_.empty()) {
            start_piper_process();
        }
    }

    ~Impl() {
        stop_piper_process();
    }

    AudioBuffer synth(const std::string& text) {
        std::lock_guard<std::mutex> lock(synth_mutex_);

        // Check cache first
        auto it = cache_.find(text);
        if (it != cache_.end()) {
            LOG_TTS("Cache hit for: \"" + text + "\"");
            return it->second;
        }

        // Generate using persistent piper process
        AudioBuffer audio;
        if (piper_initialized_) {
            audio = synth_via_persistent_piper(text);
        } else {
            // Fallback to spawning process (slower)
            audio = synth_via_system_call(text);
        }

        // Apply gain
        for (auto& sample : audio) {
            sample = static_cast<Sample>(std::clamp(
                static_cast<float>(sample) * config_.output_gain,
                -32768.0f, 32767.0f));
        }

        // Cache if short enough
        if (!audio.empty() && text.length() < 50) {
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

    AudioBuffer get_preroll_buffer() {
        return generate_preroll();
    }

    AudioBuffer get_end_tone_buffer() {
        if (end_tone_samples_ == 0) return AudioBuffer();
        return generate_end_tone();
    }

    void preload_phrase(const std::string& text) {
        // Try to preload, but don't throw on failure
        AudioBuffer audio = synth(text);
        if (!audio.empty() && text.length() < 50) {
            cache_[text] = audio;
        }
    }

private:
    void find_piper_path() {
        // Config first: if piper_path is set and exists, use it
        if (!config_.piper_path.empty()) {
            std::ifstream test(config_.piper_path);
            if (test.good()) {
                piper_path_ = config_.piper_path;
                LOG_TTS("Using config piper path: " + piper_path_);
                return;
            }
        }

        // Platform-agnostic search list ($HOME/bin, $HOME/.local/bin, /usr/local/bin, /usr/bin; macOS: /opt/homebrew/bin)
        std::vector<std::string> possible_paths;
        const char* home = std::getenv("HOME");
        if (home) {
            possible_paths.push_back(std::string(home) + "/bin/piper");
            possible_paths.push_back(std::string(home) + "/.local/bin/piper");
        }
        possible_paths.push_back("/usr/local/bin/piper");
        possible_paths.push_back("/usr/bin/piper");
#ifdef __APPLE__
        possible_paths.push_back("/opt/homebrew/bin/piper");
#endif

        for (const auto& path : possible_paths) {
            std::ifstream test(path);
            if (test.good()) {
                piper_path_ = path;
                LOG_TTS("Found piper at: " + piper_path_);
                return;
            }
        }

        // Try PATH
        FILE* fp = popen("which piper 2>/dev/null", "r");
        if (fp) {
            char buffer[256];
            if (fgets(buffer, sizeof(buffer), fp)) {
                piper_path_ = buffer;
                // Remove trailing newline
                while (!piper_path_.empty() && (piper_path_.back() == '\n' || piper_path_.back() == '\r')) {
                    piper_path_.pop_back();
                }
                LOG_TTS("Found piper in PATH: " + piper_path_);
            }
            pclose(fp);
        }

        if (piper_path_.empty()) {
            LOG_TTS("WARNING: Piper not found!");
        }
    }

    void start_piper_process() {
        LOG_TTS("Starting persistent piper process...");

        // Create pipes for communication
        if (pipe(pipe_to_piper_) == -1 || pipe(pipe_from_piper_) == -1) {
            LOG_TTS("Failed to create pipes for piper");
            return;
        }

        piper_pid_ = fork();

        if (piper_pid_ == -1) {
            LOG_TTS("Failed to fork piper process");
            close(pipe_to_piper_[0]);
            close(pipe_to_piper_[1]);
            close(pipe_from_piper_[0]);
            close(pipe_from_piper_[1]);
            return;
        }

        if (piper_pid_ == 0) {
            // Child process - exec piper

            // Redirect stdin from pipe
            dup2(pipe_to_piper_[0], STDIN_FILENO);
            close(pipe_to_piper_[0]);
            close(pipe_to_piper_[1]);

            // Redirect stdout to pipe
            dup2(pipe_from_piper_[1], STDOUT_FILENO);
            close(pipe_from_piper_[0]);
            close(pipe_from_piper_[1]);

            // Redirect stderr to /dev/null (or keep for debugging)
            int devnull = open("/dev/null", O_WRONLY);
            if (devnull >= 0) {
                dup2(devnull, STDERR_FILENO);
                close(devnull);
            }

            // Execute piper with --json-input for persistent mode
            // Output raw audio to stdout
            std::string espeak = config_.espeak_data_path.empty() ? default_espeak_data_path() : config_.espeak_data_path;
            execl(piper_path_.c_str(), "piper",
                  "--model", config_.voice_path.c_str(),
                  "--espeak_data", espeak.c_str(),
                  "--json-input",
                  "--output_raw",
                  "--quiet",
                  nullptr);

            // If exec fails
            _exit(1);
        }

        // Parent process
        close(pipe_to_piper_[0]);   // Close read end of stdin pipe
        close(pipe_from_piper_[1]); // Close write end of stdout pipe

        // Set stdout pipe to non-blocking
        int flags = fcntl(pipe_from_piper_[0], F_GETFL, 0);
        fcntl(pipe_from_piper_[0], F_SETFL, flags | O_NONBLOCK);

        // Give piper time to load model (Jetson/slow devices may need 2s+)
        usleep(2000000);  // 2s

        // Check if process is still running
        int status = 0;
        pid_t result = waitpid(piper_pid_, &status, WNOHANG);
        if (result == 0) {
            // Process is still running
            piper_initialized_ = true;
            LOG_TTS("Persistent piper process started (PID: " + std::to_string(piper_pid_) + ")");
        } else {
            std::ostringstream oss;
            oss << "Piper process failed to start: ";
            if (WIFEXITED(status))
                oss << "exit " << WEXITSTATUS(status);
            else if (WIFSIGNALED(status))
                oss << "signal " << WTERMSIG(status);
            else
                oss << "status " << status;
            LOG_TTS(oss.str());
            piper_pid_ = -1;
        }
    }

    void stop_piper_process() {
        if (piper_pid_ > 0) {
            LOG_TTS("Stopping persistent piper process...");

            // Close pipes
            if (pipe_to_piper_[1] >= 0) {
                close(pipe_to_piper_[1]);
                pipe_to_piper_[1] = -1;
            }
            if (pipe_from_piper_[0] >= 0) {
                close(pipe_from_piper_[0]);
                pipe_from_piper_[0] = -1;
            }

            // Send SIGTERM
            kill(piper_pid_, SIGTERM);

            // Wait for process to exit
            int status;
            waitpid(piper_pid_, &status, 0);

            piper_pid_ = -1;
            piper_initialized_ = false;
        }
    }

    AudioBuffer synth_via_persistent_piper(const std::string& text) {
        auto start_time = std::chrono::steady_clock::now();

        // Create JSON input for piper
        // Format: {"text": "hello world"}
        std::string json_input = "{\"text\": \"" + escape_json(text) + "\"}\n";

        LOG_TTS("Sending to piper: " + json_input);

        // Write to piper stdin
        ssize_t written = write(pipe_to_piper_[1], json_input.c_str(), json_input.size());
        if (written != static_cast<ssize_t>(json_input.size())) {
            LOG_TTS("Failed to write to piper stdin");
            // Try to restart piper
            stop_piper_process();
            start_piper_process();
            return synth_via_system_call(text);  // Fallback
        }

        // Read raw audio from piper stdout
        // Piper outputs 16-bit PCM at 22050Hz
        AudioBuffer raw_audio;
        raw_audio.reserve(22050 * 10);  // Reserve space for up to 10 seconds

        const int piper_sample_rate = 22050;
        char buffer[8192];  // Larger buffer
        int consecutive_empty = 0;
        const int max_empty_after_data = 30;   // 30 * 10ms = 300ms of no data after receiving some
        const int max_empty_initial = 300;     // 300 * 10ms = 3s timeout waiting for first data
        bool received_any_data = false;

        // Read audio data
        while (true) {
            ssize_t bytes_read = read(pipe_from_piper_[0], buffer, sizeof(buffer));

            if (bytes_read > 0) {
                received_any_data = true;
                consecutive_empty = 0;
                // Convert bytes to samples
                for (ssize_t i = 0; i + 1 < bytes_read; i += 2) {
                    Sample sample = static_cast<Sample>(
                        static_cast<uint8_t>(buffer[i]) |
                        (static_cast<int8_t>(buffer[i + 1]) << 8)
                    );
                    raw_audio.push_back(sample);
                }
            } else if (bytes_read == 0) {
                // EOF - piper closed stdout (shouldn't happen)
                LOG_TTS("Piper closed stdout unexpectedly");
                break;
            } else {
                // EAGAIN/EWOULDBLOCK - no data available yet
                consecutive_empty++;

                // Different timeouts depending on whether we've received any data
                int max_empty = received_any_data ? max_empty_after_data : max_empty_initial;

                if (consecutive_empty >= max_empty) {
                    if (!received_any_data) {
                        LOG_TTS("Timeout waiting for piper data");
                    }
                    break;  // Done (either timeout or end of audio)
                }

                usleep(10000);  // Wait 10ms
            }
        }

        if (raw_audio.empty()) {
            LOG_TTS("No audio received from piper, falling back to system call");
            return synth_via_system_call(text);
        }

        auto synth_time = std::chrono::steady_clock::now();
        auto synth_ms = std::chrono::duration_cast<std::chrono::milliseconds>(synth_time - start_time).count();

        // Resample from 22050Hz to 16000Hz
        AudioBuffer resampled = resample(raw_audio, piper_sample_rate, DEFAULT_SAMPLE_RATE);

        auto total_time = std::chrono::steady_clock::now();
        auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(total_time - start_time).count();

        std::ostringstream oss;
        oss << "Synthesized " << resampled.size() << " samples in " << total_ms << "ms "
            << "(synth=" << synth_ms << "ms)";
        LOG_TTS(oss.str());

        return resampled;
    }

    AudioBuffer synth_via_system_call(const std::string& text) {
        // Fallback: fork/exec piper (no shell) to avoid shell escaping and child abort issues
        LOG_TTS("Using fallback fork/exec for TTS");

        std::string temp_wav = "/tmp/memo_rf_tts_temp_" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".wav";

        std::string json_input = "{\"text\": \"" + escape_json(text) + "\"}\n";
        std::string espeak = config_.espeak_data_path.empty() ? default_espeak_data_path() : config_.espeak_data_path;

        int pipe_fd[2];
        int stderr_pipe[2];
        if (pipe(pipe_fd) == -1 || pipe(stderr_pipe) == -1) {
            LOG_TTS("Failed to create pipe for piper fallback");
            if (pipe_fd[0] >= 0) { close(pipe_fd[0]); close(pipe_fd[1]); }
            if (stderr_pipe[0] >= 0) { close(stderr_pipe[0]); close(stderr_pipe[1]); }
            return AudioBuffer();
        }

        pid_t pid = fork();
        if (pid == -1) {
            close(pipe_fd[0]);
            close(pipe_fd[1]);
            close(stderr_pipe[0]);
            close(stderr_pipe[1]);
            LOG_TTS("Failed to fork for piper fallback");
            return AudioBuffer();
        }

        if (pid == 0) {
            // Child: redirect stdin from pipe, stderr to pipe so parent can log it
            dup2(pipe_fd[0], STDIN_FILENO);
            close(pipe_fd[0]);
            close(pipe_fd[1]);
            close(stderr_pipe[0]);
            dup2(stderr_pipe[1], STDERR_FILENO);
            close(stderr_pipe[1]);
            execl(piper_path_.c_str(), "piper",
                  "--model", config_.voice_path.c_str(),
                  "--espeak_data", espeak.c_str(),
                  "--json-input",
                  "--output_file", temp_wav.c_str(),
                  "--quiet",
                  nullptr);
            _exit(127);
        }

        // Parent: close child's stderr write end so we get EOF when child exits
        close(stderr_pipe[1]);
        close(pipe_fd[0]);
        ssize_t written = write(pipe_fd[1], json_input.c_str(), json_input.size());
        close(pipe_fd[1]);
        if (written != static_cast<ssize_t>(json_input.size())) {
            waitpid(pid, nullptr, 0);
            close(stderr_pipe[0]);
            LOG_TTS("Failed to write JSON to piper fallback");
            return AudioBuffer();
        }

        int status = 0;
        pid_t w = waitpid(pid, &status, 0);
        // Read any stderr from Piper for diagnostics
        std::string piper_stderr;
        char buf[256];
        for (;;) {
            ssize_t n = read(stderr_pipe[0], buf, sizeof(buf) - 1);
            if (n <= 0) break;
            buf[n] = '\0';
            piper_stderr += buf;
        }
        close(stderr_pipe[0]);
        while (!piper_stderr.empty() && (piper_stderr.back() == '\n' || piper_stderr.back() == '\r')) {
            piper_stderr.pop_back();
        }

        if (w != pid || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            std::ostringstream oss;
            oss << "Piper fallback failed: ";
            if (WIFEXITED(status))
                oss << "exit " << WEXITSTATUS(status);
            else if (WIFSIGNALED(status))
                oss << "signal " << WTERMSIG(status);
            else
                oss << "status " << status;
            if (!piper_stderr.empty())
                oss << " stderr=\"" << piper_stderr << "\"";
            LOG_TTS(oss.str());
            remove(temp_wav.c_str());
            return AudioBuffer();
        }

        AudioBuffer audio = read_wav(temp_wav);
        remove(temp_wav.c_str());

        return audio;
    }

    std::string escape_json(const std::string& text) {
        std::string result;
        result.reserve(text.size() * 2);

        for (char c : text) {
            switch (c) {
                case '"':  result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default:   result += c; break;
            }
        }

        return result;
    }

    std::string escape_shell(const std::string& text) {
        std::string result = text;
        size_t pos = 0;

        // Escape special shell characters
        while ((pos = result.find("\"", pos)) != std::string::npos) {
            result.replace(pos, 1, "\\\"");
            pos += 2;
        }
        pos = 0;
        while ((pos = result.find("$", pos)) != std::string::npos) {
            result.replace(pos, 1, "\\$");
            pos += 2;
        }
        pos = 0;
        while ((pos = result.find("`", pos)) != std::string::npos) {
            result.replace(pos, 1, "\\`");
            pos += 2;
        }

        return result;
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

            float t = input_pos - idx0;
            float sample0 = static_cast<float>(input[idx0]);
            float sample1 = static_cast<float>(input[idx1]);
            float interpolated = sample0 * (1.0f - t) + sample1 * t;

            output.push_back(static_cast<Sample>(interpolated));
        }

        return output;
    }

    AudioBuffer read_wav(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            return AudioBuffer();
        }

        char header[44];
        file.read(header, 44);
        if (file.gcount() < 44) {
            return AudioBuffer();
        }

        int wav_sample_rate = static_cast<unsigned char>(header[24]) |
                              (static_cast<unsigned char>(header[25]) << 8) |
                              (static_cast<unsigned char>(header[26]) << 16) |
                              (static_cast<unsigned char>(header[27]) << 24);

        int channels = static_cast<unsigned char>(header[22]) |
                       (static_cast<unsigned char>(header[23]) << 8);

        AudioBuffer raw_audio;
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
        if (wav_sample_rate != DEFAULT_SAMPLE_RATE) {
            return resample(mono_audio, wav_sample_rate, DEFAULT_SAMPLE_RATE);
        }

        return mono_audio;
    }

    AudioBuffer generate_preroll() {
        AudioBuffer preroll(preroll_samples_);

        const float freq = 440.0f;
        const float sample_rate = static_cast<float>(DEFAULT_SAMPLE_RATE);
        const float amplitude = config_.vox_preroll_amplitude;

        for (size_t i = 0; i < preroll_samples_; i++) {
            float t = static_cast<float>(i) / sample_rate;
            float value = amplitude * std::sin(2.0f * M_PI * freq * t);
            preroll[i] = static_cast<Sample>(std::clamp(value * 32767.0f, -32768.0f, 32767.0f));
        }

        return preroll;
    }

    AudioBuffer generate_end_tone() {
        if (end_tone_samples_ == 0) return AudioBuffer();
        AudioBuffer tone(end_tone_samples_);
        const float freq = config_.vox_end_tone_freq_hz;
        const float sample_rate = static_cast<float>(DEFAULT_SAMPLE_RATE);
        const float amplitude = config_.vox_end_tone_amplitude;
        for (size_t i = 0; i < end_tone_samples_; i++) {
            float t = static_cast<float>(i) / sample_rate;
            float value = amplitude * std::sin(2.0f * static_cast<float>(M_PI) * freq * t);
            tone[i] = static_cast<Sample>(std::clamp(value * 32767.0f, -32768.0f, 32767.0f));
        }
        return tone;
    }

    TTSConfig config_;
    std::string piper_path_;
    std::map<std::string, AudioBuffer> cache_;
    size_t preroll_samples_;
    size_t end_tone_samples_;

    // Persistent piper process
    pid_t piper_pid_;
    int pipe_to_piper_[2];    // [0] = read, [1] = write
    int pipe_from_piper_[2];  // [0] = read, [1] = write
    bool piper_initialized_;
    std::mutex synth_mutex_;
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

AudioBuffer TTSEngine::get_preroll_buffer() {
    return pimpl_->get_preroll_buffer();
}

AudioBuffer TTSEngine::get_end_tone_buffer() {
    return pimpl_->get_end_tone_buffer();
}

void TTSEngine::preload_phrase(const std::string& text) {
    pimpl_->preload_phrase(text);
}

} // namespace memo_rf
