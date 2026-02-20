#!/bin/bash
set -e

echo "Building memo-RF..."

# Create build directory (remove if exists to avoid permission issues)
if [ -d "build" ]; then
    echo "Removing existing build directory..."
    rm -rf build
fi

mkdir -p build
cd build

# Pass WHISPER_DIR from environment so Jetson/CI can do: export WHISPER_DIR=... && ./build.sh
CMAKE_OPTS=""
if [ -n "$WHISPER_DIR" ]; then
  CMAKE_OPTS="-DWHISPER_DIR=$WHISPER_DIR"
  echo "Using WHISPER_DIR=$WHISPER_DIR"
fi

# Configure with CMake
cmake $CMAKE_OPTS ..

# Build
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo ""
echo "Build complete! Binary: build/memo-rf"
echo ""
echo "To run:"
echo "  cd build && ./memo-rf"
