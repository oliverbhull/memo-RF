#!/bin/bash
# Cross-build memo-RF for aarch64 Linux (NVIDIA Jetson Orin Nano).
# Run from the memo-RF repo root. Requires a Linux host with aarch64 toolchain
# and a sysroot (see docs/JETSON_SETUP.md).
set -e

MEMO_RF_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$MEMO_RF_ROOT"

# Toolchain file (default: cmake/aarch64-linux-gnu.cmake in repo)
TOOLCHAIN_FILE="${TOOLCHAIN_FILE:-$MEMO_RF_ROOT/cmake/aarch64-linux-gnu.cmake}"
# Sysroot: copy of Jetson/arm64 root (required). Set via CMAKE_SYSROOT or JETSON_SYSROOT.
CMAKE_SYSROOT="${CMAKE_SYSROOT:-$JETSON_SYSROOT}"
# whisper.cpp build dir for aarch64 (required). Set via WHISPER_DIR.
WHISPER_DIR="${WHISPER_DIR:-}"
# Build directory (default: build-jetson to avoid clashing with native build)
BUILD_DIR="${BUILD_DIR:-build-jetson}"

usage() {
  echo "Usage: CMAKE_SYSROOT=/path/to/sysroot WHISPER_DIR=/path/to/whisper.cpp $0"
  echo "   or: JETSON_SYSROOT=... WHISPER_DIR=... $0"
  echo ""
  echo "Optional env: TOOLCHAIN_FILE, BUILD_DIR"
  exit 1
}

if [ -z "$CMAKE_SYSROOT" ]; then
  echo "Error: CMAKE_SYSROOT or JETSON_SYSROOT must be set to the path of the Jetson/arm64 sysroot."
  usage
fi
if [ -z "$WHISPER_DIR" ]; then
  echo "Error: WHISPER_DIR must be set to the path of whisper.cpp (cross-built for aarch64)."
  usage
fi
if [ ! -f "$TOOLCHAIN_FILE" ]; then
  echo "Error: Toolchain file not found: $TOOLCHAIN_FILE"
  exit 1
fi
if [ ! -d "$CMAKE_SYSROOT" ]; then
  echo "Error: Sysroot not found: $CMAKE_SYSROOT"
  exit 1
fi
if [ ! -d "$WHISPER_DIR" ]; then
  echo "Error: WHISPER_DIR not found: $WHISPER_DIR"
  exit 1
fi

# So pkg-config finds PortAudio/curl etc. in the sysroot
export PKG_CONFIG_SYSROOT_DIR="$CMAKE_SYSROOT"
export PKG_CONFIG_PATH="${PKG_CONFIG_PATH:-$CMAKE_SYSROOT/usr/lib/aarch64-linux-gnu/pkgconfig:$CMAKE_SYSROOT/usr/lib/pkgconfig}"

echo "Building memo-RF for aarch64 (Jetson)..."
echo "  TOOLCHAIN_FILE=$TOOLCHAIN_FILE"
echo "  CMAKE_SYSROOT=$CMAKE_SYSROOT"
echo "  WHISPER_DIR=$WHISPER_DIR"
echo "  BUILD_DIR=$BUILD_DIR"

if [ -d "$BUILD_DIR" ]; then
  echo "Removing existing $BUILD_DIR..."
  rm -rf "$BUILD_DIR"
fi
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake \
  -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
  -DCMAKE_SYSROOT="$CMAKE_SYSROOT" \
  -DWHISPER_DIR="$WHISPER_DIR" \
  ..

make -j$(nproc 2>/dev/null || echo 4)

echo ""
echo "Build complete. Binary: $BUILD_DIR/memo-rf"
echo "Copy to Jetson and run there (e.g. ./memo-rf config/config.json)."
