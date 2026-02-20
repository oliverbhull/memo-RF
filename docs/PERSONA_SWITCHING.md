# Dynamic Persona Switching & Translation Configuration

## Overview

The system now supports two powerful features:
1. **Build-time translation language configuration** for translator persona
2. **Runtime persona switching** via voice commands

## Feature 1: Translation Language Build Flag

### Usage

When building with the translator persona, you can specify the target translation language:

```bash
cmake -B build -DAGENT_PERSONA=translator -DTRANSLATE_LANGUAGE=Spanish
cmake --build build
```

### Supported Languages

You can specify any language (e.g., `Spanish`, `French`, `German`, `Italian`, `Japanese`, `Arabic`, etc.). The language is passed verbatim to the LLM translation prompt.

### How It Works

- Uses **Qwen 2.5** for translation (no need for separate translategemma model)
- Tight system prompt ensures verbatim translation only
- Memory-efficient: works within Jetson's constraints

### Translation Prompt

The system uses this optimized prompt:
```
Translate this English radio transmission to [LANGUAGE] verbatim.
Output ONLY the [LANGUAGE] translation.
Do not add explanations, preamble, or commentary.
Preserve the exact meaning and radio terminology.
End with "over".
```

## Feature 2: Dynamic Persona Switching

### Voice Commands

Change the agent's persona on the fly using voice commands:

```
"Memo change persona to manufacturing"
"Memo change persona to retail"
"Memo change persona to translator"
"Memo change persona security"
"Memo change persona trucker_cb"
```

### Available Personas

All personas from `config/personas.json` can be switched to:

**Professional:**
- manufacturing, retail, hospitality, healthcare, security
- construction, warehouse, airline_ramp, maritime
- ems_dispatch, school_admin, stadium_operations

**Specialized:**
- translator (with build-time language config)
- film_production, theme_park, golf_course
- wildland_fire, ski_patrol, pit_crew

**Fun/Themed:**
- astronaut, mission_control, submarine, detective_noir
- trucker_cb, surfer, butler, drill_sergeant
- zombie_survivor, ghost_hunters, ranch_hand

**Personality:**
- asshole, startup_bro, burnt_out_engineer
- conspiracy_theorist, doom_prepper, edgelord
- rich_prick, corporate_hr, finance_bro

### How It Works

1. **Command Detection**: System listens for "Memo change persona..." phrase
2. **Persona Loading**: Loads system prompt from `config/personas.json`
3. **Runtime Update**: Updates persona without restarting
4. **Acknowledgment**: Confirms the change over the radio

### Example Interaction

```
User: "Memo change persona to astronaut"
Agent: "Persona changed to ISS Crew. Over."

User: "What's the status?"
Agent: "Houston, all systems nominal. Over."

User: "Memo change persona to trucker CB"
Agent: "Persona changed to CB Trucker. Over."

User: "What's your location?"
Agent: "10-4 good buddy, eastbound on I-40. Over."
```

## Implementation Details

### Build Flags

**CMakeLists.txt** now supports:
- `-DAGENT_PERSONA=<persona_id>` - Set initial persona at compile time
- `-DTRANSLATE_LANGUAGE=<language>` - Set translation target language

### Runtime State

**AgentPipeline** maintains mutable runtime state:
- `current_persona_` - Current persona ID
- `current_system_prompt_` - Current system prompt
- `current_persona_name_` - Current display name
- `target_language_` - Translation target language

These override the config values when changed dynamically.

### Persona File Location

The system looks for `config/personas.json` relative to the working directory. Make sure to run the binary from the project root or adjust the path.

## Translation vs Response Language

**Important distinction:**

- `response_language` (config.json) - Used for response language with TTS voice selection
- `TRANSLATE_LANGUAGE` (build flag) - Used specifically for translator persona to translate user input

When using translator persona:
- Set `TRANSLATE_LANGUAGE` at build time for the translation target
- The translation happens in the LLM stage (user input → target language)
- TTS voice is determined by `response_language` in config

## Memory Considerations

- **Qwen for Translation**: Uses existing Qwen 2.5:1.5b (986 MB)
- **No Additional Model**: Does not load translategemma (would require 1.5-2GB more)
- **Jetson Compatible**: Works within 7.4GB RAM constraint
- **Translation Quality**: Qwen provides good translation quality for most languages

## Testing

Build with translator persona and French target:
```bash
cmake -B build -DAGENT_PERSONA=translator -DTRANSLATE_LANGUAGE=French
cmake --build build
./build/memo-rf
```

Try dynamic switching:
```
Say: "Memo change persona to manufacturing"
Say: "What's the safety protocol?"
Say: "Memo change persona to astronaut"
Say: "What's the status?"
```

## Troubleshooting

**Persona not changing:**
- Check that `config/personas.json` exists and is readable
- Verify persona ID matches exactly (case-sensitive in file, case-insensitive in voice command)
- Check logs for persona loading errors

**Translation not working:**
- Verify you built with `-DTRANSLATE_LANGUAGE=<language>`
- Ensure persona is set to "translator"
- Check Ollama is running and Qwen model is loaded

**Command not detected:**
- Speak clearly: "Memo change persona to [persona_name]"
- Check STT is transcribing correctly
- Make sure wake_word is properly configured
