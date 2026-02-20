# Auto-Restart Feature

## Overview

The feed server now automatically restarts the C++ agent when you change settings that require model reloading, such as:
- **Persona changes** (requires reloading system prompts)
- **Language changes** (requires loading different TTS voice models)

This eliminates the need to manually stop and restart the process from the terminal.

## How It Works

### 1. Detection
When you save configuration changes in the web UI, the system detects if the persona or language has changed.

### 2. Automatic Restart
If agent management is enabled, the system will:
1. Save the configuration
2. Stop the current C++ process
3. Start a new C++ process
4. Show a loading overlay with progress bar

### 3. Loading Indicator
While models are loading, you'll see:
- **Loading overlay** - prevents interaction until ready
- **Progress bar** - shows approximate loading progress
- **Status messages** - "Loading models, please wait..."

The UI automatically polls the `/api/agent/ready` endpoint to detect when models are fully loaded.

### 4. Ready Detection
The system determines the agent is ready when:
- The process is running (via `pgrep`)
- Recent session activity is detected (session logs updated in last 30 seconds)

## Enabling Auto-Restart

Auto-restart is available when you run the feed server:

**With simple_feed.py:**
```bash
python3 scripts/simple_feed.py
```

**Or use the run script with feed server enabled:**
```bash
MEMO_RF_FEED_SERVER=1 ./run.sh
```

### Custom Binary Path

If your memo-rf binary is in a non-standard location, configure it in the feed UI or config (see docs).

## Manual Restart

If agent management is not enabled, you'll see a message:
> ⚠️ Restart memo-rf manually for changes to take effect (persona/language changed)

In this case, restart manually:
```bash
# Stop current process
pkill -f memo-rf

# Start new process
./build/memo-rf config/config.json
```

## Technical Details

### API Endpoints

- **`/api/agent/status`** - Check if agent management is enabled and if process is running
- **`/api/agent/ready`** - Check if agent is ready (models loaded and active)
- **`/api/agent/restart`** - Restart the agent process (POST)

### Ready Check Logic

The agent is considered "ready" when:
1. Process is running (checked via `pgrep -f 'build/memo-rf'`)
2. Session logs show recent activity (modified within 30 seconds)

This ensures models are not just loaded, but the agent is actively listening.

### Timeout

The UI waits up to 30 seconds for the agent to be ready. If this times out:
- The loading overlay will close
- An error message will be shown
- You may need to manually restart or check logs

## Troubleshooting

### Loading Takes Too Long
If loading takes more than 30 seconds:
1. Check if models are on a slow filesystem
2. Check system resources (CPU/RAM)
3. Look at agent logs for errors

### Process Won't Start
If the agent won't start after restart:
1. Check terminal where feed server is running for errors
2. Try starting the agent manually: `./build/memo-rf config/config.json`
3. Check that model paths in config are correct

### No Auto-Restart Option
If you don't see automatic restart happening:
1. Verify feed server started with `--enable-agent-management`
2. Check that `build/memo-rf` binary exists
3. Look at browser console for errors

## Benefits

- **No terminal access needed** - manage everything from the web UI
- **Visual feedback** - loading bar shows progress
- **Safer** - prevents talking to the agent while models are loading
- **Convenient** - switch personas/languages instantly
