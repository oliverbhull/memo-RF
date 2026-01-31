# Memo-RF: Offline Radio Voice Agent

A bare-bones, fully local voice agent that talks over a BaoFeng UV-5R using VOX for transmit control.

## Architecture

Single C++ binary with modular components:
- **AudioIO**: PortAudio-based input/output
- **VAD + Endpointing**: Energy-based voice activity detection
- **STT Engine**: Whisper.cpp for transcription
- **Router**: Fast path rules + LLM routing
- **LLM Client**: HTTP client for Ollama or llama.cpp server (e.g. Qwen)
- **TTS Engine**: Piper (external process) for speech synthesis
- **TX Controller**: VOX-based transmission control
- **State Machine**: Core loop state management
- **Session Recorder**: Full session logging and replay support

## Requirements

### Dependencies

- **CMake** 3.15+
- **C++17** compiler
- **PortAudio** (libportaudio2-dev on Ubuntu, portaudio via Homebrew on Mac)
- **whisper.cpp** (source code repository, must be built)
  - Clone: `git clone https://github.com/ggerganov/whisper.cpp.git`
  - Build: `cd whisper.cpp && make`
  - The build should create `libwhisper.a` or `libwhisper.dylib` in the whisper.cpp directory
- **nlohmann/json** (header-only library)
- **libcurl** (for LLM HTTP client)
- **Piper** TTS (external binary, must be in PATH)

#### Installing nlohmann/json

**macOS (Homebrew):**
```bash
brew install nlohmann-json
```

**Manual installation:**
```bash
mkdir -p third_party/nlohmann
curl -L -o third_party/nlohmann/json.hpp \
  https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp
```

### Models

1. **Whisper model**: Download `ggml-small.en-q5_1.bin` from whisper.cpp
2. **Piper voice**: Download an English voice model (e.g., `en_US-lessac-medium.onnx`)

## Building

```bash
./build.sh
```

Or manually:
```bash
mkdir build
cd build
cmake ..
make
```

**Note**: If whisper.cpp is in a non-standard location, specify it:
```bash
cd build
cmake -DWHISPER_DIR=/path/to/whisper.cpp ..
make
```

## Configuration

1. Copy the example config:
   ```bash
   cp config/config.json.example config/config.json
   ```

