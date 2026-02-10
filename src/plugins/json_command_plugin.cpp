#include "plugins/json_command_plugin.h"
#include "logger.h"
#include <fstream>
#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>
#include <curl/curl.h>

using json = nlohmann::json;

namespace memo_rf {

// Spoken number words → numeric values
static const std::map<std::string, double> SPOKEN_NUMBERS = {
    {"zero", 0}, {"one", 1}, {"two", 2}, {"three", 3}, {"four", 4},
    {"five", 5}, {"six", 6}, {"seven", 7}, {"eight", 8}, {"nine", 9},
    {"ten", 10}, {"eleven", 11}, {"twelve", 12}, {"thirteen", 13},
    {"fourteen", 14}, {"fifteen", 15}, {"sixteen", 16}, {"seventeen", 17},
    {"eighteen", 18}, {"nineteen", 19}, {"twenty", 20}, {"thirty", 30},
    {"forty", 40}, {"fifty", 50}, {"sixty", 60}, {"seventy", 70},
    {"eighty", 80}, {"ninety", 90}, {"hundred", 100},
    // Negative prefix handled separately
};

static size_t curl_write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    std::string* buffer = static_cast<std::string*>(userp);
    buffer->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

static std::string to_lower(const std::string& s) {
    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return lower;
}

JsonCommandPlugin::ExtractType JsonCommandPlugin::parse_extract_type(const std::string& s) {
    if (s == "first_number") return ExtractType::FirstNumber;
    if (s == "second_number") return ExtractType::SecondNumber;
    if (s == "keyword_after_phrase") return ExtractType::KeywordAfterPhrase;
    Logger::warn("[JsonCommandPlugin] Unknown extract type: " + s + ", defaulting to first_number");
    return ExtractType::FirstNumber;
}

JsonCommandPlugin::JsonCommandPlugin(const std::string& config_path) {
    std::ifstream file(config_path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open plugin config: " + config_path);
    }

    json config;
    try {
        file >> config;
    } catch (const json::exception& e) {
        throw std::runtime_error("Failed to parse " + config_path + ": " + std::string(e.what()));
    }

    // Plugin metadata
    name_ = config.value("plugin", "unknown");
    priority_ = config.value("priority", 50);

    // API config
    if (config.contains("api") && config["api"].is_object()) {
        auto& api = config["api"];
        api_.base_url = api.value("base_url", "");
        api_.api_key = api.value("api_key", "");
        api_.default_rover_id = api.value("default_rover_id", "");
    }

    // Vocab words (explicit list)
    if (config.contains("vocab") && config["vocab"].is_array()) {
        for (const auto& word : config["vocab"]) {
            if (word.is_string()) {
                vocab_words_.push_back(word.get<std::string>());
            }
        }
    }

    // Load commands
    if (config.contains("commands") && config["commands"].is_array()) {
        for (const auto& cmd_json : config["commands"]) {
            CommandDef cmd;
            cmd.id = cmd_json.value("id", "");
            cmd.priority = cmd_json.value("priority", 100);

            // Phrases
            if (cmd_json.contains("phrases") && cmd_json["phrases"].is_array()) {
                for (const auto& phrase : cmd_json["phrases"]) {
                    if (phrase.is_string()) {
                        std::string p = to_lower(phrase.get<std::string>());
                        cmd.phrases.push_back(p);
                        // Add phrases to vocab for STT boosting
                        vocab_words_.push_back(phrase.get<std::string>());
                    }
                }
            }

            // Parameters
            if (cmd_json.contains("params") && cmd_json["params"].is_array()) {
                for (const auto& param_json : cmd_json["params"]) {
                    ParamDef param;
                    param.name = param_json.value("name", "");
                    param.type = param_json.value("type", "string");
                    param.extract = parse_extract_type(param_json.value("extract", "first_number"));

                    // Enum values
                    if (param.type == "enum" && param_json.contains("values") && param_json["values"].is_object()) {
                        for (auto& [canonical, variants] : param_json["values"].items()) {
                            std::vector<std::string> spoken_variants;
                            if (variants.is_array()) {
                                for (const auto& v : variants) {
                                    if (v.is_string()) {
                                        std::string variant = v.get<std::string>();
                                        spoken_variants.push_back(to_lower(variant));
                                        // Add enum variants to vocab
                                        vocab_words_.push_back(variant);
                                    }
                                }
                            }
                            param.enum_values.mapping[canonical] = std::move(spoken_variants);
                        }
                    }

                    cmd.params.push_back(std::move(param));
                }
            }

            // API config
            cmd.api_endpoint = cmd_json.value("api_endpoint", "");
            cmd.api_method = cmd_json.value("api_method", "POST");
            if (cmd_json.contains("api_body")) {
                cmd.api_body = cmd_json["api_body"];
            }
            cmd.confirm_text = cmd_json.value("confirm_text", "Command sent.");

            commands_.push_back(std::move(cmd));
        }
    }

    // Sort commands by priority
    std::sort(commands_.begin(), commands_.end(),
        [](const CommandDef& a, const CommandDef& b) { return a.priority < b.priority; });

    // Deduplicate vocab
    std::sort(vocab_words_.begin(), vocab_words_.end());
    vocab_words_.erase(std::unique(vocab_words_.begin(), vocab_words_.end()), vocab_words_.end());

    Logger::info("[JsonCommandPlugin] Loaded \"" + name_ + "\" with "
        + std::to_string(commands_.size()) + " commands, "
        + std::to_string(vocab_words_.size()) + " vocab words");
}

bool JsonCommandPlugin::try_handle(const std::string& transcript, ActionResult& result) {
    std::string lower = to_lower(transcript);

    for (const auto& cmd : commands_) {
        std::map<std::string, std::string> extracted_params;
        std::string matched_phrase;

        if (!try_match_command(lower, cmd, extracted_params, matched_phrase)) {
            continue;
        }

        Logger::info("[JsonCommandPlugin] Matched command \"" + cmd.id
            + "\" via phrase \"" + matched_phrase + "\"");

        // Add default params (e.g. rover_id)
        if (!api_.default_rover_id.empty() && extracted_params.find("rover_id") == extracted_params.end()) {
            extracted_params["rover_id"] = api_.default_rover_id;
        }

        // Build API URL
        std::string url = api_.base_url + substitute(cmd.api_endpoint, extracted_params);

        // Build API body
        std::string body;
        if (!cmd.api_body.is_null()) {
            json substituted_body = substitute_json(cmd.api_body, extracted_params);
            body = substituted_body.dump();
        }

        // Make HTTP call
        int status_code = 0;
        std::string response_body;
        bool http_ok = send_http(cmd.api_method, url, body, api_.api_key, status_code, response_body);

        if (http_ok && status_code >= 200 && status_code < 300) {
            result.success = true;
            result.response_text = substitute(cmd.confirm_text, extracted_params);
            Logger::info("[JsonCommandPlugin] Command \"" + cmd.id + "\" succeeded (HTTP " + std::to_string(status_code) + ")");
        } else {
            result.success = false;
            result.response_text = std::string("Command failed. ") + (cmd.id == "estop" ? "Emergency stop may not have been received." : "Robot may be offline.");
            result.error = http_ok
                ? "HTTP " + std::to_string(status_code) + ": " + response_body
                : "Connection failed";
            Logger::warn("[JsonCommandPlugin] Command \"" + cmd.id + "\" failed: " + result.error);
        }

        return true;  // We matched — whether it succeeded or failed, we handled it
    }

    return false;  // No command matched
}

bool JsonCommandPlugin::try_match_command(const std::string& lower_transcript,
                                           const CommandDef& cmd,
                                           std::map<std::string, std::string>& extracted_params,
                                           std::string& matched_phrase) const {
    // Sort phrases longest-first to prefer more specific matches
    std::vector<std::string> sorted_phrases = cmd.phrases;
    std::sort(sorted_phrases.begin(), sorted_phrases.end(),
        [](const std::string& a, const std::string& b) { return a.size() > b.size(); });

    for (const auto& phrase : sorted_phrases) {
        size_t pos = lower_transcript.find(phrase);
        if (pos == std::string::npos) continue;

        matched_phrase = phrase;
        size_t phrase_end = pos + phrase.size();

        // If command has no params, match is complete
        if (cmd.params.empty()) {
            return true;
        }

        // Extract parameters from text after the phrase
        std::string after_phrase = lower_transcript.substr(phrase_end);
        if (extract_params(after_phrase, 0, cmd.params, extracted_params)) {
            return true;
        }
    }

    return false;
}

bool JsonCommandPlugin::extract_params(const std::string& text_after_phrase,
                                        size_t /*phrase_end_pos*/,
                                        const std::vector<ParamDef>& params,
                                        std::map<std::string, std::string>& extracted) const {
    // Pre-extract all numbers from the remaining text
    std::vector<double> numbers = extract_numbers(text_after_phrase);

    int number_index = 0;
    for (const auto& param : params) {
        switch (param.extract) {
            case ExtractType::FirstNumber: {
                if (number_index >= static_cast<int>(numbers.size())) return false;
                std::ostringstream oss;
                oss << numbers[number_index];
                extracted[param.name] = oss.str();
                number_index++;
                break;
            }
            case ExtractType::SecondNumber: {
                // Use the next number in sequence
                if (number_index >= static_cast<int>(numbers.size())) return false;
                std::ostringstream oss;
                oss << numbers[number_index];
                extracted[param.name] = oss.str();
                number_index++;
                break;
            }
            case ExtractType::KeywordAfterPhrase: {
                if (param.type != "enum") return false;
                bool found = false;
                // Check each canonical value's spoken variants
                for (const auto& [canonical, variants] : param.enum_values.mapping) {
                    for (const auto& variant : variants) {
                        if (text_after_phrase.find(variant) != std::string::npos) {
                            extracted[param.name] = canonical;
                            found = true;
                            break;
                        }
                    }
                    if (found) break;
                }
                if (!found) return false;
                break;
            }
        }
    }

    return true;
}

std::vector<double> JsonCommandPlugin::extract_numbers(const std::string& text) const {
    std::vector<double> numbers;
    std::string lower = to_lower(text);

    // Tokenize by spaces and non-alphanumeric chars
    std::vector<std::string> tokens;
    std::string current_token;
    for (char c : lower) {
        if (std::isalnum(c) || c == '.' || c == '-') {
            current_token += c;
        } else {
            if (!current_token.empty()) {
                tokens.push_back(current_token);
                current_token.clear();
            }
        }
    }
    if (!current_token.empty()) {
        tokens.push_back(current_token);
    }

    for (const auto& token : tokens) {
        // Try as numeric literal first
        try {
            size_t pos;
            double val = std::stod(token, &pos);
            if (pos == token.size()) {
                numbers.push_back(val);
                continue;
            }
        } catch (...) {}

        // Try as spoken number word
        auto it = SPOKEN_NUMBERS.find(token);
        if (it != SPOKEN_NUMBERS.end()) {
            numbers.push_back(it->second);
        }
    }

    return numbers;
}

std::string JsonCommandPlugin::substitute(const std::string& tmpl,
                                           const std::map<std::string, std::string>& params) const {
    std::string result = tmpl;
    for (const auto& [key, value] : params) {
        std::string placeholder = "{" + key + "}";
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.size(), value);
            pos += value.size();
        }
    }
    return result;
}

