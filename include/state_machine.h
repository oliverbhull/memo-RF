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
    IdleListening,   ///< Listening for speech input
    ReceivingSpeech, ///< Currently receiving speech (VAD active)
    Thinking,        ///< Processing transcript and generating response
    Transmitting     ///< Playing response audio
};

/**
 * @brief State machine for voice agent lifecycle
 * 
 * Manages state transitions:
 * - IdleListening -> ReceivingSpeech (on SpeechStart)
 * - ReceivingSpeech -> Thinking (on SpeechEnd)
 * - Thinking -> Transmitting (on response ready)
 * - Transmitting -> IdleListening (on playback complete)
 * 
 * Supports interruption: Transmitting -> ReceivingSpeech (on SpeechStart)
 */
class StateMachine {
public:
    StateMachine();
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
     */
    void on_response_ready(const AudioBuffer& audio);
    
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
