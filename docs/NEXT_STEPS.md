# Next Steps - Getting memo-RF Running

## ✅ Build Complete!

The build succeeded! Your executable is at: `build/memo-rf`

## 1. Configure Audio Devices

First, list available audio devices:

```bash
cd build
./memo-rf --list-devices
```

Update `config/config.json` with your device names or use "default".

## 2. Configure Model Paths

Edit `config/config.json`:

```json
{
  "stt": {
    "model_path": "~/models/whisper/ggml-small.en-q5_1.bin"
  },
  "tts": {
    "voice_path": "path/to/piper/voice.onnx"
  }
}
```

## 3. Start LLM Server (Ollama or llama.cpp with Qwen)

In a separate terminal:

- **Ollama**: `ollama serve` then `ollama pull qwen2.5:7b`. Set `llm.endpoint` to `http://localhost:11434/api/chat` and `llm.model_name` to `qwen2.5:7b` in config.json.
- **llama.cpp**: `./scripts/start_server.sh qwen 8080` or `./server -m /path/to/qwen2-1_5b-instruct-q5_k_m.gguf -c 2048 --port 8080`

Update the endpoint in config.json to match (Ollama or http://localhost:8080).

## 4. Install/Configure Piper TTS

Piper needs to be in your PATH:

```bash
# Check if piper is installed
which piper

# If not, install it:
# https://github.com/rhasspy/piper
```

Download a voice model and update `config/config.json`:
```json
{
  "tts": {
    "voice_path": "/path/to/en_US-lessac-medium.onnx"
  }
}
```

## 5. Test Run

```bash
cd build
./memo-rf ../config/config.json
```

## 6. Connect Radio Hardware

- **Input**: Connect radio audio output to computer audio input
- **Output**: Connect computer audio output to radio audio input (for VOX)

## 7. Test the Pipeline

1. Speak into the radio (or microphone)
2. Wait for "copy." acknowledgement
3. Wait for LLM response
4. Check `sessions/` directory for logs

## Troubleshooting

### "Audio device not found"
- Run `./memo-rf --list-devices` to see available devices
- Update config.json with correct device name

### "Whisper model not loading"
- Verify model path in config.json
- Ensure model file exists and is readable

### "LLM timeout"
- Check llama.cpp server is running: `curl http://localhost:8080/health`
- Verify endpoint in config.json matches server port

### "Piper not found"
- Ensure `piper` is in PATH: `which piper`
- Test: `piper --help`

## Session Logs

All sessions are logged to `sessions/YYYYMMDD_HHMMSS/`:
- `raw_input.wav` - Complete audio input
- `utterance_N.wav` - Segmented utterances
- `tts_N.wav` - TTS output
- `session_log.json` - Event log with timings

## Quick Test (Minimal Config)

For a quick test without full setup:

1. **Skip LLM** - Comment out LLM calls in main.cpp temporarily
2. **Use fast path only** - Test with "copy", "roger", etc.
3. **Skip TTS** - Return empty audio buffer temporarily
4. **Test VAD** - Just verify speech detection works

## Production Setup

Once everything works:

1. Tune VAD thresholds for your environment
2. Adjust latency targets in code if needed
3. Set up proper audio routing for radio
4. Configure GPIO PTT (future upgrade, v1 uses VOX)
