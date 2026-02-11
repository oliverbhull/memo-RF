#!/usr/bin/env python3
"""
Memo-RF Feed Server
Serves a web UI for viewing radio transmissions and configuring the agent.

Usage: python3 scripts/feed_server.py [--port PORT] [--host HOST]
"""

import json
import os
import sys
import shutil
import subprocess
import signal
import time
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse
from pathlib import Path
import argparse

# ============================================================================
# Styles
# ============================================================================

CSS = """
* { margin: 0; padding: 0; box-sizing: border-box; }
body {
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
    background: #0a0a0a; color: #e0e0e0; line-height: 1.6;
}
.container { max-width: 1200px; margin: 0 auto; padding: 20px; }
header {
    text-align: center; padding: 30px 0;
    border-bottom: 2px solid #333; margin-bottom: 20px;
}
h1 {
    font-size: 2.5em; font-weight: 300; color: #ff7700;
    text-shadow: 0 0 10px rgba(255, 119, 0, 0.5);
    font-family: 'Courier New', monospace;
}
.subtitle { color: #888; margin-top: 10px; font-size: 0.9em; }

/* Tabs */
.tabs { display: flex; gap: 10px; margin-bottom: 20px; border-bottom: 2px solid #333; }
.tab {
    padding: 12px 24px; background: #1a1a1a; border: none;
    border-radius: 8px 8px 0 0; color: #888; cursor: pointer;
    font-family: 'Courier New', monospace; transition: all 0.2s;
}
.tab:hover { color: #ff7700; background: #222; }
.tab.active {
    color: #ff7700; background: #222;
    border-bottom: 2px solid #ff7700; position: relative; bottom: -2px;
}
.tab-content { display: none; }
.tab-content.active { display: block; }

/* Common */
.controls {
    display: flex; justify-content: space-between; align-items: center;
    margin-bottom: 20px; padding: 15px; background: #1a1a1a;
    border-radius: 8px; border: 1px solid #333;
}
.status { display: flex; align-items: center; gap: 10px; }
.status-indicator {
    width: 12px; height: 12px; border-radius: 50%; background: #666;
}
.status-indicator.active {
    background: #ff7700; box-shadow: 0 0 10px rgba(255, 119, 0, 0.5);
    animation: pulse 2s infinite;
}
@keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.5; } }

button {
    padding: 8px 16px; background: #222; border: 1px solid #444;
    color: #ff7700; border-radius: 4px; cursor: pointer;
    font-family: 'Courier New', monospace; transition: all 0.2s;
}
button:hover { background: #333; border-color: #ff7700; }
button:disabled { opacity: 0.5; cursor: not-allowed; }
.btn-primary { background: #ff7700; color: #0a0a0a; font-weight: 600; }
.btn-primary:hover { background: #ff8800; }

/* Feed */
.feed { display: flex; flex-direction: column; gap: 15px; }
.exchange {
    background: #1a1a1a; border: 1px solid #333;
    border-radius: 8px; padding: 15px; transition: all 0.2s; margin-bottom: 10px;
}
.exchange:hover { border-color: #ff7700; box-shadow: 0 0 15px rgba(255, 119, 0, 0.1); }
.exchange-header {
    display: flex; justify-content: space-between; align-items: center;
    margin-bottom: 12px; padding-bottom: 8px; border-bottom: 1px solid #2a2a2a;
}
.session-id { font-family: 'Courier New', monospace; color: #ff7700; font-size: 0.75em; }
.timestamp { color: #888; font-size: 0.8em; }
.metadata { display: flex; gap: 8px; align-items: center; margin-bottom: 10px; }
.badge {
    padding: 3px 8px; border-radius: 4px; font-size: 0.75em;
    font-family: 'Courier New', monospace; font-weight: 600;
}
.badge-persona { background: #2a3a4a; color: #6ab0f3; }
.badge-language { background: #3a2a4a; color: #c96ab0; }
.transmission, .response { margin-bottom: 8px; }
.label {
    font-weight: 600; margin-bottom: 4px; font-size: 0.75em;
    text-transform: uppercase; letter-spacing: 0.05em;
}
.transmission .label { color: #4a9eff; }
.response .label { color: #ff6b6b; }
.content {
    padding: 10px; background: #0f0f0f; border-left: 3px solid #333;
    border-radius: 4px; font-family: 'Courier New', monospace; font-size: 0.9em;
    line-height: 1.5;
}
.transmission .content { border-left-color: #4a9eff; }
.response .content { border-left-color: #ff6b6b; }
.no-response { opacity: 0.5; font-style: italic; }

/* Config */
.config-form {
    background: #1a1a1a; border: 1px solid #333;
    border-radius: 8px; padding: 30px; max-width: 800px;
}
.form-section { margin-bottom: 30px; }
.section-title {
    color: #ff7700; font-size: 1.2em; margin-bottom: 15px;
    padding-bottom: 10px; border-bottom: 1px solid #333;
    font-family: 'Courier New', monospace;
}
.form-group { margin-bottom: 20px; }
.form-group label { display: block; margin-bottom: 8px; color: #aaa; font-size: 0.9em; }
.form-group input[type="text"],
.form-group input[type="number"],
.form-group select {
    width: 100%; padding: 10px; background: #0f0f0f;
    border: 1px solid #333; border-radius: 4px; color: #e0e0e0;
    font-family: 'Courier New', monospace; font-size: 0.95em;
}
.form-group input:focus,
.form-group select:focus { outline: none; border-color: #ff7700; }
.form-group input[type="checkbox"] {
    width: 20px; height: 20px; margin-right: 10px; cursor: pointer;
}
.checkbox-group { display: flex; align-items: center; }
.checkbox-group label { margin-bottom: 0; cursor: pointer; }
.form-actions {
    display: flex; gap: 10px; margin-top: 30px;
    padding-top: 20px; border-top: 1px solid #333;
}

/* Messages */
.success {
    background: #1a2a1a; border: 1px solid #00ff00; color: #00ff00;
    padding: 15px; border-radius: 8px; margin-bottom: 20px;
}
.error {
    background: #2a1515; border: 1px solid #ff6b6b; color: #ff6b6b;
    padding: 15px; border-radius: 8px; margin-bottom: 20px;
}
.info {
    background: #2a1a0a; border: 1px solid #ff7700; color: #ff7700;
    padding: 15px; border-radius: 8px; margin-bottom: 20px;
}
.loading, .empty-state { text-align: center; padding: 40px; color: #666; }
.empty-state-icon { font-size: 4em; margin-bottom: 20px; opacity: 0.3; }
"""

