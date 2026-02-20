# Memo-RF Feed Server API Documentation

## Overview

The Memo-RF Feed Server provides real-time event broadcasting via Server-Sent Events (SSE). Any service can POST events to the feed server, which instantly broadcasts them to all connected web clients.

**Base URL**: `http://localhost:5050`

---

## Real-Time Event Notification Endpoint

### POST /api/feed/notify

Send events (transcripts, responses, or custom events) to broadcast in real-time to all connected web clients.

#### Request

**Headers**:
```
Content-Type: application/json
```

**JSON Body**:
```json
{
  "session_id": "20260210_171530",
  "timestamp_ms": 1707585330000,
  "event_type": "transcript" | "llm_response" | "custom",
  "data": "The actual message content",
  "persona_name": "Optional: Display name",
  "language": "en" | "es" | "fr" | "de"
}
```

#### Field Descriptions

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `session_id` | string | Yes | Session identifier (e.g., timestamp-based) |
| `timestamp_ms` | number | Yes | Unix timestamp in milliseconds |
| `event_type` | string | Yes | `"transcript"`, `"llm_response"`, or custom type |
| `data` | string | Yes | The message/event content |
| `persona_name` | string | No | Display name for the sender (default: "Unknown") |
| `language` | string | No | Language code (default: "en") |

#### Response

**Success (200 OK)**:
```json
{
  "status": "ok"
}
```

**Error (400 Bad Request)**:
```json
{
  "error": "Invalid JSON: ..."
}
```

**Error (500 Internal Server Error)**:
```json
{
  "error": "Error message here"
}
```

---

## Event Types

### 1. Transcript Event

Represents user input/speech.

**Example**:
```json
{
  "session_id": "20260210_171530",
  "timestamp_ms": 1707585330000,
  "event_type": "transcript",
  "data": "What is the current temperature?",
  "persona_name": "Field Operator",
  "language": "en"
}
```

**Result**: Creates a new exchange in the UI with the transcript. Response field will be empty until an `llm_response` event is received.

### 2. LLM Response Event

Represents agent/system response.

**Example**:
```json
{
  "session_id": "20260210_171530",
  "timestamp_ms": 1707585331000,
  "event_type": "llm_response",
  "data": "Current temperature is 72°F. Over.",
  "persona_name": "Control Center",
  "language": "en"
}
```

**Result**: If a matching transcript exists (same `session_id` and close `timestamp_ms`), updates that exchange. Otherwise creates a new exchange with only the response.

---

## Usage Examples

### Example 1: Using curl

**Send a transcript**:
```bash
curl -X POST http://localhost:5050/api/feed/notify \
  -H "Content-Type: application/json" \
  -d '{
    "session_id": "test_session_001",
    "timestamp_ms": 1707585330000,
    "event_type": "transcript",
    "data": "Request status update",
    "persona_name": "External System",
    "language": "en"
  }'
```

**Send a response**:
```bash
curl -X POST http://localhost:5050/api/feed/notify \
  -H "Content-Type: application/json" \
  -d '{
    "session_id": "test_session_001",
    "timestamp_ms": 1707585331000,
    "event_type": "llm_response",
    "data": "All systems operational. Over.",
    "persona_name": "Automated Monitor",
    "language": "en"
  }'
```

### Example 2: Using Python

```python
import requests
import time

def post_transcript(session_id, message, persona="External Service"):
    """Post a transcript event to the feed server."""
    payload = {
        "session_id": session_id,
        "timestamp_ms": int(time.time() * 1000),
        "event_type": "transcript",
        "data": message,
        "persona_name": persona,
        "language": "en"
    }

    response = requests.post(
        "http://localhost:5050/api/feed/notify",
        json=payload,
        timeout=1
    )
    return response.json()

def post_response(session_id, message, persona="System"):
    """Post a response event to the feed server."""
    payload = {
        "session_id": session_id,
        "timestamp_ms": int(time.time() * 1000),
        "event_type": "llm_response",
        "data": message,
        "persona_name": persona,
        "language": "en"
    }

    response = requests.post(
        "http://localhost:5050/api/feed/notify",
        json=payload,
        timeout=1
    )
    return response.json()

# Usage
session_id = f"external_{int(time.time())}"
post_transcript(session_id, "Temperature reading requested", "Sensor Node")
time.sleep(0.5)
post_response(session_id, "Temperature: 68°F", "Weather Station")
```

### Example 3: Using Node.js

