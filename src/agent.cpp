#include "agent.h"
#include "agent_pipeline.h"
#include "audio_io.h"
#include "vad_endpointing.h"
#include "stt_engine.h"
#include "router.h"
#include "llm_client.h"
#include "tts_engine.h"
#include "tx_controller.h"
#include "state_machine.h"
#include "session_recorder.h"
#include "logger.h"
#include "utils.h"
#include "transcript_gate.h"
#include "common.h"
#include <atomic>
#include <thread>
#include <chrono>
#include <cmath>
#include <sstream>
#include <vector>

namespace memo_rf {

class VoiceAgent::Impl {
public:
    Impl(const Config& config) 
        : config_(config), running_(true), initialized_(false),
          transmission_end_time_(std::chrono::steady_clock::now()),
          previous_state_(State::IdleListening),
          speech_frame_count_(0),
          speech_start_time_(std::chrono::steady_clock::now()),
          last_speech_log_time_(std::chrono::steady_clock::now()),
          last_speech_end_time_(std::chrono::steady_clock::now()) {}
    
    ~Impl() {
        shutdown();
    }
    
    bool initialize() {
        if (initialized_) {
            return true;
        }
        
        // Initialize components
        audio_io_ = std::make_unique<AudioIO>();
        vad_ = std::make_unique<VADEndpointing>(config_.vad);
        stt_ = std::make_unique<STTEngine>(config_.stt);
        router_ = std::make_unique<Router>();
        llm_ = std::make_unique<LLMClient>(config_.llm);
        tts_ = std::make_unique<TTSEngine>(config_.tts);
        tx_ = std::make_unique<TXController>(config_.tx);
        state_machine_ = std::make_unique<StateMachine>(config_.wake_word.enabled);
        recorder_ = std::make_unique<SessionRecorder>(config_.session_log_dir);
        
        // Set up TX controller
        tx_->set_audio_io(audio_io_.get());
        
        // Start audio I/O
        Logger::info("Initializing audio I/O...");
        Logger::info("  Input device: " + config_.audio.input_device);
        Logger::info("  Output device: " + config_.audio.output_device);
        
        if (!audio_io_->start(config_.audio.input_device,
                              config_.audio.output_device,
                              config_.audio.sample_rate,
                              config_.audio.input_sample_rate)) {
            Logger::error("Failed to start audio I/O");
            return false;
        }
        
        Logger::info("Audio I/O started successfully");
        
        // Start session recording
        Logger::info("Starting session recording...");
        recorder_->start_session();
        Logger::info("Session recording started");

        // Record session metadata
        recorder_->set_session_metadata("persona", config_.llm.agent_persona);
        recorder_->set_session_metadata("persona_name", config_.llm.persona_name);
        recorder_->set_session_metadata("response_language", config_.llm.response_language);

        pipeline_ = std::make_unique<AgentPipeline>(
            &config_, audio_io_.get(), vad_.get(), stt_.get(), router_.get(),
            llm_.get(), tts_.get(), tx_.get(), state_machine_.get(), recorder_.get(),
            &running_, &transmission_end_time_);

        // Optional: warmup translation model so first user request avoids load_duration (Ollama only)
        if (config_.llm.warmup_translation_model && !config_.llm.translation_model.empty()
            && config_.llm.endpoint.find("/api/chat") != std::string::npos) {
            std::string lang = config_.llm.response_language.empty() ? "Spanish" : config_.llm.response_language;
            if (lang == "es") lang = "Spanish";
            else if (lang == "fr") lang = "French";
            else if (lang == "de") lang = "German";
            std::string warmup_prompt = "You are a professional English to " + lang + " translator. "
                "Output only the " + lang + " translation, no explanations. End transmissions with \"over\".";
            std::vector<std::string> empty_history;
            LLMResponse warmup_r = llm_->generate_with_tools("Hi", "", empty_history,
                config_.llm.timeout_ms, 5, config_.llm.translation_model, warmup_prompt);
            (void)warmup_r;
            Logger::info("Translation model warmup complete");
        }

        initialized_ = true;
        return true;
    }
    
