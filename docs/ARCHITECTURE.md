# Memo-RF Architecture

## Overview

Memo-RF is a single-threaded C++ application with a modular architecture. The core loop processes audio frames sequentially, maintaining state through a state machine.

This document covers both the legacy architecture and the new refactored components introduced for better reliability and maintainability.

---

## Directory Structure

```
memo-RF/
├── include/
│   ├── core/           # Core types, constants, and utilities (NEW)
│   │   ├── types.h         # Fundamental type definitions
│   │   ├── constants.h     # System-wide constants
│   │   ├── config.h        # Configuration system
│   │   ├── ring_buffer.h   # Lock-free ring buffer
│   │   └── compat.h        # Backward compatibility
│   ├── vad/            # Voice Activity Detection (NEW)
│   │   ├── vad_interface.h # Abstract VAD interface
│   │   └── energy_vad.h    # Energy-based VAD implementation
│   ├── tts/            # Text-to-Speech (NEW)
│   │   ├── tts_interface.h # Abstract TTS interface
│   │   └── piper_tts.h     # Piper TTS implementation
│   ├── memory/         # Conversation Memory (NEW)
│   │   └── conversation_memory.h
│   └── [legacy]        # Existing components (agent.h, etc.)
├── src/
│   ├── core/           # Core implementations
│   ├── vad/            # VAD implementations
│   ├── tts/            # TTS implementations
│   ├── memory/         # Memory implementations
│   └── [legacy]        # Existing implementations
├── config/             # Configuration files
└── docs/               # Documentation
```

---

## Module Responsibilities

### AudioIO
- **PortAudio** wrapper for audio I/O
- Manages input stream (radio audio in) and output stream (radio audio out)
- Frame-based processing (20ms frames @ 16kHz)
- Non-blocking playback queue

### VADEndpointing (Legacy)
- **Energy-based VAD** (simple, deterministic)
- Endpointing with configurable silence thresholds
- Emits `SpeechStart` and `SpeechEnd` events
- Maintains current segment buffer

### EnergyVAD (NEW - Improved)
- **Energy-based VAD** with improved reliability
- **Pre-speech buffer** (300ms) to capture word beginnings
- **Adaptive thresholding** based on noise floor
- **Fixed pause tolerance** for natural speech pauses
- **Hysteresis** to prevent oscillation

### STTEngine
- **whisper.cpp** integration
- Transcribes complete audio segments
- Returns transcript with confidence and timing
- CPU-only inference (v1)

### Router
- Fast path rule matching (keyword → response)
- LLM routing decision
- Returns `Plan` objects (Speak, SpeakAckThenAnswer, etc.)

### LLMClient
- **HTTP client** for llama.cpp/Ollama server
- Timeout enforcement
- Radio-style prompt formatting
- Response cleaning
- **Tool support** for function calling

### TTSEngine (Legacy)
- **Piper** integration (external process)
- Phrase caching for common responses
- VOX pre-roll generation (tone burst)
- Output gain control

### PiperTTS (NEW - Improved)
- **Piper** integration with optimizations
- **Path caching** (find piper once)
- **LRU phrase cache** (100 entries)
- **Warmup support** for preloading
- **Better error handling**

### ConversationMemory (NEW)
- **Bounded message history** (max 20 messages)
- **Token estimation** and automatic pruning
- **Persistence** (save/load to JSON)
- **Export formats** for LLM APIs

### TXController
- VOX-based transmission
- Max transmit time enforcement
- Playback interruption support

### StateMachine
- Four states: `IdleListening`, `ReceivingSpeech`, `Thinking`, `Transmitting`
- Event-driven transitions
- Transmission interruption logic

### SessionRecorder
- WAV file recording (raw input, utterances, TTS output)
- JSON event log with timings
- Session directory structure

---

## Audio Pipeline