# ============================================================================
# JavaScript
# ============================================================================

JAVASCRIPT = """
let autoRefreshInterval = null;
let currentTab = 'feed';

async function switchTab(tabName) {
    currentTab = tabName;
    document.querySelectorAll('.tab').forEach(tab => tab.classList.remove('active'));
    event.target.classList.add('active');
    document.querySelectorAll('.tab-content').forEach(content => content.classList.remove('active'));
    document.getElementById(tabName + '-tab').classList.add('active');

    if (tabName === 'feed') { loadFeed(); startAutoRefresh(); }
    else if (tabName === 'config') { stopAutoRefresh(); await loadPersonas(); await loadConfig(); }
}

async function loadFeed() {
    const indicator = document.getElementById('status-indicator');
    const statusText = document.getElementById('status-text');
    const feedContainer = document.getElementById('feed');
    const errorContainer = document.getElementById('error-container');

    try {
        statusText.textContent = 'Loading...';
        indicator.classList.remove('active');

        const response = await fetch('/api/feed');
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        const data = await response.json();

        indicator.classList.add('active');
        statusText.textContent = `Connected • ${data.exchanges.length} exchanges`;
        errorContainer.innerHTML = '';

        if (data.exchanges.length === 0) {
            feedContainer.innerHTML = `
                <div class="empty-state">
                    <div class="empty-state-icon">📻</div>
                    <h2>No Transmissions Yet</h2>
                    <p>Start Memo-RF to see radio transmissions appear here.</p>
                </div>`;
            return;
        }

        feedContainer.innerHTML = data.exchanges.map(ex => {
            const date = new Date(ex.timestamp_ms);
            // Check both persona fields and trim whitespace
            let personaName = 'Unknown';
            if (ex.persona_name && ex.persona_name.trim()) {
                personaName = ex.persona_name.trim();
            } else if (ex.persona && ex.persona.trim()) {
                personaName = ex.persona.trim();
            }
            const language = ex.language || 'en';
            const langLabel = {en: 'EN', es: 'ES', fr: 'FR', de: 'DE'}[language] || language.toUpperCase();
            const hasResponse = ex.response && ex.response !== '(no response)';

            return `
                <div class="exchange">
                    <div class="exchange-header">
                        <div style="display: flex; align-items: center; gap: 10px;">
                            <span class="session-id">${ex.session_id}</span>
                            <div class="metadata">
                                <span class="badge badge-persona">${escapeHtml(personaName)}</span>
                                <span class="badge badge-language">${langLabel}</span>
                            </div>
                        </div>
                        <span class="timestamp">${date.toLocaleString()}</span>
                    </div>
                    <div class="transmission">
                        <div class="label">Transmission</div>
                        <div class="content">${escapeHtml(ex.transcript || '(no transcript)')}</div>
                    </div>
                    <div class="response">
                        <div class="label">Response</div>
                        <div class="content ${hasResponse ? '' : 'no-response'}">${escapeHtml(ex.response || '(no response)')}</div>
                    </div>
                </div>`;
        }).join('');
    } catch (error) {
        indicator.classList.remove('active');
        statusText.textContent = 'Error';
        errorContainer.innerHTML = `<div class="error">Failed to load feed: ${escapeHtml(error.message)}</div>`;
        feedContainer.innerHTML = '';
    }
}

async function loadConfig() {
    const msgContainer = document.getElementById('config-message-container');
    msgContainer.innerHTML = '<div class="loading">Loading configuration...</div>';

    try {
        const response = await fetch('/api/config');
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        const config = await response.json();

        document.getElementById('agent-persona').value = config.llm?.agent_persona || '';
        document.getElementById('response-language').value = config.llm?.response_language || 'en';
        document.getElementById('model-name').value = config.llm?.model_name || '';
        document.getElementById('temperature').value = config.llm?.temperature || 0.7;
        document.getElementById('wake-word-enabled').checked = config.wake_word?.enabled || false;

        msgContainer.innerHTML = '';
    } catch (error) {
        msgContainer.innerHTML = `<div class="error">Failed to load: ${escapeHtml(error.message)}</div>`;
    }
}

async function loadPersonas() {
    try {
        const response = await fetch('/api/personas');
        if (!response.ok) return;
        const data = await response.json();
        document.getElementById('agent-persona').innerHTML =
            data.personas.map(p => `<option value="${p.id}">${p.name}</option>`).join('');
    } catch (error) {
        console.error('Failed to load personas:', error);
    }
}

async function saveConfig() {
    const msgContainer = document.getElementById('config-message-container');
    msgContainer.innerHTML = '<div class="loading">Saving configuration...</div>';

    try {
        const updates = {
            llm: {
                agent_persona: document.getElementById('agent-persona').value,
                response_language: document.getElementById('response-language').value,
                model_name: document.getElementById('model-name').value,
                temperature: parseFloat(document.getElementById('temperature').value)
            },
            wake_word: {
                enabled: document.getElementById('wake-word-enabled').checked
            }
        };

        const response = await fetch('/api/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(updates)
        });

        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        const result = await response.json();

        msgContainer.innerHTML = '<div class="success">Configuration saved!</div>';

        // Check if agent management is enabled
        const statusResp = await fetch('/api/agent/status');
        if (statusResp.ok) {
            const status = await statusResp.json();
            if (status.enabled) {
                msgContainer.innerHTML = '<div class="loading">Restarting agent...</div>';
                const restartResp = await fetch('/api/agent/restart', { method: 'POST' });
                if (restartResp.ok) {
                    msgContainer.innerHTML = '<div class="success">✓ Configuration saved and agent restarted!</div>';
                } else {
                    msgContainer.innerHTML = `
                        <div class="success">Configuration saved!</div>
                        <div class="info">⚠️ Restart memo-rf manually for changes to take effect</div>`;
                }
            } else {
                msgContainer.innerHTML = `
                    <div class="success">Configuration saved!</div>
                    <div class="info">⚠️ Restart memo-rf manually for changes to take effect</div>`;
            }
        }
    } catch (error) {
        msgContainer.innerHTML = `<div class="error">Failed to save: ${escapeHtml(error.message)}</div>`;
    }
}

function refreshFeed() { loadFeed(); }
function startAutoRefresh() {
    if (autoRefreshInterval) clearInterval(autoRefreshInterval);
    autoRefreshInterval = setInterval(loadFeed, 2000);  // Poll every 2 seconds for real-time updates
}
function stopAutoRefresh() {
    if (autoRefreshInterval) { clearInterval(autoRefreshInterval); autoRefreshInterval = null; }
}
function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

if (currentTab === 'feed') { loadFeed(); startAutoRefresh(); }
"""

