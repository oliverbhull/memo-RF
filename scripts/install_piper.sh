#!/bin/bash
# Install Piper TTS

set -e

echo "=== Installing Piper TTS ==="

# Check if already installed
if command -v piper &> /dev/null; then
    echo "Piper is already installed at: $(which piper)"
    read -p "Reinstall? [y/N]: " reinstall
    if [[ ! $reinstall =~ ^[Yy]$ ]]; then
        exit 0
    fi
fi

echo ""
echo "Choose installation method:"
echo "1. Build from source (recommended)"
echo "2. Download pre-built binary"
echo "3. Install via pip (piper-tts Python package)"
echo ""
read -p "Enter choice [1-3] (default: 1): " choice
choice=${choice:-1}

INSTALL_DIR="$HOME/dev/piper"
MODELS_DIR="$HOME/models/piper"

case $choice in
    1)
        echo ""
        echo "Building Piper from source..."
        mkdir -p "$HOME/dev"
        cd "$HOME/dev"
        
        if [ -d "piper" ]; then
            echo "Piper directory exists, updating..."
            cd piper
            git pull
        else
            echo "Cloning Piper repository..."
            git clone https://github.com/rhasspy/piper.git
            cd piper
        fi
        
        echo "Building..."
        make
        
        if [ -f "./piper" ]; then
            echo ""
            echo "✓ Piper built successfully!"
            echo "  Binary: $(pwd)/piper"
            echo ""
            echo "Add to PATH by adding this to ~/.zshrc:"
            echo "  export PATH=\"\$PATH:$(pwd)\""
            echo ""
            echo "Or use full path: $(pwd)/piper"
        else
            echo "✗ Build failed"
            exit 1
        fi
        ;;
        
    2)
        echo ""
        echo "Downloading pre-built binary..."
        mkdir -p "$INSTALL_DIR"
        cd "$INSTALL_DIR"
        
        # Detect architecture
        ARCH=$(uname -m)
        if [ "$ARCH" = "arm64" ]; then
            ARCH="arm64"
        else
            ARCH="amd64"
        fi
        
        VERSION="1.2.0"
        TARBALL="piper_macos_${ARCH}.tar.gz"
        URL="https://github.com/rhasspy/piper/releases/download/v${VERSION}/${TARBALL}"
        
        echo "Downloading from: $URL"
        
        if command -v wget &> /dev/null; then
            wget "$URL"
        elif command -v curl &> /dev/null; then
            curl -L -o "$TARBALL" "$URL"
        else
            echo "Please download manually from: $URL"
            exit 1
        fi
        
        tar -xzf "$TARBALL"
        
        if [ -f "./piper" ]; then
            echo ""
            echo "✓ Piper downloaded successfully!"
            echo "  Binary: $(pwd)/piper"
        else
            echo "✗ Download/extraction failed"
            exit 1
        fi
        ;;
        
    3)
        echo ""
        echo "Installing piper-tts via pip..."
        pip install piper-tts
        
        if command -v piper-tts &> /dev/null; then
            echo ""
            echo "✓ piper-tts installed successfully!"
            echo "  Note: This is the Python package. Use 'piper-tts' command."
        else
            echo "✗ Installation failed"
            exit 1
        fi
        ;;
esac

# Download voice model
echo ""
read -p "Download voice model? [Y/n]: " download_voice
if [[ ! $download_voice =~ ^[Nn]$ ]]; then
    echo ""
    echo "Downloading English US voice (lessac-medium)..."
    mkdir -p "$MODELS_DIR"
    cd "$MODELS_DIR"
    
    VOICE_URL="https://huggingface.co/rhasspy/piper-voices/resolve/main/en/en_US/lessac/medium"
    
    if command -v wget &> /dev/null; then
        wget "${VOICE_URL}/en_US-lessac-medium.onnx"
        wget "${VOICE_URL}/en_US-lessac-medium.onnx.json"
    elif command -v curl &> /dev/null; then
        curl -L -o "en_US-lessac-medium.onnx" "${VOICE_URL}/en_US-lessac-medium.onnx"
        curl -L -o "en_US-lessac-medium.onnx.json" "${VOICE_URL}/en_US-lessac-medium.onnx.json"
    else
        echo "Please download manually from:"
        echo "  ${VOICE_URL}/en_US-lessac-medium.onnx"
        echo "  ${VOICE_URL}/en_US-lessac-medium.onnx.json"
    fi
    
    if [ -f "en_US-lessac-medium.onnx" ]; then
        echo ""
        echo "✓ Voice model downloaded!"
        echo "  Location: $MODELS_DIR/en_US-lessac-medium.onnx"
    fi
fi

echo ""
echo "=== Installation Complete ==="
echo ""
echo "Update config.json with:"
echo "  \"tts\": {"
echo "    \"voice_path\": \"$MODELS_DIR/en_US-lessac-medium.onnx\""
echo "  }"
