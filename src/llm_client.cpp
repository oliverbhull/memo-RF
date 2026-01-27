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
        is_ollama_ = config.endpoint.find("/api/chat") != std::string::npos;
    }
    
    ~Impl() {
        curl_global_cleanup();
    }
    
    LLMResponse generate_with_tools(const std::string& prompt,
                                   const std::string& tool_definitions_json,
                                   const std::vector<std::string>& conversation_history,
                                   int timeout_ms, int max_tokens) {
        if (timeout_ms == 0) timeout_ms = config_.timeout_ms;
        if (max_tokens == 0) max_tokens = config_.max_tokens;
        
        LLMResponse response;
        
        if (is_ollama_) {
            return generate_ollama_chat(prompt, tool_definitions_json, conversation_history, 
                                       timeout_ms, max_tokens);
        } else {
            // Fallback to legacy format
            response.content = generate(prompt, "", timeout_ms, max_tokens);
            return response;
        }
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
    LLMResponse generate_ollama_chat(const std::string& prompt,
                                    const std::string& tool_definitions_json,
                                    const std::vector<std::string>& conversation_history,
                                    int timeout_ms, int max_tokens) {
        LLMResponse response;
        
        // Build messages array
        json messages = json::array();
        
        // Add system message first (always, it should be first)
        json system_msg;
        system_msg["role"] = "system";
        system_msg["content"] = "You are a radio operator. Give brief, direct answers. "
                                "Keep responses concise (1-2 sentences, under 20 words).";
        messages.push_back(system_msg);
        
        // Add conversation history (which should start with user message, then assistant/tool messages)
        for (const auto& msg : conversation_history) {
            try {
                json msg_json = json::parse(msg);
                // Skip system messages from history (we add it above)
                if (msg_json.contains("role") && msg_json["role"] == "system") {
                    continue;
                }
                messages.push_back(msg_json);
            } catch (const json::exception& e) {
                Logger::warn("Failed to parse conversation history message: " + std::string(e.what()));
            }
        }
        
        // Add user message only if prompt is provided and not already in history
        // (This handles the case where conversation_history is empty)
        if (!prompt.empty()) {
            // Check if last message in history is already a user message with this content
            bool already_has_user = false;
            if (!conversation_history.empty()) {
                try {
                    json last_msg = json::parse(conversation_history.back());
                    if (last_msg.contains("role") && last_msg["role"] == "user" && 
                        last_msg.contains("content") && last_msg["content"] == prompt) {
                        already_has_user = true;
                    }
                } catch (...) {
                    // Ignore parse errors
                }
            }
            if (!already_has_user) {
                json user_msg;
                user_msg["role"] = "user";
                user_msg["content"] = prompt;
                messages.push_back(user_msg);
            }
        }
        
        // Build request
        json request;
        request["model"] = config_.model_name;
        request["messages"] = messages;
        request["temperature"] = config_.temperature;
        request["stream"] = false;
        
        // Add tools if provided
        if (!tool_definitions_json.empty()) {
            try {
                json tools = json::parse(tool_definitions_json);
                request["tools"] = tools;
                request["tool_choice"] = "auto";
            } catch (const json::exception& e) {
                Logger::warn("Failed to parse tool definitions: " + std::string(e.what()));
            }
        }
        
        std::string request_json = request.dump();
        
        // Make HTTP request
        std::atomic<bool> request_complete(false);
        std::string response_buffer;
        std::string error_msg;
        
        std::thread request_thread([&]() {
            LOG_LLM(std::string("Starting Ollama chat request to: ") + config_.endpoint);
            CURL* curl = curl_easy_init();
            if (!curl) {
                error_msg = "Failed to initialize CURL";
                request_complete = true;
                return;
            }
            
            struct curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, "Content-Type: application/json");
            
            curl_easy_setopt(curl, CURLOPT_URL, config_.endpoint.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_json.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buffer);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 1000);
            
            LOG_LLM(std::string("Sending Ollama request: ") + request_json);
            CURLcode res = curl_easy_perform(curl);
            
            if (res != CURLE_OK) {
                error_msg = curl_easy_strerror(res);
            } else {
                try {
                    json response_json = json::parse(response_buffer);
                    
                    // Debug: log full response for troubleshooting
                    LOG_LLM(std::string("Ollama response: ") + response_buffer);
                    
                    // Parse response
                    if (response_json.contains("message")) {
                        json message = response_json["message"];
                        
                        // Get content
                        if (message.contains("content") && !message["content"].is_null()) {
                            response.content = message["content"].get<std::string>();
                        }
                        
                        // Parse tool calls
                        if (message.contains("tool_calls") && message["tool_calls"].is_array()) {
                            static int tool_call_counter = 0;  // Generate IDs if Ollama doesn't provide them
                            for (const auto& tool_call : message["tool_calls"]) {
                                ToolCall tc;
                                if (tool_call.contains("id") && !tool_call["id"].is_null()) {
                                    std::string id_str = tool_call["id"].get<std::string>();
                                    if (!id_str.empty()) {
                                        tc.id = id_str;
                                    } else {
                                        // Generate ID if empty
                                        tc.id = "call_" + std::to_string(++tool_call_counter);
                                    }
                                } else {
                                    // Generate ID if missing
                                    tc.id = "call_" + std::to_string(++tool_call_counter);
                                }
                                if (tool_call.contains("function")) {
                                    json func = tool_call["function"];
                                    if (func.contains("name")) {
                                        tc.name = func["name"].get<std::string>();
                                    }
                                    if (func.contains("arguments")) {
                                        // Ollama returns arguments as object, convert to JSON string
                                        if (func["arguments"].is_string()) {
                                            tc.arguments = func["arguments"].get<std::string>();
                                        } else if (func["arguments"].is_object() || func["arguments"].is_array()) {
                                            tc.arguments = func["arguments"].dump();
                                        } else {
                                            // Fallback: convert to string
                                            tc.arguments = func["arguments"].dump();
                                        }
                                    }
                                }
                                if (!tc.name.empty()) {
                                    response.tool_calls.push_back(tc);
                                }
                            }
                        }
                    } else {
                        error_msg = "No message in Ollama response";
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
        while (!request_complete) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() > timeout_ms) {
                LOG_LLM("Ollama request timeout");
                request_thread.detach();
                response.content = "Stand by.";
                return response;
            }
        }
        
        request_thread.join();
        
        if (!error_msg.empty()) {
            LOG_LLM(std::string("Ollama error: ") + error_msg);
            response.content = "Error. Stand by.";
            return response;
        }
        
        // Clean content if present
        if (!response.content.empty()) {
            response.content = clean_response(response.content);
        }
        
        return response;
    }
    
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
        
        // For Ollama responses, don't truncate to first sentence (they're already concise)
        // Just limit to max words to prevent extremely long responses
        result = truncate_to_max_words(result, 100);  // Increased from 75
        
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
    bool is_ollama_;
};

LLMClient::LLMClient(const LLMConfig& config) 
    : pimpl_(std::make_unique<Impl>(config)) {}

LLMClient::~LLMClient() = default;

LLMResponse LLMClient::generate_with_tools(const std::string& prompt,
                                          const std::string& tool_definitions_json,
                                          const std::vector<std::string>& conversation_history,
                                          int timeout_ms, int max_tokens) {
    return pimpl_->generate_with_tools(prompt, tool_definitions_json, conversation_history,
                                      timeout_ms, max_tokens);
}

std::string LLMClient::generate(const std::string& prompt, const std::string& context,
                                int timeout_ms, int max_tokens) {
    return pimpl_->generate(prompt, context, timeout_ms, max_tokens);
}

std::string LLMClient::format_tool_result(const std::string& tool_call_id, 
                                         const std::string& result_content) {
    json result_msg;
    result_msg["role"] = "tool";
    result_msg["tool_call_id"] = tool_call_id;
    result_msg["content"] = result_content;
    return result_msg.dump();
}

bool LLMClient::is_ready() const {
    return pimpl_->is_ready();
}

} // namespace memo_rf
