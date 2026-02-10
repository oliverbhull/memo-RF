#pragma once

#include "action_plugin.h"
#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

namespace memo_rf {

/// A generic, data-driven command plugin that loads command definitions from a JSON file.
/// Supports phrase matching, parameter extraction, HTTP API calls, and vocab boosting.
/// The same class handles any integration (Muni, Home Assistant, etc.) — the JSON
/// file defines the commands, phrases, API endpoints, and vocabulary.
class JsonCommandPlugin : public ActionPlugin {
public:
    /// Load plugin from a JSON config file (e.g. config/plugins/muni.json)
    explicit JsonCommandPlugin(const std::string& config_path);

    std::string name() const override { return name_; }
    bool try_handle(const std::string& transcript, ActionResult& result) override;
    int priority() const override { return priority_; }
    std::vector<std::string> vocab() const override { return vocab_words_; }

private:
    /// Parameter extraction types
    enum class ExtractType {
        FirstNumber,          ///< First numeric value after phrase match
        SecondNumber,         ///< Second numeric value after phrase match
        KeywordAfterPhrase,   ///< Match enum values after the trigger phrase
    };

    /// Enum value mapping: canonical_value → [spoken variants]
    struct EnumValues {
        std::map<std::string, std::vector<std::string>> mapping;
    };

    /// Parameter definition loaded from JSON
    struct ParamDef {
        std::string name;
        std::string type;       ///< "float", "int", "enum", "string"
        ExtractType extract;
        EnumValues enum_values; ///< Only for type=="enum"
    };

    /// Command definition loaded from JSON
    struct CommandDef {
        std::string id;
        int priority = 100;
        std::vector<std::string> phrases;
        std::vector<ParamDef> params;
        std::string api_endpoint;  ///< e.g. "/rovers/{rover_id}/command"
        std::string api_method;    ///< "POST" or "GET"
        nlohmann::json api_body;   ///< Template with {param} placeholders
        std::string confirm_text;  ///< Template for TTS response
    };

    /// API configuration
    struct ApiConfig {
        std::string base_url;
        std::string api_key;
        std::string default_rover_id;
    };

    /// Try to match transcript against a single command
    bool try_match_command(const std::string& lower_transcript,
                           const CommandDef& cmd,
                           std::map<std::string, std::string>& extracted_params,
                           std::string& matched_phrase) const;

    /// Extract parameters from transcript after a phrase match
    bool extract_params(const std::string& lower_transcript,
                        size_t phrase_end_pos,
                        const std::vector<ParamDef>& params,
                        std::map<std::string, std::string>& extracted) const;

    /// Extract numbers from text (handles spoken numbers like "five" → 5)
    std::vector<double> extract_numbers(const std::string& text) const;

    /// Substitute {param} placeholders in a template string
    std::string substitute(const std::string& tmpl,
                          const std::map<std::string, std::string>& params) const;

    /// Substitute {param} placeholders in a JSON template
    nlohmann::json substitute_json(const nlohmann::json& tmpl,
                                   const std::map<std::string, std::string>& params) const;

    /// Make HTTP request to the API
    bool send_http(const std::string& method, const std::string& url,
                   const std::string& body, const std::string& api_key,
                   int& status_code, std::string& response_body) const;

    /// Parse ExtractType from string
    static ExtractType parse_extract_type(const std::string& s);

    std::string name_;
    int priority_ = 100;
    ApiConfig api_;
    std::vector<CommandDef> commands_;       ///< Sorted by priority
    std::vector<std::string> vocab_words_;   ///< Merged vocab for STT
};

} // namespace memo_rf
