#include "audio_io.h"
#include "logger.h"
#include <portaudio.h>
#include <vector>
#include <mutex>
#include <queue>
#include <atomic>
#include <cstring>
#include <sstream>

namespace memo_rf {

class AudioIO::Impl {
public:
    Impl() : input_stream_(nullptr), output_stream_(nullptr),
             sample_rate_(16000), playback_complete_(true), stop_playback_(false) {}
    
    ~Impl() {
        stop();
    }
    
    bool start(const std::string& input_device, const std::string& output_device, int sample_rate) {
        sample_rate_ = sample_rate;
        playback_complete_ = true;
        stop_playback_ = false;
        
        PaError err = Pa_Initialize();
        if (err != paNoError) {
            Logger::error("PortAudio init error: " + std::string(Pa_GetErrorText(err)));
            return false;
        }
        
        // Find input device
        int input_idx = find_device(input_device, true);
        if (input_idx < 0) {
            Logger::error("Input device not found: " + input_device);
            Pa_Terminate();
            return false;
        }
        
        // Verify input device is available
        const PaDeviceInfo* input_info = Pa_GetDeviceInfo(input_idx);
        if (!input_info || input_info->maxInputChannels == 0) {
            Logger::error("Input device has no input channels: " + input_device);
            Pa_Terminate();
            return false;
        }
        
        // Find output device
        int output_idx = find_device(output_device, false);
        if (output_idx < 0) {
            Logger::error("Output device not found: " + output_device);
            Pa_Terminate();
            return false;
        }
        
        // Verify output device is available
        const PaDeviceInfo* output_info = Pa_GetDeviceInfo(output_idx);
        if (!output_info || output_info->maxOutputChannels == 0) {
            Logger::error("Output device has no output channels: " + output_device);
            Pa_Terminate();
            return false;
        }
        
        // Log device info for debugging
        std::ostringstream dev_oss;
        dev_oss << "Using input device: [" << input_idx << "] " << input_info->name;
        Logger::info(dev_oss.str());
        std::ostringstream dev_oss2;
        dev_oss2 << "Using output device: [" << output_idx << "] " << output_info->name;
        Logger::info(dev_oss2.str());
        
        // Open input stream
        PaStreamParameters input_params;
        input_params.device = input_idx;
        input_params.channelCount = 1;
        input_params.sampleFormat = paInt16;
        input_params.suggestedLatency = Pa_GetDeviceInfo(input_idx)->defaultLowInputLatency;
        input_params.hostApiSpecificStreamInfo = nullptr;
        
        err = Pa_OpenStream(&input_stream_, &input_params, nullptr, sample_rate_,
                           SAMPLES_PER_FRAME, paClipOff, nullptr, nullptr);
        if (err != paNoError) {
            Logger::error("Failed to open input stream: " + std::string(Pa_GetErrorText(err)));
            Pa_Terminate();
            return false;
        }
        
        // Open output stream
        PaStreamParameters output_params;
        output_params.device = output_idx;
        output_params.channelCount = 1;
        output_params.sampleFormat = paInt16;
        output_params.suggestedLatency = Pa_GetDeviceInfo(output_idx)->defaultLowOutputLatency;
        output_params.hostApiSpecificStreamInfo = nullptr;
        
        err = Pa_OpenStream(&output_stream_, nullptr, &output_params, sample_rate_,
                           SAMPLES_PER_FRAME, paClipOff, audio_callback, this);
        if (err != paNoError) {
            Logger::error("Failed to open output stream: " + std::string(Pa_GetErrorText(err)));
            Pa_CloseStream(input_stream_);
            Pa_Terminate();
            return false;
        }
        
        err = Pa_StartStream(input_stream_);
        if (err != paNoError) {
            std::ostringstream err_oss;
            err_oss << "Failed to start input stream: " << Pa_GetErrorText(err);
            err_oss << " (Error code: " << err << ")";
            Logger::error(err_oss.str());
            
            // Provide helpful macOS-specific guidance
            if (err == paUnanticipatedHostError) {
                Logger::error("This may be a macOS permissions issue.");
                Logger::error("Please check System Settings > Privacy & Security > Microphone");
                Logger::error("and ensure Terminal/Cursor has microphone access.");
            }
            
            Pa_CloseStream(input_stream_);
            Pa_CloseStream(output_stream_);
            Pa_Terminate();
            return false;
        }
        
        err = Pa_StartStream(output_stream_);
        if (err != paNoError) {
            Logger::error("Failed to start output stream: " + std::string(Pa_GetErrorText(err)));
            Pa_StopStream(input_stream_);
            Pa_CloseStream(input_stream_);
            Pa_CloseStream(output_stream_);
            Pa_Terminate();
            return false;
        }
        
        return true;
    }
    
