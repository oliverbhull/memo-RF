# agents-sdk integration

Memo-RF can use the [RunEdgeAI agents-sdk](https://github.com/RunEdgeAI/agents-sdk) (C++ agent framework) for the LLM path instead of the built-in `LLMClient` + tool loop.

## Build with agents-sdk

1. Clone agents-sdk as a sibling of memo-RF (or set `AGENTS_SDK_DIR`):
   ```bash
   cd /path/to/parent
   git clone https://github.com/RunEdgeAI/agents-sdk.git
   cd memo-RF
   ```

2. Configure and build:
   ```bash
   mkdir build && cd build
   cmake .. -DUSE_AGENTS_SDK=ON
   make
   ```
   Or with a custom SDK path:
   ```bash
   cmake .. -DUSE_AGENTS_SDK=ON -DAGENTS_SDK_DIR=/path/to/agents-sdk
   make
   ```

3. Requirements when `USE_AGENTS_SDK=ON`:
   - C++20 (enabled automatically)
   - agents-sdk `include/` and prebuilt `lib/<platform>/libagents_cpp_shared_lib.*`
   - Supported platforms: macOS, Linux, Windows (paths in `CMakeLists.txt`)

## Behaviour

- **LLM:** Ollama only. By default endpoint and model come from memo-RF `config.json` (`llm.endpoint`, `llm.model_name`). If `llm.agents_sdk_env_path` is set to the path of agents-sdk’s `.env`, the bridge reads **OLLAMA_BASE_URL** and **MODEL** from that file and uses them for Ollama (so you match the SDK demos and avoid 400s from mismatched base URL or model).
- **Agent:** One `ActorAgent` per run, created at init. Each user utterance is handled with `blockingWait(agent.run(transcript))`; the reply is taken from the result `"answer"` (or `"response"`) and sent to TTS as before.
- **Router / fast path:** Unchanged. Keywords (roger, copy, etc.) still skip the agent.
- **Tools:** With the **prebuilt** agents-sdk shared library, memo-RF tools (log_memo, internal_search, external_research) are **not** registered with the SDK because of nlohmann/json ABI differences between the SDK build and memo-RF. If you need tools, build memo-RF **without** `-DUSE_AGENTS_SDK=ON` (default) so the legacy LLM + tool loop is used.

## Manufacturing floor router

When `llm.use_manufacturing_router` is `true` (and `USE_AGENTS_SDK=ON`), the bridge uses a **manufacturing floor router** instead of a single ActorAgent:

1. **Classify:** The transcript is sent to the LLM with a short prompt asking for one category: **maintenance**, **production**, **safety**, or **status**.
2. **Dispatch:** Based on the category, the bridge calls in-memory stub tools and formats a single radio reply:
   - **maintenance** – `log_work_order` (returns ticket ID) → "Work order N logged. Maintenance notified. Over."
   - **production** – `get_run_count` → "Run count N. Over."
   - **safety** – `log_safety_incident` → "Incident logged. Safety notified. Over."
   - **status** – `get_work_order_status(transcript)` → short status line + "Over."
3. **Radio:** Replies are kept to one or two short sentences and end with "Over."; memo-RF then runs `ensure_ends_with_over`, TTS, and TX as usual.

**Status updates:**

- **Same-turn:** The maintenance reply includes the work order number in that one transmission.
- **Follow-up:** The user can ask later (e.g. "Status on Machine 3?"); the **status** route calls `get_work_order_status` and returns a short line (e.g. "Ticket 447 dispatched. ETA 15 minutes. Over.").
- **Proactive** (base transmits without user asking) is not supported in the current pipeline; it would require a way to inject a message into the TX queue when the radio is idle.

Stub data (work orders, run counts, safety incidents) is in-memory only and resets when memo-RF restarts.

## Config

Use the same `config/config.json` as without the SDK. Relevant keys:

- `llm.endpoint` – Ollama API base path (e.g. `http://localhost:11434/api/chat`). Ignored for Ollama when `agents_sdk_env_path` is set.
- `llm.model_name` – Model name (e.g. `qwen2.5:7b`). Ignored for Ollama when `agents_sdk_env_path` is set.
- `llm.agents_sdk_env_path` – Optional. Path to agents-sdk’s `.env`. When set, the bridge uses **OLLAMA_BASE_URL** and **MODEL** from that file for the Ollama client (same as SDK demos). Example: `"/path/to/agents-sdk/.env"`.
- `llm.system_prompt`, `llm.temperature`, `llm.max_tokens`, `llm.timeout_ms` – Applied to the SDK LLM/agent
- `llm.use_manufacturing_router` – When `true`, use the manufacturing floor router (maintenance/production/safety/status) instead of the generic ActorAgent. Requires `USE_AGENTS_SDK=ON`.

If you use Ollama with the same model as agents-sdk (e.g. `mistral:instruct`), set `llm.agents_sdk_env_path` to your agents-sdk `.env` so base URL and model match the SDK.

## Adding tools with agents-sdk later

To use memo-RF tools through the SDK (log_memo, internal_search, external_research), the SDK must be built from source with the same nlohmann/json version/ABI as memo-RF (or memo-RF built against the SDK’s bundled json). The bridge code in `src/agents_sdk_bridge.cpp` has a placeholder for registering tools via `agents::createTool`; once the ABI matches, that block can be re-enabled and the tool adapters wired to the memo-RF tool implementations.
