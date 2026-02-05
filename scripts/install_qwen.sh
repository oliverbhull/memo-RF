#!/bin/bash
# Install QWEN model for llama.cpp

set -e

echo "=== Installing QWEN Model ==="

# Create models directory
MODELS_DIR="$HOME/models/llm"
mkdir -p "$MODELS_DIR"
cd "$MODELS_DIR"

echo ""
echo "Choose a QWEN model to download:"
echo "1. Qwen2-1.5B-Instruct-q5_k_m.gguf (Recommended - good balance)"
echo "2. Qwen2-1.5B-Instruct-q4_k_m.gguf (Faster, slightly lower quality)"
echo "3. Qwen2-0.5B-Instruct-q5_k_m.gguf (Fastest, good for short responses)"
echo ""
read -p "Enter choice [1-3] (default: 1): " choice
choice=${choice:-1}

case $choice in
    1)
        MODEL_FILE="qwen2-1_5b-instruct-q5_k_m.gguf"
        MODEL_REPO="Qwen/Qwen2-1.5B-Instruct-GGUF"
        ;;
    2)
        MODEL_FILE="qwen2-1_5b-instruct-q4_k_m.gguf"
        MODEL_REPO="Qwen/Qwen2-1.5B-Instruct-GGUF"
        ;;
    3)
        MODEL_FILE="qwen2-0_5b-instruct-q5_k_m.gguf"
        MODEL_REPO="Qwen/Qwen2-0.5B-Instruct-GGUF"
        ;;
    *)
        echo "Invalid choice, using default"
        MODEL_FILE="qwen2-1_5b-instruct-q5_k_m.gguf"
        MODEL_REPO="Qwen/Qwen2-1.5B-Instruct-GGUF"
        ;;
esac

echo ""
echo "Downloading $MODEL_FILE..."

# Check if huggingface-cli is available
if command -v huggingface-cli &> /dev/null; then
    echo "Using huggingface-cli..."
    huggingface-cli download "$MODEL_REPO" "$MODEL_FILE" \
        --local-dir . \
        --local-dir-use-symlinks False
elif command -v wget &> /dev/null; then
    echo "Using wget..."
    # Construct Hugging Face download URL
    # Note: This is a simplified approach - you may need to get the actual download URL
    echo "Please download manually from: https://huggingface.co/$MODEL_REPO"
    echo "Save the file as: $MODELS_DIR/$MODEL_FILE"
    exit 1
elif command -v curl &> /dev/null; then
    echo "Using curl..."
    echo "Please download manually from: https://huggingface.co/$MODEL_REPO"
    echo "Save the file as: $MODELS_DIR/$MODEL_FILE"
    exit 1
else
    echo "No download tool found. Please install huggingface-cli:"
    echo "  pip install huggingface_hub"
    echo ""
    echo "Or download manually from: https://huggingface.co/$MODEL_REPO"
    echo "Save the file as: $MODELS_DIR/$MODEL_FILE"
    exit 1
fi

if [ -f "$MODEL_FILE" ]; then
    echo ""
    echo "✓ Model downloaded successfully!"
    echo "  Location: $MODELS_DIR/$MODEL_FILE"
    echo ""
    echo "To start llama.cpp server with this model:"
    echo "  ./scripts/start_server.sh qwen 8080"
else
    echo "✗ Download failed. Please download manually."
    exit 1
fi
