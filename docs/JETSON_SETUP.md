# Jetson Orin Nano / Linux Setup

Short guide to build and run memo-RF on Linux (Ubuntu) or Nvidia Jetson Orin Nano.

## 1. System dependencies

```bash
sudo apt-get update
sudo apt-get install -y build-essential libportaudio2 libcurl4-openssl-dev pkg-config
```

For nlohmann/json:

```bash
sudo apt-get install -y nlohmann-json3-dev
```

If the package is not available, place `json.hpp` in `third_party/nlohmann/json.hpp` (see README).

## 2. whisper.cpp

Clone and build (CPU; on Jetson you can enable CUDA if desired):

```bash
cd ~
git clone https://github.com/ggerganov/whisper.cpp.git
cd whisper.cpp
make
```

Download a Whisper model (e.g. small English) and note the path, e.g. `~/models/whisper/ggml-small.en-q5_1.bin`.

## 3. Piper TTS

Option A: build from source (recommended on Jetson):

```bash
cd ~/dev  # or wherever you keep repos
git clone https://github.com/rhasspy/piper.git
cd piper
make
# Binary: $(pwd)/piper
```

Option B: use the install script (downloads Linux pre-built binary):

```bash
./scripts/install_piper.sh
# Choose option 2 for pre-built; script detects Linux and downloads piper_linux_arm64.tar.gz (Jetson) or piper_linux_amd64.tar.gz
```

Put Piper voice models in e.g. `~/models/piper/` (see `docs/INSTALL_MODELS.md`).

## 4. Build memo-RF

```bash
cd /path/to/memo-RF
./build.sh
```

If whisper.cpp is not in a sibling directory or in `$HOME/dev/whisper.cpp` / `$HOME/whisper.cpp`:

```bash
mkdir -p build
cd build
cmake -DWHISPER_DIR=/path/to/whisper.cpp ..
make
```

## 5. Configuration

```bash
cp config/config.json.example config/config.json
```

Edit `config/config.json`:

- **stt.model_path**: e.g. `~/models/whisper/ggml-small.en-q5_1.bin` (paths with `~` are expanded at load)
- **tts.voice_path**: e.g. `~/models/piper/en_US-lessac-medium.onnx`
- **tts.piper_path**: set to the full path to the `piper` binary if it is not in PATH (e.g. `~/dev/piper/piper`)
- **tts.espeak_data_path**: leave empty for default (`/usr/share/espeak-ng-data` on Linux), or set if you installed espeak-ng elsewhere
- **llm.endpoint**: e.g. `http://localhost:8080/completion` if you run llama.cpp server locally

## 6. Run

Start the LLM server (if using llama.cpp) in one terminal:

```bash
./scripts/start_server.sh qwen 8080
# Or set MEMO_RF_LLAMA_DIR or LLAMA_CPP_DIR to your llama.cpp clone
```

In another terminal:

```bash
cd build
./memo-rf ../config/config.json
```

## Transfer from another machine

1. Copy the memo-RF repo (and optionally `~/models/`) to the Jetson, e.g.:
   ```bash
   rsync -av --exclude build /path/to/memo-RF user@jetson:~/memo-RF/
   rsync -av ~/models user@jetson:~/
   ```
2. On the Jetson: install system deps, build whisper.cpp, install Piper, build memo-RF, then configure and run as above.
3. Use the same `config.json` paths with `~` so they resolve to the Jetson user’s home.
