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

Use the project script (uses `$HOME` and env vars; see `scripts/start_server.sh`):

```bash
./scripts/start_server.sh qwen 8080
# Optional: set MEMO_RF_LLAMA_DIR or LLAMA_CPP_DIR to your llama.cpp clone
# Optional: set MEMO_RF_LLM_MODEL to override model path
```

Or run the server binary directly:

```bash
# After building llama.cpp (e.g. cd $HOME/dev/llama.cpp && cmake -B build -DLLAMA_SERVER=ON && cmake --build build)
./llama-server -m ~/models/llm/qwen2-1_5b-instruct-q5_k_m.gguf -c 2048 --port 8080 --gpu-layers 35 --host 0.0.0.0
# On Jetson: use a lower --gpu-layers or 0 for CPU-only if OOM
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

Use the install script (detects macOS vs Linux and architecture):

```bash
./scripts/install_piper.sh
# Choose option 2; script downloads piper_macos_*.tar.gz or piper_linux_*.tar.gz as appropriate
```

Or download manually from: https://github.com/rhasspy/piper/releases

**macOS:** `piper_macos_arm64.tar.gz` or `piper_macos_amd64.tar.gz`  
**Linux (including Jetson):** `piper_linux_arm64.tar.gz` (aarch64/Jetson) or `piper_linux_amd64.tar.gz` (x86_64)

```bash
cd ~/dev  # or $HOME/models/piper for voice-only
# Example Linux aarch64 (Jetson):
wget https://github.com/rhasspy/piper/releases/download/v1.2.0/piper_linux_arm64.tar.gz
tar -xzf piper_linux_arm64.tar.gz
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
# macOS:
afplay test.wav
# Linux:
aplay test.wav
# or: ffplay -nodisp -autoexit test.wav
```

## 3. Update config.json

After installation, update your config. Paths support `~` expansion at load time. Suggested layout: `~/models/whisper/`, `~/models/piper/`, `~/models/llm/`.

```json
{
  "stt": {
    "model_path": "~/models/whisper/ggml-small.en-q5_1.bin"
  },
  "llm": {
    "endpoint": "http://localhost:8080/completion",
    "timeout_ms": 2000,
    "max_tokens": 100,
    "model_name": "qwen"
  },
  "tts": {
    "voice_path": "~/models/piper/en_US-lessac-medium.onnx",
    "piper_path": "",
    "espeak_data_path": ""
  }
}
```

- **piper_path**: Leave empty to auto-detect piper (PATH or common locations). On Linux/Jetson, set to full path if piper is not in PATH (e.g. `~/dev/piper/piper`).
- **espeak_data_path**: Leave empty for platform default (Linux: `/usr/share/espeak-ng-data`, macOS: `/opt/homebrew/share/espeak-ng-data`).

## 4. Quick Start Script

Start LLM server in one terminal, then memo-RF in another:

```bash
# Terminal 1: start llama.cpp server (uses $HOME/models/llm/ or MEMO_RF_LLM_MODEL)
./scripts/start_server.sh qwen 8080

# Terminal 2: run memo-RF
cd build
./memo-rf ../config/config.json
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
