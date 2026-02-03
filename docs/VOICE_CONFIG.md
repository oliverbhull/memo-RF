# Voice Configuration Guide

## Changing the Agent's Voice

The agent's voice is controlled by the Piper TTS model specified in `config.json`:

```json
{
  "tts": {
    "voice_path": "~/models/piper/en_US-lessac-medium.onnx",
    "vox_preroll_ms": 200,
    "output_gain": 1.0
  }
}
```

## Available Piper Voices

Piper supports many voices in different languages and quality levels. Browse available voices at:
- **HuggingFace**: https://huggingface.co/rhasspy/piper-voices/tree/main
- **Local VOICES.md**: Check `piper/VOICES.md` in your Piper clone if you have the repo

### Popular English (en_US) Voices

- **lessac-medium** (current default) - Clear, professional
- **amy-medium** - Female voice, warm tone
- **arctic-medium** - Neutral, clear
- **bryce-medium** - Male voice
- **danny-low** - Fast, lower quality
- **hfc_female-medium** - Female, conversational
- **hfc_male-medium** - Male, conversational

### Downloading a New Voice

```bash
# Create models directory if needed
mkdir -p ~/models/piper

# Download a voice model (example: amy-medium)
cd ~/models/piper
wget https://huggingface.co/rhasspy/piper-voices/resolve/v1.0.0/en/en_US/amy/medium/en_US-amy-medium.onnx
wget https://huggingface.co/rhasspy/piper-voices/resolve/v1.0.0/en/en_US/amy/medium/en_US-amy-medium.onnx.json
```

### Updating config.json

After downloading, update the `voice_path` in `config.json`:

```json
{
  "tts": {
    "voice_path": "~/models/piper/en_US-amy-medium.onnx",
    "vox_preroll_ms": 200,
    "output_gain": 1.0
  }
}
```

### Quality Levels

- **x_low**: Fastest, smallest, lowest quality
- **low**: Fast, small, good quality
- **medium**: Balanced (recommended)
- **high**: Slower, larger, best quality

For radio use, **medium** is recommended for good balance of quality and speed.

### Response language (any persona in Spanish, French, German)

You can have any persona respond in a specific language by setting `llm.response_language` in `config.json` (e.g. `"es"`, `"fr"`, `"de"`). The agent will follow the persona’s style but answer only in that language, and the Piper voice is chosen automatically from `config/language_voices.json` under the base directory given by `tts.voice_models_dir` (default `~/models/piper`).

1. Set `llm.response_language` to the desired code (`es`, `fr`, or `de`).
2. Optionally set `tts.voice_models_dir` if your Piper voice models are elsewhere (e.g. `~/models/piper`).
3. Ensure the matching voice files exist. To download the Spanish, French, and German voices:

```bash
./scripts/download_translate_voices.sh
```

This fetches into `~/models/piper/` (or `$MEMO_RF_PIPER_MODELS`). The paths in `config/language_voices.json` point to these models. You can add more languages by editing `language_voices.json` (copy from `config/language_voices.json.example`).

**Example:** `"agent_persona": "asshole"` with `"response_language": "fr"` gives an asshole persona that responds in French, using the French Piper voice automatically.

## Voice Parameters

- **vox_preroll_ms**: Duration of beep/tone before speech (default: 200ms)
- **output_gain**: Audio volume multiplier (1.0 = normal, >1.0 = louder, <1.0 = quieter)