# ============================================================================
# HTML Template
# ============================================================================

HTML_TEMPLATE = """<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Memo-RF Dashboard</title>
    <style>{css}</style>
</head>
<body>
    <div class="container">
        <header>
            <h1>MEMO-RF</h1>
            <div class="subtitle">Radio Agent Dashboard</div>
        </header>

        <div class="tabs">
            <button class="tab active" onclick="switchTab('feed')">Feed</button>
            <button class="tab" onclick="switchTab('config')">Configuration</button>
        </div>

        <div id="feed-tab" class="tab-content active">
            <div class="controls">
                <div class="status">
                    <div class="status-indicator" id="status-indicator"></div>
                    <span id="status-text">Connecting...</span>
                </div>
                <button onclick="refreshFeed()">Refresh</button>
            </div>
            <div id="error-container"></div>
            <div class="feed" id="feed"></div>
        </div>

        <div id="config-tab" class="tab-content">
            <div id="config-message-container"></div>
            <div class="config-form">
                <div class="form-section">
                    <h2 class="section-title">Agent Configuration</h2>
                    <div class="form-group">
                        <label for="agent-persona">Persona</label>
                        <select id="agent-persona"><option>Loading...</option></select>
                    </div>
                    <div class="form-group">
                        <label for="response-language">Response Language</label>
                        <select id="response-language">
                            <option value="en">English (en)</option>
                            <option value="es">Spanish (es)</option>
                            <option value="fr">French (fr)</option>
                            <option value="de">German (de)</option>
                        </select>
                    </div>
                    <div class="form-group">
                        <label for="model-name">LLM Model</label>
                        <input type="text" id="model-name" placeholder="e.g., mistral:instruct">
                    </div>
                    <div class="form-group">
                        <label for="temperature">Temperature (0.0 - 1.0)</label>
                        <input type="number" id="temperature" min="0" max="1" step="0.1">
                    </div>
                    <div class="form-group checkbox-group">
                        <input type="checkbox" id="wake-word-enabled">
                        <label for="wake-word-enabled">Wake Word Enabled</label>
                    </div>
                </div>
                <div class="form-actions">
                    <button class="btn-primary" onclick="saveConfig()">Save Configuration</button>
                    <button onclick="loadConfig()">Reload</button>
                </div>
            </div>
        </div>
    </div>
    <script>{javascript}</script>
</body>
</html>
"""