2. Edit `config/config.json` and update:
   - `stt.model_path`: Path to your Whisper model
   - `tts.voice_path`: Path to your Piper voice model
   - `llm.endpoint`: LLM server endpoint (default: http://localhost:8080/completion)
   - Audio device names if not using "default"
   - `wake_word.enabled`: When true (default), the agent responds only when the transcript contains "hey memo"; when false, it responds to every utterance (legacy).
   - `tx.channel_clear_silence_ms`: Half-duplex: wait this many ms of silence after the last speech before keying up (default 500). See `docs/WAKE_WORD.md`.

See `docs/INSTALL_MODELS.md` for detailed model installation instructions.
See `docs/VOICE_CONFIG.md` for voice configuration options.
See `docs/WAKE_WORD.md` for wake-word ("hey memo") and half-duplex channel-clear behavior.

### Personas

You can swap the agent’s role by name instead of editing the system prompt. Set `llm.agent_persona` in `config.json` to a persona id; the loader reads `personas.json` from the same directory as the config file and uses that persona’s system prompt (and optional display name for logs). If `agent_persona` is set, it overrides any inline `llm.system_prompt`.

- **Persona library:** `config/personas.json` (or copy from `config/personas.json.example`). Each entry has `name` and `system_prompt`. Ids are simple strings: `manufacturing`, `security`, `warehouse`, etc. (not display names).
- **Built-in ids (examples):** Business — `manufacturing`, `retail`, `hospitality`, `healthcare`, `security`, `warehouse`, `construction`, `film_production`, `ski_patrol`, `theme_park`, `airline_ramp`, `maritime`, `wildland_fire`, `school_admin`, `golf_course`. Demo — `ems_dispatch`, `pit_crew`, `mission_control`, `food_truck_rally`. Fun — `trucker_cb`, `submarine`, `detective_noir`, `drill_sergeant`, `butler`, `surfer`, `astronaut`, `ghost_hunters`, `zombie_survivor`, `sports_coach`, `wedding_planner`, `ranch_hand`, `asshole`. See `config/personas.json` for the full list.
- **Example:** In `config.json`, set `"agent_persona": "manufacturing"` under `llm`. Omit `agent_persona` to use the inline `system_prompt` as before.

### Finding Audio Devices

```bash
./memo-rf --list-devices
```

## Usage

### Prerequisites

1. **Start LLM server** (Ollama or llama.cpp):
   - **Ollama**: `ollama serve` then `ollama pull qwen2.5:7b`; set `llm.endpoint` to `http://localhost:11434/api/chat` and `llm.model_name` to `qwen2.5:7b`
   - **llama.cpp**: `./scripts/start_server.sh qwen 8080` (uses Qwen GGUF) or `./server -m models/qwen2-1_5b-instruct-q5_k_m.gguf -c 2048 --port 8080`

2. **Ensure Piper is in PATH**:
   ```bash
   which piper
   ```

### Run

```bash
./memo-rf [config.json]
```

### Operation

1. User speaks into radio (PTT on radio)
2. Agent listens on computer audio input; all speech is transcribed (continual STT)
3. When **wake word enabled** (default): the agent responds on the channel only when the transcript contains "hey memo"; otherwise it stays silent. Before keying up, it waits for the channel to be clear (`tx.channel_clear_silence_ms` of silence) so it does not talk over someone else (half-duplex).
4. Agent responds over radio (VOX triggers transmit)
5. All sessions are logged to `sessions/`

## Session Logging

Each session creates a directory in `sessions/` with:
- `raw_input.wav`: Complete raw audio input
- `utterance_N.wav`: Segmented utterances
- `tts_N.wav`: TTS output audio
- `session_log.json`: Complete event log with timings

## Replay Mode

(To be implemented) Replay a session from recorded audio:

```bash
./memo-rf --replay sessions/20250126_120000/raw_input.wav
```

## Latency Targets

- Speech end → Acknowledgement start: ≤ 500 ms (goal)
- Speech end → Answer start: ≤ 2.0 s (hard target)
- Max transmit window: 8 s per response

## Fast Path Rules

Pre-configured fast responses (no LLM):
- "copy" → "copy."
- "roger" → "roger."
- "affirmative" → "affirmative."
- "negative" → "negative."
- "stand by" → "stand by."
- "over" → "over."

## Future Upgrades (not in v1)

- GPIO PTT control (replace VOX)
- Streaming STT partials
- Streaming LLM tokens
- Multi-turn memory
- Pi AI HAT+ 2 Hailo Ollama backend

## Troubleshooting

### Audio device not found
- Run `./memo-rf --list-devices` to see available devices
- Update `config.json` with correct device name or use "default"

### Whisper model not loading
- Check model path in config
- Ensure model file exists and is readable
- Verify whisper.cpp is built and libraries are linked

### LLM timeout
- Check Ollama: `curl http://localhost:11434/api/tags` or llama.cpp: `curl http://localhost:8080/health`
- Increase `timeout_ms` in config if needed
- Check network connectivity

### Piper not found
- Install piper: https://github.com/rhasspy/piper
- Ensure `piper` binary is in PATH
- Verify voice model path in config

## Documentation

Additional documentation is available in the `docs/` folder:
- `ARCHITECTURE.md` - System architecture details
- `WAKE_WORD.md` - Wake word ("hey memo"), continual STT, and half-duplex channel clear
- `INSTALL_MODELS.md` - Model installation guide
- `VOICE_CONFIG.md` - Voice configuration options
- `QUICKSTART.md` - Quick start guide
- `NEXT_STEPS.md` - Future development plans
- `RUNNING.md` - Running with agentic tools (Ollama, tools.enabled, troubleshooting)

## License

[Your License Here]
