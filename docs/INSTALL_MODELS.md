# Installing QWEN Model and Piper TTS

## 1. Installing QWEN Model for llama.cpp

### Option A: Using huggingface-cli (Recommended)

```bash
# Install huggingface-cli
pip install huggingface_hub

# Create models directory
mkdir -p ~/models/llm

# Download QWEN model (smaller, faster for radio use)
cd ~/models/llm
huggingface-cli download Qwen/Qwen2-1.5B-Instruct-GGUF \
    qwen2-1_5b-instruct-q5_k_m.gguf \
    --local-dir . \
    --local-dir-use-symlinks False
```

### Option B: Manual Download

1. Visit: https://huggingface.co/Qwen/Qwen2-1.5B-Instruct-GGUF
2. Download a quantized model (recommended: `q5_k_m` or `q4_k_m` for balance of quality/speed)
3. Save to `~/models/llm/`

### Recommended Models for Radio Use

- **Qwen2-1.5B-Instruct-q5_k_m.gguf** - Good balance (recommended)
- **Qwen2-1.5B-Instruct-q4_k_m.gguf** - Faster, slightly lower quality
- **Qwen2-0.5B-Instruct-q5_k_m.gguf** - Fastest, good for short responses

### Starting llama.cpp Server with QWEN

```bash
cd /path/to/whisper.cpp/build/bin
./whisper-server \
    -m ~/models/llm/qwen2-1_5b-instruct-q5_k_m.gguf \
    -c 2048 \
    --port 8080 \
    -ngl 35  # Offload layers to GPU (adjust based on your GPU)
```

## 2. Installing Piper TTS

### Option A: Using pip (Python package)

```bash
pip install piper-tts
```

Then use it in your code or via command line.

### Option B: Native Binary (Recommended for C++ integration)

```bash
# Clone and build Piper
cd ~/dev
git clone https://github.com/rhasspy/piper.git
cd piper
make

# This creates ./piper binary
# Add to PATH or use full path
export PATH="$PATH:$(pwd)"
```

### Option C: Pre-built Binary

Download from: https://github.com/rhasspy/piper/releases

```bash
# Download for macOS
cd ~/dev
wget https://github.com/rhasspy/piper/releases/download/v1.2.0/piper_macos_amd64.tar.gz
tar -xzf piper_macos_amd64.tar.gz
# piper binary is now in the extracted directory
```

### Downloading Piper Voice Models

```bash
# Create voice models directory
mkdir -p ~/models/piper

# Download a voice (English US, medium quality)
cd ~/models/piper
wget https://huggingface.co/rhasspy/piper-voices/resolve/main/en/en_US/lessac/medium/en_US-lessac-medium.onnx
wget https://huggingface.co/rhasspy/piper-voices/resolve/main/en/en_US/lessac/medium/en_US-lessac-medium.onnx.json

# Or use piper-tts to download
piper-tts --download-voice en_US-lessac-medium
```

### Testing Piper

```bash
# Test with a simple phrase
echo "Hello, this is a test." | piper --model ~/models/piper/en_US-lessac-medium.onnx --output_file test.wav

# Play the output
afplay test.wav
```

## 3. Update config.json

After installation, update your config:

```json
{
  "llm": {
    "endpoint": "http://localhost:8080/completion",
    "timeout_ms": 2000,
    "max_tokens": 100,
    "model_name": "qwen"
  },
  "tts": {
    "voice_path": "~/models/piper/en_US-lessac-medium.onnx",
    "vox_preroll_ms": 200,
    "output_gain": 1.0
  }
}
```

## 4. Quick Start Script

Create a startup script:

```bash
#!/bin/bash
# start-memo-rf.sh

# Start llama.cpp server in background
cd /path/to/whisper.cpp/build/bin
./whisper-server -m ~/models/llm/qwen2-1_5b-instruct-q5_k_m.gguf -c 2048 --port 8080 &
SERVER_PID=$!

# Wait for server to start
sleep 3

# Start memo-rf
cd /path/to/memo-RF/build
./memo-rf ../config/config.json

# Cleanup
kill $SERVER_PID
```

## Troubleshooting

### QWEN Model Issues
- **Model too large**: Use smaller quantized version (q4_k_m or q3_k_m)
- **Out of memory**: Reduce `-ngl` parameter or use CPU-only
- **Server won't start**: Check if port 8080 is available

### Piper Issues
- **Command not found**: Add piper to PATH or use full path
- **Model not found**: Check voice_path in config.json
- **No audio output**: Check file permissions and audio device