json JsonCommandPlugin::substitute_json(const json& tmpl,
                                         const std::map<std::string, std::string>& params) const {
    if (tmpl.is_string()) {
        std::string s = tmpl.get<std::string>();
        // Check if entire string is a single placeholder like "{x}"
        if (s.size() >= 3 && s.front() == '{' && s.back() == '}') {
            std::string key = s.substr(1, s.size() - 2);
            auto it = params.find(key);
            if (it != params.end()) {
                // Try to parse as number for numeric API fields
                try {
                    size_t pos;
                    double val = std::stod(it->second, &pos);
                    if (pos == it->second.size()) {
                        return json(val);
                    }
                } catch (...) {}
                return json(it->second);
            }
        }
        // General substitution for mixed text like "Navigating to {x}"
        return json(substitute(s, params));
    }
    if (tmpl.is_object()) {
        json result = json::object();
        for (auto& [key, value] : tmpl.items()) {
            result[key] = substitute_json(value, params);
        }
        return result;
    }
    if (tmpl.is_array()) {
        json result = json::array();
        for (const auto& elem : tmpl) {
            result.push_back(substitute_json(elem, params));
        }
        return result;
    }
    // Numbers, booleans, null — pass through
    return tmpl;
}

bool JsonCommandPlugin::send_http(const std::string& method, const std::string& url,
                                   const std::string& body, const std::string& api_key,
                                   int& status_code, std::string& response_body) const {
    CURL* curl = curl_easy_init();
    if (!curl) {
        Logger::warn("[JsonCommandPlugin] Failed to init curl");
        return false;
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    if (!api_key.empty()) {
        std::string auth_header = "X-API-Key: " + api_key;
        headers = curl_slist_append(headers, auth_header.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 5000L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 2000L);

    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    } else if (method == "GET") {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    }

    Logger::info("[JsonCommandPlugin] HTTP " + method + " " + url);
    if (!body.empty()) {
        Logger::info("[JsonCommandPlugin] Body: " + body);
    }

    CURLcode res = curl_easy_perform(curl);
    bool ok = (res == CURLE_OK);

    if (ok) {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        status_code = static_cast<int>(http_code);
    } else {
        Logger::warn("[JsonCommandPlugin] curl error: " + std::string(curl_easy_strerror(res)));
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return ok;
}

} // namespace memo_rf