```
┌─────────────────────────────────────────────────────────────────┐
│                        Audio Input                              │
└─────────────────────────┬───────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────────┐
│                 VAD (Voice Activity Detection)                  │
│  ┌──────────────┐  ┌───────────────────┐  ┌──────────────────┐ │
│  │ Pre-speech   │  │ Energy Detection  │  │ Adaptive         │ │
│  │ Buffer       │──│ + Hysteresis      │──│ Thresholding     │ │
│  │ (300ms)      │  │                   │  │                  │ │
│  └──────────────┘  └───────────────────┘  └──────────────────┘ │
└─────────────────────────┬───────────────────────────────────────┘
                          │ SpeechEnd event + audio buffer
                          ▼
┌─────────────────────────────────────────────────────────────────┐
│                    STT (Whisper)                                │
└─────────────────────────┬───────────────────────────────────────┘
                          │ Transcript
                          ▼
┌─────────────────────────────────────────────────────────────────┐
│                    Router                                       │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ Fast-path keywords (roger, affirmative, etc.) → Direct   │  │
│  │ Everything else → LLM                                    │  │
│  └──────────────────────────────────────────────────────────┘  │
└─────────────────────────┬───────────────────────────────────────┘
                          │
            ┌─────────────┴─────────────┐
            │                           │
            ▼ Fast Path                 ▼ LLM Path
┌───────────────────┐        ┌────────────────────────────────────┐
│ Cached Response   │        │ Conversation Memory                │
│                   │        │  ┌──────────────────────────────┐ │
└─────────┬─────────┘        │  │ System Prompt               │ │
          │                   │  │ + History (max 20 msgs)     │ │
          │                   │  │ + Current User Input        │ │
          │                   │  └──────────────────────────────┘ │
          │                   └────────────────┬───────────────────┘
          │                                    │
          │                                    ▼
          │                   ┌────────────────────────────────────┐
          │                   │ LLM (Ollama/llama.cpp)             │
          │                   │  + Tool Execution Loop             │
          │                   └────────────────┬───────────────────┘
          │                                    │
          └─────────────────┬──────────────────┘
                            │ Response text
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                    TTS (Piper)                                  │
│  ┌──────────────┐  ┌───────────────────┐  ┌──────────────────┐ │
│  │ Phrase       │  │ Piper Synthesis   │  │ Pre-roll         │ │
│  │ Cache (LRU)  │──│ (path cached)     │──│ Generation       │ │
│  │              │  │                   │  │ (440Hz tone)     │ │
│  └──────────────┘  └───────────────────┘  └──────────────────┘ │
└─────────────────────────┬───────────────────────────────────────┘
                          │ Audio buffer
                          ▼
┌─────────────────────────────────────────────────────────────────┐
│                    TX Controller                                │
│  (Max duration limit, VOX support)                              │
└─────────────────────────┬───────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────────┐
│                    Audio Output                                 │
└─────────────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────────┐
│              VAD Guard Period (1.5s)                            │
│  (Prevents feedback from own voice)                             │
└─────────────────────────────────────────────────────────────────┘
```

---

## State Transitions

```
IdleListening
    ↓ (SpeechStart)
 ReceivingSpeech
    ↓ (SpeechEnd)
Thinking
    ↓ (Response ready)
Transmitting
    ↓ (Playback complete)
IdleListening

Transmitting
    ↓ (SpeechStart detected)
ReceivingSpeech (interrupt)
```

---

## VAD Improvements (NEW)

The new `EnergyVAD` class includes several reliability improvements:

### Pre-speech Buffer
A ring buffer captures the last 300ms of audio continuously. When speech is detected, this buffer is prepended to capture word beginnings that might be cut off.

### Adaptive Thresholding
The noise floor is tracked using exponential moving average during silence periods. The speech detection threshold adapts to ambient noise levels while staying within configured bounds.

### Fixed Pause Tolerance
The pause tolerance logic now correctly handles natural pauses during speech:
- Pauses shorter than `pause_tolerance_ms` don't trigger end detection
- Only sustained silence beyond `end_silence_ms` ends the utterance

### Hysteresis
Two thresholds prevent oscillation:
- `start_threshold` - must exceed to start speech detection
- `end_threshold` (50% of start) - must drop below to end speech

---

## TTS Improvements (NEW)

The new `PiperTTS` class addresses latency issues:

### Path Caching
The Piper binary path is found once at startup and cached, avoiding repeated `which piper` calls.

### LRU Phrase Cache
Short phrases (<50 chars) are cached in an LRU cache (100 entries). Common responses like "roger" are served instantly from cache.

### Warmup Support
Call `warmup()` at initialization to:
1. Find and verify the Piper binary
2. Verify the voice model exists
3. Pre-load common phrases into cache

