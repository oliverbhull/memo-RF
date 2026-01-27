#!/bin/bash
# Start llama.cpp server with QWEN model

set -e

MODEL_PATH="${1:-$HOME/models/llm/qwen2-1_5b-instruct-q5_k_m.gguf}"
PORT="${2:-8080}"

if [ ! -f "$MODEL_PATH" ]; then
    echo "Error: Model not found at: $MODEL_PATH"
    echo ""
    echo "Usage: $0 [model_path] [port]"
    echo "Example: $0 ~/models/llm/qwen2-1_5b-instruct-q5_k_m.gguf 8080"
    echo ""
    echo "To download a model, run: ./scripts/install_qwen.sh"
    exit 1
fi

SERVER_BIN="/Users/oliverhull/dev/whisper.cpp/build/bin/whisper-server"

if [ ! -f "$SERVER_BIN" ]; then
    echo "Error: whisper-server not found at: $SERVER_BIN"
    echo "Please build whisper.cpp first"
    exit 1
fi

echo "Starting llama.cpp server..."
echo "  Model: $MODEL_PATH"
echo "  Port: $PORT"
echo ""

cd "$(dirname "$SERVER_BIN")"
./whisper-server \
    -m "$MODEL_PATH" \
    -c 2048 \
    --port "$PORT" \
    -ngl 35