    bool read_frame(AudioFrame& frame) {
        if (!input_stream_) return false;
        
        frame.resize(SAMPLES_PER_FRAME);
        PaError err = Pa_ReadStream(input_stream_, frame.data(), SAMPLES_PER_FRAME);
        
        if (err == paInputOverflow) {
            Logger::warn("Input overflow");
        } else if (err != paNoError) {
            return false;
        }
        
        return true;
    }
    
    bool play(const AudioBuffer& buffer) {
        std::lock_guard<std::mutex> lock(playback_mutex_);
        stop_playback_ = false;
        playback_complete_ = false;
        
        // Clear queue and add new buffer
        while (!playback_queue_.empty()) {
            playback_queue_.pop();
        }
        
        // Split buffer into frames
        for (size_t i = 0; i < buffer.size(); i += SAMPLES_PER_FRAME) {
            AudioFrame frame;
            size_t remaining = buffer.size() - i;
            size_t frame_size = (remaining < SAMPLES_PER_FRAME) ? remaining : SAMPLES_PER_FRAME;
            frame.assign(buffer.begin() + i, buffer.begin() + i + frame_size);
            
            // Pad if needed
            if (frame.size() < SAMPLES_PER_FRAME) {
                frame.resize(SAMPLES_PER_FRAME, 0);
            }
            
            playback_queue_.push(frame);
        }
        
        return true;
    }
    
    bool is_playback_complete() const {
        std::lock_guard<std::mutex> lock(playback_mutex_);
        return playback_complete_ && playback_queue_.empty();
    }
    
    void stop_playback() {
        std::lock_guard<std::mutex> lock(playback_mutex_);
        stop_playback_ = true;
        while (!playback_queue_.empty()) {
            playback_queue_.pop();
        }
        playback_complete_ = true;
    }
    
    void stop() {
        if (input_stream_) {
            Pa_StopStream(input_stream_);
            Pa_CloseStream(input_stream_);
            input_stream_ = nullptr;
        }
        
        if (output_stream_) {
            Pa_StopStream(output_stream_);
            Pa_CloseStream(output_stream_);
            output_stream_ = nullptr;
        }
        
        Pa_Terminate();
    }
    
    static void list_devices() {
        PaError err = Pa_Initialize();
        if (err != paNoError) {
            Logger::error("PortAudio init error: " + std::string(Pa_GetErrorText(err)));
            return;
        }
        
        int num_devices = Pa_GetDeviceCount();
        Logger::info("Available audio devices:");
        
        for (int i = 0; i < num_devices; i++) {
            const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
            std::ostringstream oss;
            oss << "  [" << i << "] " << info->name;
            if (info->maxInputChannels > 0) oss << " (IN)";
            if (info->maxOutputChannels > 0) oss << " (OUT)";
            Logger::info(oss.str());
        }
        
        Pa_Terminate();
    }
    
