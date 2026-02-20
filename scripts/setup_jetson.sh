#!/bin/bash
# One-time system deps for building memo-RF on Jetson (or Ubuntu). Run once per machine.
# After this: build whisper.cpp (see docs/JETSON_SETUP.md), set config, then:
#   export WHISPER_DIR=/path/to/whisper.cpp && ./build.sh
set -e
echo "Installing system dependencies for memo-RF..."
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  libportaudio2 \
  portaudio19-dev \
  libcurl4-openssl-dev \
  pkg-config
# nlohmann/json (optional: CMake may find it; if not, use third_party)
sudo apt-get install -y nlohmann-json3-dev 2>/dev/null || true
echo "Done. Next: build whisper.cpp, then export WHISPER_DIR=/path/to/whisper.cpp && ./build.sh"
