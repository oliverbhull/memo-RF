# Testing Memo-RF Plugin System with Radio

## Quick Start

**Mock API server is already running!** 🟢

You can now test the full Memo-RF system with voice commands over the radio.

---

## Option 1: Quick Test (Recommended)

In your terminal, run:

```bash
cd /path/to/memo-RF/build
./memo-rf
```

---

## Option 2: Use the Startup Script

If the mock server isn't running, use the helper script:

```bash
cd /path/to/memo-RF
./scripts/start_mock_test.sh
```

Then in another terminal:
```bash
cd /path/to/memo-RF/build
./memo-rf
```

---

## Voice Commands to Test

### 🛑 Emergency Stop (Priority 1 - Checked First)
- "stop the robot"
- "emergency stop"
- "halt"
- "freeze"
- "kill it"

### ✅ Release E-Stop (Priority 2)
- "release"
- "resume"
- "clear stop"
- "start up"

### 🎯 Navigation (Priority 10)
- "go to position five three"
- "navigate to ten twenty"
- "move to position zero zero"
- "go to five three" (spoken numbers work!)

### ⚙️ Mode Changes (Priority 10)
- "set mode to autonomous"
- "put the robot in sleep"
- "change to dance"
- "set mode to idle"

---

## What to Expect

### 1. Plugin Loads at Startup
You'll see log messages:
```
[INFO] Loaded action plugin: muni from config/plugins/muni_test.json
[INFO] STT vocab boost from 1 plugin(s): 312 chars
```

### 2. Voice Command Processing
When you speak a command:
1. **VAD** detects your voice
2. **Whisper STT** transcribes (with vocab boost from plugin)
3. **Plugin dispatcher** checks if command matches
4. **HTTP call** made to mock API at localhost:4890
5. **TTS response** spoken back ("Emergency stop activated.")
6. **No LLM used** - instant response!

### 3. Mock Server Logs
The mock server terminal will show received commands:
```
============================================================
Rover: frog-0
Command: {
  "type": "estop"
}
============================================================
```

### 4. Non-Plugin Commands
If you say something not in the plugin (e.g., "what time is it?"):
- Plugin won't match
- Falls through to LLM path (Ollama)
- Normal conversation mode

---

## Monitoring

### Check Mock Server Status
```bash
curl http://localhost:4890/health
```

### View Real-Time Logs
Memo-RF logs will show:
- `[JsonCommandPlugin] Matched command "estop" via phrase "stop the robot"`
- `[JsonCommandPlugin] Command "estop" succeeded (HTTP 200)`
- `[ActionDispatcher] Plugin "muni" handled transcript`

---

## Troubleshooting

### "Connection failed" errors
- Check if mock server is running: `curl http://localhost:4890/health`
- Restart with: `python3 scripts/test_muni_api.py &`

### Commands not matching
- Check plugin loaded: Look for `[INFO] Loaded action plugin: muni` at startup
- Verify config: `cat config/config.json | grep plugins`
- Should show: `"config_files": ["config/plugins/muni_test.json"]`

### Audio issues
- Check your audio device config in `config/config.json`
- Test with: `arecord -l` (list input devices)
- Test with: `aplay -l` (list output devices)

---

## Stop Testing

1. Press Ctrl+C to stop memo-rf
2. Stop mock server: `pkill -f test_muni_api.py`

---

## Switch Back to Production

To use the real depot endpoint:

```bash
# Edit config/config.json, change line 46:
"config_files": ["config/plugins/muni.json"]
# (instead of muni_test.json)
```

---

## Current Configuration

✅ **Config updated**: Using `config/plugins/muni_test.json`
✅ **Mock server**: Running on http://localhost:4890
✅ **Build**: Up to date with plugin system
✅ **Ready**: Just run `./build/memo-rf`

---

## Architecture Flow

```
You speak → Radio RX → Audio Input → VAD → Whisper STT (+vocab boost)
                                                   ↓
                                         "stop the robot"
                                                   ↓
                                         ActionDispatcher
                                                   ↓
                                   JsonCommandPlugin matches "estop"
                                                   ↓
                             HTTP POST localhost:4890/rovers/frog-0/command
                                        {"type": "estop"}
                                                   ↓
                                   Mock API responds HTTP 200
                                                   ↓
                           TTS: "Emergency stop activated."
                                                   ↓
                                    Audio Output → Radio TX
```

No LLM, no latency, instant command execution! 🚀