```javascript
const axios = require('axios');

async function postEvent(eventType, data, sessionId, personaName) {
  const payload = {
    session_id: sessionId || `node_${Date.now()}`,
    timestamp_ms: Date.now(),
    event_type: eventType,
    data: data,
    persona_name: personaName || 'Node Service',
    language: 'en'
  };

  try {
    const response = await axios.post(
      'http://localhost:5050/api/feed/notify',
      payload,
      { timeout: 1000 }
    );
    return response.data;
  } catch (error) {
    console.error('Failed to post event:', error.message);
  }
}

// Usage
const sessionId = `external_${Date.now()}`;
await postEvent('transcript', 'System check requested', sessionId, 'Monitor');
await postEvent('llm_response', 'All systems nominal', sessionId, 'Automation');
```

### Example 4: IoT Integration

```python
#!/usr/bin/env python3
"""
Example: IoT temperature sensor posting to feed server
"""
import requests
import time
import random

FEED_URL = "http://localhost:5050/api/feed/notify"
SENSOR_ID = "sensor_001"

def post_temperature_reading():
    """Simulate temperature sensor posting data."""
    temp = round(random.uniform(65.0, 75.0), 1)
    humidity = random.randint(40, 60)
    session_id = f"iot_{SENSOR_ID}_{int(time.time())}"

    # Post the query
    requests.post(FEED_URL, json={
        "session_id": session_id,
        "timestamp_ms": int(time.time() * 1000),
        "event_type": "transcript",
        "data": f"Temperature reading request from {SENSOR_ID}",
        "persona_name": "IoT Sensor",
        "language": "en"
    }, timeout=1)

    # Simulate processing time
    time.sleep(0.5)

    # Post the reading
    requests.post(FEED_URL, json={
        "session_id": session_id,
        "timestamp_ms": int(time.time() * 1000),
        "event_type": "llm_response",
        "data": f"Temperature: {temp}°F | Humidity: {humidity}%",
        "persona_name": "Sensor Node",
        "language": "en"
    }, timeout=1)

    print(f"Posted: {temp}°F, {humidity}%")

if __name__ == '__main__':
    print(f"Starting IoT sensor: {SENSOR_ID}")
    while True:
        try:
            post_temperature_reading()
        except Exception as e:
            print(f"Error: {e}")
        time.sleep(10)  # Every 10 seconds
```

---

## SSE Stream Endpoint (for clients)

### GET /api/feed/stream

Web clients connect here to receive real-time updates via Server-Sent Events.

#### Response

**Headers**:
```
Content-Type: text/event-stream
Cache-Control: no-cache
Connection: keep-alive
```

#### Events Sent

**Initial snapshot**:
```
event: snapshot
data: {"exchanges": [...]}
```

**New transcript**:
```
event: new_transcript
data: {"session_id": "...", "transcript": "...", "timestamp_ms": ..., ...}
```

**New response**:
```
event: new_response
data: {"session_id": "...", "response": "...", "timestamp_ms": ..., ...}
```

**Heartbeat** (every 30 seconds):
```
: heartbeat
```

#### JavaScript Client Example

```javascript
const eventSource = new EventSource('http://localhost:5050/api/feed/stream');

eventSource.addEventListener('snapshot', (event) => {
  const data = JSON.parse(event.data);
  console.log('Initial exchanges:', data.exchanges);
});

eventSource.addEventListener('new_transcript', (event) => {
  const exchange = JSON.parse(event.data);
  console.log('New transcript:', exchange.data);
});

eventSource.addEventListener('new_response', (event) => {
  const exchange = JSON.parse(event.data);
  console.log('New response:', exchange.data);
});

eventSource.onerror = (error) => {
  console.error('SSE error:', error);
  // EventSource will auto-reconnect
};
```

---

## Architecture

### How It Works

```
External Service (Python/Node/curl/etc.)
    ↓
POST /api/feed/notify
    ↓
FeedHandler.handle_notify()
    ↓
SSEBroadcaster.broadcast()
    ↓
All connected web clients (via SSE)
    ↓
UI updates in real-time
```

### Data Flow

1. **External service POSTs** event to `/api/feed/notify`
2. **Feed server validates** JSON and extracts event data
3. **Broadcaster sends** to all SSE client queues
4. **JavaScript EventSource** receives event
5. **UI updates** immediately (< 100ms latency)

---

## Configuration

### C++ Agent Configuration

The Memo-RF C++ agent is pre-configured to POST events. To enable/disable:

**config/config.json**:
```json
{
  "feed_server_url": "http://localhost:5050/api/feed/notify"
}
```

- **Set to URL**: Agent will POST transcripts and responses
- **Set to empty string `""`**: Agent will not POST (feed server optional)

### Feed Server Configuration

Start the feed server:

```bash
python3 scripts/simple_feed.py
```

