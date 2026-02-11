#!/usr/bin/env python3
"""
DEAD SIMPLE Feed Server
Just receives POSTs and shows them. No complexity.
"""

from http.server import HTTPServer, BaseHTTPRequestHandler
import json
from datetime import datetime
from pathlib import Path

# In-memory storage
events = []
MAX_EVENTS = 100

# Config paths
CONFIG_PATH = Path(__file__).parent.parent / 'config' / 'config.json'
PERSONAS_PATH = Path(__file__).parent.parent / 'config' / 'personas.json'

# Simple HTML UI
HTML = """<!DOCTYPE html>
<html>
<head>
    <title>MEMO</title>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Courier New', monospace;
            background: #0a0a0a;
            color: #e0e0e0;
            padding: 20px;
        }
        .header {
            margin-bottom: 30px;
            padding-bottom: 20px;
            border-bottom: 2px solid #ff7700;
        }
        .header-top {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 15px;
            gap: 20px;
        }
        .control-group {
            display: flex;
            flex-direction: column;
            gap: 5px;
            width: 200px;
        }
        .control-group:last-child {
            align-items: flex-end;
        }
        .control-label {
            color: #888;
            font-size: 0.75em;
            text-transform: uppercase;
        }
        select {
            background: #1a1a1a;
            color: #ff7700;
            border: 1px solid #333;
            padding: 8px 12px;
            border-radius: 4px;
            font-family: 'Courier New', monospace;
            cursor: pointer;
            font-size: 0.9em;
        }
        select:hover {
            border-color: #ff7700;
        }
        h1 {
            color: #ff7700;
            font-size: 2.5em;
            text-shadow: 0 0 10px rgba(255, 119, 0, 0.5);
            margin: 0;
            flex: 1;
            text-align: center;
        }
        .status {
            text-align: center;
            color: #888;
            font-size: 0.9em;
        }
        .status.active { color: #00ff00; }
        .update-message {
            text-align: center;
            padding: 10px;
            background: #2a1a0a;
            border: 1px solid #ff7700;
            border-radius: 4px;
            color: #ff7700;
            font-size: 0.85em;
            margin-top: 10px;
            display: none;
        }
        .exchange {
            background: #1a1a1a;
            border: 1px solid #333;
            border-radius: 8px;
            padding: 15px;
            margin-bottom: 15px;
            transition: all 0.2s;
        }
        .exchange:hover {
            border-color: #ff7700;
            box-shadow: 0 0 15px rgba(255, 119, 0, 0.1);
        }
        .exchange-header {
            display: flex;
            justify-content: space-between;
            margin-bottom: 10px;
            padding-bottom: 8px;
            border-bottom: 1px solid #2a2a2a;
        }
        .persona {
            color: #ff7700;
            font-weight: bold;
        }
        .language-pill {
            display: inline-block;
            background: #2a2a2a;
            color: #6ab0f3;
            padding: 2px 8px;
            border-radius: 12px;
            font-size: 0.75em;
            margin-left: 8px;
            font-weight: 600;
        }
        .timestamp {
            color: #888;
            font-size: 0.85em;
        }
        .content {
            background: #0f0f0f;
            padding: 15px;
            border-left: 3px solid #333;
            border-radius: 4px;
            line-height: 1.6;
        }
        .transmission-label {
            color: #4a9eff;
            font-weight: bold;
            margin-right: 5px;
        }
        .response-label {
            color: #ff6b6b;
            font-weight: bold;
            margin-right: 5px;
        }
        .empty {
            text-align: center;
            padding: 60px 20px;
            color: #666;
            font-size: 1.2em;
        }
    </style>
</head>
<body>
    <div class="header">
        <div class="header-top">
            <div class="control-group">
                <label class="control-label">Persona</label>
                <select id="persona-select">
                    <option>Loading...</option>
                </select>
            </div>
            <h1>MEMO</h1>
            <div class="control-group">
                <label class="control-label">Language</label>
                <select id="language-select">
                    <option value="en">English</option>
                    <option value="fr">Français</option>
                    <option value="es">Español</option>
                    <option value="de">Deutsch</option>
                </select>
            </div>
        </div>
        <div class="status" id="status">Connecting...</div>
        <div class="update-message" id="update-msg"></div>
    </div>
    <div id="feed"></div>
    <script>
        let lastUpdate = 0;
        let currentPersona = '';
        let currentLanguage = '';

        async function loadConfig() {
            try {
                // Load personas
                const personasResp = await fetch('/api/personas');
                const personasData = await personasResp.json();
                const personaSelect = document.getElementById('persona-select');
                personaSelect.innerHTML = personasData.personas
                    .map(p => `<option value="${p.id}">${p.name}</option>`)
                    .join('');

                // Load current config
                const configResp = await fetch('/api/config');
                const config = await configResp.json();

                currentPersona = config.persona;
                currentLanguage = config.language;

                personaSelect.value = currentPersona;
                document.getElementById('language-select').value = currentLanguage;

                // Add change listeners
                personaSelect.addEventListener('change', async (e) => {
                    await updateConfig({ persona: e.target.value });
                });

                document.getElementById('language-select').addEventListener('change', async (e) => {
                    await updateConfig({ language: e.target.value });
                });

            } catch (error) {
                console.error('Failed to load config:', error);
            }
        }

        async function updateConfig(updates) {
            try {
                const response = await fetch('/api/config', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(updates)
                });
                const result = await response.json();

                const msg = document.getElementById('update-msg');
                msg.textContent = '✓ ' + result.message;
                msg.style.display = 'block';
                setTimeout(() => { msg.style.display = 'none'; }, 5000);

                if (updates.persona) currentPersona = updates.persona;
                if (updates.language) currentLanguage = updates.language;

            } catch (error) {
                console.error('Failed to update config:', error);
                const msg = document.getElementById('update-msg');
                msg.textContent = '✗ Update failed: ' + error.message;
                msg.style.display = 'block';
            }
        }

        async function loadFeed() {
            try {
                const response = await fetch('/api/feed');
                const data = await response.json();

                document.getElementById('status').textContent =
                    `Live • ${data.exchanges.length} exchanges`;
                document.getElementById('status').className = 'status active';

                if (data.exchanges.length === 0) {
                    document.getElementById('feed').innerHTML =
                        '<div class="empty">📻 No transmissions yet. Speak into the radio...</div>';
                    return;
                }

                let html = '';
                for (const ex of data.exchanges) {
                    const date = new Date(ex.timestamp_ms);
                    const time = date.toLocaleTimeString();

                    const langMap = {en: 'EN', fr: 'FR', es: 'ES', de: 'DE'};
                    const langCode = langMap[ex.language] || ex.language.toUpperCase();

                    html += `
                        <div class="exchange">
                            <div class="exchange-header">
                                <div>
                                    <span class="persona">${ex.persona_name || 'Unknown'}</span>
                                    <span class="language-pill">${langCode}</span>
                                </div>
                                <span class="timestamp">${time}</span>
                            </div>
                            <div class="content">
                                ${ex.transcript ? `
                                    <div><span class="label transmission-label">▶ </span>${escapeHtml(ex.transcript)}</div>
                                ` : ''}
                                ${ex.response ? `
                                    <div style="margin-top: 10px;"><span class="label response-label">◀ </span>${escapeHtml(ex.response)}</div>
                                ` : ''}
                            </div>
                        </div>
                    `;
                }

                document.getElementById('feed').innerHTML = html;
            } catch (error) {
                document.getElementById('status').textContent = 'Error: ' + error.message;
                document.getElementById('status').className = 'status';
            }
        }

        function escapeHtml(text) {
            const div = document.createElement('div');
            div.textContent = text;
            return div.innerHTML;
        }

        // Load config and personas
        loadConfig();

        // Load feed immediately
        loadFeed();

        // Poll every 2 seconds
        setInterval(loadFeed, 2000);
    </script>
</body>
</html>
"""

