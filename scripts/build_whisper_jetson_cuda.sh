#!/bin/bash
# Build whisper.cpp with CUDA on Jetson Orin Nano (native build).
# Run from memo-RF repo root, or pass the path to whisper.cpp as the first argument.
# After build, set WHISPER_DIR to the whisper.cpp SOURCE directory when building memo-RF.

set -e

MEMO_RF_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WHISPER_DIR="${1:-$HOME/dev/whisper.cpp}"

if [ ! -d "$WHISPER_DIR" ]; then
    echo "Error: whisper.cpp directory not found: $WHISPER_DIR"
    echo ""
    echo "Usage: $0 [path/to/whisper.cpp]"
    echo "  Default: \$HOME/dev/whisper.cpp"
    echo ""
    echo "Example: $0 $HOME/whisper.cpp"
    exit 1
fi

echo "Building whisper.cpp with CUDA for Jetson Orin Nano..."
echo "  Source: $WHISPER_DIR"
echo ""

cd "$WHISPER_DIR"
cmake -B build -DGGML_CUDA=1 -DCMAKE_CUDA_ARCHITECTURES=87
cmake --build build

echo ""
echo "Build complete. Set WHISPER_DIR when building memo-RF:"
echo "  export WHISPER_DIR=$WHISPER_DIR"
echo "  cd $MEMO_RF_ROOT && ./build.sh"
echo "  (or: cmake -DWHISPER_DIR=$WHISPER_DIR ..)"
