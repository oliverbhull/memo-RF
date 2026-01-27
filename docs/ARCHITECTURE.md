# Memo-RF Architecture

## Overview

Memo-RF is a single-threaded C++ application with a modular architecture. The core loop processes audio frames sequentially, maintaining state through a state machine.

## Module Responsibilities

### AudioIO
- **PortAudio** wrapper for audio I/O
- Manages input stream (radio audio in) and output stream (radio audio out)
- Frame-based processing (20ms frames @ 16kHz)
- Non-blocking playback queue

### VADEndpointing
- **Energy-based VAD** (simple, deterministic)
- Endpointing with configurable silence thresholds
- Emits `SpeechStart` and `SpeechEnd` events
- Maintains current segment buffer

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
- **HTTP client** for llama.cpp server
- Timeout enforcement
- Radio-style prompt formatting
- Response cleaning

### TTSEngine
- **Piper** integration (external process)
- Phrase caching for common responses
- VOX pre-roll generation (tone burst)
- Output gain control

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

## Data Flow

```
Audio Frame → VAD → [SpeechEnd] → STT → Router → [LLM?] → TTS → TX → Audio Out
                ↓
         SessionRecorder (all stages)
```

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

## Timing Constraints

- **Frame processing**: ~20ms per frame
- **VAD latency**: Minimal (frame-by-frame)
- **STT latency**: Target < 500ms for acknowledgement
- **LLM latency**: Hard timeout 2000ms
- **TTS latency**: Depends on Piper (external)
- **Total end-to-end**: Target < 2s for substantive answer

## Threading Model

**v1: Single-threaded**
- All processing in main loop
- LLM request uses thread with timeout
- Audio I/O callbacks run in PortAudio threads

**Future**: Could add async processing for STT/LLM/TTS pipeline

## Error Handling

- Audio I/O failures: Log and exit
- STT failures: Return empty transcript
- LLM timeouts: Return fallback response
- TTS failures: Return empty audio buffer
- All errors logged to session recorder

## Configuration

Single JSON file (`config/config.json`) with:
- Audio device selection
- VAD thresholds
- Model paths
- LLM endpoint
- TTS voice
- TX limits

## Session Logging

Every run creates:
```
sessions/YYYYMMDD_HHMMSS/
  ├── raw_input.wav
  ├── utterance_1.wav
  ├── tts_1.wav
  └── session_log.json
```

## Replay Mode (Future)

Replay from `raw_input.wav`:
- Skip audio I/O
- Use recorded audio as input
- Reproduce same transcript/response (if LLM deterministic)
