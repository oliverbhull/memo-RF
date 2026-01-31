#include "state_machine.h"
#include "vad_endpointing.h"

namespace memo_rf {

class StateMachine::Impl {
public:
    explicit Impl(bool wake_word_enabled)
        : state_(State::IdleListening), wake_word_enabled_(wake_word_enabled), has_pending_response_(false) {}

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
                    if (has_pending_response_) {
                        state_ = State::WaitingForChannelClear;
                    } else if (wake_word_enabled_) {
                        state_ = State::IdleListening;
                    } else {
                        state_ = State::Thinking;
                    }
                }
                break;

            case State::Thinking:
                break;

            case State::WaitingForChannelClear:
                if (event == VADEvent::SpeechStart) {
                    state_ = State::ReceivingSpeech;
                }
                break;

            case State::Transmitting:
                if (event == VADEvent::SpeechStart) {
                    state_ = State::ReceivingSpeech;
                }
                break;
        }
    }

    void on_transcript_ready(const Transcript& transcript) {
        (void)transcript;
    }

    void on_response_ready(const AudioBuffer& audio) {
        (void)audio;
        if (state_ == State::IdleListening) {
            state_ = State::WaitingForChannelClear;
            has_pending_response_ = true;
        } else if (state_ == State::Thinking) {
            state_ = State::Transmitting;
        }
    }

    void on_channel_clear() {
        if (state_ == State::WaitingForChannelClear) {
            state_ = State::Transmitting;
            has_pending_response_ = false;
        }
    }

    void on_playback_complete() {
        if (state_ == State::Transmitting) {
            state_ = State::IdleListening;
        }
    }

    bool should_interrupt_transmission() const {
        return state_ == State::Transmitting;
    }

    void reset() {
        state_ = State::IdleListening;
        has_pending_response_ = false;
    }

private:
    State state_;
    bool wake_word_enabled_;
    bool has_pending_response_;
};

StateMachine::StateMachine(bool wake_word_enabled) : pimpl_(std::make_unique<Impl>(wake_word_enabled)) {}
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

void StateMachine::on_channel_clear() {
    pimpl_->on_channel_clear();
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
