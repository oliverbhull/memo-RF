#pragma once

#include "common.h"
#include "config.h"
#include "router.h"
#include <chrono>
#include <atomic>
#include <memory>

namespace memo_rf {

class AudioIO;
class VADEndpointing;
class STTEngine;
class LLMClient;
class TTSEngine;
class TXController;
class StateMachine;
class SessionRecorder;

/**
 * Pipeline for speech-end processing: STT → gate → router → execute plan (fast path / LLM / fallback).
 * VoiceAgent owns components and delegates to AgentPipeline on SpeechEnd.
 */
class AgentPipeline {
public:
    AgentPipeline(
        const Config* config,
        AudioIO* audio_io,
        VADEndpointing* vad,
        STTEngine* stt,
        Router* router,
        LLMClient* llm,
        TTSEngine* tts,
        TXController* tx,
        StateMachine* state_machine,
        SessionRecorder* recorder,
        std::atomic<bool>* running,
        std::chrono::steady_clock::time_point* transmission_end_time);

    void handle_speech_end(
        AudioBuffer& current_utterance,
        Transcript& current_transcript,
        Plan& current_plan,
        AudioBuffer& response_audio,
        int& utterance_id,
        std::chrono::steady_clock::time_point& last_speech_end_time,
        AudioBuffer& pending_response_audio);

    void handle_blank_behavior();

private:
    void execute_plan(const Plan& plan, const Transcript& transcript,
                     AudioBuffer& response_audio, int utterance_id, bool wait_for_channel_clear,
                     AudioBuffer& pending_response_audio);
    void execute_fast_path(const Plan& plan, AudioBuffer& response_audio, int utterance_id,
                           bool wait_for_channel_clear);
    void execute_llm_path(const Plan& plan, const Transcript& transcript,
                          AudioBuffer& response_audio, int utterance_id,
                          bool wait_for_channel_clear);
    void execute_fallback(const Plan& plan, AudioBuffer& response_audio, int utterance_id,
                          bool wait_for_channel_clear);

    /// Check if transcript is a persona change command ("Memo change persona to X")
    /// If so, load persona and update runtime state. Returns true if handled.
    bool check_and_handle_persona_change(const std::string& transcript, AudioBuffer& response_audio, int utterance_id);

    /// Load persona from personas.json by ID
    bool load_persona(const std::string& persona_id, std::string& out_system_prompt, std::string& out_persona_name);

    const Config* config_;
    AudioIO* audio_io_;
    VADEndpointing* vad_;
    STTEngine* stt_;
    Router* router_;
    LLMClient* llm_;
    TTSEngine* tts_;
    TXController* tx_;
    StateMachine* state_machine_;
    SessionRecorder* recorder_;
    std::atomic<bool>* running_;
    std::chrono::steady_clock::time_point* transmission_end_time_;

    /// Runtime persona state (overrides config when set dynamically)
    std::string current_persona_;        // Current persona ID (e.g. "manufacturing")
    std::string current_system_prompt_;  // Current system prompt (overrides config)
    std::string current_persona_name_;   // Current persona display name
    std::string target_language_;        // Translation target language (for translator persona)
};

} // namespace memo_rf
