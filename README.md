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
- **PortAudio** (libportaudio2 + portaudio19-dev on Ubuntu/Jetson, portaudio via Homebrew on Mac)
- **whisper.cpp** (source code repository, must be built)
  - Clone: `git clone https://github.com/ggerganov/whisper.cpp.git`
  - Build: `cd whisper.cpp && make` (on macOS, use `make GGML_METAL=1` for Metal GPU acceleration and lower STT latency)
  - The build should create `libwhisper.a` or `libwhisper.dylib` in the whisper.cpp directory
  - **Low latency on Mac:** Build whisper.cpp with Metal support so STT uses the GPU. If your build has no Metal, set `stt.use_gpu` to `false` in config to avoid startup errors.
- **nlohmann/json** (header-only library)
- **libcurl** (for LLM HTTP client)
- **Piper** TTS (external binary; set `tts.piper_path` in config or ensure in PATH)

All model and binary paths are configurable via `config.json`; paths support `~` expansion (resolved at load time). Optional env: `MEMO_RF_LLM_MODEL`, `MEMO_RF_LLAMA_DIR` / `LLAMA_CPP_DIR` for scripts.

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

1. **Whisper model**: Download `ggml-small-q5_1.bin` (small multilingual) from whisper.cpp; set `stt.language` in config for your language
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

### Linux / Jetson Orin Nano

On Ubuntu or Jetson (aarch64):

1. Install system deps: `sudo apt-get install -y build-essential libportaudio2 portaudio19-dev libcurl4-openssl-dev pkg-config`
2. Install nlohmann/json: `sudo apt-get install -y nlohmann-json3-dev` or place `json.hpp` in `third_party/nlohmann/`
3. Build whisper.cpp (and optionally llama.cpp) for your platform; for CUDA on Jetson Orin Nano see **docs/JETSON_SETUP.md** (CUDA section).
4. Build memo-RF: `./build.sh` or `mkdir build && cd build && cmake -DWHISPER_DIR=/path/to/whisper.cpp .. && make`
5. Install Piper (see `docs/INSTALL_MODELS.md`); set `tts.voice_path` and optionally `tts.piper_path` / `tts.espeak_data_path` in config
6. Copy `config/config.json.example` to `config/config.json` and set `stt.model_path`, `tts.voice_path`, `llm.endpoint`

See `docs/JETSON_SETUP.md` for a concise Jetson setup and transfer steps.

## Configuration

1. Copy the example config:
   ```bash
   cp config/config.json.example config/config.json
   ```

2. Edit `config/config.json` and update:
   - `stt.model_path`: Path to your Whisper model (e.g. `~/models/whisper/ggml-small-q5_1.bin`; `~` is expanded at load). Use the multilingual small model and set `stt.language` for your language.
   - `tts.voice_path`: Path to your Piper voice model (e.g. `~/models/piper/en_US-lessac-medium.onnx`)
   - `tts.piper_path`: Optional; Piper binary path (empty = auto-detect). Set on Linux/Jetson if piper is not in PATH.
   - `tts.espeak_data_path`: Optional; espeak-ng data dir (empty = platform default: `/usr/share/espeak-ng-data` on Linux, `/opt/homebrew/share/espeak-ng-data` on macOS)
   - `llm.endpoint`: LLM server endpoint (default: http://localhost:8080/completion)
   - `llm.translation_model`: Optional. When `llm.agent_persona` is `"translator"`, set this (e.g. `"translategemma"`) to use a dedicated translation model for lower latency. Run `ollama pull translategemma` first. If empty, the main `llm.model_name` is used.
   - `stt.use_gpu`: When true (default), Whisper uses Metal on macOS if the whisper.cpp build supports it. Set to false if your whisper.cpp was built without Metal.
   - `vad.silence_threshold`: RMS below this (e.g. 0.02) counts as silence; frames between this and `vad.threshold` are speech dips and reset the silence counter. Lower = stricter silence; slightly higher for noisier environments.
   - `vad.start_frames_required`: Consecutive speech frames required to trigger SpeechStart (default 2; reduces false start on pops).
   - `llm.keep_alive_sec`: (Ollama) Seconds to keep model in memory after each request (0 = default; e.g. 300 to avoid load_duration on subsequent requests).
   - `llm.warmup_translation_model`: When true and `llm.translation_model` is set, send one tiny request at startup to load the model (Ollama only); first user request then avoids load_duration.
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
- **Response language:** Set `llm.response_language` to `"es"`, `"fr"`, or `"de"` to have any persona respond in that language; the Piper voice is chosen automatically from `config/language_voices.json`. See `docs/VOICE_CONFIG.md`.
- **Translator persona (low latency):** For the translator persona, set `llm.translation_model` to `translategemma` in config and run `ollama pull translategemma`. The agent will use this dedicated translation model instead of the general-purpose LLM for faster, translation-only responses.

**Low latency (translator):** To reduce time-to-first-audio when using the translator persona: set `llm.keep_alive_sec` (e.g. 300) so the model stays loaded; set `llm.warmup_translation_model` to `true` so the first request doesn't pay load_duration; set `vad.end_of_utterance_silence_ms` (e.g. 350–450) and `vad.silence_threshold` (e.g. 0.02); optionally set `tts.vox_preroll_ms` (e.g. 200) to trade a bit of VOX margin for faster first audio; add short Spanish phrases to `tts.preload_phrases` (e.g. "Un momento.", "over.") so fallbacks can hit the TTS cache. See `config/config.json.example` for a reference.

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
- Install piper: https://github.com/rhasspy/piper or run `./scripts/install_piper.sh`
- Set `tts.piper_path` in config to the full path to the piper binary, or ensure `piper` is in PATH
- Verify `tts.voice_path` in config

## Documentation

Additional documentation is available in the `docs/` folder:
- `ARCHITECTURE.md` - System architecture details
- `WAKE_WORD.md` - Wake word ("hey memo"), continual STT, and half-duplex channel clear
- `INSTALL_MODELS.md` - Model installation guide
- `VOICE_CONFIG.md` - Voice configuration options
- `QUICKSTART.md` - Quick start guide
- `JETSON_SETUP.md` - Linux / Jetson Orin Nano setup and transfer
- `NEXT_STEPS.md` - Future development plans
- `RUNNING.md` - Running with agentic tools (Ollama, tools.enabled, troubleshooting)
- `RUN_AS_SERVICE.md` - Run as a systemd user service (survive closing terminal / Jetson session)

## License

[Your License Here]
