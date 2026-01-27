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

# Configure with CMake
cmake ..

# Build
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo ""
echo "Build complete! Binary: build/memo-rf"
echo ""
echo "To run:"
echo "  cd build && ./memo-rf"