    static std::string get_device_name(int index, bool is_input) {
        PaError err = Pa_Initialize();
        if (err != paNoError) return "";
        
        const PaDeviceInfo* info = Pa_GetDeviceInfo(index);
        std::string name = info ? info->name : "";
        
        Pa_Terminate();
        return name;
    }
    
private:
    int find_device(const std::string& name, bool is_input) {
        int num_devices = Pa_GetDeviceCount();
        
        // Try default device first (most common case)
        if (name == "default" || name.empty()) {
            int default_idx = is_input ? Pa_GetDefaultInputDevice() : Pa_GetDefaultOutputDevice();
            if (default_idx != paNoDevice) {
                const PaDeviceInfo* info = Pa_GetDeviceInfo(default_idx);
                std::ostringstream oss;
                oss << "Using default " << (is_input ? "input" : "output") 
                    << " device: [" << default_idx << "] " << info->name;
                Logger::debug(oss.str());
                return default_idx;
            }
        }
        
        // Try parsing as numeric device index
        try {
            int device_idx = std::stoi(name);
            if (device_idx >= 0 && device_idx < num_devices) {
                const PaDeviceInfo* info = Pa_GetDeviceInfo(device_idx);
                if (info) {
                    // Verify device type matches
                    if (is_input && info->maxInputChannels > 0) {
                        std::ostringstream oss;
                        oss << "Using input device by index: [" << device_idx << "] " << info->name;
                        Logger::debug(oss.str());
                        return device_idx;
                    }
                    if (!is_input && info->maxOutputChannels > 0) {
                        std::ostringstream oss;
                        oss << "Using output device by index: [" << device_idx << "] " << info->name;
                        Logger::debug(oss.str());
                        return device_idx;
                    }
                }
            }
        } catch (const std::exception&) {
            // Not a number, continue to name matching
        }
        
        // Try exact name match
        for (int i = 0; i < num_devices; i++) {
            const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
            if (info && info->name == name) {
                if (is_input && info->maxInputChannels > 0) {
                    std::ostringstream oss;
                    oss << "Found input device: [" << i << "] " << info->name;
                    Logger::debug(oss.str());
                    return i;
                }
                if (!is_input && info->maxOutputChannels > 0) {
                    std::ostringstream oss;
                    oss << "Found output device: [" << i << "] " << info->name;
                    Logger::debug(oss.str());
                    return i;
                }
            }
        }
        
        return -1;
    }
    
    static int audio_callback(const void* input, void* output,
                             unsigned long frame_count,
                             const PaStreamCallbackTimeInfo* time_info,
                             PaStreamCallbackFlags status_flags,
                             void* user_data) {
        Impl* self = static_cast<Impl*>(user_data);
        std::lock_guard<std::mutex> lock(self->playback_mutex_);
        
        Sample* out = static_cast<Sample*>(output);
        
        if (self->stop_playback_ || self->playback_queue_.empty()) {
            // Output silence
            std::memset(out, 0, frame_count * sizeof(Sample));
            if (self->playback_queue_.empty() && !self->playback_complete_) {
                // Mark playback as complete when queue is empty
                self->playback_complete_ = true;
            }
            return paContinue;
        }
        
        // Get next frame
        AudioFrame frame = self->playback_queue_.front();
        self->playback_queue_.pop();
        
        std::memcpy(out, frame.data(), frame_count * sizeof(Sample));
        
        return paContinue;
    }
    
    PaStream* input_stream_;
    PaStream* output_stream_;
    int sample_rate_;
    
    mutable std::mutex playback_mutex_;
    std::queue<AudioFrame> playback_queue_;
    std::atomic<bool> playback_complete_;
    std::atomic<bool> stop_playback_;
};

AudioIO::AudioIO() : pimpl_(std::make_unique<Impl>()) {}
AudioIO::~AudioIO() = default;

bool AudioIO::start(const std::string& input_device, const std::string& output_device, int sample_rate) {
    return pimpl_->start(input_device, output_device, sample_rate);
}

bool AudioIO::read_frame(AudioFrame& frame) {
    return pimpl_->read_frame(frame);
}

bool AudioIO::play(const AudioBuffer& buffer) {
    return pimpl_->play(buffer);
}

bool AudioIO::is_playback_complete() const {
    return pimpl_->is_playback_complete();
}

void AudioIO::stop_playback() {
    pimpl_->stop_playback();
}

void AudioIO::stop() {
    pimpl_->stop();
}

void AudioIO::list_devices() {
    Impl::list_devices();
}

std::string AudioIO::get_device_name(int index, bool is_input) {
    return Impl::get_device_name(index, is_input);
}

} // namespace memo_rf
