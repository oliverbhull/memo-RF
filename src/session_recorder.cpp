#include "session_recorder.h"
#include "common.h"
#include "logger.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <filesystem>
#include <algorithm>
#include <chrono>
#include <map>
#include <curl/curl.h>
#include <thread>

namespace memo_rf {

class SessionRecorder::Impl {
public:
    Impl(const std::string& session_dir, const std::string& feed_server_url)
        : session_dir_(session_dir), session_id_(), feed_server_url_(feed_server_url),
          session_started_(false), utterance_counter_(0), raw_audio_buffer_() {
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
        metadata_.clear();
        session_start_time_ = std::chrono::steady_clock::now();

        // Create session directory
        std::string session_path = session_dir_ + "/" + session_id_;
        std::filesystem::create_directories(session_path);
    }

    void set_session_metadata(const std::string& key, const std::string& value) {
        if (!session_started_) return;
        metadata_[key] = value;
        write_session_log_incremental();
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
        write_session_log_incremental();

        // Notify feed server (non-blocking)
        notify_feed_server("transcript", transcript.text);
    }
    
    void record_llm_prompt(const std::string& prompt, int utterance_id) {
        if (!session_started_) return;

        SessionEvent event;
        event.timestamp_ms = ms_since(session_start_time_);
        event.event_type = "llm_prompt";
        event.data = prompt;
        events_.push_back(event);
        write_session_log_incremental();
    }
    
    void record_llm_response(const std::string& response, int utterance_id) {
        if (!session_started_) return;

        SessionEvent event;
        event.timestamp_ms = ms_since(session_start_time_);
        event.event_type = "llm_response";
        event.data = response;
        events_.push_back(event);
        write_session_log_incremental();

        // Notify feed server (non-blocking)
        notify_feed_server("llm_response", response);
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
        write_session_log_incremental();
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

    // Notify feed server of new event (fire-and-forget, non-blocking)
    void notify_feed_server(const std::string& event_type, const std::string& data) {
        if (feed_server_url_.empty()) return;  // Not configured

        // Escape JSON before lambda
        std::string escaped_data = escape_json(data);

        // Launch in detached thread to avoid blocking
        std::thread([url = feed_server_url_, sid = session_id_, etype = event_type, d = escaped_data, ts = ms_since(session_start_time_), meta = metadata_]() {
            CURL* curl = curl_easy_init();
            if (!curl) return;

            // Build JSON payload
            std::string persona = meta.count("persona_name") ? meta.at("persona_name") :
                                 (meta.count("persona") ? meta.at("persona") : "");
            std::string language = meta.count("response_language") ? meta.at("response_language") : "en";

            std::ostringstream json;
            json << "{"
                 << "\"session_id\":\"" << sid << "\","
                 << "\"timestamp_ms\":" << ts << ","
                 << "\"event_type\":\"" << etype << "\","
                 << "\"data\":\"" << d << "\","
                 << "\"persona_name\":\"" << persona << "\","
                 << "\"language\":\"" << language << "\""
                 << "}";

            std::string payload = json.str();

            // Set curl options
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
            curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 500L);  // 500ms timeout
            curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

            struct curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, "Content-Type: application/json");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

            // Perform request (ignore errors - fire-and-forget)
            curl_easy_perform(curl);

            // Cleanup
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
        }).detach();
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

        // Write metadata
        file << "  \"metadata\": {\n";
        size_t meta_idx = 0;
        for (const auto& [key, value] : metadata_) {
            file << "    \"" << key << "\": \"" << escape_json(value) << "\"";
            if (meta_idx < metadata_.size() - 1) file << ",";
            file << "\n";
            meta_idx++;
        }
        file << "  },\n";

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

    void write_session_log_incremental() {
        if (!session_started_) return;
        std::string log_filename = get_session_path() + "/session_log.json";
        write_session_log(log_filename);
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
    std::string feed_server_url_;
    bool session_started_;
    int utterance_counter_;
    AudioBuffer raw_audio_buffer_;
    std::vector<SessionEvent> events_;
    std::map<std::string, std::string> metadata_;
    TimePoint session_start_time_;
};

SessionRecorder::SessionRecorder(const std::string& session_dir, const std::string& feed_server_url)
    : pimpl_(std::make_unique<Impl>(session_dir, feed_server_url)) {}

SessionRecorder::~SessionRecorder() = default;

void SessionRecorder::start_session() {
    pimpl_->start_session();
}

void SessionRecorder::set_session_metadata(const std::string& key, const std::string& value) {
    pimpl_->set_session_metadata(key, value);
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
