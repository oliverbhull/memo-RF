#!/bin/bash
# Start llama.cpp server with LLM model

set -e

# Default model selection
MODEL_CHOICE="${1:-qwen}"  # 'qwen' or 'gpt-oss'
PORT="${2:-8080}"

# Set model path based on choice
case "$MODEL_CHOICE" in
    qwen|qwen2)
        MODEL_PATH="$HOME/models/llm/qwen2-1_5b-instruct-q5_k_m.gguf"
        ;;
    gpt-oss|gptoss)
        MODEL_PATH="$HOME/models/llm/gpt-oss-20b-Q4_K_M.gguf"
        ;;
    mistral)
        # Common Mistral GGUF names in ~/models/llm/
        MODEL_PATH=""
        for f in \
            "$HOME/models/llm/mistral-7b-instruct-v0.2-q4_k_m.gguf" \
            "$HOME/models/llm/mistral-7b-instruct-v0.2.Q4_K_M.gguf" \
            "$HOME/models/llm/Mistral-7B-Instruct-v0.2-GGUF.q4_k_m.gguf" \
            "$HOME/models/llm/mistral-7b-instruct-v0.1.Q4_K_M.gguf"; do
            if [ -f "$f" ]; then
                MODEL_PATH="$f"
                break
            fi
        done
        [ -n "$MODEL_PATH" ] || MODEL_PATH="$HOME/models/llm/mistral-7b-instruct-v0.2-q4_k_m.gguf"
        ;;
    *)
        # Treat as direct path
        MODEL_PATH="$MODEL_CHOICE"
        ;;
esac

# Optional env override for model path
if [ -n "${MEMO_RF_LLM_MODEL:-}" ] && [ -f "${MEMO_RF_LLM_MODEL}" ]; then
    MODEL_PATH="$MEMO_RF_LLM_MODEL"
fi

if [ ! -f "$MODEL_PATH" ]; then
    echo "Error: Model not found at: $MODEL_PATH"
    echo ""
    echo "Install a Qwen model first: ./scripts/install_qwen.sh"
    echo "Or set MEMO_RF_LLM_MODEL to a .gguf model path."
    echo ""
    echo "Usage: $0 [model_choice|model_path] [port]"
    echo ""
    echo "Model choices:"
    echo "  qwen     - QWEN2 1.5B (default)"
    echo "  mistral  - Mistral 7B (if GGUF in ~/models/llm/)"
    echo "  gpt-oss  - GPT-OSS 20B"
    echo ""
    echo "Or use Ollama with Mistral (no download):"
    echo "  ollama serve && ollama pull mistral"
    echo "  Then set config: llm.endpoint = http://localhost:11434/api/chat, llm.model_name = mistral"
    echo ""
    echo "Or provide full path:"
    echo "  $0 ~/models/llm/your-model.gguf 8080"
    echo ""
    echo "Examples:"
    echo "  $0 qwen 8080"
    echo "  $0 mistral 8080"
    echo "  $0 ~/models/llm/mistral-7b-instruct-v0.2-q4_k_m.gguf 8080"
    exit 1
fi

# Try to find llama.cpp server binary ($MEMO_RF_LLAMA_DIR or $LLAMA_CPP_DIR, then $HOME paths)
SERVER_BIN=""
if [ -n "${MEMO_RF_LLAMA_DIR:-}" ] && [ -x "${MEMO_RF_LLAMA_DIR}/build/bin/llama-server" ]; then
    SERVER_BIN="${MEMO_RF_LLAMA_DIR}/build/bin/llama-server"
elif [ -n "${LLAMA_CPP_DIR:-}" ] && [ -x "${LLAMA_CPP_DIR}/build/bin/llama-server" ]; then
    SERVER_BIN="${LLAMA_CPP_DIR}/build/bin/llama-server"
else
    for path in \
        "$HOME/dev/llama.cpp/build/bin/llama-server" \
        "$HOME/dev/llama.cpp/build/bin/server" \
        "$HOME/llama.cpp/build/bin/llama-server" \
        "$HOME/llama.cpp/build/bin/server" \
        "$HOME/llama.cpp/bin/server" \
        "/usr/local/bin/llama-server" \
        "/usr/local/bin/llama.cpp/build/bin/llama-server"; do
        if [ -f "${path}" ] && [ -x "${path}" ]; then
            SERVER_BIN="$path"
            break
        fi
    done
fi

if [ -z "$SERVER_BIN" ]; then
    echo "Error: llama.cpp server binary not found"
    echo ""
    echo "Set MEMO_RF_LLAMA_DIR or LLAMA_CPP_DIR to your llama.cpp clone, or build:"
    echo "  cd \$HOME/dev/llama.cpp"
    echo "  cmake -B build -DLLAMA_BUILD_SERVER=ON"
    echo "  cmake --build build --config Release"
    echo ""
    exit 1
fi

echo "Starting llama.cpp server..."
echo "  Model: $MODEL_PATH"
echo "  Port: $PORT"
echo "  Server: $SERVER_BIN"
echo ""

cd "$(dirname "$SERVER_BIN")"

# Adjust GPU layers and context based on model size
# GPT-OSS 20B is large - use fewer GPU layers to fit in memory
if [[ "$MODEL_PATH" == *"gpt-oss-20b"* ]] || [[ "$MODEL_PATH" == *"gpt-oss"* ]]; then
    # Large model: try CPU-only first, or very few GPU layers
    # Check if user wants CPU-only mode
    if [[ "${3:-}" == "cpu" ]] || [[ "${3:-}" == "--cpu" ]]; then
        GPU_LAYERS=0
        CONTEXT_SIZE=512
        echo "  Large model detected - using CPU-only mode:"
        echo "    GPU layers: 0 (CPU-only)"
        echo "    Context size: $CONTEXT_SIZE"
        echo "    Note: This will be slower but uses less memory"
    else
        # Try with minimal GPU layers
        GPU_LAYERS=8
        CONTEXT_SIZE=256
        echo "  Large model detected - using minimal GPU settings:"
        echo "    GPU layers: $GPU_LAYERS (very conservative)"
        echo "    Context size: $CONTEXT_SIZE"
        echo "    If still OOM, try: $0 $MODEL_CHOICE $PORT cpu"
    fi
else
    # Smaller models (QWEN, etc.)
    GPU_LAYERS=35
    CONTEXT_SIZE=2048
    # On Jetson (aarch64 + NVIDIA GPU), use conservative GPU layers to avoid OOM on 8GB
    if [ "$(uname -m)" = "aarch64" ] && (command -v nvidia-smi >/dev/null 2>&1 || [ -d /usr/local/cuda ]); then
        GPU_LAYERS=22
        echo "  Jetson detected - using conservative GPU layers for 8GB: $GPU_LAYERS"
    fi
fi

if [ "$GPU_LAYERS" -eq 0 ]; then
    # CPU-only mode
    "$SERVER_BIN" \
        -m "$MODEL_PATH" \
        -c "$CONTEXT_SIZE" \
        --port "$PORT" \
        --gpu-layers 0 \
        --host 0.0.0.0
else
    # GPU mode
    "$SERVER_BIN" \
        -m "$MODEL_PATH" \
        -c "$CONTEXT_SIZE" \
        --port "$PORT" \
        --gpu-layers "$GPU_LAYERS" \
        --host 0.0.0.0
fi