    int run() {
        if (!initialized_ && !initialize()) {
            return 1;
        }
        
        Logger::info("=== Memo-RF Voice Agent Started ===");
        if (!config_.llm.persona_name.empty()) {
            Logger::info("Agent persona: " + config_.llm.persona_name);
        } else if (!config_.llm.agent_persona.empty()) {
            Logger::info("Agent persona: " + config_.llm.agent_persona);
        }
        Logger::info("Listening for speech...");
        
        // State tracking
        int utterance_id = 0;
        AudioBuffer current_utterance;
        Transcript current_transcript;
        Plan current_plan;
        AudioBuffer response_audio;
        
        // Main loop
        AudioFrame frame;
        int frame_count = 0;
        constexpr int RMS_LOG_INTERVAL = 50; // Log RMS every 50 frames (~1 second)
        
        while (running_) {
            // Read audio frame
            if (!audio_io_->read_frame(frame)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            
            frame_count++;
            
            // Debug: print audio level periodically
            if (frame_count % RMS_LOG_INTERVAL == 0) {
                log_rms_level(frame, frame_count);
            }
            
            // Record raw input (always record, even during guard period)
            recorder_->record_input_frame(frame);
            
            // Detect state transitions for guard period timing
            State current_state = state_machine_->get_state();
            
            // Echo probe: log RMS 1-3 s after TX end to measure whether we capture our own TTS in the mic
            if (current_state == State::IdleListening && (frame_count % 50) == 0) {
                auto now = std::chrono::steady_clock::now();
                int64_t elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - transmission_end_time_).count();
                if (elapsed_ms >= 1000 && elapsed_ms <= 3000) {
                    float sum_sq = 0.0f;
                    for (auto s : frame) {
                        float normalized = static_cast<float>(s) / 32768.0f;
                        sum_sq += normalized * normalized;
                    }
                    float rms = frame.empty() ? 0.0f : std::sqrt(sum_sq / frame.size());
                    std::ostringstream eoss;
                    eoss << "[echo_probe] post_tx_ms=" << elapsed_ms << " rms=" << rms;
                    LOG_AUDIO(eoss.str());
                }
            }
            
            // Check if playback completed while in Transmitting state
            // Wait POST_PLAYBACK_DELAY_MS after playback_complete before transitioning so DAC/mic can settle
            if (current_state == State::Transmitting) {
                bool playback_done = audio_io_->is_playback_complete();
                // Log periodically to debug stuck state (every 100 frames = ~6 seconds at 16kHz)
                if (frame_count % 100 == 0) {
                    std::ostringstream oss;
                    oss << "Transmitting state: playback_complete=" << (playback_done ? "true" : "false");
                    LOG_TX(oss.str());
                }
                if (playback_done) {
                    if (!post_playback_delay_started_) {
                        playback_complete_at_ = std::chrono::steady_clock::now();
                        post_playback_delay_started_ = true;
                    }
                    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - playback_complete_at_).count();
                    if (elapsed_ms >= POST_PLAYBACK_DELAY_MS) {
                        LOG_TX("Playback complete, transitioning to IdleListening");
                        state_machine_->on_playback_complete();
                        transmission_end_time_ = std::chrono::steady_clock::now();
                        vad_->reset(); // Clear any accumulated audio
                        audio_io_->flush_input_queue();
                        current_state = state_machine_->get_state();
                        post_playback_delay_started_ = false;
                    }
                }
            }
            
            previous_state_ = current_state;
            
            // Process frame through state machine and VAD
            // (process_frame will return early if in guard period or transmitting)
            process_frame(frame, current_utterance, current_transcript, 
                        current_plan, response_audio, utterance_id);
            
            // Small sleep to prevent busy-waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        // Cleanup
        cleanup();
        
        return 0;
    }
    
    void shutdown() {
        running_ = false;
    }

private:
    void log_rms_level(const AudioFrame& frame, int frame_count) {
        float sum_sq = 0.0f;
        for (auto s : frame) {
            float normalized = static_cast<float>(s) / 32768.0f;
            sum_sq += normalized * normalized;
        }
        float rms = std::sqrt(sum_sq / frame.size());
        std::ostringstream oss;
        oss << "Frame " << frame_count << ", RMS level: " << rms 
            << " (threshold: " << config_.vad.threshold << ")";
        LOG_AUDIO(oss.str());
    }
    
    void process_frame(const AudioFrame& frame, AudioBuffer& current_utterance,
                      Transcript& current_transcript, Plan& current_plan,
                      AudioBuffer& response_audio, int& utterance_id) {
        State current_state = state_machine_->get_state();
        
        // CRITICAL: Never process VAD during transmission or thinking
        // This is the primary defense against feedback loops
        // We do process VAD in WaitingForChannelClear so we can detect SpeechStart (interrupt on channel)
        if (current_state == State::Transmitting || current_state == State::Thinking) {
            return;
        }

        // When WaitingForChannelClear: check if channel is clear; if so, transmit pending response
        if (current_state == State::WaitingForChannelClear && !pending_response_audio_.empty()) {
            auto now = std::chrono::steady_clock::now();
            auto silence_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_speech_end_time_).count();
            if (silence_ms >= config_.tx.channel_clear_silence_ms) {
                state_machine_->on_channel_clear();
                vad_->reset();
                tx_->transmit(pending_response_audio_);
                pending_response_audio_.clear();
                return;
            }
        }
        
        // Check guard period when in IdleListening (not when WaitingForChannelClear)
        if (current_state == State::IdleListening) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - transmission_end_time_).count();
            
