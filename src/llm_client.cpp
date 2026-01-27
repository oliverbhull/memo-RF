#include "llm_client.h"
#include "logger.h"
#include <curl/curl.h>
#include <sstream>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace memo_rf {

class LLMClient::Impl {
public:
    Impl(const LLMConfig& config) : config_(config), ready_(true) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }
    
    ~Impl() {
        curl_global_cleanup();
    }
    
    std::string generate(const std::string& prompt, const std::string& context,
                        int timeout_ms, int max_tokens) {
        if (timeout_ms == 0) timeout_ms = config_.timeout_ms;
        if (max_tokens == 0) max_tokens = config_.max_tokens;
        
        // Build prompt with radio style constraints
        std::string full_prompt = build_prompt(prompt, context);
        
        // Prepare JSON request (llama.cpp server format)
        json request;
        request["prompt"] = full_prompt;
        request["n_predict"] = max_tokens;
        request["temperature"] = config_.temperature;
        request["stop"] = json(config_.stop_sequences);
        request["stream"] = false;
        
        std::string request_json = request.dump();
        
        // Make HTTP request with timeout
        std::atomic<bool> request_complete(false);
        std::string response_text;
        std::string error_msg;
        
        std::thread request_thread([&]() {
            LOG_LLM(std::string("Starting HTTP request to: ") + config_.endpoint);
            CURL* curl = curl_easy_init();
            if (!curl) {
                error_msg = "Failed to initialize CURL";
                request_complete = true;
                return;
            }
            
            struct curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, "Content-Type: application/json");
            
            std::string response_buffer;
            
            curl_easy_setopt(curl, CURLOPT_URL, config_.endpoint.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_json.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buffer);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 1000);  // 1 second connect timeout
            
            LOG_LLM(std::string("Sending request: ") + request_json);
            CURLcode res = curl_easy_perform(curl);
            std::ostringstream res_oss;
            res_oss << "curl_easy_perform returned: " << res;
            LOG_LLM(res_oss.str());
            
            if (res != CURLE_OK) {
                error_msg = curl_easy_strerror(res);
            } else {
                // Parse response
                try {
                    json response_json = json::parse(response_buffer);
                    if (response_json.contains("content")) {
                        response_text = response_json["content"].get<std::string>();
                        // Log raw response for debugging
                        std::ostringstream raw_oss;
                        raw_oss << "Raw LLM response: \"" << response_text << "\"";
                        LOG_LLM(raw_oss.str());
                    } else {
                        error_msg = "No content in response";
                        // Log the full response for debugging
                        std::ostringstream debug_oss;
                        debug_oss << "Response JSON (no content field): " << response_buffer;
                        LOG_LLM(debug_oss.str());
                    }
                } catch (const json::exception& e) {
                    error_msg = "JSON parse error: " + std::string(e.what());
                    LOG_LLM(std::string("Response buffer: ") + response_buffer);
                }
            }
            
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            request_complete = true;
        });
        
        // Wait with timeout
        auto start = std::chrono::steady_clock::now();
        int wait_count = 0;
        while (!request_complete) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            wait_count++;
            if (wait_count % 100 == 0) {  // Print every second
                auto elapsed = std::chrono::steady_clock::now() - start;
                int64_t elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
                std::ostringstream wait_oss;
                wait_oss << "Waiting for response... (" << elapsed_ms << "ms / " << timeout_ms << "ms)";
                LOG_LLM(wait_oss.str());
            }
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() > timeout_ms) {
                LOG_LLM(std::string("Timeout reached, detaching thread"));
                request_thread.detach(); // Let it finish in background
                return "Stand by.";
            }
        }
        
        LOG_LLM(std::string("Request complete, joining thread..."));
        request_thread.join();
        
        if (!error_msg.empty()) {
            LOG_LLM(std::string("Error: ") + error_msg);
            // Check if it's a connection error
            if (error_msg.find("Couldn't resolve") != std::string::npos || 
                error_msg.find("Connection refused") != std::string::npos ||
                error_msg.find("Failed to connect") != std::string::npos) {
                return "Server offline. Stand by.";
            }
            return "Error. Stand by.";
        }
        
        // Clean up response (remove newlines, make radio-style)
        std::string cleaned = clean_response(response_text);
        
        // If response is suspiciously short, log details for debugging
        if (cleaned.length() < 10) {
            std::ostringstream warn_oss;
            warn_oss << "Warning: Response very short (" << cleaned.length() 
                     << " chars): \"" << cleaned << "\"";
            warn_oss << " (raw: \"" << response_text << "\")";
            LOG_LLM(warn_oss.str());
            LOG_LLM("This may indicate the model hit a stop sequence too early or max_tokens is too low.");
        }
        
        return cleaned;
    }
    
    bool is_ready() const {
        return ready_;
    }

