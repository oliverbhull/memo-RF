# Feed Server API - Quick Reference

## Send Events to the Feed

**Endpoint**: `POST http://localhost:5050/api/feed/notify`

**Headers**: `Content-Type: application/json`

**Body**:
```json
{
  "session_id": "your_app_123",
  "timestamp_ms": 1707585330000,
  "event_type": "transcript",
  "data": "Your message here",
  "persona_name": "Your App Name",
  "language": "en"
}
```

## Fields

- **session_id**: Unique ID for this conversation (required)
- **timestamp_ms**: Current time in milliseconds (required)
- **event_type**: Either `"transcript"` (incoming message) or `"llm_response"` (reply) (required)
- **data**: The actual message text (required)
- **persona_name**: Display name in the UI (optional)
- **language**: `"en"`, `"es"`, `"fr"`, or `"de"` (optional, defaults to "en")

## Examples

### Python
```python
import requests
import time

# Send a message
requests.post("http://localhost:5050/api/feed/notify", json={
    "session_id": f"myapp_{int(time.time())}",
    "timestamp_ms": int(time.time() * 1000),
    "event_type": "transcript",
    "data": "Hello from my app!",
    "persona_name": "My App",
    "language": "en"
})

# Send a reply
requests.post("http://localhost:5050/api/feed/notify", json={
    "session_id": f"myapp_{int(time.time())}",
    "timestamp_ms": int(time.time() * 1000),
    "event_type": "llm_response",
    "data": "Response from my app!",
    "persona_name": "My App",
    "language": "en"
})
```

### curl
```bash
curl -X POST http://localhost:5050/api/feed/notify \
  -H "Content-Type: application/json" \
  -d '{
    "session_id": "test_123",
    "timestamp_ms": 1707585330000,
    "event_type": "transcript",
    "data": "Hello!",
    "persona_name": "Test App",
    "language": "en"
  }'
```

### JavaScript/Node.js
```javascript
fetch("http://localhost:5050/api/feed/notify", {
  method: "POST",
  headers: { "Content-Type": "application/json" },
  body: JSON.stringify({
    session_id: `myapp_${Date.now()}`,
    timestamp_ms: Date.now(),
    event_type: "transcript",
    data: "Hello from JS!",
    persona_name: "My App",
    language: "en"
  })
});
```

## That's It!

Messages appear instantly in the feed UI at http://localhost:5050

**Success Response**: `{"status": "ok"}`

**Error Response**: `{"error": "message"}` with HTTP 400/500
