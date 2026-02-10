# Memo-RF Plugin System

## Overview

Memo-RF supports **data-driven voice command plugins** that let external systems (robots, home automation, APIs) be controlled by voice — without writing any C++. Each plugin is defined by a JSON file that specifies:

- **Commands**: what the user can say and what API call to make
- **Vocabulary**: domain-specific words to boost speech recognition
- **API config**: endpoint URL, authentication, default parameters

The Muni rover integration is the first plugin. Others can be added by dropping a new JSON file into `config/plugins/`.

## Architecture

```
config/plugins/muni.json          ← command definitions (data, not code)
config/plugins/future_robot.json  ← another integration (just add the file)
       │
       ▼
┌───────────────────────────────────────────────────────┐
│                   Memo-RF Core                         │
│                                                        │
│  Mic → VAD → STT (Whisper + plugin vocab boost)        │
│                    │                                   │
│             ActionDispatcher                            │
│                    │                                   │
│          iterates plugins by priority                  │
│                    │                                   │
│           ┌────────┼────────┐                          │
│           ▼        ▼        ▼                          │
│       Plugin A  Plugin B  Plugin C                     │
│       (muni)   (future)  (future)                      │
│                                                        │
│  No plugin matched? → falls through to LLM path       │
└───────────────────────────────────────────────────────┘
```

## Quick Start

### 1. Enable a plugin

Add the plugin JSON file path to `config/config.json`:

```json
{
  "plugins": {
    "config_files": ["config/plugins/muni.json"]
  }
}
```

### 2. Restart memo-rf

The plugin loads at startup. You'll see log lines:

```
[JsonCommandPlugin] Loaded "muni" with 4 commands, 28 vocab words
STT vocab boost from 1 plugin(s): 312 chars
```

### 3. Speak a command

Say "stop the robot" or "navigate to position five three" — the plugin matches, calls the API, and speaks a confirmation.

## Plugin JSON Format

Each plugin is a single JSON file. Here's the full schema:

```json
{
  "plugin": "my_plugin_name",
  "priority": 50,

  "api": {
    "base_url": "http://hostname:port",
    "api_key": "optional-secret",
    "default_rover_id": "optional-default-id"
  },

  "vocab": [
    "domain-specific", "words", "for", "speech", "recognition"
  ],

  "commands": [
    {
      "id": "command_name",
      "priority": 1,
      "phrases": ["spoken trigger phrase", "alternative phrase"],
      "params": [],
      "api_endpoint": "/path/{rover_id}/action",
      "api_method": "POST",
      "api_body": { "type": "command_name" },
      "confirm_text": "Spoken confirmation after success."
    }
  ]
}
```

### Top-Level Fields

| Field | Type | Description |
|-------|------|-------------|
| `plugin` | string | Plugin name (used in logs) |
| `priority` | int | Plugin-level priority. Lower = checked first across plugins |
| `api` | object | API connection settings |
| `vocab` | array of strings | Words to boost in Whisper STT (merged with command phrases automatically) |
| `commands` | array | Command definitions (see below) |

### API Config

| Field | Type | Description |
|-------|------|-------------|
| `base_url` | string | Base URL for all API calls (e.g. `http://depot:4890`) |
| `api_key` | string | Sent as `X-API-Key` header. Leave empty if no auth required |
| `default_rover_id` | string | Default value for `{rover_id}` template variable |

### Command Definition

| Field | Type | Description |
|-------|------|-------------|
| `id` | string | Unique command identifier (for logging) |
| `priority` | int | Within this plugin, lower = matched first. Use 1 for safety-critical commands |
| `phrases` | array of strings | Trigger phrases. If any phrase appears in the transcript, this command matches. Longer phrases are checked first automatically |
| `params` | array | Parameter extraction rules (see below). Omit for commands with no parameters |
| `api_endpoint` | string | URL path appended to `base_url`. Supports `{param}` template variables |
| `api_method` | string | `POST` or `GET` |
| `api_body` | object | JSON body template. String values like `"{x}"` are substituted with extracted params. Numeric placeholders are converted to JSON numbers automatically |
| `confirm_text` | string | Spoken via TTS after a successful API call. Supports `{param}` substitution |

### Parameter Extraction

Parameters are extracted from the transcript text that appears **after** the matched phrase.

```json
{
  "name": "x",
  "type": "float",
  "extract": "first_number"
}
```

| Field | Type | Description |
|-------|------|-------------|
| `name` | string | Parameter name (used in `{name}` template substitution) |
| `type` | string | `float`, `int`, `enum`, or `string` |
| `extract` | string | Extraction method (see table below) |
| `values` | object | For `enum` type only: maps canonical values to spoken variants |

#### Extraction Methods

