#include "agent.h"
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
#include "common.h"
#include "tool_registry.h"
#include "tool_executor.h"
#include "tools/log_memo_tool.h"
#include "tools/external_research_tool.h"
#include "tools/internal_search_tool.h"
#include <thread>
#include <atomic>
#include <chrono>
#include <cmath>
#include <sstream>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace memo_rf {

class VoiceAgent::Impl {
public:
    Impl(const Config& config) 
        : config_(config), running_(true), initialized_(false),
          transmission_end_time_(std::chrono::steady_clock::now()),
          previous_state_(State::IdleListening),
          speech_frame_count_(0),
          speech_start_time_(std::chrono::steady_clock::now()),
          last_speech_log_time_(std::chrono::steady_clock::now()) {}
    
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
        state_machine_ = std::make_unique<StateMachine>();
        recorder_ = std::make_unique<SessionRecorder>(config_.session_log_dir);
        
        // Set up TX controller
        tx_->set_audio_io(audio_io_.get());
        
        // Start audio I/O
        Logger::info("Initializing audio I/O...");
        Logger::info("  Input device: " + config_.audio.input_device);
        Logger::info("  Output device: " + config_.audio.output_device);
        
        if (!audio_io_->start(config_.audio.input_device, 
                              config_.audio.output_device,
                              config_.audio.sample_rate)) {
            Logger::error("Failed to start audio I/O");
            return false;
        }
        
        Logger::info("Audio I/O started successfully");
        
        // Start session recording
        Logger::info("Starting session recording...");
        recorder_->start_session();
        Logger::info("Session recording started");
        
        // Initialize tool system
        Logger::info("Initializing tool system...");
        tool_registry_ = std::make_unique<ToolRegistry>();
        tool_executor_ = std::make_unique<ToolExecutor>(
            tool_registry_.get(), 
            config_.tools.max_concurrent
        );
        
        // Register tools based on config
        for (const auto& tool_name : config_.tools.enabled) {
            if (tool_name == "log_memo") {
                auto tool = std::make_shared<LogMemoTool>(config_.session_log_dir);
                tool_registry_->register_tool(tool);
            } else if (tool_name == "external_research") {
                auto tool = std::make_shared<ExternalResearchTool>();
                tool_registry_->register_tool(tool);
            } else if (tool_name == "internal_search") {
                auto tool = std::make_shared<InternalSearchTool>(config_.session_log_dir);
                tool_registry_->register_tool(tool);
            } else {
                Logger::warn("Unknown tool name in config: " + tool_name);
            }
        }
        Logger::info("Tool system initialized with " + std::to_string(tool_registry_->size()) + " tools");
        
