# Quick Start: Translation & Dynamic Persona Switching

## Build with Translation

```bash
# Spanish translator
cmake -B build -DAGENT_PERSONA=translator -DTRANSLATE_LANGUAGE=Spanish
cmake --build build

# French translator
cmake -B build -DAGENT_PERSONA=translator -DTRANSLATE_LANGUAGE=French
cmake --build build

# Manufacturing (default)
cmake -B build -DAGENT_PERSONA=manufacturing
cmake --build build
```

## Run

```bash
./build/memo-rf
```

## Voice Commands

### Change Persona On-The-Fly

```
"Memo change persona to manufacturing"
"Memo change persona to astronaut"
"Memo change persona to trucker CB"
"Memo change persona to translator"
```

### Test Translation (when in translator persona)

```
User: "The equipment needs maintenance"
Agent: [Spanish] "El equipo necesita mantenimiento. Over."

User: "Safety check complete"
Agent: [Spanish] "Verificación de seguridad completa. Over."
```

## How Translation Works

✅ **Uses Qwen 2.5** (already loaded)
✅ **Memory efficient** (no additional model)
✅ **Tight prompt** for verbatim translation only
✅ **Build-time language config** or runtime switching

## Key Features

1. **Build-time persona**: `-DAGENT_PERSONA=<persona>`
2. **Build-time language**: `-DTRANSLATE_LANGUAGE=<language>`
3. **Runtime switching**: "Memo change persona to X"
4. **28+ personas**: from professional to fun themed

## Example Workflow

```bash
# Build with Spanish translator
cmake -B build -DAGENT_PERSONA=translator -DTRANSLATE_LANGUAGE=Spanish
cmake --build build
./build/memo-rf

# During runtime, switch to different persona:
# Say: "Memo change persona to manufacturing"
# Now it's manufacturing persona instead of translator

# Switch back to translator:
# Say: "Memo change persona to translator"
# Back to Spanish translation mode
```

## Translation Prompt (Used with Qwen)

```
Translate this English radio transmission to [LANGUAGE] verbatim.
Output ONLY the [LANGUAGE] translation.
Do not add explanations, preamble, or commentary.
Preserve the exact meaning and radio terminology.
End with "over".
```

This tight prompt ensures Qwen translates verbatim without adding commentary.

## Memory Usage

- Qwen 2.5:1.5b: 986 MB
- Whisper: ~500 MB
- System + App: ~6 GB
- **Total**: Fits in Jetson's 7.4 GB RAM
- **TranslateGemma**: Would require additional 1.5-2GB (not feasible)

## See Also

- `PERSONA_SWITCHING.md` - Full documentation
- `config/personas.json` - All available personas
- `build_with_persona.sh` - Build script examples
