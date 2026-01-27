#!/bin/bash
# Download GPT-OSS 20B model to ~/models/llm/

set -e

MODEL_DIR="$HOME/models/llm"
REPO_ID="unsloth/gpt-oss-20b-GGUF"

# Available quantizations (choose one):
# - gpt-oss-20b-Q4_K_M.gguf (11.6 GB) - Recommended: Good balance
# - gpt-oss-20b-Q5_K_M.gguf (11.7 GB) - Better quality
# - gpt-oss-20b-Q4_0.gguf (11.5 GB) - Smaller, faster
# - gpt-oss-20b-Q8_0.gguf (12.1 GB) - Higher quality

QUANTIZATION="${1:-Q4_K_M}"  # Default to Q4_K_M
FILENAME="gpt-oss-20b-${QUANTIZATION}.gguf"

echo "Downloading GPT-OSS 20B model..."
echo "Quantization: ${QUANTIZATION}"
echo "Target: ${MODEL_DIR}/${FILENAME}"
echo ""

# Create directory if it doesn't exist
mkdir -p "$MODEL_DIR"
cd "$MODEL_DIR"

# Check if file already exists
if [ -f "$FILENAME" ]; then
    echo "File already exists: $FILENAME"
    echo "Size: $(du -h "$FILENAME" | cut -f1)"
    read -p "Overwrite? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Skipping download."
        exit 0
    fi
fi

# Download using wget (more reliable for large files)
echo "Starting download (this may take a while, ~11-12 GB)..."
wget --continue --progress=bar \
    "https://huggingface.co/${REPO_ID}/resolve/main/${FILENAME}" \
    -O "$FILENAME"

if [ $? -eq 0 ]; then
    echo ""
    echo "✓ Download complete!"
    echo "File: ${MODEL_DIR}/${FILENAME}"
    echo "Size: $(du -h "$FILENAME" | cut -f1)"
    echo ""
    echo "To use this model, update your llama.cpp server command:"
    echo "  ./server -m ${MODEL_DIR}/${FILENAME} -c 2048 --port 8080"
else
    echo "✗ Download failed. You can try manually:"
    echo "  wget https://huggingface.co/${REPO_ID}/resolve/main/${FILENAME}"
    exit 1
fi
