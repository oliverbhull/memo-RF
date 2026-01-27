# Quick Start Guide

## Prerequisites

1. **Install dependencies** (macOS):
   ```bash
   brew install portaudio cmake libcurl nlohmann-json
   ```

2. **Build whisper.cpp** (if not already built):
   ```bash
   cd /Users/oliverhull/dev/whisper.cpp
   make
   ```
   This should create `libwhisper.a` (or `libwhisper.dylib` on macOS) in the whisper.cpp directory.

3. **Download Whisper model**:
   ```bash
   # Download ggml-small.en-q5_1.bin to ../whisper.cpp/models/
   ```

4. **Install Piper TTS**:
   ```bash
   # Follow: https://github.com/rhasspy/piper
   # Download voice model to memo-RF/models/
   ```

5. **Start llama.cpp server**:
   ```bash
   cd ../whisper.cpp
   ./server -m models/llama-2-7b.gguf -c 2048 --port 8080
   ```

## Build

```bash
./build.sh
# or
mkdir build && cd build && cmake .. && make
```

## Configure

Edit `config/config.json`:
- Set `stt.model_path` to your Whisper model
- Set `tts.voice_path` to your Piper voice model
- Set audio devices (or use "default")

## Run

```bash
cd build
./memo-rf ../config/config.json
```

## Test

1. Speak into your microphone/radio input
2. Wait for "copy." acknowledgement
3. Wait for LLM response
4. Check `sessions/` directory for logs

## Troubleshooting

### "Audio device not found"
- Run: `./memo-rf --list-devices`
- Update `config.json` with correct device name

### "Whisper model not loading"
- Check model path in config
- Ensure model file exists

### "LLM timeout"
- Verify llama.cpp server is running: `curl http://localhost:8080/health`
- Check server logs

### "Piper not found"
- Ensure `piper` is in PATH: `which piper`
- Test: `piper --help`
