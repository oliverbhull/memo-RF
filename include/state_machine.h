#pragma once

#include "common.h"
#include "config.h"
#include "vad_endpointing.h"
#include <memory>
#include <functional>

namespace memo_rf {

/**
 * @brief Agent state enumeration
 */
enum class State {
    IdleListening,         ///< Listening for speech input
    ReceivingSpeech,       ///< Currently receiving speech (VAD active)
    Thinking,              ///< Processing transcript and generating response (legacy path when wake_word disabled)
    WaitingForChannelClear,///< Response ready; waiting for channel clear before TX (half-duplex)
    Transmitting           ///< Playing response audio
};

/**
 * @brief State machine for voice agent lifecycle
 *
 * When wake_word_enabled:
 * - IdleListening -> ReceivingSpeech (on SpeechStart)
 * - ReceivingSpeech -> IdleListening (on SpeechEnd; agent runs STT, only responds if "hey memo")
 * - IdleListening -> WaitingForChannelClear (on_response_ready when agent built response)
 * - WaitingForChannelClear -> Transmitting (on_channel_clear)
 * - WaitingForChannelClear -> ReceivingSpeech (on SpeechStart, interrupt on channel)
 * - ReceivingSpeech (with pending) -> WaitingForChannelClear (on SpeechEnd)
 * - Transmitting -> IdleListening (on playback complete)
 *
 * When !wake_word_enabled (legacy):
 * - ReceivingSpeech -> Thinking (on SpeechEnd), Thinking -> Transmitting (on response ready)
 */
class StateMachine {
public:
    explicit StateMachine(bool wake_word_enabled = true);
    ~StateMachine();
    
    /**
     * @brief Get current state
     * @return Current state
     */
    State get_state() const;
    
    /**
     * @brief Process VAD event (speech start/end)
     * @param event VAD event type
     */
    void on_vad_event(VADEvent event);
    
    /**
     * @brief Notify that transcript is ready
     * @param transcript Completed transcript
     */
    void on_transcript_ready(const Transcript& transcript);
    
    /**
     * @brief Notify that response audio is ready
     * @param audio Response audio buffer
     * When wake_word enabled: if IdleListening, transition to WaitingForChannelClear; else if Thinking, transition to Transmitting.
     */
    void on_response_ready(const AudioBuffer& audio);

    /**
     * @brief Notify that channel is clear (half-duplex); transition WaitingForChannelClear -> Transmitting.
     */
    void on_channel_clear();

    /**
     * @brief Notify that playback is complete
     */
    void on_playback_complete();
    
    /**
     * @brief Check if transmission should be interrupted
     * @return True if currently transmitting
     */
    bool should_interrupt_transmission() const;
    
    /**
     * @brief Reset state machine to IdleListening
     */
    void reset();

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace memo_rf
