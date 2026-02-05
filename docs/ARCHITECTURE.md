# Memo-RF Architecture

## Overview

Memo-RF is a single-threaded C++ application with a modular architecture. The core loop processes audio frames sequentially, maintaining state through a state machine. The pipeline is STT → Router → LLM (single-turn) or fast path → TTS → TX.

---

## Directory Structure

```
memo-RF/
├── include/
│   ├── agent.h, audio_io.h, config.h, common.h, logger.h, path_utils.h
│   ├── vad_endpointing.h, stt_engine.h, router.h, llm_client.h
│   ├── tts_engine.h, tx_controller.h, state_machine.h, session_recorder.h
│   ├── transcript_gate.h, utils.h
│   └── (optional) agent_pipeline.h
├── src/
│   ├── main.cpp, agent.cpp, (optional) agent_pipeline.cpp
│   ├── audio_io.cpp, config.cpp, path_utils.cpp, logger.cpp
│   ├── vad_endpointing.cpp, stt_engine.cpp, router.cpp, llm_client.cpp
│   ├── tts_engine.cpp, tx_controller.cpp, state_machine.cpp, session_recorder.cpp
│   └── ...
├── config/             # config.json, personas.json, language_voices.json
└── docs/               # Documentation
```

---

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

### Router
- Fast path rule matching (keyword → response)
- LLM routing decision
- Returns `Plan` objects (Speak, SpeakAckThenAnswer, Fallback)

### LLMClient
- **HTTP client** for llama.cpp/Ollama server
- Timeout enforcement
- Radio-style prompt formatting
- Response cleaning
- **Single-turn** only (no conversation memory; empty history per request)

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
- Five states: `IdleListening`, `ReceivingSpeech`, `Thinking`, `WaitingForChannelClear`, `Transmitting`
- When **wake word enabled**: continual STT (every segment transcribed); respond only when transcript contains "hey memo"; response is stored and we enter `WaitingForChannelClear` until channel is clear (half-duplex), then transmit.
- When **wake word disabled**: `ReceivingSpeech` → `Thinking` on SpeechEnd; `Thinking` → `Transmitting` on response ready; transmit immediately.
- `WaitingForChannelClear`: response audio ready but we wait for `channel_clear_silence_ms` of silence after last SpeechEnd before keying up; if SpeechStart (someone else talking), we go to `ReceivingSpeech` and wait for them to finish, then re-check channel clear.
- Transmission interruption logic (e.g. SpeechStart during Transmitting → ReceivingSpeech).

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
│                 VAD (VADEndpointing)                            │
│  Energy-based detection, configurable silence thresholds       │
└─────────────────────────┬───────────────────────────────────────┘
                          │ SpeechEnd event + audio buffer
                          ▼
┌─────────────────────────────────────────────────────────────────┐
│                    STT (Whisper)                                │
└─────────────────────────┬───────────────────────────────────────┘
                          │ Transcript
                          ▼
┌─────────────────────────────────────────────────────────────────┐
│                    Transcript Gate                              │
│  (Block low-signal / blank transcripts)                         │
└─────────────────────────┬───────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────────┐
│                    Router                                       │
│  Fast-path keywords → Direct response                           │
│  Everything else → LLM (single-turn, no tools)                   │
└─────────────────────────┬───────────────────────────────────────┘
                          │
            ┌─────────────┴─────────────┐
            │                           │
            ▼ Fast Path                 ▼ LLM Path
┌───────────────────┐        ┌────────────────────────────────────┐
│ Cached Response   │        │ LLM (Ollama/llama.cpp)              │
│                   │        │  Single-turn: prompt + empty history│
└─────────┬─────────┘        └────────────────┬───────────────────┘
          │                                    │
          └─────────────────┬──────────────────┘
                            │ Response text
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                    TTS (TTSEngine / Piper)                      │
│  Phrase cache, VOX pre-roll, output gain                        │
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

**When wake word enabled** (continual STT; respond only on "hey memo"; half-duplex):

- Every VAD segment is transcribed. If transcript does not contain "hey memo", no response (stay IdleListening).
- If transcript contains "hey memo", strip it, use remainder as command, build response, then enter `WaitingForChannelClear` (do not key up yet). When channel has been silent for `tx.channel_clear_silence_ms`, transmit. If SpeechStart while waiting (someone else talking), go to ReceivingSpeech; on their SpeechEnd return to WaitingForChannelClear and reset the silence timer.

```
IdleListening
    ↓ (SpeechStart)
 ReceivingSpeech
    ↓ (SpeechEnd) → STT; no "hey memo" → IdleListening
    ↓ (SpeechEnd) → "hey memo" + command → build response → WaitingForChannelClear
WaitingForChannelClear
    ↓ (channel clear: silence >= channel_clear_silence_ms)
Transmitting
    ↓ (Playback complete)
IdleListening

WaitingForChannelClear
    ↓ (SpeechStart, interrupt on channel)
ReceivingSpeech
    ↓ (SpeechEnd)
WaitingForChannelClear
```

**When wake word disabled**:

```
IdleListening → ReceivingSpeech (SpeechStart) → Thinking (SpeechEnd) → Transmitting (response ready) → IdleListening (playback complete)
Transmitting → ReceivingSpeech (SpeechStart, interrupt)
```

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

- All processing in main loop (single-threaded)
- LLM request uses thread with timeout (inside LLMClient)
- Audio I/O callbacks run in PortAudio threads

---

## Error Handling

- Audio I/O failures: Log and exit
- STT failures: Return empty transcript
- LLM timeouts: Return fallback response ("Stand by.")
- TTS failures: Return empty audio buffer
- All errors logged to session recorder

---

## Configuration

See `config/config.json.example` for a complete example. Key sections: `audio`, `vad`, `stt`, `transcript_gate`, `router`, `llm`, `tts`, `tx`, `wake_word`, `session_log_dir`.

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