**Options**:
- `--port`: Port to listen on (default: 5050)
- `--host`: Host to bind to (default: 0.0.0.0)
- `--sessions-dir`: Path to sessions directory
- `--config-path`: Path to config.json

---

## Security Considerations

### Current Limitations

#### ⚠️ No Authentication
- **Current**: Any service on the network can POST events
- **Risk**: Unauthorized services can inject fake events

**Recommended Fix**:
```python
# Add to FeedHandler.handle_notify()
API_KEY = "your-secret-key-here"

def handle_notify(self):
    api_key = self.headers.get('X-API-Key')
    if api_key != API_KEY:
        return self.json_response(401, {'error': 'Unauthorized'})
    # ... rest of method
```

**Client usage**:
```bash
curl -X POST http://localhost:5050/api/feed/notify \
  -H "Content-Type: application/json" \
  -H "X-API-Key: your-secret-key-here" \
  -d '{...}'
```

#### ⚠️ No Rate Limiting
- **Current**: Unlimited POSTs accepted
- **Risk**: DoS attacks or accidental flooding

**Recommended Fix**: Add rate limiting per IP/session

#### ⚠️ No Persistence
- **Current**: External POSTs are only broadcast, not saved to disk
- **Note**: Only the C++ agent saves events to `sessions/` directory
- **Result**: External events only appear in real-time UI, not in session logs

---

## Testing

### Test the API with curl

```bash
# Test 1: Send a transcript
curl -X POST http://localhost:5050/api/feed/notify \
  -H "Content-Type: application/json" \
  -d '{
    "session_id": "test_001",
    "timestamp_ms": 1707585330000,
    "event_type": "transcript",
    "data": "Hello from external service",
    "persona_name": "Test Client",
    "language": "en"
  }'

# Expected response: {"status": "ok"}
```

```bash
# Test 2: Send a response
curl -X POST http://localhost:5050/api/feed/notify \
  -H "Content-Type: application/json" \
  -d '{
    "session_id": "test_001",
    "timestamp_ms": 1707585331000,
    "event_type": "llm_response",
    "data": "Message received. Over.",
    "persona_name": "Test System",
    "language": "en"
  }'

# Expected response: {"status": "ok"}
```

### Verify in UI

1. Open http://localhost:5050 in browser
2. Run curl commands above
3. See events appear instantly in the feed

---

## Troubleshooting

### Events not appearing in UI

**Check**:
1. Feed server is running: `ps aux | grep simple_feed`
2. Browser console for errors: F12 → Console
3. SSE connection active: Look for `Connected • N exchanges` in UI
4. POST response is `{"status": "ok"}`

### Connection refused errors

**Check**:
1. Feed server URL is correct: `http://localhost:5050/api/feed/notify`
2. Feed server is running and listening on correct port
3. Firewall allows connections to port 5050

### Events delayed or not real-time

**Check**:
1. Browser is using SSE (not polling fallback)
2. Check browser console: Should see `EventSource` connection
3. Network tab: Should see persistent `/api/feed/stream` connection

---

## API Summary

| Endpoint | Method | Purpose |
|----------|--------|---------|
| `/api/feed/notify` | POST | Submit events for broadcast |
| `/api/feed/stream` | GET | SSE stream for real-time updates |
| `/api/feed` | GET | Legacy polling endpoint (fallback) |
| `/api/config` | GET | Get current configuration |
| `/api/config` | POST | Update configuration |

---

## Example Integrations

### Slack Bot
```python
from slack_sdk import WebClient
import requests

def on_slack_message(message):
    """Forward Slack messages to feed."""
    requests.post("http://localhost:5050/api/feed/notify", json={
        "session_id": f"slack_{message['ts']}",
        "timestamp_ms": int(float(message['ts']) * 1000),
        "event_type": "transcript",
        "data": message['text'],
        "persona_name": message['user'],
        "language": "en"
    })
```

### Discord Bot
```javascript
client.on('messageCreate', async (message) => {
  await axios.post('http://localhost:5050/api/feed/notify', {
    session_id: `discord_${message.id}`,
    timestamp_ms: message.createdTimestamp,
    event_type: 'transcript',
    data: message.content,
    persona_name: message.author.username,
    language: 'en'
  });
});
```

### Home Assistant
```yaml
automation:
  - alias: "Temperature Alert to Feed"
    trigger:
      platform: numeric_state
      entity_id: sensor.temperature
      above: 80
    action:
      service: rest_command.post_to_feed
      data:
        session_id: "{{ now().timestamp() | int }}"
        data: "Temperature alert: {{ states('sensor.temperature') }}°F"
```

---

## License

This API is part of the Memo-RF project.

## Support

For issues or questions, see the main [README.md](../README.md) or open an issue on GitHub.