class SimpleFeedHandler(BaseHTTPRequestHandler):

    def send_json(self, data, status=200):
        """Helper to send JSON response"""
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.end_headers()
        self.wfile.write(json.dumps(data).encode())

    def do_POST(self):
        """Handle POST requests"""
        if self.path == '/api/config':
            # Update config
            try:
                length = int(self.headers.get('Content-Length', 0))
                body = self.rfile.read(length)
                updates = json.loads(body.decode('utf-8'))

                # Read current config
                with open(CONFIG_PATH, 'r') as f:
                    config = json.load(f)

                # Update persona and/or language
                if 'persona' in updates:
                    config['llm']['agent_persona'] = updates['persona']
                if 'language' in updates:
                    config['llm']['response_language'] = updates['language']

                # Write back
                with open(CONFIG_PATH, 'w') as f:
                    json.dump(config, f, indent=2)

                print(f"Config updated: persona={updates.get('persona', 'unchanged')}, language={updates.get('language', 'unchanged')}")
                self.send_json({'status': 'ok', 'message': 'Config updated. Restart agent to apply.'})

            except Exception as e:
                self.send_json({'error': str(e)}, 500)

        elif self.path == '/api/feed/notify':
            try:
                length = int(self.headers.get('Content-Length', 0))
                body = self.rfile.read(length)
                data = json.loads(body.decode('utf-8'))

                # Debug output
                print(f"[{data.get('event_type')}] Session: {data.get('session_id')} | {data.get('data')[:50]}...")

                # Add to memory
                events.insert(0, data)  # Newest first
                if len(events) > MAX_EVENTS:
                    events.pop()  # Remove oldest

                print(f"Total events in memory: {len(events)}")

                # Return OK
                self.send_response(200)
                self.send_header('Content-Type', 'application/json')
                self.end_headers()
                self.wfile.write(b'{"status":"ok"}')

            except Exception as e:
                self.send_response(500)
                self.send_header('Content-Type', 'application/json')
                self.end_headers()
                self.wfile.write(json.dumps({'error': str(e)}).encode())
        else:
            self.send_response(404)
            self.end_headers()

    def do_GET(self):
        """Handle GET requests"""
        if self.path == '/' or self.path == '/index.html':
            # Serve HTML UI
            self.send_response(200)
            self.send_header('Content-Type', 'text/html; charset=utf-8')
            self.end_headers()
            self.wfile.write(HTML.encode('utf-8'))
        elif self.path == '/api/personas':
            # Return list of personas
            try:
                with open(PERSONAS_PATH, 'r') as f:
                    personas_data = json.load(f)
                personas_list = [{'id': k, 'name': v['name']} for k, v in personas_data.items()]
                self.send_json({'personas': personas_list})
            except Exception as e:
                self.send_json({'error': str(e)}, 500)
        elif self.path == '/api/config':
            # Return current config
            try:
                with open(CONFIG_PATH, 'r') as f:
                    config = json.load(f)
                self.send_json({
                    'persona': config['llm']['agent_persona'],
                    'language': config['llm']['response_language']
                })
            except Exception as e:
                self.send_json({'error': str(e)}, 500)
        elif self.path == '/api/feed':
            # Build exchanges by pairing transcripts with responses
            # Events are stored newest first, so reverse to process in order
            exchanges = []
            pending_response = None

            # Process oldest to newest to pair correctly
            for event in reversed(events):
                event_type = event.get('event_type')

                if event_type == 'transcript':
                    # Create new exchange with transcript
                    exchange = {
                        'session_id': event.get('session_id', 'unknown'),
                        'timestamp_ms': event.get('timestamp_ms', 0),
                        'transcript': event.get('data', ''),
                        'response': '',
                        'persona_name': event.get('persona_name', 'Unknown'),
                        'language': event.get('language', 'en')
                    }
                    exchanges.append(exchange)

                elif event_type == 'llm_response':
                    # Find the most recent exchange without a response and add it
                    for ex in reversed(exchanges):
                        if not ex['response']:
                            ex['response'] = event.get('data', '')
                            break

            # Reverse to show newest first
            exchanges.reverse()

            response_data = json.dumps({'exchanges': exchanges})

            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(response_data.encode())
        else:
            self.send_response(404)
            self.end_headers()

    def log_message(self, format, *args):
        """Suppress request logging"""
        pass

if __name__ == '__main__':
    port = 5050
    server = HTTPServer(('0.0.0.0', port), SimpleFeedHandler)
    print(f"\n{'='*60}")
    print(f"Simple Feed Server running on port {port}")
    print(f"{'='*60}")
    print(f"Open in browser: http://localhost:{port}")
    print(f"API endpoint:    http://localhost:{port}/api/feed/notify")
    print(f"{'='*60}\n")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down...")
        server.shutdown()
