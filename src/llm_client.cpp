#include "llm_client.h"
#include "logger.h"
#include <curl/curl.h>
#include <sstream>
#include <thread>
#include <chrono>
#include <atomic>
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
                    } else {
                        error_msg = "No content in response";
                    }
                } catch (const json::exception& e) {
                    error_msg = "JSON parse error: " + std::string(e.what());
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
        return clean_response(response_text);
    }
    
    bool is_ready() const {
        return ready_;
    }

private:
    std::string build_prompt(const std::string& prompt, const std::string& context) {
        std::ostringstream oss;
        oss << "You are a radio operator. Keep responses short, direct, and under 8 seconds when spoken. ";
        oss << "No pleasantries, just the answer. ";
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
        
        return result;
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