| Method | Behavior | Example Input | Result |
|--------|----------|---------------|--------|
| `first_number` | First numeric value found after the phrase | "go to **5** three" | 5.0 |
| `second_number` | Second numeric value found after the phrase | "go to 5 **3**" | 3.0 |
| `keyword_after_phrase` | Match enum values in the text after the phrase | "set mode to **dance**" | "dance" |

**Spoken numbers are supported.** "five" becomes 5, "twenty" becomes 20, etc. Both "go to 5 3" and "go to five three" work.

#### Enum Values

For `keyword_after_phrase` extraction, define all spoken variants:

```json
{
  "name": "mode",
  "type": "enum",
  "extract": "keyword_after_phrase",
  "values": {
    "idle": ["idle", "standby", "stand by"],
    "autonomous": ["autonomous", "autonomy", "auto", "self drive"],
    "sleep": ["sleep", "shut down", "power down"],
    "dance": ["dance", "party", "boogie"]
  }
}
```

The keys (`idle`, `autonomous`, etc.) are the canonical values sent to the API. The arrays are what users might actually say. All variants are automatically added to the STT vocab boost.

## Muni Plugin Reference

The included `config/plugins/muni.json` provides four commands for the Muni rover dispatch API:

| Command | Priority | Example Phrases | Parameters |
|---------|----------|-----------------|------------|
| `estop` | 1 | "stop", "halt", "freeze", "emergency stop", "kill it" | None |
| `estop_release` | 2 | "release", "resume", "clear stop", "start up" | None |
| `set_goal` | 10 | "go to position 5 3", "navigate to five three" | x (float), y (float) |
| `set_mode` | 10 | "set mode to dance", "put the robot in sleep" | mode (enum: idle, autonomous, sleep, dance) |

### API Details

- **Base URL**: `http://depot:4890`
- **Endpoint**: `POST /rovers/{rover_id}/command`
- **Auth**: `X-API-Key` header (configure in `api.api_key`)
- **Default rover**: `frog-0`

## STT Vocabulary Boosting

Each plugin contributes domain-specific vocabulary that gets passed to Whisper's `initial_prompt` parameter. This biases the speech recognition model toward correctly transcribing words it might otherwise miss.

Vocabulary is collected from three sources in each plugin:
1. The explicit `vocab` array
2. All `phrases` from every command
3. All enum variant words from parameters

These are merged across all loaded plugins, deduplicated, and set on the STT engine at startup. The merged prompt is applied to every Whisper transcription call.

## Adding a New Plugin

### Step 1: Create the JSON file

Create `config/plugins/my_system.json`:

```json
{
  "plugin": "my_system",
  "priority": 100,
  "api": {
    "base_url": "http://localhost:8080",
    "api_key": ""
  },
  "vocab": ["my system", "custom term"],
  "commands": [
    {
      "id": "do_thing",
      "priority": 10,
      "phrases": ["do the thing", "activate", "run it"],
      "api_endpoint": "/api/action",
      "api_method": "POST",
      "api_body": { "action": "do_thing" },
      "confirm_text": "Done."
    }
  ]
}
```

### Step 2: Register it

Add to `config/config.json`:

```json
{
  "plugins": {
    "config_files": [
      "config/plugins/muni.json",
      "config/plugins/my_system.json"
    ]
  }
}
```

### Step 3: Restart

No recompilation needed. The new plugin loads at startup.

## Updating Commands Without Recompiling

| Change | What to Edit | Recompile? |
|--------|-------------|------------|
| Add a spoken phrase variant | Add to `phrases` array | No |
| Add a new enum value (e.g. new robot mode) | Add to `values` object | No |
| Add a new command type | Add entry to `commands` array | No |
| Fix STT misrecognition | Add the misheard word to `vocab` or `phrases` | No |
| Change API endpoint or auth | Edit `api` object | No |
| Add a new integration | Create new JSON file, add to `config_files` | No |
| Change confirm text | Edit `confirm_text` | No |

All changes take effect on restart.

## Processing Order

When a transcript arrives, it is checked in this order:

1. **Persona change** — "Memo change persona to manufacturing"
2. **Action plugins** — checked by plugin priority, then command priority within each plugin
3. **Wake word** — "Hey Memo" prefix (if enabled)
4. **Router** — fast-path vs LLM decision
5. **LLM path** — Ollama/cloud model generates a response

If a plugin matches at step 2, steps 3-5 are skipped entirely. The plugin's confirmation text is spoken and the system returns to listening.

## Error Handling

- **Plugin JSON fails to parse**: Warning logged, plugin skipped, system continues
- **API call fails (network error)**: TTS speaks "Command failed. Robot may be offline."
- **API returns error status**: TTS speaks error message, error details logged
- **No phrase matches**: Plugin returns false, transcript falls through to LLM path
- **Parameter extraction fails** (e.g. no numbers found for set_goal): Command not matched, falls through
