# Jetson Orin Nano / Linux Setup

Short guide to build and run memo-RF on Linux (Ubuntu) or Nvidia Jetson Orin Nano.

## 1. System dependencies

```bash
sudo apt-get update
sudo apt-get install -y build-essential libportaudio2 portaudio19-dev libcurl4-openssl-dev pkg-config
```

For nlohmann/json:

```bash
sudo apt-get install -y nlohmann-json3-dev
```

If the package is not available, place `json.hpp` in `third_party/nlohmann/json.hpp` (see README).

## 2. CUDA on Jetson Orin Nano

To use the GPU for STT (whisper.cpp) and LLM (llama.cpp server) on a Jetson Orin Nano, build those projects with CUDA. memo-RF does not link CUDA directly; it uses whatever whisper library and llama-server you build.

**Prerequisites:** JetPack with CUDA toolkit; CMake 3.15+ (3.18+ recommended to avoid ARM NEON + CUDA compile issues on Orin Nano). You can use `scripts/build_whisper_jetson_cuda.sh` and `scripts/build_llama_jetson_cuda.sh` from the memo-RF repo root to run the builds below.

### whisper.cpp with CUDA

Build **natively on the Jetson** (not cross-compile). Use CMake:

```bash
cd /path/to/whisper.cpp
cmake -B build -DGGML_CUDA=1
```

If nvcc errors about GPU architecture, add `-DCMAKE_CUDA_ARCHITECTURES=87` (Orin Nano is compute capability 8.7):

```bash
cmake -B build -DGGML_CUDA=1 -DCMAKE_CUDA_ARCHITECTURES=87
```

Then:

```bash
cmake --build build
```

The library is produced under `build/` (e.g. `build/src/libwhisper.a` or a shared lib). Set `WHISPER_DIR` to the **source** directory of whisper.cpp (e.g. `/path/to/whisper.cpp`); memo-RF's CMake looks under `WHISPER_DIR/build/src`, `build`, etc.

### llama.cpp with CUDA

Build **natively on the Jetson** with the server enabled:

```bash
cd /path/to/llama.cpp
cmake -B build -DLLAMA_BUILD_SERVER=ON
```

GGML_CUDA is usually auto-enabled when CUDA is found. If you need to force it or set the GPU arch:

```bash
cmake -B build -DLLAMA_BUILD_SERVER=ON -DGGML_CUDA=1 -DCMAKE_CUDA_ARCHITECTURES=87
```

Then:

```bash
cmake --build build --config Release
```

If the build fails with ARM NEON or CUDA-related errors, upgrading CMake (e.g. to 3.18+) often fixes it.

### Config

Set **stt.use_gpu** to `true` in `config/config.json` when whisper is built with CUDA (so STT uses the GPU). Set it to `false` if you use a CPU-only whisper build.

### LLM server on Jetson (8GB)

On an 8GB Orin Nano, use a conservative `--gpu-layers` value (e.g. 20–25 for Qwen 1.5B) to avoid OOM. The project script `scripts/start_server.sh` can detect Jetson and use a lower default; you can also start the server manually with e.g. `--gpu-layers 22`.

## 3. whisper.cpp

Clone and build (CPU; on Jetson use the CUDA steps above if desired):

```bash
cd ~
git clone https://github.com/ggerganov/whisper.cpp.git
cd whisper.cpp
make
```

Download a Whisper model (e.g. small multilingual `ggml-small-q5_1.bin`) and note the path, e.g. `~/models/whisper/ggml-small-q5_1.bin`. Set `stt.language` in config for your language.

## 4. Piper TTS

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

## 5. Build memo-RF

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

## 6. Configuration

```bash
cp config/config.json.example config/config.json
```

Edit `config/config.json`:

- **stt.model_path**: e.g. `~/models/whisper/ggml-small-q5_1.bin` (small multilingual; paths with `~` are expanded at load). Set **stt.language** (e.g. `en`, `es`, `fr`) for your language.
- **stt.use_gpu**: set to `true` when whisper is built with CUDA (Jetson); set to `false` for CPU-only whisper
- **tts.voice_path**: e.g. `~/models/piper/en_US-lessac-medium.onnx`
- **tts.piper_path**: set to the full path to the `piper` binary if it is not in PATH (e.g. `~/dev/piper/piper`)
- **tts.espeak_data_path**: leave empty for default (`/usr/share/espeak-ng-data` on Linux), or set if you installed espeak-ng elsewhere
- **llm.endpoint**: e.g. `http://localhost:8080/completion` if you run llama.cpp server locally

## 7. Run

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
  - PortAudio (e.g. `libportaudio2` + `portaudio19-dev`)
  - libcurl (e.g. `libcurl4-openssl-dev`)
  - nlohmann/json (e.g. `nlohmann-json3-dev`) or place `json.hpp` in memo-RF `third_party/nlohmann/`
- **whisper.cpp** cross-built for aarch64 (see below)

### 1. Get or build a sysroot

**Option A – From the Jetson:** On the Jetson, install build deps and copy the root to your host:

```bash
# On the Jetson:
sudo apt-get update
sudo apt-get install -y build-essential libportaudio2 portaudio19-dev \
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
