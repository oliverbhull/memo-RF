#!/bin/bash
# Run memo-rf with default config
# Usage: ./run.sh [optional-config-path]

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

if [ ! -f "build/memo-rf" ]; then
    echo "Error: build/memo-rf not found. Run ./build.sh first."
    exit 1
fi

if [ $# -eq 0 ]; then
    exec ./build/memo-rf config/config.json
else
    exec ./build/memo-rf "$@"
fi