# ============================================================================
# HTTP Handler
# ============================================================================

class FeedHandler(BaseHTTPRequestHandler):
    """HTTP request handler for the feed server."""

    sessions_dir = None
    config_path = None
    config_dir = None
    agent_binary = None
    agent_enabled = False

    def do_GET(self):
        """Handle GET requests."""
        routes = {
            '/': self.serve_html,
            '/api/feed': self.serve_feed,
            '/api/config': self.serve_config,
            '/api/personas': self.serve_personas,
            '/api/languages': self.serve_languages,
            '/api/agent/status': self.agent_status,
        }
        handler = routes.get(urlparse(self.path).path)
        if handler:
            handler()
        else:
            self.send_error(404)

    def do_POST(self):
        """Handle POST requests."""
        path = urlparse(self.path).path
        if path == '/api/config':
            self.update_config()
        elif path == '/api/agent/restart':
            self.agent_restart()
        else:
            self.send_error(404)

    def serve_html(self):
        """Serve the HTML UI."""
        html = HTML_TEMPLATE.format(css=CSS, javascript=JAVASCRIPT)
        self.send_response(200)
        self.send_header('Content-type', 'text/html; charset=utf-8')
        self.end_headers()
        self.wfile.write(html.encode('utf-8'))

    def serve_feed(self):
        """Serve feed data."""
        self.json_response(200, {
            'exchanges': self.get_exchanges(),
            'total': len(self.get_exchanges())
        })

    def serve_config(self):
        """Serve current configuration."""
        if not self.config_path or not Path(self.config_path).exists():
            return self.json_response(404, {'error': 'Config not found'})

        try:
            with open(self.config_path, 'r') as f:
                self.json_response(200, json.load(f))
        except Exception as e:
            self.json_response(500, {'error': str(e)})

    def serve_personas(self):
        """Serve available personas."""
        personas_path = Path(self.config_dir) / 'personas.json'
        if not personas_path.exists():
            return self.json_response(404, {'error': 'Personas not found'})

        try:
            with open(personas_path, 'r') as f:
                data = json.load(f)
            personas = [
                {'id': k, 'name': v.get('name', k), 'system_prompt': v.get('system_prompt', '')}
                for k, v in data.items()
            ]
            self.json_response(200, {'personas': personas})
        except Exception as e:
            self.json_response(500, {'error': str(e)})

    def serve_languages(self):
        """Serve available languages."""
        languages_path = Path(self.config_dir) / 'language_voices.json'
        if not languages_path.exists():
            return self.json_response(404, {'error': 'Languages not found'})

        try:
            with open(languages_path, 'r') as f:
                data = json.load(f)
            languages = [{'code': k, 'voice': v} for k, v in data.items()]
            self.json_response(200, {'languages': languages})
        except Exception as e:
            self.json_response(500, {'error': str(e)})

    def update_config(self):
        """Update configuration file."""
        if not self.config_path or not Path(self.config_path).exists():
            return self.json_response(404, {'error': 'Config not found'})

        try:
            content_length = int(self.headers.get('Content-Length', 0))
            if content_length == 0:
                return self.json_response(400, {'error': 'Empty request'})

            body = self.rfile.read(content_length)
            updates = json.loads(body.decode('utf-8'))

            # Read current config
            with open(self.config_path, 'r') as f:
                config = json.load(f)

            # Deep merge updates
            def merge(base, updates):
                for key, value in updates.items():
                    if isinstance(value, dict) and key in base and isinstance(base[key], dict):
                        merge(base[key], value)
                    else:
                        base[key] = value

            merge(config, updates)

            # Backup and save
            backup_path = self.config_path + '.backup'
            shutil.copy2(self.config_path, backup_path)

            with open(self.config_path, 'w') as f:
                json.dump(config, f, indent=2)
                f.write('\n')

            self.json_response(200, {
                'success': True,
                'message': 'Configuration updated. Restart memo-rf for changes to take effect.',
                'backup': backup_path
            })

        except json.JSONDecodeError as e:
            self.json_response(400, {'error': f'Invalid JSON: {str(e)}'})
        except Exception as e:
            self.json_response(500, {'error': str(e)})

    def get_exchanges(self):
        """Extract exchanges from session logs."""
        exchanges = []
        sessions_path = Path(self.sessions_dir)

        if not sessions_path.exists():
            return exchanges

        for session_dir in sorted(sessions_path.iterdir(), key=lambda d: d.name, reverse=True):
            if not session_dir.is_dir():
                continue

            log_path = session_dir / 'session_log.json'
            if not log_path.exists():
                continue

            try:
                with open(log_path, 'r') as f:
                    data = json.load(f)

                session_id = data.get('session_id', session_dir.name)
                events = data.get('events', [])
                metadata = data.get('metadata', {})

                # Parse session start time from session_id (format: YYYYMMDD_HHMMSS)
                session_start_ms = 0
                try:
                    from datetime import datetime
                    dt = datetime.strptime(session_id, '%Y%m%d_%H%M%S')
                    session_start_ms = int(dt.timestamp() * 1000)
                except:
                    pass

                seen_exchanges = set()  # Deduplicate by (session_id, timestamp_ms, transcript)
                i = 0
                while i < len(events):
                    event = events[i]
                    # Only process 'transcript' events to avoid duplicates from llm_prompt
                    if event['event_type'] == 'transcript':
                        transcript = event['data'].strip()
                        relative_timestamp_ms = event['timestamp_ms']
                        absolute_timestamp_ms = session_start_ms + relative_timestamp_ms

                        # Deduplication key - use absolute timestamp and full transcript
                        exchange_key = (session_id, absolute_timestamp_ms, transcript)
                        if exchange_key in seen_exchanges:
                            i += 1
                            continue
                        seen_exchanges.add(exchange_key)

                        # Look ahead for response (find the next llm_response event)
                        response = None
                        for j in range(i + 1, len(events)):
                            if events[j]['event_type'] == 'llm_response':
                                response = events[j]['data'].strip()
                                break
                            # Stop if we hit another transcript (new exchange)
                            if events[j]['event_type'] == 'transcript':
                                break

                        exchanges.append({
                            'session_id': session_id,
                            'timestamp_ms': absolute_timestamp_ms,
                            'transcript': transcript,
                            'response': response,
                            'persona': metadata.get('persona', ''),
                            'persona_name': metadata.get('persona_name', ''),
                            'language': metadata.get('response_language', 'en')
                        })
                    i += 1

            except Exception as e:
                print(f"Warning: Failed to parse {log_path}: {e}", file=sys.stderr)
                continue

        # Sort by timestamp descending (newest first) - this is how feeds work
        exchanges.sort(key=lambda x: x['timestamp_ms'], reverse=True)
        return exchanges

    def agent_status(self):
        """Get agent status."""
        if not self.agent_enabled:
            return self.json_response(200, {'enabled': False, 'message': 'Agent management disabled'})

        try:
            # Check if memo-rf is running
            result = subprocess.run(['pgrep', '-f', 'memo-rf'],
                                  capture_output=True, text=True)
            is_running = result.returncode == 0
            pid = result.stdout.strip() if is_running else None

            self.json_response(200, {
                'enabled': True,
                'running': is_running,
                'pid': pid
            })
        except Exception as e:
            self.json_response(500, {'error': str(e)})

    def agent_restart(self):
        """Restart the agent."""
        if not self.agent_enabled or not self.agent_binary:
            return self.json_response(400, {'error': 'Agent management not enabled'})

        try:
            # Stop existing agent
            result = subprocess.run(['pgrep', '-f', 'build/memo-rf'],
                                  capture_output=True, text=True)
            if result.returncode == 0:
                pids = result.stdout.strip().split('\n')
                for pid in pids:
                    try:
                        os.kill(int(pid), signal.SIGTERM)
                    except:
                        pass
                time.sleep(1)

            # Start new agent in background
            subprocess.Popen(
                [self.agent_binary, self.config_path],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                start_new_session=True
            )

            time.sleep(0.5)
            self.json_response(200, {
                'success': True,
                'message': 'Agent restarted successfully'
            })
        except Exception as e:
            self.json_response(500, {'error': f'Failed to restart agent: {str(e)}'})

    def json_response(self, status, data):
        """Send JSON response."""
        self.send_response(status)
        self.send_header('Content-type', 'application/json; charset=utf-8')
        self.end_headers()
        self.wfile.write(json.dumps(data, indent=2).encode('utf-8'))

    def log_message(self, format, *args):
        """Custom log format."""
        sys.stderr.write(f"[{self.log_date_time_string()}] {format % args}\n")


