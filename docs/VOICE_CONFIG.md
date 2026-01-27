# Voice Configuration Guide

## Changing the Agent's Voice

The agent's voice is controlled by the Piper TTS model specified in `config.json`:

```json
{
  "tts": {
    "voice_path": "/Users/oliverhull/models/piper/en_US-lessac-medium.onnx",
    "vox_preroll_ms": 200,
    "output_gain": 1.0
  }
}
```

## Available Piper Voices

Piper supports many voices in different languages and quality levels. Browse available voices at:
- **HuggingFace**: https://huggingface.co/rhasspy/piper-voices/tree/main
- **Local VOICES.md**: Check `~/dev/piper/VOICES.md` if you have the piper repo

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
    "voice_path": "/Users/oliverhull/models/piper/en_US-amy-medium.onnx",
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

## Voice Parameters

- **vox_preroll_ms**: Duration of beep/tone before speech (default: 200ms)
- **output_gain**: Audio volume multiplier (1.0 = normal, >1.0 = louder, <1.0 = quieter)
