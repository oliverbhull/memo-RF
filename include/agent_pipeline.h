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
};

} // namespace memo_rf