            if (elapsed_ms < VAD_GUARD_PERIOD_MS) {
                // Still in guard period - completely skip VAD processing
                // This prevents feedback loops from agent's own voice (echo/reverb)
                return;
            }
        }
        
        // Process VAD when in IdleListening (past guard period), ReceivingSpeech, or WaitingForChannelClear (to detect interrupt)
        VADEvent vad_event = VADEvent::None;
        if (current_state == State::IdleListening || 
            current_state == State::ReceivingSpeech ||
            current_state == State::WaitingForChannelClear) {
            vad_event = vad_->process(frame);
            
            // Debug VAD events
            if (vad_event == VADEvent::SpeechStart) {
                LOG_VAD("Speech detected - START");
            } else if (vad_event == VADEvent::SpeechEnd) {
                LOG_VAD("Speech detected - END");
            }
        }
        
        if (vad_event == VADEvent::SpeechStart) {
            handle_speech_start(current_state);
            speech_frame_count_ = 0; // Reset speech frame counter
            speech_start_time_ = std::chrono::steady_clock::now(); // Track when speech started
            last_speech_log_time_ = speech_start_time_; // Initialize last log time
            current_utterance.clear();
            current_utterance = vad_->get_current_segment();
        } else if (vad_event == VADEvent::SpeechEnd) {
            pipeline_->handle_speech_end(current_utterance, current_transcript,
                                        current_plan, response_audio, utterance_id,
                                        last_speech_end_time_, pending_response_audio_);
        } else {
            // Continue accumulating if in ReceivingSpeech
            if (current_state == State::ReceivingSpeech) {
                current_utterance = vad_->get_current_segment();
                speech_frame_count_++;
                
                // Log periodically to show speech is happening (~every 1 second)
                auto now = std::chrono::steady_clock::now();
                auto elapsed_since_last_log = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - last_speech_log_time_).count();
                
                if (elapsed_since_last_log >= 1000) { // Log every 1 second
                    auto speech_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - speech_start_time_).count();
                    std::ostringstream oss;
                    oss << "Receiving speech... (" << speech_duration << "ms)";
                    Logger::info(oss.str());
                    last_speech_log_time_ = now;
                }
            } else {
                // Reset counter when not in ReceivingSpeech
                speech_frame_count_ = 0;
            }
        }
    }
    
    void handle_speech_start(State current_state) {
        if (current_state == State::Transmitting) {
            // Interrupt transmission (shouldn't happen now, but keep as safety)
            tx_->stop();
            audio_io_->stop_playback();
            state_machine_->on_playback_complete();
        }
        state_machine_->on_vad_event(VADEvent::SpeechStart);
    }

    void cleanup() {
        if (tx_) {
            tx_->stop();
        }
        if (audio_io_) {
            audio_io_->stop();
        }
        if (recorder_) {
            recorder_->finalize_session();
            Logger::info("Session saved: " + recorder_->get_session_id());
        }
    }

    Config config_;
    std::atomic<bool> running_;
    bool initialized_;
    
    // Track transmission end time for guard period
    std::chrono::steady_clock::time_point transmission_end_time_;
    
    // Post-playback delay: wait before transitioning to IdleListening so DAC/mic can settle
    std::chrono::steady_clock::time_point playback_complete_at_;
    bool post_playback_delay_started_ = false;
    
    // Track previous state for transition detection
    State previous_state_;
    
    // Track speech frame count for logging
    int speech_frame_count_;
    
        // Track speech timing for periodic logging
    std::chrono::steady_clock::time_point speech_start_time_;
    std::chrono::steady_clock::time_point last_speech_log_time_;

    // Half-duplex: channel clear before TX
    std::chrono::steady_clock::time_point last_speech_end_time_;
    AudioBuffer pending_response_audio_;
    
    std::unique_ptr<AudioIO> audio_io_;
    std::unique_ptr<VADEndpointing> vad_;
    std::unique_ptr<STTEngine> stt_;
    std::unique_ptr<Router> router_;
    std::unique_ptr<LLMClient> llm_;
    std::unique_ptr<TTSEngine> tts_;
        std::unique_ptr<TXController> tx_;
        std::unique_ptr<StateMachine> state_machine_;
        std::unique_ptr<SessionRecorder> recorder_;
        std::unique_ptr<AgentPipeline> pipeline_;
};

VoiceAgent::VoiceAgent(const Config& config)
    : pimpl_(std::make_unique<Impl>(config)) {}

VoiceAgent::~VoiceAgent() = default;

bool VoiceAgent::initialize() {
    return pimpl_->initialize();
}

int VoiceAgent::run() {
    return pimpl_->run();
}

void VoiceAgent::shutdown() {
    pimpl_->shutdown();
}

} // namespace memo_rf
