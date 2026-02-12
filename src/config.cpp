#include "config.h"
#include "logger.h"
#include "path_utils.h"
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <filesystem>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {

/// Resolve agent_persona from personas.json in the same directory as the config file.
/// On success: sets cfg.llm.system_prompt and cfg.llm.persona_name. On failure: logs warning, leaves system_prompt unchanged.
void resolve_persona(memo_rf::Config& cfg, const std::string& config_path) {
    const std::string& id = cfg.llm.agent_persona;
    if (id.empty()) return;

    std::string config_dir;
    std::string::size_type pos = config_path.find_last_of("/\\");
    if (pos != std::string::npos) {
        config_dir = config_path.substr(0, pos + 1);
    }
    std::string personas_path = config_dir + "personas.json";

    std::ifstream pf(personas_path);
    if (!pf.is_open()) {
        memo_rf::Logger::warn("agent_persona \"" + id + "\" set but could not open " + personas_path + "; using existing system_prompt.");
        return;
    }
    json personas_json;
    try {
        pf >> personas_json;
    } catch (const json::exception& e) {
        memo_rf::Logger::warn("agent_persona \"" + id + "\" set but failed to parse " + personas_path + ": " + e.what());
        return;
    }
    if (!personas_json.contains(id) || !personas_json[id].is_object()) {
        memo_rf::Logger::warn("agent_persona \"" + id + "\" not found in " + personas_path + "; using existing system_prompt.");
        return;
    }
    const auto& persona = personas_json[id];
    if (persona.contains("system_prompt") && persona["system_prompt"].is_string()) {
        cfg.llm.system_prompt = persona["system_prompt"].get<std::string>();
    }
    if (persona.contains("name") && persona["name"].is_string()) {
        cfg.llm.persona_name = persona["name"].get<std::string>();
    }
    memo_rf::Logger::info("Agent persona: " + (cfg.llm.persona_name.empty() ? id : cfg.llm.persona_name));
}

/// Apply response_language: append language instruction to system_prompt and set voice_path from language_voices.json.
/// Call after resolve_persona(), before path expansion.
void apply_response_language(memo_rf::Config& cfg, const std::string& config_path) {
    const std::string& code = cfg.llm.response_language;
    if (code.empty()) return;

    static const std::unordered_map<std::string, std::string> code_to_name = {
        {"es", "Spanish"},
        {"fr", "French"},
        {"de", "German"},
    };
    auto it = code_to_name.find(code);
    std::string language_name = (it != code_to_name.end()) ? it->second : code;

    cfg.llm.system_prompt += " Always respond in " + language_name + ". No other language.";
    memo_rf::Logger::info("Response language: " + language_name);

    std::string config_dir;
    std::string::size_type pos = config_path.find_last_of("/\\");
    if (pos != std::string::npos) {
        config_dir = config_path.substr(0, pos + 1);
    }
    std::string voices_path = config_dir + "language_voices.json";

    std::ifstream vf(voices_path);
    if (!vf.is_open()) {
        memo_rf::Logger::warn("response_language \"" + code + "\" set but could not open " + voices_path + "; voice_path unchanged.");
        return;
    }
    json voices_json;
    try {
        vf >> voices_json;
    } catch (const json::exception& e) {
        memo_rf::Logger::warn("response_language \"" + code + "\" set but failed to parse " + voices_path + ": " + e.what());
        return;
    }
    if (!voices_json.contains(code) || !voices_json[code].is_string()) {
        memo_rf::Logger::warn("response_language \"" + code + "\" not found in " + voices_path + "; voice_path unchanged.");
        return;
    }
    std::string rel = voices_json[code].get<std::string>();
    while (!rel.empty() && (rel[0] == '/' || rel[0] == '\\')) {
        rel.erase(0, 1);
    }
    std::string base = memo_rf::expand_path(cfg.tts.voice_models_dir.empty() ? "~/models/piper" : cfg.tts.voice_models_dir);
    if (!base.empty() && base.back() != '/' && base.back() != '\\') {
        base += "/";
    }
    cfg.tts.voice_path = base + rel;
}

/// Apply identity-file overlay (persona, identity, llm, voice, behavior, plugins) onto an already-loaded config.
void merge_identity_into_config(memo_rf::Config& cfg, const json& j) {
    if (j.contains("persona") && j["persona"].is_object()) {
        const auto& p = j["persona"];
        if (p.contains("system_prompt") && p["system_prompt"].is_string())
            cfg.llm.system_prompt = p["system_prompt"].get<std::string>();
        if (p.contains("name") && p["name"].is_string())
            cfg.llm.persona_name = p["name"].get<std::string>();
    }
    if (j.contains("identity") && j["identity"].is_object()) {
        const auto& i = j["identity"];
        if (i.contains("id") && i["id"].is_string())
            cfg.llm.agent_persona = i["id"].get<std::string>();
        if (i.contains("name") && i["name"].is_string())
            cfg.llm.persona_name = i["name"].get<std::string>();
    }
    if (j.contains("llm") && j["llm"].is_object()) {
        const auto& l = j["llm"];
        if (l.contains("endpoint") && l["endpoint"].is_string()) cfg.llm.endpoint = l["endpoint"];
        if (l.contains("timeout_ms") && l["timeout_ms"].is_number_integer()) cfg.llm.timeout_ms = l["timeout_ms"];
        if (l.contains("max_tokens") && l["max_tokens"].is_number_integer()) cfg.llm.max_tokens = l["max_tokens"];
        if (l.contains("context_max_turns_to_send") && l["context_max_turns_to_send"].is_number_integer()) cfg.llm.context_max_turns_to_send = l["context_max_turns_to_send"];
        if (l.contains("keep_alive_sec") && l["keep_alive_sec"].is_number_integer()) cfg.llm.keep_alive_sec = l["keep_alive_sec"];
        if (l.contains("model_name") && l["model_name"].is_string()) cfg.llm.model_name = l["model_name"];
        if (l.contains("translation_model") && l["translation_model"].is_string())
            cfg.llm.translation_model = l["translation_model"].get<std::string>();
        if (l.contains("warmup_translation_model")) cfg.llm.warmup_translation_model = l["warmup_translation_model"];
        if (l.contains("temperature") && l["temperature"].is_number()) cfg.llm.temperature = l["temperature"];
        if (l.contains("system_prompt") && l["system_prompt"].is_string()) cfg.llm.system_prompt = l["system_prompt"];
        if (l.contains("response_language") && l["response_language"].is_string())
            cfg.llm.response_language = l["response_language"].get<std::string>();
        if (l.contains("stop_sequences") && l["stop_sequences"].is_array()) {
            cfg.llm.stop_sequences.clear();
            for (const auto& seq : l["stop_sequences"])
                if (seq.is_string()) cfg.llm.stop_sequences.push_back(seq.get<std::string>());
        }
        if (l.contains("truncation") && l["truncation"].is_object() && l["truncation"].contains("fallback_phrase") && l["truncation"]["fallback_phrase"].is_string())
            cfg.llm.truncation.fallback_phrase = l["truncation"]["fallback_phrase"];
    }
    if (j.contains("voice") && j["voice"].is_object()) {
        const auto& v = j["voice"];
        if (v.contains("voice_path") && v["voice_path"].is_string()) cfg.tts.voice_path = v["voice_path"];
        if (v.contains("voice_models_dir") && v["voice_models_dir"].is_string()) cfg.tts.voice_models_dir = v["voice_models_dir"];
        if (v.contains("vox_preroll_ms") && v["vox_preroll_ms"].is_number_integer()) cfg.tts.vox_preroll_ms = v["vox_preroll_ms"];
        if (v.contains("output_gain") && v["output_gain"].is_number()) cfg.tts.output_gain = v["output_gain"];
        if (v.contains("language") && v["language"].is_string())
            cfg.llm.response_language = v["language"].get<std::string>();
    }
    if (j.contains("behavior") && j["behavior"].is_object()) {
        const auto& b = j["behavior"];
        if (b.contains("mode") && b["mode"].is_string())
            cfg.behavior.mode = b["mode"].get<std::string>();
        if (b.contains("router_enabled")) cfg.behavior.router_enabled = b["router_enabled"];
        if (b.contains("wake_word_enabled")) cfg.wake_word.enabled = b["wake_word_enabled"];
    }
    if (j.contains("plugins") && j["plugins"].is_object()) {
        const auto& p = j["plugins"];
        if (p.contains("config_files") && p["config_files"].is_array()) {
            cfg.plugins.config_files.clear();
            for (const auto& path_val : p["config_files"])
                if (path_val.is_string()) cfg.plugins.config_files.push_back(path_val.get<std::string>());
        }
    } else if (j.contains("plugin_paths") && j["plugin_paths"].is_array()) {
        cfg.plugins.config_files.clear();
        for (const auto& path_val : j["plugin_paths"])
            if (path_val.is_string()) cfg.plugins.config_files.push_back(path_val.get<std::string>());
    }
}

/// Apply full JSON config (all sections) into cfg. Used for defaults and legacy single-file load.
void apply_json_to_config(memo_rf::Config& cfg, const json& j) {
    // Audio config
    if (j.contains("audio")) {
        auto& a = j["audio"];
        if (a.contains("input_device")) cfg.audio.input_device = a["input_device"];
        if (a.contains("output_device")) cfg.audio.output_device = a["output_device"];
        if (a.contains("sample_rate")) cfg.audio.sample_rate = a["sample_rate"];
        if (a.contains("input_sample_rate")) cfg.audio.input_sample_rate = a["input_sample_rate"];
    }
    
    // VAD config
    if (j.contains("vad")) {
        auto& v = j["vad"];
        if (v.contains("threshold")) cfg.vad.threshold = v["threshold"];
        if (v.contains("silence_threshold")) cfg.vad.silence_threshold = v["silence_threshold"];
        if (v.contains("start_frames_required")) cfg.vad.start_frames_required = v["start_frames_required"];
        if (v.contains("end_of_utterance_silence_ms")) 
            cfg.vad.end_of_utterance_silence_ms = v["end_of_utterance_silence_ms"];
        if (v.contains("min_speech_ms")) cfg.vad.min_speech_ms = v["min_speech_ms"];
        if (v.contains("hangover_ms")) cfg.vad.hangover_ms = v["hangover_ms"];
        if (v.contains("pause_tolerance_ms")) cfg.vad.pause_tolerance_ms = v["pause_tolerance_ms"];
        if (v.contains("debug_log_rms_each_frame")) cfg.vad.debug_log_rms_each_frame = v["debug_log_rms_each_frame"];
    }
    
    // STT config
    if (j.contains("stt")) {
        auto& s = j["stt"];
        if (s.contains("model_path")) cfg.stt.model_path = s["model_path"];
        if (s.contains("language")) cfg.stt.language = s["language"];
        if (s.contains("blank_sentinel")) cfg.stt.blank_sentinel = s["blank_sentinel"];
        if (s.contains("use_gpu")) cfg.stt.use_gpu = s["use_gpu"];
    }
    
    // Transcript gate config
    if (j.contains("transcript_gate")) {
        auto& g = j["transcript_gate"];
        if (g.contains("min_transcript_chars")) cfg.transcript_gate.min_transcript_chars = g["min_transcript_chars"];
        if (g.contains("min_transcript_tokens")) cfg.transcript_gate.min_transcript_tokens = g["min_transcript_tokens"];
        if (g.contains("min_confidence")) cfg.transcript_gate.min_confidence = g["min_confidence"];
    }
    
    // Transcript blank behavior
    if (j.contains("transcript_blank_behavior")) {
        auto& b = j["transcript_blank_behavior"];
        if (b.contains("behavior")) cfg.transcript_blank_behavior.behavior = b["behavior"];
        if (b.contains("say_again_phrase")) cfg.transcript_blank_behavior.say_again_phrase = b["say_again_phrase"];
    }
    
    // Clarifier config
    if (j.contains("clarifier")) {
        auto& c = j["clarifier"];
        if (c.contains("min_chars")) cfg.clarifier.min_chars = c["min_chars"];
        if (c.contains("min_confidence")) cfg.clarifier.min_confidence = c["min_confidence"];
        if (c.contains("unknown_sentinel")) cfg.clarifier.unknown_sentinel = c["unknown_sentinel"];
    }
    
    // Router (repair) config
    if (j.contains("router")) {
        auto& r = j["router"];
        if (r.contains("repair_confidence_threshold")) cfg.router.repair_confidence_threshold = r["repair_confidence_threshold"];
        if (r.contains("repair_phrase")) cfg.router.repair_phrase = r["repair_phrase"];
    }
    
    // LLM config
    if (j.contains("llm")) {
        auto& l = j["llm"];
        if (l.contains("endpoint")) cfg.llm.endpoint = l["endpoint"];
        if (l.contains("timeout_ms")) cfg.llm.timeout_ms = l["timeout_ms"];
        if (l.contains("max_tokens")) cfg.llm.max_tokens = l["max_tokens"];
        if (l.contains("context_max_turns_to_send")) cfg.llm.context_max_turns_to_send = l["context_max_turns_to_send"];
        if (l.contains("keep_alive_sec")) cfg.llm.keep_alive_sec = l["keep_alive_sec"];
        if (l.contains("model_name")) cfg.llm.model_name = l["model_name"];
        if (l.contains("translation_model") && l["translation_model"].is_string())
            cfg.llm.translation_model = l["translation_model"].get<std::string>();
        if (l.contains("warmup_translation_model")) cfg.llm.warmup_translation_model = l["warmup_translation_model"];
        if (l.contains("temperature")) cfg.llm.temperature = l["temperature"];
        if (l.contains("system_prompt")) cfg.llm.system_prompt = l["system_prompt"];
        if (l.contains("agent_persona") && l["agent_persona"].is_string()) {
            cfg.llm.agent_persona = l["agent_persona"].get<std::string>();
        }
        if (l.contains("response_language") && l["response_language"].is_string()) {
            cfg.llm.response_language = l["response_language"].get<std::string>();
        }
        if (l.contains("stop_sequences") && l["stop_sequences"].is_array()) {
            cfg.llm.stop_sequences.clear();
            for (const auto& seq : l["stop_sequences"]) {
                cfg.llm.stop_sequences.push_back(seq.get<std::string>());
            }
        }
        if (l.contains("truncation") && l["truncation"].is_object()) {
            auto& tr = l["truncation"];
            if (tr.contains("fallback_phrase")) cfg.llm.truncation.fallback_phrase = tr["fallback_phrase"];
        }
    }
    
    // TTS config
    if (j.contains("tts")) {
        auto& t = j["tts"];
        if (t.contains("voice_path")) cfg.tts.voice_path = t["voice_path"];
        if (t.contains("voice_models_dir")) cfg.tts.voice_models_dir = t["voice_models_dir"];
        if (t.contains("piper_path")) cfg.tts.piper_path = t["piper_path"];
        if (t.contains("espeak_data_path")) cfg.tts.espeak_data_path = t["espeak_data_path"];
        if (t.contains("vox_preroll_ms")) cfg.tts.vox_preroll_ms = t["vox_preroll_ms"];
        if (t.contains("vox_preroll_amplitude")) cfg.tts.vox_preroll_amplitude = t["vox_preroll_amplitude"];
        if (t.contains("vox_end_tone_ms")) cfg.tts.vox_end_tone_ms = t["vox_end_tone_ms"];
        if (t.contains("vox_end_tone_amplitude")) cfg.tts.vox_end_tone_amplitude = t["vox_end_tone_amplitude"];
        if (t.contains("vox_end_tone_freq_hz")) cfg.tts.vox_end_tone_freq_hz = t["vox_end_tone_freq_hz"];
        if (t.contains("output_gain")) cfg.tts.output_gain = t["output_gain"];
    }
    
    // TX config
    if (j.contains("tx")) {
        auto& tx = j["tx"];
        if (tx.contains("max_transmit_ms")) cfg.tx.max_transmit_ms = tx["max_transmit_ms"];
        if (tx.contains("standby_delay_ms")) cfg.tx.standby_delay_ms = tx["standby_delay_ms"];
        if (tx.contains("channel_clear_silence_ms")) cfg.tx.channel_clear_silence_ms = tx["channel_clear_silence_ms"];
        if (tx.contains("enable_start_chirp")) cfg.tx.enable_start_chirp = tx["enable_start_chirp"];
        if (tx.contains("enable_end_chirp")) cfg.tx.enable_end_chirp = tx["enable_end_chirp"];
    }

    // Wake word config (respond only on "hey memo" when enabled)
    if (j.contains("wake_word")) {
        auto& w = j["wake_word"];
        if (w.contains("enabled")) cfg.wake_word.enabled = w["enabled"];
    }

    // Behavior config (robot/agent mode)
    if (j.contains("behavior") && j["behavior"].is_object()) {
        auto& b = j["behavior"];
        if (b.contains("mode") && b["mode"].is_string()) cfg.behavior.mode = b["mode"];
        if (b.contains("router_enabled")) cfg.behavior.router_enabled = b["router_enabled"];
    }
    
    // Tools config
    if (j.contains("tools")) {
        auto& tools = j["tools"];
        if (tools.contains("timeout_ms")) cfg.tools.timeout_ms = tools["timeout_ms"];
        if (tools.contains("max_concurrent")) cfg.tools.max_concurrent = tools["max_concurrent"];
        if (tools.contains("enabled") && tools["enabled"].is_array()) {
            cfg.tools.enabled.clear();
            for (const auto& tool_name : tools["enabled"]) {
                if (tool_name.is_string()) {
                    cfg.tools.enabled.push_back(tool_name.get<std::string>());
                }
            }
        }
    }

    // Plugin config (data-driven command plugins)
    if (j.contains("plugins")) {
        auto& p = j["plugins"];
        if (p.contains("config_files") && p["config_files"].is_array()) {
            cfg.plugins.config_files.clear();
            for (const auto& path_val : p["config_files"]) {
                if (path_val.is_string()) {
                    cfg.plugins.config_files.push_back(path_val.get<std::string>());
                }
            }
        }
    }

    // Memory config (conversation history for multi-turn)
    if (j.contains("memory")) {
        auto& mem = j["memory"];
        if (mem.contains("enabled")) cfg.memory.enabled = mem["enabled"];
        if (mem.contains("max_messages")) cfg.memory.max_messages = mem["max_messages"];
        if (mem.contains("max_tokens")) cfg.memory.max_tokens = mem["max_tokens"];
    }
    
    // Session/replay
    if (j.contains("session_log_dir")) cfg.session_log_dir = j["session_log_dir"];
    if (j.contains("enable_replay_mode")) cfg.enable_replay_mode = j["enable_replay_mode"];
    if (j.contains("replay_wav_path")) cfg.replay_wav_path = j["replay_wav_path"];
    if (j.contains("feed_server_url")) cfg.feed_server_url = j["feed_server_url"];
}

} // anonymous namespace

