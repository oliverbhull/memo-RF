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
CONFIG_DIR = Path(__file__).parent.parent / 'config'
CONFIG_PATH = CONFIG_DIR / 'config.json'
PERSONAS_PATH = CONFIG_DIR / 'personas.json'
ACTIVE_PATH = CONFIG_DIR / 'active.json'
ROBOTS_DIR = CONFIG_DIR / 'robots'
AGENTS_DIR = CONFIG_DIR / 'agents'

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
            margin-bottom: 20px;
            padding-bottom: 15px;
            border-bottom: 2px solid #ff7700;
        }
        h1 {
            color: #ff7700;
            font-size: 2.5em;
            text-shadow: 0 0 10px rgba(255, 119, 0, 0.5);
            margin: 0;
            text-align: center;
        }
        .subtitle {
            text-align: center;
            color: #666;
            font-size: 0.8em;
            margin-top: 4px;
        }
        .status {
            text-align: center;
            color: #888;
            font-size: 0.9em;
            margin-top: 8px;
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

        /* --- Robot Selector Grid --- */
        .robots-section {
            margin-bottom: 24px;
        }
        .section-label {
            color: #888;
            font-size: 0.75em;
            text-transform: uppercase;
            letter-spacing: 1px;
            margin-bottom: 10px;
        }
        .robot-grid {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 10px;
        }
        @media (max-width: 700px) {
            .robot-grid { grid-template-columns: repeat(2, 1fr); }
        }
        .robot-card {
            background: #1a1a1a;
            border: 2px solid #2a2a2a;
            border-radius: 8px;
            padding: 14px;
            cursor: pointer;
            transition: all 0.15s;
            position: relative;
        }
        .robot-card:hover {
            border-color: #ff7700;
            background: #1f1a14;
            box-shadow: 0 0 20px rgba(255, 119, 0, 0.1);
        }
        .robot-card.active {
            border-color: #ff7700;
            background: #1f1a14;
            box-shadow: 0 0 20px rgba(255, 119, 0, 0.15);
        }
        .robot-card.active::after {
            content: 'ACTIVE';
            position: absolute;
            top: 8px;
            right: 8px;
            background: #ff7700;
            color: #000;
            font-size: 0.6em;
            font-weight: bold;
            padding: 2px 6px;
            border-radius: 3px;
            letter-spacing: 0.5px;
        }
        .robot-name {
            color: #ff7700;
            font-weight: bold;
            font-size: 1em;
            margin-bottom: 2px;
        }
        .robot-tagline {
            color: #888;
            font-size: 0.75em;
        }
        .robot-desc {
            color: #555;
            font-size: 0.7em;
            margin-top: 6px;
            line-height: 1.4;
        }

        /* --- Controls row --- */
        .controls-row {
            display: flex;
            gap: 12px;
            align-items: flex-end;
            margin-bottom: 24px;
        }
        .control-group {
            display: flex;
            flex-direction: column;
            gap: 4px;
        }
        .control-label {
            color: #888;
            font-size: 0.7em;
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }
        select {
            background: #1a1a1a;
            color: #ff7700;
            border: 1px solid #333;
            padding: 8px 12px;
            border-radius: 4px;
            font-family: 'Courier New', monospace;
            cursor: pointer;
            font-size: 0.85em;
        }
        select:hover { border-color: #ff7700; }

        /* --- Feed --- */
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
        <h1>MEMO</h1>
        <div class="subtitle">Founders, Inc. | Artifact 2026 Demo Day</div>
        <div class="status" id="status">Connecting...</div>
        <div class="update-message" id="update-msg"></div>
    </div>

    <div class="robots-section">
        <div class="section-label">Robots</div>
        <div class="robot-grid" id="robot-grid">
            <!-- Populated by JS -->
        </div>
    </div>
    <div class="robots-section">
        <div class="section-label">Agents</div>
        <div class="robot-grid" id="agent-grid">
            <!-- Populated by JS -->
        </div>
    </div>

    <div class="controls-row" id="controls-row">
        <div class="control-group" id="persona-control">
            <label class="control-label">All Personas</label>
            <select id="persona-select">
                <option>Loading...</option>
            </select>
        </div>
        <div class="control-group">
            <label class="control-label">Language</label>
            <select id="language-select">
                <option value="">Default</option>
                <option value="en">English</option>
                <option value="es">Espa&ntilde;ol</option>
                <option value="fr">Fran&ccedil;ais</option>
                <option value="de">Deutsch</option>
            </select>
        </div>
    </div>

    <div class="section-label">Feed</div>
    <div id="feed"></div>

    <script>
        let currentActive = '';
        let currentPersona = '';
        let currentLanguage = '';
        let allPersonas = [];
        let identityMode = false;

        async function loadConfig() {
            try {
                const [configResp, robotsResp, agentsResp] = await Promise.all([
                    fetch('/api/config'),
                    fetch('/api/robots').catch(() => ({ json: () => ({ robots: [] }) })),
                    fetch('/api/agents').catch(() => ({ json: () => ({ agents: [] }) }))
                ]);
                const config = await configResp.json();
                const robotsData = await robotsResp.json();
                const agentsData = await agentsResp.json();
                const robots = robotsData.robots || [];
                const agents = agentsData.agents || [];

                currentLanguage = config.language || config.response_language || '';
                const langSelect = document.getElementById('language-select');
                const langValues = Array.from(langSelect.options).map(o => o.value);
                langSelect.value = langValues.includes(currentLanguage) ? currentLanguage : '';

                const hasRobotsOrAgents = robots.length > 0 || agents.length > 0;
                if (hasRobotsOrAgents) {
                    identityMode = true;
                    document.getElementById('persona-control').style.display = 'none';
                    currentActive = (config.active && [].concat(
                        robots.map(r => 'robots/' + r.id),
                        agents.map(a => 'agents/' + a.id)
                    ).includes(config.active)) ? config.active : (robots[0] ? 'robots/' + robots[0].id : agents[0] ? 'agents/' + agents[0].id : '');
                    const robotGrid = document.getElementById('robot-grid');
                    robotGrid.innerHTML = robots.map(r => {
                        const activeVal = 'robots/' + r.id;
                        return `<div class="robot-card" data-active="${activeVal}" onclick="selectIdentity('${activeVal.replace(/'/g, "\\'")}')">
                            <div class="robot-name">${escapeHtml(r.name)}</div>
                            <div class="robot-tagline">${escapeHtml(r.tagline || '')}</div>
                            <div class="robot-desc">${escapeHtml(r.description || '')}</div>
                        </div>`;
                    }).join('');
                    const agentGrid = document.getElementById('agent-grid');
                    agentGrid.innerHTML = agents.map(a => {
                        const activeVal = 'agents/' + a.id;
                        return `<div class="robot-card" data-active="${activeVal}" onclick="selectIdentity('${activeVal.replace(/'/g, "\\'")}')">
                            <div class="robot-name">${escapeHtml(a.name)}</div>
                            <div class="robot-tagline">${escapeHtml(a.tagline || '')}</div>
                            <div class="robot-desc">${escapeHtml(a.description || '')}</div>
                        </div>`;
                    }).join('');
                    highlightActiveIdentity();
                } else {
                    identityMode = false;
                    document.getElementById('persona-control').style.display = '';
                    currentPersona = config.persona || '';
                    const personasResp = await fetch('/api/personas');
                    const personasData = await personasResp.json();
                    allPersonas = personasData.personas || [];
                    const demoRobots = allPersonas.filter(p => p.category === 'founders_demo');
                    document.getElementById('robot-grid').innerHTML = demoRobots.map(r => `
                        <div class="robot-card" data-id="${escapeHtml(r.id)}" onclick="selectRobot('${escapeHtml(r.id)}')">
                            <div class="robot-name">${escapeHtml(r.name)}</div>
                            <div class="robot-tagline">${escapeHtml(r.tagline || '')}</div>
                            <div class="robot-desc">${escapeHtml(r.description || '')}</div>
                        </div>
                    `).join('');
                    document.getElementById('agent-grid').innerHTML = '';
                    const personaSelect = document.getElementById('persona-select');
                    const personaOptions = allPersonas.map(p => `<option value="${escapeHtml(p.id)}">${escapeHtml(p.name)}</option>`).join('');
                    personaSelect.innerHTML = personaOptions || '<option value="">— No personas —</option>';
                    const hasCurrent = allPersonas.some(p => p.id === currentPersona);
                    personaSelect.value = hasCurrent ? currentPersona : (allPersonas[0] ? allPersonas[0].id : '');
                    if (!hasCurrent && currentPersona) currentPersona = personaSelect.value;
                    highlightActiveRobot();
                }

                const personaSelect = document.getElementById('persona-select');
                if (personaSelect && !identityMode) {
                    personaSelect.replaceWith(personaSelect.cloneNode(true));
                    document.getElementById('persona-select').addEventListener('change', async (e) => {
                        await updateConfig({ persona: e.target.value });
                    });
                }
            } catch (error) {
                console.error('Failed to load config:', error);
            }
        }

        function highlightActiveIdentity() {
            document.querySelectorAll('#robot-grid .robot-card, #agent-grid .robot-card').forEach(card => {
                card.classList.toggle('active', card.dataset.active === currentActive);
            });
        }

        function highlightActiveRobot() {
            document.querySelectorAll('.robot-card').forEach(card => {
                if (card.dataset.id !== undefined)
                    card.classList.toggle('active', card.dataset.id === currentPersona);
            });
            const personaSelect = document.getElementById('persona-select');
            if (personaSelect) personaSelect.value = currentPersona;
        }

        async function selectIdentity(activeValue) {
            await updateConfig({ active: activeValue });
            currentActive = activeValue;
            highlightActiveIdentity();
        }

        async function selectRobot(id) {
            await updateConfig({ persona: id });
            currentPersona = id;
            highlightActiveRobot();
        }

        async function updateConfig(updates) {
            try {
                const response = await fetch('/api/config', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(updates)
                });
                const result = await response.json();
                if (result.error) throw new Error(result.error);
                const msg = document.getElementById('update-msg');
                msg.textContent = result.message || 'Config updated. Restart agent to apply.';
                msg.style.display = 'block';
                setTimeout(() => { msg.style.display = 'none'; }, 5000);
                if (updates.active) currentActive = updates.active;
                if (updates.persona) currentPersona = updates.persona;
                if (updates.language) currentLanguage = updates.language;
            } catch (error) {
                console.error('Failed to update config:', error);
                const msg = document.getElementById('update-msg');
                msg.textContent = 'Update failed: ' + (error.message || error);
                msg.style.display = 'block';
            }
        }

        async function loadFeed() {
            try {
                const response = await fetch('/api/feed');
                const data = await response.json();

                document.getElementById('status').textContent =
                    `Live | ${data.exchanges.length} exchanges`;
                document.getElementById('status').className = 'status active';

                if (data.exchanges.length === 0) {
                    document.getElementById('feed').innerHTML =
                        '<div class="empty">No transmissions yet. Speak into the radio...</div>';
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
                                    <div><span class="label transmission-label">&gt; </span>${escapeHtml(ex.transcript)}</div>
                                ` : ''}
                                ${ex.response ? `
                                    <div style="margin-top: 10px;"><span class="label response-label">&lt; </span>${escapeHtml(ex.response)}</div>
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

        document.getElementById('language-select').addEventListener('change', async (e) => {
            await updateConfig({ language: e.target.value });
        });
        loadConfig();
        loadFeed();
        setInterval(loadFeed, 2000);
    </script>
</body>
</html>
"""

def scan_identity_dir(dir_path):
    """Scan a directory for *.json identity files; return list of { id, name, tagline, description }."""
    out = []
    if not dir_path.is_dir():
        return out
    for f in sorted(dir_path.glob('*.json')):
        try:
            with open(f, 'r') as fp:
                data = json.load(fp)
            ident = data.get('identity') or {}
            out.append({
                'id': ident.get('id') or f.stem,
                'name': ident.get('name') or f.stem,
                'tagline': ident.get('tagline', ''),
                'description': ident.get('description', ''),
            })
        except Exception:
            continue
    return out


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
            try:
                length = int(self.headers.get('Content-Length', 0))
                body = self.rfile.read(length)
                updates = json.loads(body.decode('utf-8'))

                if 'active' in updates or ACTIVE_PATH.exists():
                    active_data = {}
                    if ACTIVE_PATH.exists():
                        try:
                            with open(ACTIVE_PATH, 'r') as f:
                                active_data = json.load(f)
                        except Exception:
                            pass
                    if 'active' in updates:
                        active_data['active'] = updates['active']
                    if 'language' in updates:
                        active_data['response_language'] = updates['language']
                    with open(ACTIVE_PATH, 'w') as f:
                        json.dump(active_data, f, indent=2)
                    print(f"Active updated: {active_data.get('active', '')}")
                    self.send_json({'status': 'ok', 'message': 'Config updated. Restart agent to apply.'})
                else:
                    if not CONFIG_PATH.exists():
                        self.send_json({'error': 'config.json not found'}, 500)
                        return
                    with open(CONFIG_PATH, 'r') as f:
                        config = json.load(f)
                    if 'persona' in updates:
                        config['llm']['agent_persona'] = updates['persona']
                    if 'language' in updates:
                        config['llm']['response_language'] = updates['language']
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
        elif self.path == '/api/robots':
            try:
                robots = scan_identity_dir(ROBOTS_DIR)
                self.send_json({'robots': robots})
            except Exception as e:
                self.send_json({'error': str(e)}, 500)
        elif self.path == '/api/agents':
            try:
                agents = scan_identity_dir(AGENTS_DIR)
                self.send_json({'agents': agents})
            except Exception as e:
                self.send_json({'error': str(e)}, 500)
        elif self.path == '/api/personas':
            try:
                with open(PERSONAS_PATH, 'r') as f:
                    personas_data = json.load(f)
                personas_list = [
                    {
                        'id': k,
                        'name': v['name'],
                        'category': v.get('category', ''),
                        'tagline': v.get('tagline', ''),
                        'description': v.get('description', ''),
                    }
                    for k, v in personas_data.items()
                ]
                self.send_json({'personas': personas_list})
            except Exception as e:
                self.send_json({'error': str(e)}, 500)
        elif self.path == '/api/config':
            try:
                if ACTIVE_PATH.exists():
                    with open(ACTIVE_PATH, 'r') as f:
                        active_data = json.load(f)
                    self.send_json({
                        'active': active_data.get('active', ''),
                        'language': active_data.get('response_language', ''),
                        'response_language': active_data.get('response_language', ''),
                    })
                else:
                    with open(CONFIG_PATH, 'r') as f:
                        config = json.load(f)
                    self.send_json({
                        'persona': config.get('llm', {}).get('agent_persona', ''),
                        'language': config.get('llm', {}).get('response_language', '')
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
