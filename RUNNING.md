# Running memo-RF with Agentic Tools

## Prerequisites

1. **Ollama installed and running**
   - Check if Ollama is running: `curl http://localhost:11434/api/tags`
   - If not running, start it: `ollama serve` (usually runs automatically)

2. **Ollama model with function calling support**
   - You have `qwen2.5:7b` installed ✓
   - If you need to pull it: `ollama pull qwen2.5:7b`

3. **Other dependencies** (from original README):
   - Whisper model for STT
   - Piper TTS voice model
   - Audio devices configured

## Quick Start

### 1. Build the project

```bash
cd /Users/oliverhull/dev/memo-RF
./build.sh
```

Or manually:
```bash
mkdir -p build
cd build
cmake ..
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
```

### 2. Verify configuration

Check that `config/config.json` has:
- `llm.endpoint`: `"http://localhost:11434/api/chat"`
- `llm.model_name`: `"qwen2.5:7b"` (or whatever model you have)
- `tools.enabled`: `["log_memo", "external_research", "internal_search"]`

### 3. Run the agent

```bash
cd build
./memo-rf
```

Or from project root:
```bash
./build/memo-rf
```

## What to Expect

1. **Initialization**: The agent will:
   - Initialize audio I/O
   - Load Whisper STT model
   - Connect to Ollama
   - Register tools (log_memo, external_research, internal_search)
   - Start session recording

2. **Operation**: 
   - Listen for speech via microphone
   - Transcribe using Whisper
   - Route to LLM (with tool support)
   - If LLM calls tools, execute them and continue conversation
   - Synthesize response with Piper TTS
   - Transmit over radio audio output

3. **Tool Usage Examples**:
   - "Remember that I have a meeting at 3pm" → `log_memo` tool
   - "Search for weather today" → `external_research` tool (placeholder)
   - "What did I say about the project?" → `internal_search` tool

## Troubleshooting

### Ollama connection issues
```bash
# Check if Ollama is running
curl http://localhost:11434/api/tags

# Check if model exists
curl -X POST http://localhost:11434/api/show -d '{"name": "qwen2.5:7b"}'
```

### Model name mismatch
- If you see "model not found" errors, check what models you have:
  ```bash
  curl http://localhost:11434/api/tags
  ```
- Update `config/config.json` `llm.model_name` to match exactly

### Tool execution errors
- Check logs in the terminal output
- Tool results are logged with `LOG_LLM` prefix
- Memos are saved to `sessions/memos.txt`

### Audio issues
- List audio devices: `./build/memo-rf --list-devices` (if supported)
- Update `config/config.json` with specific device names if needed

## Session Logs

All sessions are logged to `sessions/YYYYMMDD_HHMMSS/`:
- `raw_input.wav` - Raw audio input
- `utterance_N.wav` - Individual speech segments
- `tts_N.wav` - Synthesized responses
- `session_log.json` - Event log with timings
- `memos.txt` - Memos created by `log_memo` tool

## Next Steps

- Implement actual web search API for `external_research` tool
- Connect to database for `internal_search` tool
- Add more tools as needed
- Fine-tune tool descriptions for better LLM usage
