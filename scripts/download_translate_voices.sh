#!/bin/bash
# Download Piper TTS voices for Spanish, French, and German (for translate_* personas).
# Puts models in ~/models/piper/ (or MEMO_RF_PIPER_MODELS if set).

set -e

BASE="https://huggingface.co/rhasspy/piper-voices/resolve/main"
MODELS_DIR="${MEMO_RF_PIPER_MODELS:-$HOME/models/piper}"

mkdir -p "$MODELS_DIR"
cd "$MODELS_DIR"

download_voice() {
  local rel_path="$1"
  local name=$(basename "$rel_path")
  local dir=$(dirname "$rel_path")
  if [ -f "$name" ]; then
    echo "Already have $name, skipping."
    return 0
  fi
  echo "Downloading $name..."
  mkdir -p "$dir"
  if command -v wget &> /dev/null; then
    wget -q -O "$rel_path" "${BASE}/${rel_path}"
    wget -q -O "${rel_path}.json" "${BASE}/${rel_path}.json"
  elif command -v curl &> /dev/null; then
    curl -sL -o "$rel_path" "${BASE}/${rel_path}"
    curl -sL -o "${rel_path}.json" "${BASE}/${rel_path}.json"
  else
    echo "Need wget or curl."
    exit 1
  fi
  echo "  -> $MODELS_DIR/$rel_path"
}

# Spanish (Spain, medium) – for translate_spanish
download_voice "es/es_ES/davefx/medium/es_ES-davefx-medium.onnx"

# French (France, medium) – for translate_french
download_voice "fr/fr_FR/siwis/medium/fr_FR-siwis-medium.onnx"

# German (Germany, medium) – for translate_german
download_voice "de/de_DE/thorsten/medium/de_DE-thorsten-medium.onnx"

echo ""
echo "Done. Voices are in: $MODELS_DIR"
echo ""
echo "Use in config.json when using translate personas:"
echo "  translate_spanish -> \"voice_path\": \"$MODELS_DIR/es/es_ES/davefx/medium/es_ES-davefx-medium.onnx\""
echo "  translate_french  -> \"voice_path\": \"$MODELS_DIR/fr/fr_FR/siwis/medium/fr_FR-siwis-medium.onnx\""
echo "  translate_german  -> \"voice_path\": \"$MODELS_DIR/de/de_DE/thorsten/medium/de_DE-thorsten-medium.onnx\""
