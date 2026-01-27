#!/bin/bash
# Add Piper to PATH and download voice model

PIPER_BIN="/Users/oliverhull/dev/piper/build/piper"
MODELS_DIR="$HOME/models/piper"

echo "=== Setting up Piper ==="

# Check if piper exists
if [ ! -f "$PIPER_BIN" ]; then
    echo "Error: Piper not found at $PIPER_BIN"
    exit 1
fi

echo "✓ Piper found at: $PIPER_BIN"

# Add to PATH in ~/.zshrc
if ! grep -q "piper/build" ~/.zshrc 2>/dev/null; then
    echo ""
    echo "Adding Piper to PATH in ~/.zshrc..."
    echo "" >> ~/.zshrc
    echo "# Piper TTS" >> ~/.zshrc
    echo "export PATH=\"\$PATH:/Users/oliverhull/dev/piper/build\"" >> ~/.zshrc
    echo "✓ Added to ~/.zshrc"
    echo "  Run: source ~/.zshrc  (or restart terminal)"
else
    echo "✓ Already in PATH"
fi

# Download voice model
echo ""
read -p "Download English voice model? [Y/n]: " download
if [[ ! $download =~ ^[Nn]$ ]]; then
    mkdir -p "$MODELS_DIR"
    cd "$MODELS_DIR"
    
    echo "Downloading en_US-lessac-medium voice..."
    
    if command -v wget &> /dev/null; then
        wget https://huggingface.co/rhasspy/piper-voices/resolve/main/en/en_US/lessac/medium/en_US-lessac-medium.onnx
        wget https://huggingface.co/rhasspy/piper-voices/resolve/main/en/en_US/lessac/medium/en_US-lessac-medium.onnx.json
    elif command -v curl &> /dev/null; then
        curl -L -o en_US-lessac-medium.onnx \
            https://huggingface.co/rhasspy/piper-voices/resolve/main/en/en_US/lessac/medium/en_US-lessac-medium.onnx
        curl -L -o en_US-lessac-medium.onnx.json \
            https://huggingface.co/rhasspy/piper-voices/resolve/main/en/en_US/lessac/medium/en_US-lessac-medium.onnx.json
    else
        echo "Please download manually:"
        echo "  https://huggingface.co/rhasspy/piper-voices/resolve/main/en/en_US/lessac/medium/en_US-lessac-medium.onnx"
        echo "  https://huggingface.co/rhasspy/piper-voices/resolve/main/en/en_US/lessac/medium/en_US-lessac-medium.onnx.json"
        exit 1
    fi
    
    if [ -f "en_US-lessac-medium.onnx" ]; then
        echo "✓ Voice model downloaded to: $MODELS_DIR"
    fi
fi

echo ""
echo "=== Setup Complete ==="
echo ""
echo "Piper binary: $PIPER_BIN"
echo "Voice model: $MODELS_DIR/en_US-lessac-medium.onnx"
echo ""
echo "Update config.json with:"
echo "  \"tts\": {"
echo "    \"voice_path\": \"$MODELS_DIR/en_US-lessac-medium.onnx\""
echo "  }"
echo ""
echo "Test Piper:"
echo "  echo 'Hello world' | $PIPER_BIN -m $MODELS_DIR/en_US-lessac-medium.onnx --output_file test.wav"
echo "  afplay test.wav"