---

## Conversation Memory (NEW)

The `ConversationMemory` class provides bounded context management:

### Message Limits
- Maximum 20 messages in history (configurable)
- Maximum 2000 estimated tokens (configurable)
- Oldest messages pruned automatically

### Persistence
- Save/load conversation history to JSON files
- Optional auto-save after each message

### Export Formats
- `to_json()` - Full JSON for Ollama/OpenAI APIs
- `to_json_strings()` - Vector of JSON strings for existing code

---

## Timing Constraints

- **Frame processing**: ~20ms per frame
- **VAD latency**: Minimal (frame-by-frame)
- **STT latency**: Target < 500ms for acknowledgement
- **LLM latency**: Hard timeout 30000ms (configurable)
- **TTS latency**: Depends on Piper; cached phrases instant
- **Total end-to-end**: Target < 2s for substantive answer

---

## Threading Model

**v1: Single-threaded**
- All processing in main loop
- LLM request uses thread with timeout
- Audio I/O callbacks run in PortAudio threads

**Future**: Could add async processing for STT/LLM/TTS pipeline

---

## Error Handling

- Audio I/O failures: Log and exit
- STT failures: Return empty transcript
- LLM timeouts: Return fallback response ("Stand by.")
- TTS failures: Return empty audio buffer
- All errors logged to session recorder

---

## Configuration

### New Options

```json
{
  "vad": {
    "pre_speech_buffer_ms": 300,  // NEW: Pre-speech buffer duration
    "adaptive_threshold": true     // NEW: Enable adaptive threshold
  },
  "tts": {
    "piper_path": "",              // NEW: Custom piper binary path
    "preload_phrases": [...]       // NEW: Phrases to preload at startup
  },
  "memory": {                      // NEW: Conversation memory config
    "max_messages": 20,
    "max_tokens": 2000,
    "system_prompt": "...",
    "persistence_path": "",
    "auto_save": false
  }
}
```

See `config/config.example.json` for a complete example.

---

## Session Logging

Every run creates:
```
sessions/YYYYMMDD_HHMMSS/
  ├── raw_input.wav
  ├── utterance_1.wav
  ├── tts_1.wav
  └── session_log.json
```

---

## Extending the Framework

### Adding a New VAD Implementation

1. Create `include/vad/my_vad.h` implementing `vad::IVAD`
2. Create `src/vad/my_vad.cpp` with implementation
3. Add to CMakeLists.txt
4. Use via factory pattern or direct instantiation

### Adding a New TTS Implementation

1. Create `include/tts/my_tts.h` implementing `tts::ITTS`
2. Create `src/tts/my_tts.cpp` with implementation
3. Add to CMakeLists.txt
4. Implement `warmup()` for initialization
5. Handle caching internally or use the provided LRU cache

---

## Migration Guide

The new components are additive - existing code continues to work. To migrate:

### Using New VAD
```cpp
// Old
VADEndpointing vad(config.vad);

// New
vad::EnergyVADConfig vad_config;
vad_config.threshold = config.vad.threshold;
vad_config.adaptive_threshold = true;  // New feature!
vad::EnergyVAD vad(vad_config);
```

### Using New TTS
```cpp
// Old
TTSEngine tts(config.tts);
AudioBuffer audio = tts.synth_vox(text);

// New
tts::PiperConfig tts_config;
tts_config.voice_path = config.tts.voice_path;
tts::PiperTTS tts(tts_config);
tts.warmup();  // New: preload at startup
auto result = tts.synth_with_preroll(text);
if (result.ok()) {
    AudioBuffer audio = result.audio;
}
```

### Using Conversation Memory
```cpp
// New
memory::ConversationConfig mem_config;
mem_config.max_messages = 20;
mem_config.system_prompt = "You are...";
memory::ConversationMemory memory(mem_config);

memory.add_user_message(transcript.text);
auto history = memory.to_json_strings();
auto response = llm.generate_with_tools("", tools_json, history, ...);
memory.add_assistant_message(response.content);
```

---

## Replay Mode (Future)

Replay from `raw_input.wav`:
- Skip audio I/O
- Use recorded audio as input
- Reproduce same transcript/response (if LLM deterministic)