        initialized_ = true;
        return true;
    }
    
    int run() {
        if (!initialized_ && !initialize()) {
            return 1;
        }
        
        Logger::info("=== Memo-RF Voice Agent Started ===");
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
            
            // Check if playback completed while in Transmitting state
            // This must be checked every frame to catch the transition
            if (current_state == State::Transmitting) {
                bool playback_done = audio_io_->is_playback_complete();
                // Log periodically to debug stuck state (every 100 frames = ~6 seconds at 16kHz)
                if (frame_count % 100 == 0) {
                    std::ostringstream oss;
                    oss << "Transmitting state: playback_complete=" << (playback_done ? "true" : "false");
                    LOG_TX(oss.str());
                }
                if (playback_done) {
                    // Playback finished - transition to IdleListening
                    LOG_TX("Playback complete, transitioning to IdleListening");
                    state_machine_->on_playback_complete();
                    transmission_end_time_ = std::chrono::steady_clock::now();
                    vad_->reset(); // Clear any accumulated audio
                    current_state = state_machine_->get_state(); // Update current_state after transition
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
        if (current_state == State::Transmitting || current_state == State::Thinking) {
            // Skip all VAD processing during these states
            return;
        }
        
        // Check guard period when in IdleListening
        // Must wait before re-enabling VAD after transmission
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
        
        // Process VAD only when in IdleListening (past guard period) or ReceivingSpeech
        VADEvent vad_event = VADEvent::None;
        if (current_state == State::IdleListening || 
            current_state == State::ReceivingSpeech) {
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
            handle_speech_end(current_utterance, current_transcript, 
                            current_plan, response_audio, utterance_id);
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
    
    void handle_speech_end(AudioBuffer& current_utterance, Transcript& current_transcript,
                          Plan& current_plan, AudioBuffer& response_audio, int& utterance_id) {
        state_machine_->on_vad_event(VADEvent::SpeechEnd);
        current_utterance = vad_->finalize_segment();
        
        // Check if we have enough speech
        size_t min_samples = (config_.vad.min_speech_ms * config_.audio.sample_rate) / 1000;
        if (current_utterance.size() < min_samples) {
            return;
        }
        
        utterance_id++;
        
        // Record utterance
        recorder_->record_utterance(current_utterance, utterance_id);
        int duration_ms = (current_utterance.size() * 1000) / config_.audio.sample_rate;
        recorder_->record_event("speech_end", "duration_ms=" + std::to_string(duration_ms));
        
        // Transcribe
        auto transcribe_start = std::chrono::steady_clock::now();
        current_transcript = stt_->transcribe(current_utterance);
        auto transcribe_end = std::chrono::steady_clock::now();
        
        int64_t transcribe_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            transcribe_end - transcribe_start).count();
        
        std::ostringstream oss;
        oss << "(" << transcribe_ms << "ms) " << current_transcript.text;
        LOG_STT(oss.str());
        
        recorder_->record_transcript(current_transcript, utterance_id);
        
        // Decide on plan
        LOG_ROUTER(std::string("Deciding on plan for: \"") + current_transcript.text + "\"");
        current_plan = router_->decide(current_transcript.text);
        std::ostringstream plan_oss;
        plan_oss << "Plan type: " << (int)current_plan.type 
                 << ", needs_llm: " << current_plan.needs_llm;
        LOG_ROUTER(plan_oss.str());
        state_machine_->on_transcript_ready(current_transcript);
        
        // Execute plan
        execute_plan(current_plan, current_transcript, response_audio, utterance_id);
    }
    
    void execute_plan(const Plan& plan, const Transcript& transcript,
                     AudioBuffer& response_audio, int utterance_id) {
        if (plan.type == PlanType::NoOp) {
            LOG_ROUTER("NoOp - doing nothing");
            return;
        }
        
        if (plan.type == PlanType::Speak) {
            execute_fast_path(plan, response_audio, utterance_id);
        } else if (plan.type == PlanType::SpeakAckThenAnswer) {
            execute_llm_path(plan, transcript, response_audio, utterance_id);
        } else if (plan.type == PlanType::Fallback) {
            execute_fallback(plan, response_audio, utterance_id);
        }
    }
    
    void execute_fast_path(const Plan& plan, AudioBuffer& response_audio, int utterance_id) {
        LOG_ROUTER(std::string("Fast path - speaking: \"") + plan.answer_text + "\"");
        response_audio = tts_->synth_vox(plan.answer_text);
        recorder_->record_tts_output(response_audio, utterance_id);
        state_machine_->on_response_ready(response_audio);
        // Reset VAD before transmitting to prevent detecting our own voice
        vad_->reset();
        tx_->transmit(response_audio);
        // Guard period will be set when playback completes in process_frame
    }
    
    void execute_llm_path(const Plan& plan, const Transcript& transcript,
                         AudioBuffer& response_audio, int utterance_id) {
        // Skip acknowledgment if ack_text is empty (just beep via VOX pre-roll)
        if (!plan.ack_text.empty()) {
            LOG_ROUTER(std::string("LLM path - acknowledging first: \"") + plan.ack_text + "\"");
            
            // Reset VAD before transmitting ack to prevent detecting our own voice
            vad_->reset();
            
            // Acknowledge first
            AudioBuffer ack_audio = tts_->synth_vox(plan.ack_text);
            tx_->transmit(ack_audio);
            
            // Wait for ack to complete
            LOG_ROUTER("Waiting for ack playback to complete...");
            while (!audio_io_->is_playback_complete() && running_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            LOG_ROUTER("Ack playback complete, calling LLM...");
            
            // Mark ack end time for guard period (prevent detecting ack in VAD)
            transmission_end_time_ = std::chrono::steady_clock::now();
        } else {
            LOG_ROUTER("LLM path - skipping acknowledgment, going straight to response");
        }
        
        // Generate LLM response with tool support
        auto llm_start = std::chrono::steady_clock::now();
        std::string llm_prompt = transcript.text;
        recorder_->record_llm_prompt(llm_prompt, utterance_id);
        
        // Get tool definitions for LLM (only if tools are enabled)
        std::string tool_definitions;
        if (tool_registry_ && tool_registry_->size() > 0) {
            tool_definitions = tool_registry_->get_tool_definitions_json();
        }
        
        LOG_LLM(std::string("Calling LLM with prompt: \"") + llm_prompt + "\"");
        
        // Conversation history for multi-turn with tools
        // Start with user message
        std::vector<std::string> conversation_history;
        json user_msg;
        user_msg["role"] = "user";
        user_msg["content"] = llm_prompt;
        conversation_history.push_back(user_msg.dump());
        
        // Main LLM loop with tool execution (only if tools are enabled)
        std::string llm_response;
        
        if (tool_definitions.empty()) {
            // No tools enabled - use simple direct LLM call
            LLMResponse response = llm_->generate_with_tools(
                llm_prompt,
                "",  // No tool definitions
                {},   // No conversation history
                config_.llm.timeout_ms,
                config_.llm.max_tokens
            );
            llm_response = response.content;
        } else {
            // Tools enabled - use tool execution loop
            std::vector<std::string> conversation_history;
            json user_msg;
            user_msg["role"] = "user";
            user_msg["content"] = llm_prompt;
            conversation_history.push_back(user_msg.dump());
            
            int max_iterations = 5;  // Prevent infinite loops
            int iteration = 0;
            
            while (iteration < max_iterations) {
                iteration++;
                
                LLMResponse response = llm_->generate_with_tools(
                    "",  // Don't send prompt again - it's in conversation_history
                    tool_definitions,
                    conversation_history,
                    config_.llm.timeout_ms,
                    config_.llm.max_tokens
                );
                
                // Add assistant message to conversation
                json assistant_msg;
                assistant_msg["role"] = "assistant";
                if (response.has_content()) {
                    assistant_msg["content"] = response.content;
                }
                if (response.has_tool_calls()) {
                    assistant_msg["tool_calls"] = json::array();
                    for (const auto& tc : response.tool_calls) {
                        json tool_call;
                        tool_call["id"] = tc.id;
                        tool_call["type"] = "function";
                        tool_call["function"]["name"] = tc.name;
                        tool_call["function"]["arguments"] = tc.arguments;
                        assistant_msg["tool_calls"].push_back(tool_call);
                    }
                }
                conversation_history.push_back(assistant_msg.dump());
                
                // If no tool calls, we're done (LLM responded directly)
                if (!response.has_tool_calls()) {
                    if (response.has_content()) {
                        llm_response = response.content;
                    } else {
                        // Empty response, use fallback
                        llm_response = "Stand by.";
                    }
                    break;
                }
                
                // Execute tools
                LOG_LLM("Tool calls detected: " + std::to_string(response.tool_calls.size()));
                std::vector<ToolExecutionResult> tool_results;
                
                for (const auto& tool_call : response.tool_calls) {
                    ToolExecutionRequest call;
                    call.tool_name = tool_call.name;
                    call.tool_call_id = tool_call.id;
                    call.params_json = tool_call.arguments;
                    
                    LOG_LLM("Executing tool: " + call.tool_name);
                    
                    // Execute synchronously for now (can be made async later)
                    auto result = tool_executor_->execute_sync(call, config_.tools.timeout_ms);
                    tool_results.push_back(result);
                    
                    // Add tool result to conversation
                    std::string result_msg = LLMClient::format_tool_result(
                        result.tool_call_id,
                        result.result.success ? result.result.content : ("Error: " + result.result.error)
                    );
                    conversation_history.push_back(result_msg);
                }
                
                // Continue loop to get LLM response with tool results
            }
            
            // If we hit max iterations, use last response or fallback
            if (iteration >= max_iterations && llm_response.empty()) {
                LOG_LLM("Max tool execution iterations reached");
                llm_response = "Stand by.";
            }
        }
        
        auto llm_end = std::chrono::steady_clock::now();
        int64_t llm_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            llm_end - llm_start).count();
        
        std::ostringstream llm_oss;
        llm_oss << "(" << llm_ms << "ms) " << llm_response;
        LOG_LLM(llm_oss.str());
        
        recorder_->record_llm_response(llm_response, utterance_id);
        
        // Check if response is empty or just whitespace
        std::string trimmed_response = utils::trim_copy(llm_response);
        if (trimmed_response.empty() || utils::is_empty_or_whitespace(trimmed_response)) {
            LOG_LLM("Empty response, using fallback");
            llm_response = "Stand by.";
        }
        
        // Synthesize response
        LOG_TTS("Synthesizing response...");
        response_audio = tts_->synth_vox(llm_response);
        recorder_->record_tts_output(response_audio, utterance_id);
        state_machine_->on_response_ready(response_audio);
        
        // Reset VAD before transmitting to prevent detecting our own voice
        vad_->reset();
        
        LOG_TX("Transmitting response (" + std::to_string(response_audio.size()) + " samples)...");
        tx_->transmit(response_audio);
    }
    
    void execute_fallback(const Plan& plan, AudioBuffer& response_audio, int utterance_id) {
        LOG_ROUTER(std::string("Fallback - speaking: \"") + plan.fallback_text + "\"");
        response_audio = tts_->synth_vox(plan.fallback_text);
        recorder_->record_tts_output(response_audio, utterance_id);
        state_machine_->on_response_ready(response_audio);
        
        // Reset VAD before transmitting to prevent detecting our own voice
        vad_->reset();
        
        tx_->transmit(response_audio);
        // Guard period will be set when playback completes in process_frame
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
    
    // Track previous state for transition detection
    State previous_state_;
    
    // Track speech frame count for logging
    int speech_frame_count_;
    
    // Track speech timing for periodic logging
    std::chrono::steady_clock::time_point speech_start_time_;
    std::chrono::steady_clock::time_point last_speech_log_time_;
    
    std::unique_ptr<AudioIO> audio_io_;
    std::unique_ptr<VADEndpointing> vad_;
    std::unique_ptr<STTEngine> stt_;
    std::unique_ptr<Router> router_;
    std::unique_ptr<LLMClient> llm_;
    std::unique_ptr<TTSEngine> tts_;
        std::unique_ptr<TXController> tx_;
        std::unique_ptr<StateMachine> state_machine_;
        std::unique_ptr<SessionRecorder> recorder_;
        
        // Tool system
        std::unique_ptr<ToolRegistry> tool_registry_;
        std::unique_ptr<ToolExecutor> tool_executor_;
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