# ============================================================================
# Main
# ============================================================================

def main():
    """Run the feed server."""
    parser = argparse.ArgumentParser(description='Memo-RF Feed Server')
    parser.add_argument('--port', type=int, default=5050, help='Port (default: 5050)')
    parser.add_argument('--host', default='0.0.0.0', help='Host (default: 0.0.0.0)')
    parser.add_argument('--sessions-dir', default=None, help='Sessions directory')
    parser.add_argument('--config-path', default=None, help='Path to config.json')
    parser.add_argument('--agent-binary', default=None, help='Path to memo-rf binary for auto-restart')
    parser.add_argument('--enable-agent-management', action='store_true', help='Enable agent restart from UI')

    args = parser.parse_args()

    # Resolve paths
    sessions_dir = os.path.abspath(args.sessions_dir or os.environ.get('MEMO_RF_SESSION_DIR', 'sessions'))
    config_path = os.path.abspath(args.config_path or os.path.join(os.getcwd(), 'config', 'config.json'))
    config_dir = os.path.dirname(config_path)

    # Resolve agent binary
    agent_binary = None
    if args.agent_binary:
        agent_binary = os.path.abspath(args.agent_binary)
    elif args.enable_agent_management:
        # Try to find build/memo-rf relative to config
        project_root = os.path.dirname(config_dir)
        candidate = os.path.join(project_root, 'build', 'memo-rf')
        if os.path.exists(candidate):
            agent_binary = candidate

    # Set handler paths
    FeedHandler.sessions_dir = sessions_dir
    FeedHandler.config_path = config_path
    FeedHandler.config_dir = config_dir
    FeedHandler.agent_binary = agent_binary
    FeedHandler.agent_enabled = args.enable_agent_management and agent_binary is not None

    # Start server
    httpd = HTTPServer((args.host, args.port), FeedHandler)

    print(f"Memo-RF Feed Server running on http://{args.host}:{args.port}")
    print(f"Sessions: {sessions_dir}")
    print(f"Config: {config_path}")
    if FeedHandler.agent_enabled:
        print(f"Agent Management: ENABLED (binary: {agent_binary})")
    else:
        print(f"Agent Management: DISABLED")
    print(f"Local: http://localhost:{args.port}")

    try:
        import socket
        local_ip = socket.gethostbyname(socket.gethostname())
        if local_ip and not local_ip.startswith('127.'):
            print(f"Network: http://{local_ip}:{args.port}")
    except:
        pass

    print("\nPress Ctrl+C to stop")

    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down...")
        httpd.shutdown()


if __name__ == '__main__':
    main()
