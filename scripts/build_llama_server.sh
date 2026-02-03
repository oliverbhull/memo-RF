#!/bin/bash
# Build llama.cpp server

set -e

LLAMA_DIR="${1:-$HOME/dev/llama.cpp}"

if [ ! -d "$LLAMA_DIR" ]; then
    echo "Error: llama.cpp directory not found at: $LLAMA_DIR"
    echo ""
    echo "Usage: $0 [llama.cpp_path]"
    echo ""
    echo "To clone llama.cpp first:"
    echo "  cd ~/dev"
    echo "  git clone https://github.com/ggml-org/llama.cpp.git"
    exit 1
fi

echo "Building llama.cpp server..."
echo "Directory: $LLAMA_DIR"
echo ""

cd "$LLAMA_DIR"

# Configure build with server enabled
echo "Configuring build..."
cmake -B build -DLLAMA_SERVER=ON -DCMAKE_BUILD_TYPE=Release

# Build
echo "Building (this may take a while)..."
cmake --build build --config Release -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Check if server was built
if [ -f "build/bin/server" ]; then
    echo ""
    echo "✓ Server built successfully!"
    echo "Location: $LLAMA_DIR/build/bin/server"
elif [ -f "build/bin/llama-server" ]; then
    echo ""
    echo "✓ Server built successfully!"
    echo "Location: $LLAMA_DIR/build/bin/llama-server"
else
    echo ""
    echo "⚠ Server binary not found in expected location"
    echo "Checking build/bin/ directory..."
    ls -la build/bin/ | grep -i server || echo "No server binary found"
    echo ""
    echo "You may need to check the build output for errors"
fi
