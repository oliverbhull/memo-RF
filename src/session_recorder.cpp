#include "session_recorder.h"
#include "common.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <filesystem>
#include <algorithm>
#include <chrono>

namespace memo_rf {

class SessionRecorder::Impl {
public:
    Impl(const std::string& session_dir) 
        : session_dir_(session_dir), session_id_(), session_started_(false),
          utterance_counter_(0), raw_audio_buffer_() {
        std::filesystem::create_directories(session_dir_);
    }
    
    void start_session() {
        // Generate session ID from timestamp
        auto now = std::time(nullptr);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&now), "%Y%m%d_%H%M%S");
        session_id_ = ss.str();
        session_started_ = true;
        utterance_counter_ = 0;
        events_.clear();
        raw_audio_buffer_.clear();
        session_start_time_ = std::chrono::steady_clock::now();
        
        // Create session directory
        std::string session_path = session_dir_ + "/" + session_id_;
        std::filesystem::create_directories(session_path);
    }
    
    void record_input_frame(const AudioFrame& frame) {
        if (!session_started_) return;
        raw_audio_buffer_.insert(raw_audio_buffer_.end(), frame.begin(), frame.end());
    }
    
    void record_utterance(const AudioBuffer& audio, int utterance_id) {
        if (!session_started_) return;
        
        std::string filename = get_session_path() + "/utterance_" + 
                              std::to_string(utterance_id) + ".wav";
        write_wav(filename, audio);
    }
    
    void record_transcript(const Transcript& transcript, int utterance_id) {
        if (!session_started_) return;
        
        SessionEvent event;
        event.timestamp_ms = ms_since(session_start_time_);
        event.event_type = "transcript";
        event.data = transcript.text;
        events_.push_back(event);
    }
    
    void record_llm_prompt(const std::string& prompt, int utterance_id) {
        if (!session_started_) return;
        
        SessionEvent event;
        event.timestamp_ms = ms_since(session_start_time_);
        event.event_type = "llm_prompt";
        event.data = prompt;
        events_.push_back(event);
    }
    
    void record_llm_response(const std::string& response, int utterance_id) {
        if (!session_started_) return;
        
        SessionEvent event;
        event.timestamp_ms = ms_since(session_start_time_);
        event.event_type = "llm_response";
        event.data = response;
        events_.push_back(event);
    }
    
    void record_tts_output(const AudioBuffer& audio, int utterance_id) {
        if (!session_started_) return;
        
        std::string filename = get_session_path() + "/tts_" + 
                              std::to_string(utterance_id) + ".wav";
        write_wav(filename, audio);
    }
    
    void record_event(const std::string& event_type, const std::string& data) {
        if (!session_started_) return;
        
        SessionEvent event;
        event.timestamp_ms = ms_since(session_start_time_);
        event.event_type = event_type;
        event.data = data;
        events_.push_back(event);
    }
    
    void finalize_session() {
        if (!session_started_) return;
        
        // Write raw input audio
        std::string raw_filename = get_session_path() + "/raw_input.wav";
        write_wav(raw_filename, raw_audio_buffer_);
        
        // Write session log (JSON)
        std::string log_filename = get_session_path() + "/session_log.json";
        write_session_log(log_filename);
        
        session_started_ = false;
    }
    
    std::string get_session_id() const {
        return session_id_;
    }