private:
    std::string build_prompt(const std::string& prompt, const std::string& context) {
        std::ostringstream oss;
        
        // Simpler, less restrictive prompt for GPT-OSS
        // GPT-OSS works better with less aggressive constraints
        oss << "You are a radio operator. Give brief, direct answers. ";
        oss << "Keep responses concise (1-2 sentences, under 20 words). ";
        if (!context.empty()) {
            oss << "Context: " << context << " ";
        }
        oss << "User: " << prompt << "\nAssistant:";
        return oss.str();
    }
    
    std::string clean_response(const std::string& response) {
        std::string cleaned = response;
        
        // Remove leading/trailing whitespace
        while (!cleaned.empty() && std::isspace(cleaned[0])) {
            cleaned.erase(0, 1);
        }
        while (!cleaned.empty() && std::isspace(cleaned.back())) {
            cleaned.pop_back();
        }
        
        // Remove common repetitive patterns
        std::vector<std::string> patterns_to_remove = {
            "[end conversation]",
            "[pause]",
            "[end]",
            "Remember,",
            "Keep it",
            "Let's keep",
            "we're all in this together",
            "Keep it smooth",
            "Keep it clear",
            "Keep it going"
        };
        
        for (const auto& pattern : patterns_to_remove) {
            size_t pos = 0;
            while ((pos = cleaned.find(pattern, pos)) != std::string::npos) {
                // Remove the pattern and any following punctuation/whitespace
                cleaned.erase(pos, pattern.length());
                // Remove trailing punctuation/whitespace after pattern
                while (pos < cleaned.length() && 
                       (std::isspace(cleaned[pos]) || cleaned[pos] == '.' || 
                        cleaned[pos] == '!' || cleaned[pos] == '?')) {
                    cleaned.erase(pos, 1);
                }
            }
        }
        
        // Replace newlines with spaces
        std::replace(cleaned.begin(), cleaned.end(), '\n', ' ');
        
        // Collapse multiple spaces
        std::string result;
        bool last_was_space = false;
        for (char c : cleaned) {
            if (std::isspace(c)) {
                if (!last_was_space) {
                    result += ' ';
                    last_was_space = true;
                }
            } else {
                result += c;
                last_was_space = false;
            }
        }
        
        // Truncate to first sentence or first 75 words, whichever comes first
        // (increased from 50 to allow more complete answers)
        result = truncate_to_first_sentence(result);
        result = truncate_to_max_words(result, 75);
        
        return result;
    }
    
    static std::string truncate_to_first_sentence(const std::string& text) {
        // Find first sentence-ending punctuation
        size_t period = text.find('.');
        size_t exclamation = text.find('!');
        size_t question = text.find('?');
        
        size_t first_end = std::string::npos;
        if (period != std::string::npos) {
            first_end = period;
        }
        if (exclamation != std::string::npos && 
            (first_end == std::string::npos || exclamation < first_end)) {
            first_end = exclamation;
        }
        if (question != std::string::npos && 
            (first_end == std::string::npos || question < first_end)) {
            first_end = question;
        }
        
        if (first_end != std::string::npos) {
            return text.substr(0, first_end + 1);
        }
        return text;
    }
    
    static std::string truncate_to_max_words(const std::string& text, int max_words) {
        std::istringstream iss(text);
        std::vector<std::string> words;
        std::string word;
        
        while (iss >> word && words.size() < static_cast<size_t>(max_words)) {
            words.push_back(word);
        }
        
        if (words.size() >= static_cast<size_t>(max_words)) {
            std::ostringstream oss;
            for (size_t i = 0; i < words.size(); ++i) {
                if (i > 0) oss << " ";
                oss << words[i];
            }
            return oss.str();
        }
        return text;
    }
    
    static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
        std::string* buffer = static_cast<std::string*>(userp);
        size_t total_size = size * nmemb;
        buffer->append(static_cast<char*>(contents), total_size);
        return total_size;
    }
    
    LLMConfig config_;
    bool ready_;
};

LLMClient::LLMClient(const LLMConfig& config) 
    : pimpl_(std::make_unique<Impl>(config)) {}

LLMClient::~LLMClient() = default;

std::string LLMClient::generate(const std::string& prompt, const std::string& context,
                                int timeout_ms, int max_tokens) {
    return pimpl_->generate(prompt, context, timeout_ms, max_tokens);
}

bool LLMClient::is_ready() const {
    return pimpl_->is_ready();
}

} // namespace memo_rf
