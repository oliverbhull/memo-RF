#include "audio_io.h"
#include "logger.h"
#include <portaudio.h>
#include <vector>
#include <mutex>
#include <queue>
#include <atomic>
#include <cstring>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace memo_rf {

class AudioIO::Impl {
public:
    Impl() : input_stream_(nullptr), output_stream_(nullptr), full_duplex_stream_(nullptr),
             sample_rate_(16000), use_full_duplex_(false), playback_complete_(true), stop_playback_(false) {}
    
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
        if (!input_info) {
            Logger::error("Input device not found: " + input_device);
            Pa_Terminate();
            return false;
        }
        
        // If device reports no input channels, try to find a device with similar name that has input
        if (input_info->maxInputChannels == 0) {
            Logger::warn("Device '" + input_device + "' reports no input channels");
            
            // Try to find a device with similar name that has input capabilities
            // (On macOS, sometimes the same physical device appears as separate input/output entries)
            int num_devices = Pa_GetDeviceCount();
            std::string device_name_lower = input_info->name;
            std::transform(device_name_lower.begin(), device_name_lower.end(), 
                          device_name_lower.begin(), ::tolower);
            
            for (int i = 0; i < num_devices; i++) {
                const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
                if (info && info->maxInputChannels > 0) {
                    std::string candidate_name_lower = info->name;
                    std::transform(candidate_name_lower.begin(), candidate_name_lower.end(),
                                  candidate_name_lower.begin(), ::tolower);
                    
                    // Check if names are similar (same base name, might have different suffixes)
                    // e.g., "External Headphones" might match "External Headphones Input"
                    if (candidate_name_lower.find(device_name_lower) != std::string::npos ||
                        device_name_lower.find(candidate_name_lower) != std::string::npos) {
                        Logger::info("Found matching input device: [" + std::to_string(i) + "] " + info->name);
                        input_idx = i;
                        input_info = info;
                        break;
                    }
                }
            }
            
            if (input_info->maxInputChannels == 0) {
                Logger::warn("No matching input device found, but attempting anyway (full-duplex may work)");
            }
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
        if (!output_info) {
            Logger::error("Output device not found: " + output_device);
            Pa_Terminate();
            return false;
        }
        
        // Warn if device reports no output channels, but still try (full-duplex might work)
        if (output_info->maxOutputChannels == 0) {
            Logger::warn("Device '" + output_device + "' reports no output channels, but attempting anyway (full-duplex may work)");
        }
        
        // Log device info for debugging
        std::ostringstream dev_oss;
        dev_oss << "Using input device: [" << input_idx << "] " << input_info->name;
        Logger::info(dev_oss.str());
        std::ostringstream dev_oss2;
        dev_oss2 << "Using output device: [" << output_idx << "] " << output_info->name;
        Logger::info(dev_oss2.str());
        
        // If same device, try full-duplex stream first (works better for devices like walkie-talkies)
        if (input_idx == output_idx) {
            Logger::info("Same device for input/output, attempting full-duplex stream...");
            
            PaStreamParameters input_params;
            input_params.device = input_idx;
            input_params.channelCount = 1;
            input_params.sampleFormat = paInt16;
            // Use default latency - if device reports 0 input channels, use output latency as fallback
            if (input_info->maxInputChannels > 0) {
                input_params.suggestedLatency = input_info->defaultLowInputLatency;
            } else {
                // Device reports no input channels, but try using output latency
                // For Baofeng adapters, sometimes the device supports input but macOS doesn't report it
                input_params.suggestedLatency = output_info->defaultLowOutputLatency;
                // Try setting channelCount to 0 to see if PortAudio can figure it out
                // Actually, we can't do that - need at least 1 channel. Let's try 1 anyway.
                Logger::info("Device reports 0 input channels, but attempting full-duplex anyway (Baofeng adapter may work)");
            }
            input_params.hostApiSpecificStreamInfo = nullptr;
            
            PaStreamParameters output_params;
            output_params.device = output_idx;
            output_params.channelCount = 1;
            output_params.sampleFormat = paInt16;
            output_params.suggestedLatency = output_info->defaultLowOutputLatency;
            output_params.hostApiSpecificStreamInfo = nullptr;
            
            // For devices that report 0 input channels, skip format check and try opening directly
            // (Sometimes macOS doesn't report input capability but the device actually supports it)
            if (input_info->maxInputChannels == 0) {
                Logger::info("Device reports 0 input channels - attempting full-duplex anyway (may work for Baofeng adapters)...");
                // Try to open full-duplex stream directly without format check
                err = Pa_OpenStream(&full_duplex_stream_, &input_params, &output_params, sample_rate_,
                                   SAMPLES_PER_FRAME, paClipOff, full_duplex_callback, this);
                if (err == paNoError) {
                    err = Pa_StartStream(full_duplex_stream_);
                    if (err == paNoError) {
                        use_full_duplex_ = true;
                        Logger::info("Full-duplex stream opened successfully (despite 0 reported input channels)!");
                        return true;
                    } else {
                        Logger::warn("Failed to start full-duplex stream: " + std::string(Pa_GetErrorText(err)));
                        Pa_CloseStream(full_duplex_stream_);
                        full_duplex_stream_ = nullptr;
                    }
                } else {
                    Logger::warn("Failed to open full-duplex stream: " + std::string(Pa_GetErrorText(err)));
                }
            } else {
                // Check if format is supported before trying to open
                err = Pa_IsFormatSupported(&input_params, &output_params, sample_rate_);
                if (err == paFormatIsSupported) {
                    Logger::info("Full-duplex format is supported, opening stream...");
                    // Try to open full-duplex stream
                    err = Pa_OpenStream(&full_duplex_stream_, &input_params, &output_params, sample_rate_,
                                       SAMPLES_PER_FRAME, paClipOff, full_duplex_callback, this);
                    if (err == paNoError) {
                        err = Pa_StartStream(full_duplex_stream_);
                        if (err == paNoError) {
                            use_full_duplex_ = true;
                            Logger::info("Full-duplex stream opened successfully");
                            return true;
                        } else {
                            Logger::warn("Failed to start full-duplex stream: " + std::string(Pa_GetErrorText(err)));
                            Pa_CloseStream(full_duplex_stream_);
                            full_duplex_stream_ = nullptr;
                        }
                    } else {
                        Logger::warn("Failed to open full-duplex stream: " + std::string(Pa_GetErrorText(err)));
                    }
                } else {
                    Logger::warn("Full-duplex format not supported: " + std::string(Pa_GetErrorText(err)));
                }
            }
            Logger::info("Falling back to separate input/output streams...");
        }
        
        // Fall back to separate streams (either devices are different, or full-duplex failed)
        use_full_duplex_ = false;
        
        // Open input stream (only if device actually supports input)
        if (input_info->maxInputChannels > 0) {
            PaStreamParameters input_params;
            input_params.device = input_idx;
            input_params.channelCount = 1;
            input_params.sampleFormat = paInt16;
            input_params.suggestedLatency = input_info->defaultLowInputLatency;
            input_params.hostApiSpecificStreamInfo = nullptr;
            
            err = Pa_OpenStream(&input_stream_, &input_params, nullptr, sample_rate_,
                               SAMPLES_PER_FRAME, paClipOff, nullptr, nullptr);
            if (err != paNoError) {
                Logger::error("Failed to open input stream: " + std::string(Pa_GetErrorText(err)));
                Pa_Terminate();
                return false;
            }
        } else {
            // Device reports 0 input channels - this is the case for Baofeng adapters
            // The 3.5mm adapter typically only exposes output to macOS
            Logger::error("Cannot open separate input stream: device reports 0 input channels");
            Logger::error("");
            Logger::error("The 3.5mm adapter for Baofeng UV-5R only exposes OUTPUT to macOS.");
            Logger::error("It does not provide audio INPUT capability at the OS level.");
            Logger::error("");
            Logger::error("Hardware solutions:");
            Logger::error("  1. Use a USB audio interface that supports bidirectional audio");
            Logger::error("  2. Use a different adapter/cable that properly exposes both input/output");
            Logger::error("  3. Programming cables (USB-to-serial) don't provide audio I/O");
            Logger::error("");
            Logger::error("Software workaround:");
            Logger::error("  Use Mac mic for input, Baofeng adapter for output:");
            Logger::error("  Set input_device='1' (MacBook Pro Microphone)");
            Logger::error("  Keep output_device='0' (External Headphones/Baofeng adapter)");
            Logger::error("");
            Logger::error("Current config: input_device='" + input_device + "', output_device='" + output_device + "'");
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
        if (use_full_duplex_) {
            // In full-duplex mode, read from input queue (populated by callback)
            std::lock_guard<std::mutex> lock(input_queue_mutex_);
            if (input_queue_.empty()) {
                return false;
            }
            frame = input_queue_.front();
            input_queue_.pop();
            return true;
        } else {
            // In separate stream mode, read directly from stream
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
        if (use_full_duplex_ && full_duplex_stream_) {
            Pa_StopStream(full_duplex_stream_);
            Pa_CloseStream(full_duplex_stream_);
            full_duplex_stream_ = nullptr;
        } else {
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
        }
        
        // Clear input queue
        {
            std::lock_guard<std::mutex> lock(input_queue_mutex_);
            while (!input_queue_.empty()) {
                input_queue_.pop();
            }
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
            if (info->maxInputChannels > 0) oss << " (IN:" << info->maxInputChannels << ")";
            if (info->maxOutputChannels > 0) oss << " (OUT:" << info->maxOutputChannels << ")";
            if (info->maxInputChannels == 0 && info->maxOutputChannels == 0) oss << " (no I/O)";
            Logger::info(oss.str());
        }
        
        Logger::info("");
        Logger::info("Note: For Baofeng UV-5R bidirectional audio, you need:");
        Logger::info("  - A TRRS (4-pole) adapter/cable (not TRS/3-pole)");
        Logger::info("  - Or a proper Baofeng audio interface (e.g., BaofengUV5R-TRRS board)");
        Logger::info("  - Standard K1-to-3.5mm adapters are often output-only");
        
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
                    // Verify device type matches, but allow using same device for both if explicitly specified
                    if (is_input) {
                        if (info->maxInputChannels > 0) {
                            std::ostringstream oss;
                            oss << "Using input device by index: [" << device_idx << "] " << info->name;
                            Logger::debug(oss.str());
                            return device_idx;
                        } else if (info->maxOutputChannels > 0) {
                            // Allow using output-only device as input if explicitly specified
                            // PortAudio will handle the error if it truly doesn't support input
                            std::ostringstream oss;
                            oss << "Using device by index for input (may not support input): [" << device_idx << "] " << info->name;
                            Logger::debug(oss.str());
                            return device_idx;
                        }
                    } else {
                        if (info->maxOutputChannels > 0) {
                            std::ostringstream oss;
                            oss << "Using output device by index: [" << device_idx << "] " << info->name;
                            Logger::debug(oss.str());
                            return device_idx;
                        } else if (info->maxInputChannels > 0) {
                            // Allow using input-only device as output if explicitly specified
                            std::ostringstream oss;
                            oss << "Using device by index for output (may not support output): [" << device_idx << "] " << info->name;
                            Logger::debug(oss.str());
                            return device_idx;
                        }
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
                if (is_input) {
                    if (info->maxInputChannels > 0) {
                        std::ostringstream oss;
                        oss << "Found input device: [" << i << "] " << info->name;
                        Logger::debug(oss.str());
                        return i;
                    } else if (info->maxOutputChannels > 0) {
                        // Allow using output-only device as input if explicitly specified by name
                        std::ostringstream oss;
                        oss << "Found device for input (may not support input): [" << i << "] " << info->name;
                        Logger::debug(oss.str());
                        return i;
                    }
                } else {
                    if (info->maxOutputChannels > 0) {
                        std::ostringstream oss;
                        oss << "Found output device: [" << i << "] " << info->name;
                        Logger::debug(oss.str());
                        return i;
                    } else if (info->maxInputChannels > 0) {
                        // Allow using input-only device as output if explicitly specified by name
                        std::ostringstream oss;
                        oss << "Found device for output (may not support output): [" << i << "] " << info->name;
                        Logger::debug(oss.str());
                        return i;
                    }
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
    
    static int full_duplex_callback(const void* input, void* output,
                                    unsigned long frame_count,
                                    const PaStreamCallbackTimeInfo* time_info,
                                    PaStreamCallbackFlags status_flags,
                                    void* user_data) {
        Impl* self = static_cast<Impl*>(user_data);
        
        // Handle input: copy to input queue
        if (input) {
            const Sample* in = static_cast<const Sample*>(input);
            AudioFrame input_frame;
            input_frame.assign(in, in + frame_count);
            
            std::lock_guard<std::mutex> input_lock(self->input_queue_mutex_);
            // Limit queue size to prevent memory issues
            if (self->input_queue_.size() < 100) {
                self->input_queue_.push(input_frame);
            }
        }
        
        // Handle output: same as regular callback
        std::lock_guard<std::mutex> playback_lock(self->playback_mutex_);
        
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
    PaStream* full_duplex_stream_;
    int sample_rate_;
    bool use_full_duplex_;
    
    mutable std::mutex playback_mutex_;
    std::queue<AudioFrame> playback_queue_;
    std::atomic<bool> playback_complete_;
    std::atomic<bool> stop_playback_;
    
    mutable std::mutex input_queue_mutex_;
    std::queue<AudioFrame> input_queue_;
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
