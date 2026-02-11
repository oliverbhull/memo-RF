#pragma once

#include "common.h"
#include <string>
#include <memory>
#include <vector>

namespace memo_rf {

struct SessionEvent {
    int64_t timestamp_ms;
    std::string event_type;  // "speech_start", "speech_end", "transcript", "llm_prompt", "llm_response", "tts", "transmit"
    std::string data;
    std::string audio_path;  // Path to wav file if applicable
};

class SessionRecorder {
public:
    SessionRecorder(const std::string& session_dir, const std::string& feed_server_url = "");
    ~SessionRecorder();
    
    // Start new session
    void start_session();

    // Set session metadata (persona, language, etc.)
    void set_session_metadata(const std::string& key, const std::string& value);

    // Record raw input audio frame
    void record_input_frame(const AudioFrame& frame);
    
    // Record segmented utterance
    void record_utterance(const AudioBuffer& audio, int utterance_id);
    
    // Record transcript
    void record_transcript(const Transcript& transcript, int utterance_id);
    
    // Record LLM interaction
    void record_llm_prompt(const std::string& prompt, int utterance_id);
    void record_llm_response(const std::string& response, int utterance_id);
    
    // Record TTS output
    void record_tts_output(const AudioBuffer& audio, int utterance_id);
    
    // Record event
    void record_event(const std::string& event_type, const std::string& data);
    
    // Finalize session and write log
    void finalize_session();
    
    // Get current session ID
    std::string get_session_id() const;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace memo_rf
