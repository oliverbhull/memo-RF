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

---

## Cross-compiling for Jetson

Build memo-RF on a **Linux x86_64 host** and produce an aarch64 binary for the Jetson Orin Nano. Cross-compiling **from macOS** to Linux is not supported out of the box; use a Linux host (VM, second machine, or CI), Docker with a Linux image, or build natively on the Jetson (see above).

### Prerequisites (Linux host)

- CMake 3.15+
- Cross-compiler: `gcc-aarch64-linux-gnu`, `g++-aarch64-linux-gnu`
  ```bash
  sudo apt-get install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
  ```
- A **sysroot** (or staging prefix) for aarch64 containing:
  - PortAudio (e.g. `libportaudio2` + dev)
  - libcurl (e.g. `libcurl4-openssl-dev`)
  - nlohmann/json (e.g. `nlohmann-json3-dev`) or place `json.hpp` in memo-RF `third_party/nlohmann/`
- **whisper.cpp** cross-built for aarch64 (see below)

### 1. Get or build a sysroot

**Option A – From the Jetson:** On the Jetson, install build deps and copy the root to your host:

```bash
# On the Jetson:
sudo apt-get update
sudo apt-get install -y build-essential libportaudio2 libportaudio2-dev \
  libcurl4-openssl-dev pkg-config nlohmann-json3-dev
# Then on the host, rsync or tar the Jetson's / (or /usr) to e.g. ./sysroot-jetson
rsync -av --rsync-path="sudo rsync" user@jetson:/usr ./sysroot-jetson/
# Or a full root if needed; adjust paths below accordingly.
```

**Option B – Ubuntu arm64 rootfs:** Use a minimal Ubuntu arm64 rootfs (e.g. from Docker or `qemu-debootstrap`), install the same packages there, and use that directory as the sysroot.

Set `CMAKE_SYSROOT` (or `JETSON_SYSROOT`) to the path of this sysroot when running the build script.

### 2. Cross-build whisper.cpp

Use the **same** toolchain and sysroot so the library matches the target:

```bash
cd /path/to/whisper.cpp
mkdir -p build-jetson && cd build-jetson
export PKG_CONFIG_SYSROOT_DIR=/path/to/sysroot
export PKG_CONFIG_PATH=/path/to/sysroot/usr/lib/aarch64-linux-gnu/pkgconfig
cmake -DCMAKE_TOOLCHAIN_FILE=/path/to/memo-RF/cmake/aarch64-linux-gnu.cmake \
      -DCMAKE_SYSROOT=/path/to/sysroot \
      ..
make
```

Then set `WHISPER_DIR` to the whisper.cpp **source** directory (e.g. `/path/to/whisper.cpp`); memo-RF's CMake will find the built library in the sibling `build-jetson` (or you can point to the build dir if you adjust paths). If whisper.cpp is built in-tree with Make instead, ensure `WHISPER_DIR` points to the repo root where `libwhisper.a` and headers live.

### 3. Cross-build memo-RF

From the memo-RF repo root:

```bash
export CMAKE_SYSROOT=/path/to/sysroot
export WHISPER_DIR=/path/to/whisper.cpp
./scripts/build_jetson.sh
```

Optional env: `TOOLCHAIN_FILE`, `BUILD_DIR` (default `build-jetson`). The script sets `PKG_CONFIG_SYSROOT_DIR` and `PKG_CONFIG_PATH` so PortAudio and curl are found in the sysroot.

### 4. Deploy and run on the Jetson

Copy the binary (and config/models as needed) to the Jetson:

```bash
scp build-jetson/memo-rf user@jetson:~/memo-RF/build/
# Copy config, models, etc. as in "Transfer from another machine"
```

On the Jetson, run:

```bash
cd ~/memo-RF/build && ./memo-rf ../config/config.json
```