private:
    std::string get_session_path() const {
        return session_dir_ + "/" + session_id_;
    }
    
    void write_wav(const std::string& path, const AudioBuffer& audio) {
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) return;
        
        // Write WAV header
        int sample_rate = DEFAULT_SAMPLE_RATE;
        int num_channels = 1;
        int bits_per_sample = 16;
        int data_size = audio.size() * sizeof(Sample);
        int file_size = 36 + data_size;
        
        // RIFF header
        file.write("RIFF", 4);
        file.write(reinterpret_cast<const char*>(&file_size), 4);
        file.write("WAVE", 4);
        
        // fmt chunk
        file.write("fmt ", 4);
        int fmt_size = 16;
        file.write(reinterpret_cast<const char*>(&fmt_size), 4);
        short audio_format = 1; // PCM
        file.write(reinterpret_cast<const char*>(&audio_format), 2);
        file.write(reinterpret_cast<const char*>(&num_channels), 2);
        file.write(reinterpret_cast<const char*>(&sample_rate), 4);
        int byte_rate = sample_rate * num_channels * bits_per_sample / 8;
        file.write(reinterpret_cast<const char*>(&byte_rate), 4);
        short block_align = num_channels * bits_per_sample / 8;
        file.write(reinterpret_cast<const char*>(&block_align), 2);
        file.write(reinterpret_cast<const char*>(&bits_per_sample), 2);
        
        // data chunk
        file.write("data", 4);
        file.write(reinterpret_cast<const char*>(&data_size), 4);
        file.write(reinterpret_cast<const char*>(audio.data()), data_size);
    }
    
    void write_session_log(const std::string& path) {
        std::ofstream file(path);
        if (!file.is_open()) return;
        
        file << "{\n";
        file << "  \"session_id\": \"" << session_id_ << "\",\n";
        file << "  \"events\": [\n";
        
        for (size_t i = 0; i < events_.size(); i++) {
            file << "    {\n";
            file << "      \"timestamp_ms\": " << events_[i].timestamp_ms << ",\n";
            file << "      \"event_type\": \"" << events_[i].event_type << "\",\n";
            file << "      \"data\": \"" << escape_json(events_[i].data) << "\"";
            if (!events_[i].audio_path.empty()) {
                file << ",\n      \"audio_path\": \"" << events_[i].audio_path << "\"";
            }
            file << "\n    }";
            if (i < events_.size() - 1) file << ",";
            file << "\n";
        }
        
        file << "  ]\n";
        file << "}\n";
    }
    
    std::string escape_json(const std::string& str) {
        std::string escaped;
        for (char c : str) {
            if (c == '"') escaped += "\\\"";
            else if (c == '\\') escaped += "\\\\";
            else if (c == '\n') escaped += "\\n";
            else if (c == '\r') escaped += "\\r";
            else if (c == '\t') escaped += "\\t";
            else escaped += c;
        }
        return escaped;
    }
    
    std::string session_dir_;
    std::string session_id_;
    bool session_started_;
    int utterance_counter_;
    AudioBuffer raw_audio_buffer_;
    std::vector<SessionEvent> events_;
    TimePoint session_start_time_;
};

SessionRecorder::SessionRecorder(const std::string& session_dir) 
    : pimpl_(std::make_unique<Impl>(session_dir)) {}

SessionRecorder::~SessionRecorder() = default;

void SessionRecorder::start_session() {
    pimpl_->start_session();
}

void SessionRecorder::record_input_frame(const AudioFrame& frame) {
    pimpl_->record_input_frame(frame);
}

void SessionRecorder::record_utterance(const AudioBuffer& audio, int utterance_id) {
    pimpl_->record_utterance(audio, utterance_id);
}

void SessionRecorder::record_transcript(const Transcript& transcript, int utterance_id) {
    pimpl_->record_transcript(transcript, utterance_id);
}

void SessionRecorder::record_llm_prompt(const std::string& prompt, int utterance_id) {
    pimpl_->record_llm_prompt(prompt, utterance_id);
}

void SessionRecorder::record_llm_response(const std::string& response, int utterance_id) {
    pimpl_->record_llm_response(response, utterance_id);
}

void SessionRecorder::record_tts_output(const AudioBuffer& audio, int utterance_id) {
    pimpl_->record_tts_output(audio, utterance_id);
}

void SessionRecorder::record_event(const std::string& event_type, const std::string& data) {
    pimpl_->record_event(event_type, data);
}

void SessionRecorder::finalize_session() {
    pimpl_->finalize_session();
}

std::string SessionRecorder::get_session_id() const {
    return pimpl_->get_session_id();
}

} // namespace memo_rf