namespace memo_rf {

Config Config::load_from_file(const std::string& path) {
    Config cfg;

    std::string config_dir;
    std::string path_normalized = path;
    while (!path_normalized.empty() && (path_normalized.back() == '/' || path_normalized.back() == '\\'))
        path_normalized.pop_back();

    const bool is_dir = [&]() {
        std::error_code ec;
        return fs::is_directory(path_normalized, ec) && !ec;
    }();

    // If path is a file (e.g. config/config.json), check whether the same directory has active.json.
    // If so, use the directory so UI selection (active.json) is respected.
    if (!is_dir) {
        std::string::size_type slash = path_normalized.find_last_of("/\\");
        if (slash != std::string::npos && slash + 1 < path_normalized.size()) {
            std::string dir_part = path_normalized.substr(0, slash);
            std::string active_in_dir = dir_part + "/active.json";
            std::ifstream at(active_in_dir);
            if (at.is_open()) {
                Logger::info("Found active.json next to config file; loading identity from " + dir_part);
                return load_from_file(dir_part);
            }
        }
    }

    if (is_dir) {
        config_dir = path_normalized;
        if (!config_dir.empty() && config_dir.back() != '/' && config_dir.back() != '\\')
            config_dir += "/";

        std::string active_path = config_dir + "active.json";
        std::ifstream af(active_path);
        if (!af.is_open()) {
            std::string legacy_path = config_dir + "config.json";
            std::ifstream lf(legacy_path);
            if (lf.is_open()) {
                Logger::info("No active.json; loading legacy " + legacy_path);
                return load_from_file(legacy_path);
            }
            Logger::warn("Config directory given but could not open " + active_path + " or " + legacy_path + ". Using defaults.");
            return cfg;
        }
        json active_j;
        try { af >> active_j; } catch (const json::exception& e) {
            Logger::error("Error parsing active.json: " + std::string(e.what()));
            return cfg;
        }
        std::string active;
        if (active_j.contains("active") && active_j["active"].is_string())
            active = active_j["active"].get<std::string>();
        if (active.empty()) {
            Logger::warn("active.json missing or empty \"active\" field.");
            return cfg;
        }

        std::string defaults_path = config_dir + "defaults.json";
        std::ifstream df(defaults_path);
        if (!df.is_open()) {
            Logger::warn("Could not open " + defaults_path + ". Using defaults.");
        } else {
            try {
                json defaults_j;
                df >> defaults_j;
                apply_json_to_config(cfg, defaults_j);
            } catch (const json::exception& e) {
                Logger::warn("Error parsing defaults.json: " + std::string(e.what()));
            }
        }

        // Machine-specific overrides: if config.json exists, apply it so audio devices,
        // input_sample_rate, piper_path, etc. from the existing config are used.
        std::string machine_config_path = config_dir + "config.json";
        std::ifstream mcf(machine_config_path);
        if (mcf.is_open()) {
            try {
                json machine_j;
                mcf >> machine_j;
                apply_json_to_config(cfg, machine_j);
            } catch (const json::exception& e) {
                Logger::warn("Error parsing config.json (machine overrides): " + std::string(e.what()));
            }
        }

        std::string identity_path = config_dir + active + ".json";
        std::ifstream idf(identity_path);
        if (!idf.is_open()) {
            Logger::warn("Could not open identity file " + identity_path + ". Using defaults only.");
        } else {
            try {
                json identity_j;
                idf >> identity_j;
                merge_identity_into_config(cfg, identity_j);
            } catch (const json::exception& e) {
                Logger::error("Error parsing identity file: " + std::string(e.what()));
                return cfg;
            }
        }

#ifdef AGENT_PERSONA_OVERRIDE
        cfg.llm.agent_persona = AGENT_PERSONA_OVERRIDE;
        Logger::info("Using build-time persona override: " + std::string(AGENT_PERSONA_OVERRIDE));
#endif

        apply_response_language(cfg, config_dir + "defaults.json");
        cfg.config_dir_ = path_normalized;

        if (!cfg.stt.model_path.empty()) cfg.stt.model_path = expand_path(cfg.stt.model_path);
        if (!cfg.tts.voice_path.empty()) cfg.tts.voice_path = expand_path(cfg.tts.voice_path);
        if (!cfg.tts.piper_path.empty()) cfg.tts.piper_path = expand_path(cfg.tts.piper_path);
        if (cfg.tts.espeak_data_path.empty())
            cfg.tts.espeak_data_path = default_espeak_data_path();
        else
            cfg.tts.espeak_data_path = expand_path(cfg.tts.espeak_data_path);

        Logger::info("Loaded identity: " + (cfg.llm.persona_name.empty() ? cfg.llm.agent_persona : cfg.llm.persona_name) + " (" + active + ")");
        return cfg;
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        Logger::warn("Could not open config file: " + path + ". Using defaults.");
        return cfg;
    }
    json j;
    try { file >> j; } catch (const json::exception& e) {
        Logger::error("Error parsing config JSON: " + std::string(e.what()));
        return cfg;
    }
    apply_json_to_config(cfg, j);

#ifdef AGENT_PERSONA_OVERRIDE
    cfg.llm.agent_persona = AGENT_PERSONA_OVERRIDE;
    Logger::info("Using build-time persona override: " + std::string(AGENT_PERSONA_OVERRIDE));
#endif

    resolve_persona(cfg, path);
    apply_response_language(cfg, path);

    if (!cfg.stt.model_path.empty()) cfg.stt.model_path = expand_path(cfg.stt.model_path);
    if (!cfg.tts.voice_path.empty()) cfg.tts.voice_path = expand_path(cfg.tts.voice_path);
    if (!cfg.tts.piper_path.empty()) cfg.tts.piper_path = expand_path(cfg.tts.piper_path);
    if (cfg.tts.espeak_data_path.empty())
        cfg.tts.espeak_data_path = default_espeak_data_path();
    else
        cfg.tts.espeak_data_path = expand_path(cfg.tts.espeak_data_path);

    return cfg;
}

void Config::save_to_file(const std::string& path) const {
    json j;
    
    j["audio"]["input_device"] = audio.input_device;
    j["audio"]["output_device"] = audio.output_device;
    j["audio"]["sample_rate"] = audio.sample_rate;
    if (audio.input_sample_rate != 0) j["audio"]["input_sample_rate"] = audio.input_sample_rate;
    
    j["vad"]["threshold"] = vad.threshold;
    j["vad"]["silence_threshold"] = vad.silence_threshold;
    j["vad"]["start_frames_required"] = vad.start_frames_required;
    j["vad"]["end_of_utterance_silence_ms"] = vad.end_of_utterance_silence_ms;
    j["vad"]["min_speech_ms"] = vad.min_speech_ms;
    j["vad"]["hangover_ms"] = vad.hangover_ms;
    j["vad"]["pause_tolerance_ms"] = vad.pause_tolerance_ms;
    j["vad"]["debug_log_rms_each_frame"] = vad.debug_log_rms_each_frame;
    
    j["stt"]["model_path"] = stt.model_path;
    j["stt"]["language"] = stt.language;
    j["stt"]["blank_sentinel"] = stt.blank_sentinel;
    j["stt"]["use_gpu"] = stt.use_gpu;
    
    j["transcript_gate"]["min_transcript_chars"] = transcript_gate.min_transcript_chars;
    j["transcript_gate"]["min_transcript_tokens"] = transcript_gate.min_transcript_tokens;
    j["transcript_gate"]["min_confidence"] = transcript_gate.min_confidence;
    
    j["transcript_blank_behavior"]["behavior"] = transcript_blank_behavior.behavior;
    j["transcript_blank_behavior"]["say_again_phrase"] = transcript_blank_behavior.say_again_phrase;
    
    j["clarifier"]["min_chars"] = clarifier.min_chars;
    j["clarifier"]["min_confidence"] = clarifier.min_confidence;
    j["clarifier"]["unknown_sentinel"] = clarifier.unknown_sentinel;
    
    j["router"]["repair_confidence_threshold"] = router.repair_confidence_threshold;
    j["router"]["repair_phrase"] = router.repair_phrase;
    
    j["llm"]["endpoint"] = llm.endpoint;
    j["llm"]["timeout_ms"] = llm.timeout_ms;
    j["llm"]["max_tokens"] = llm.max_tokens;
    j["llm"]["context_max_turns_to_send"] = llm.context_max_turns_to_send;
    if (llm.keep_alive_sec > 0) j["llm"]["keep_alive_sec"] = llm.keep_alive_sec;
    j["llm"]["model_name"] = llm.model_name;
    if (!llm.translation_model.empty()) j["llm"]["translation_model"] = llm.translation_model;
    if (llm.warmup_translation_model) j["llm"]["warmup_translation_model"] = llm.warmup_translation_model;
    j["llm"]["temperature"] = llm.temperature;
    if (!llm.agent_persona.empty()) {
        j["llm"]["agent_persona"] = llm.agent_persona;
        // When using a persona, system_prompt is derived from personas.json at load time; do not persist it
    } else {
        j["llm"]["system_prompt"] = llm.system_prompt;
    }
    if (!llm.response_language.empty()) {
        j["llm"]["response_language"] = llm.response_language;
    }
    j["llm"]["stop_sequences"] = llm.stop_sequences;
    j["llm"]["truncation"]["fallback_phrase"] = llm.truncation.fallback_phrase;
    
    j["tts"]["voice_path"] = tts.voice_path;
    if (!tts.voice_models_dir.empty()) {
        j["tts"]["voice_models_dir"] = tts.voice_models_dir;
    }
    j["tts"]["piper_path"] = tts.piper_path;
    j["tts"]["espeak_data_path"] = tts.espeak_data_path;
    j["tts"]["vox_preroll_ms"] = tts.vox_preroll_ms;
    j["tts"]["vox_preroll_amplitude"] = tts.vox_preroll_amplitude;
    j["tts"]["vox_end_tone_ms"] = tts.vox_end_tone_ms;
    j["tts"]["vox_end_tone_amplitude"] = tts.vox_end_tone_amplitude;
    j["tts"]["vox_end_tone_freq_hz"] = tts.vox_end_tone_freq_hz;
    j["tts"]["output_gain"] = tts.output_gain;
    
    j["tx"]["max_transmit_ms"] = tx.max_transmit_ms;
    j["tx"]["standby_delay_ms"] = tx.standby_delay_ms;
    j["tx"]["channel_clear_silence_ms"] = tx.channel_clear_silence_ms;
    j["tx"]["enable_start_chirp"] = tx.enable_start_chirp;
    j["tx"]["enable_end_chirp"] = tx.enable_end_chirp;

    j["wake_word"]["enabled"] = wake_word.enabled;
    
    j["memory"]["enabled"] = memory.enabled;
    j["memory"]["max_messages"] = memory.max_messages;
    j["memory"]["max_tokens"] = memory.max_tokens;
    
    j["tools"]["enabled"] = tools.enabled;
    j["tools"]["timeout_ms"] = tools.timeout_ms;
    j["tools"]["max_concurrent"] = tools.max_concurrent;

    if (!plugins.config_files.empty()) {
        j["plugins"]["config_files"] = plugins.config_files;
    }
    
    j["session_log_dir"] = session_log_dir;
    j["enable_replay_mode"] = enable_replay_mode;
    j["replay_wav_path"] = replay_wav_path;
    j["feed_server_url"] = feed_server_url;
    
    std::ofstream file(path);
    if (file.is_open()) {
        file << j.dump(2);
    }
}

} // namespace memo_rf
