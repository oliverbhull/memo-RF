#pragma once

#include "common.h"
#include <string>
#include <memory>
#include <functional>

namespace memo_rf {

/**
 * @brief Audio I/O manager using PortAudio
 * 
 * Provides frame-based audio input and output with thread-safe playback queue.
 * Manages PortAudio streams for microphone input and speaker output.
 * 
 * Thread Safety:
 * - read_frame() and play() are thread-safe (protected by mutex)
 * - Audio callback runs in PortAudio's thread (separate from main loop)
 * - Playback queue is protected by mutex
 * - All public methods are safe to call from any thread
 */
class AudioIO {
public:
    AudioIO();
    ~AudioIO();
    
    // Non-copyable
    AudioIO(const AudioIO&) = delete;
    AudioIO& operator=(const AudioIO&) = delete;
    
    /**
     * @brief Initialize audio devices and start streams
     * @param input_device Device name or "default" for system default
     * @param output_device Device name or "default" for system default
     * @param sample_rate Output/pipeline sample rate in Hz (typically 16000)
     * @param input_sample_rate If non-zero, open input at this rate and resample to sample_rate (for devices that do not support 16 kHz)
     * @return True if initialization successful, false on error
     */
    bool start(const std::string& input_device,
               const std::string& output_device,
               int sample_rate,
               int input_sample_rate = 0,
               int output_sample_rate = 0);
    
    /**
     * @brief Read a frame of audio from input stream (blocking)
     * @param frame Output frame buffer (will be resized to SAMPLES_PER_FRAME)
     * @return True if frame read successfully, false on error or stream closed
     */
    bool read_frame(AudioFrame& frame);
    
    /**
     * @brief Play audio buffer (non-blocking, queues for playback)
     * @param buffer Audio samples to play
     * @return True if queued successfully
     */
    bool play(const AudioBuffer& buffer);

    /**
     * @brief Append audio buffer to current playback queue (for streaming TTS).
     * Does not clear the queue; use after play() to add more audio.
     * @param buffer Audio samples to append
     * @return True if queued successfully
     */
    bool append_playback(const AudioBuffer& buffer);
    
    /**
     * @brief Check if playback queue is empty and playback complete
     * @return True if no audio is playing
     */
    bool is_playback_complete() const;
    
    /**
     * @brief Flush the input queue (discard any accumulated frames).
     * Call when transitioning to IdleListening so we do not process stale
     * audio that accumulated while the main loop was blocked in STT/LLM/TTS.
     */
    void flush_input_queue();
    
    /**
     * @brief Stop playback immediately and clear queue
     */
    void stop_playback();
    
    /**
     * @brief Stop all audio I/O and close streams
     */
    void stop();
    
    /**
     * @brief List all available audio devices to console
     */
    static void list_devices();
    
    /**
     * @brief Get device name by index
     * @param index Device index
     * @param is_input True for input device, false for output
     * @return Device name or empty string if invalid
     */
    static std::string get_device_name(int index, bool is_input);

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace memo_rf
