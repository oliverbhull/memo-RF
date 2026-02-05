#!/bin/bash
# Build llama.cpp with CUDA and server on Jetson Orin Nano (native build).
# Run from memo-RF repo root, or pass the path to llama.cpp as the first argument.
# After build, set MEMO_RF_LLAMA_DIR or LLAMA_CPP_DIR to the llama.cpp directory for start_server.sh.

set -e

LLAMA_DIR="${1:-$HOME/dev/llama.cpp}"

if [ ! -d "$LLAMA_DIR" ]; then
    echo "Error: llama.cpp directory not found: $LLAMA_DIR"
    echo ""
    echo "Usage: $0 [path/to/llama.cpp]"
    echo "  Default: \$HOME/dev/llama.cpp"
    echo ""
    echo "Example: $0 $HOME/llama.cpp"
    exit 1
fi

echo "Building llama.cpp with CUDA and server for Jetson Orin Nano..."
echo "  Source: $LLAMA_DIR"
echo ""

cd "$LLAMA_DIR"
cmake -B build -DLLAMA_BUILD_SERVER=ON -DCMAKE_CUDA_ARCHITECTURES=87
cmake --build build --config Release

echo ""
echo "Build complete. Set MEMO_RF_LLAMA_DIR when starting the server:"
echo "  export MEMO_RF_LLAMA_DIR=$LLAMA_DIR"
echo "  ./scripts/start_server.sh qwen 8080"
echo "  (Server binary: $LLAMA_DIR/build/bin/llama-server)"
