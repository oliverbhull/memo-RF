#include "state_machine.h"
#include "vad_endpointing.h"

namespace memo_rf {

class StateMachine::Impl {
public:
    Impl() : state_(State::IdleListening) {}
    
    State get_state() const {
        return state_;
    }
    
    void on_vad_event(VADEvent event) {
        switch (state_) {
            case State::IdleListening:
                if (event == VADEvent::SpeechStart) {
                    state_ = State::ReceivingSpeech;
                }
                break;
                
            case State::ReceivingSpeech:
                if (event == VADEvent::SpeechEnd) {
                    state_ = State::Thinking;
                }
                break;
                
            case State::Thinking:
                // Can't transition from Thinking on VAD events
                break;
                
            case State::Transmitting:
                if (event == VADEvent::SpeechStart) {
                    // Interrupt transmission
                    state_ = State::ReceivingSpeech;
                }
                break;
        }
    }
    
    void on_transcript_ready(const Transcript& transcript) {
        // Transition from Thinking happens when response is ready
        // This is handled by on_response_ready
    }
    
    void on_response_ready(const AudioBuffer& audio) {
        if (state_ == State::Thinking) {
            state_ = State::Transmitting;
        }
    }
    
    void on_playback_complete() {
        if (state_ == State::Transmitting) {
            state_ = State::IdleListening;
        }
    }
    
    bool should_interrupt_transmission() const {
        // This is checked externally when SpeechStart is detected
        return state_ == State::Transmitting;
    }
    
    void reset() {
        state_ = State::IdleListening;
    }

private:
    State state_;
};

StateMachine::StateMachine() : pimpl_(std::make_unique<Impl>()) {}
StateMachine::~StateMachine() = default;

State StateMachine::get_state() const {
    return pimpl_->get_state();
}

void StateMachine::on_vad_event(VADEvent event) {
    pimpl_->on_vad_event(event);
}

void StateMachine::on_transcript_ready(const Transcript& transcript) {
    pimpl_->on_transcript_ready(transcript);
}

void StateMachine::on_response_ready(const AudioBuffer& audio) {
    pimpl_->on_response_ready(audio);
}

void StateMachine::on_playback_complete() {
    pimpl_->on_playback_complete();
}

bool StateMachine::should_interrupt_transmission() const {
    return pimpl_->should_interrupt_transmission();
}

void StateMachine::reset() {
    pimpl_->reset();
}

} // namespace memo_rf
