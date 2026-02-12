#pragma once

#include <string>
#include <cstdint>
#include <vector>

namespace memo_rf {

struct AudioConfig {
    std::string input_device;
    std::string output_device;
    int sample_rate = 16000;
    /// If set (e.g. 48000), open input device at this rate and resample to sample_rate for pipeline. Use when device does not support 16 kHz.
    int input_sample_rate = 0;
};

struct VADConfig {
    float threshold = 0.5f;
    /// RMS below this counts as silence; frames between this and threshold are speech dips (reset silence)
    float silence_threshold = 0.02f;
    /// Consecutive speech frames required to trigger SpeechStart (reduces false start on pops)
    int start_frames_required = 2;
    int end_of_utterance_silence_ms = 1000;  // Silence duration before ending utterance
    int min_speech_ms = 200;                  // Minimum speech duration to be valid
    int hangover_ms = 200;                    // Grace period after speech end
    int pause_tolerance_ms = 500;             // Allow pauses this long during speech without ending
    bool debug_log_rms_each_frame = false;    // When true, log RMS and state every frame (verbose)
};

struct STTConfig {
    std::string model_path;
    std::string language = "en";
    std::string blank_sentinel = "[BLANK_AUDIO]";  ///< Treat this exact string (after trim) as blank
    bool use_gpu = true;  ///< Use Metal/GPU when available (whisper.cpp must be built with Metal)
};

/// Transcript gate: block router/clarifier/memory when transcript is low-signal
struct TranscriptGateConfig {
    int min_transcript_chars = 1;     ///< Minimum character count (after trim)
    int min_transcript_tokens = 1;    ///< Minimum token count from STT
    float min_confidence = 0.0f;      ///< Minimum confidence (0 = disabled)
};

/// Behavior when transcript is blank or fails gate
struct TranscriptBlankBehaviorConfig {
    std::string behavior = "none";           ///< "none" | "say_again" | "beep"
    std::string say_again_phrase = "Say again, over";
};

/// Context resolver (clarifier) guards: never run on empty/low-signal
struct ClarifierConfig {
    int min_chars = 1;
    float min_confidence = 0.0f;
    std::string unknown_sentinel = "__UNKNOWN__";  ///< Treat as no-op for main LLM
};

/// Router: repair plan when confidence is low
struct RouterConfig {
    float repair_confidence_threshold = 0.0f;  ///< Below this: return repair plan (0 = disabled)
    std::string repair_phrase = "Say again, over";
};

/// LLM: fallback when response was truncated (done_reason == length)
struct LLMTruncationConfig {
    std::string fallback_phrase = "Stand by.";
};

struct LLMConfig {
    std::string endpoint = "http://localhost:8080/completion";
    int timeout_ms = 2000;
    int max_tokens = 0;  ///< 0 = no limit (generate until stop)
    int context_max_turns_to_send = 6;  ///< Only send last N turns to LLM (bounded context)
    /// Ollama: seconds to keep model in memory after request (0 = default; positive = keep loaded)
    int keep_alive_sec = 0;
    std::string model_name = "qwen";
    /// When set and agent_persona is translator, use this model with translation-only prompt (e.g. translategemma)
    std::string translation_model;
    /// If true and translation_model set, send one tiny request at startup to load model (Ollama only)
    bool warmup_translation_model = false;
    float temperature = 0.7f;
    std::vector<std::string> stop_sequences = {"</s>", "\n\n", "User:", "Human:"};
    /// If set, system_prompt is resolved from config/personas.json; persona wins over inline system_prompt
    std::string agent_persona;
    /// When set (e.g. "es", "fr", "de"), agent responds in that language; persona unchanged. TTS voice resolved from language_voices.json.
    std::string response_language;
    /// Display name from persona (set when agent_persona is resolved); used for logging
    std::string persona_name;
    std::string system_prompt = "You are a helpful radio operator supporting field operators. "
                               "Use clear, concise comms. Be succinct: one short sentence, under 15 words when possible. "
                               "No preamble. Answer in standard radio procedure.";
    LLMTruncationConfig truncation;  ///< Fallback when done_reason == length
};

struct TTSConfig {
    std::string voice_path;
    /// Base dir for Piper voice models when resolving by response_language (empty = ~/models/piper)
    std::string voice_models_dir;
    std::string piper_path;         ///< Piper binary path (empty = auto-detect)
    std::string espeak_data_path;  ///< espeak-ng data dir (empty = platform default)
    int vox_preroll_ms = 350;       ///< Tone duration before speech so VOX opens (ms)
    float vox_preroll_amplitude = 0.55f;  ///< Tone amplitude 0–1 so VOX reliably triggers
    int vox_end_tone_ms = 250;      ///< Tone duration at end of transmission (0 = none)
    float vox_end_tone_amplitude = 0.55f;  ///< End tone amplitude 0–1
    float vox_end_tone_freq_hz = 440.0f;   ///< End tone frequency (Hz)
    float output_gain = 1.0f;
};

struct WakeWordConfig {
    bool enabled = true;  ///< When true, respond only when transcript contains "hey memo"; when false, legacy (respond to every utterance)
};

/// Behavior mode for robot/agent identity: conversational (full pipeline), plugin_only (commands only), or llm_only (skip router/plugins).
struct BehaviorConfig {
    std::string mode = "conversational";  ///< "conversational" | "plugin_only" | "llm_only"
    bool router_enabled = true;           ///< When false, skip router fast-path (e.g. for translator)
};

struct TXConfig {
    int max_transmit_ms = 20000;  // Maximum transmission time in ms (0 = no limit)
    int standby_delay_ms = 200;   // Delay after user speech ends before sending "Standby, over" (ms)
    int channel_clear_silence_ms = 500;  ///< Half-duplex: wait this long after last SpeechEnd before keying up (ms)
    bool enable_start_chirp = false;
    bool enable_end_chirp = false;
};

struct ToolsConfig {
    std::vector<std::string> enabled;  // List of enabled tool names
    int timeout_ms = 5000;              // Default timeout for tool execution
    size_t max_concurrent = 1;          // Maximum concurrent tool executions
};

/// Plugin system: data-driven command plugins loaded from JSON config files.
struct PluginConfig {
    std::vector<std::string> config_files;  ///< Paths to plugin JSON files (e.g. "config/plugins/muni.json")
};

/// Conversation memory: when enabled, session history is kept for multi-turn context.
struct MemoryConfig {
    bool enabled = true;
    size_t max_messages = 20;
    size_t max_tokens = 2000;
};

struct Config {
    AudioConfig audio;
    VADConfig vad;
    STTConfig stt;
    TranscriptGateConfig transcript_gate;
    TranscriptBlankBehaviorConfig transcript_blank_behavior;
    ClarifierConfig clarifier;
    RouterConfig router;
    LLMConfig llm;
    TTSConfig tts;
    TXConfig tx;
    ToolsConfig tools;
    PluginConfig plugins;
    MemoryConfig memory;
    WakeWordConfig wake_word;
    BehaviorConfig behavior;

    /// When set, config was loaded from identity (active.json + defaults + identity file). Used for persona-change and save.
    std::string config_dir_;

    std::string session_log_dir = "sessions";
    bool enable_replay_mode = false;
    std::string replay_wav_path;
    std::string feed_server_url = "";  ///< Optional: POST events to feed server (e.g. "http://localhost:5050/api/feed/notify")

    static Config load_from_file(const std::string& path);
    void save_to_file(const std::string& path) const;
};

} // namespace memo_rf
