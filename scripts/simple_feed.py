#!/usr/bin/env python3
"""
DEAD SIMPLE Feed Server
API-only: receives POSTs, serves dashboard static files. No embedded HTML.
"""

from http.server import HTTPServer, BaseHTTPRequestHandler
import json
import csv
import mimetypes
from pathlib import Path

# In-memory storage
events = []
MAX_EVENTS = 100

# Paths (repo root = parent of scripts/)
SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
CONFIG_DIR = REPO_ROOT / 'config'
CONFIG_PATH = CONFIG_DIR / 'config.json'
PERSONAS_PATH = CONFIG_DIR / 'personas.json'
ACTIVE_PATH = CONFIG_DIR / 'active.json'
ROBOTS_DIR = CONFIG_DIR / 'robots'
AGENTS_DIR = CONFIG_DIR / 'agents'
DASHBOARD_DIR = REPO_ROOT / 'dashboard_dist'
CSV_PATH = REPO_ROOT / 'hotel_14day.csv'

# department_from -> channel (align with dashboard labels)
DEPT_TO_CHANNEL = {
    'front_desk': 1,
    'housekeeping': 2,
    'maintenance': 3,
    'security': 4,
    'food_beverage': 5,
    'management': 6,
    'concierge': 7,
}


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


def load_simulated_feed():
    """Read hotel_14day.csv and return list of { channel, timestamp, transmission_type, person_from, person_to, message, location, priority, ... }."""
    if not CSV_PATH.is_file():
        return []
    out = []
    with open(CSV_PATH, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for row in reader:
            dept = (row.get('department_from') or '').strip()
            ch = DEPT_TO_CHANNEL.get(dept)
            if ch is None:
                continue
            ts = (row.get('timestamp') or '').strip()
            out.append({
                'channel': ch,
                'timestamp': ts,
                'transmission_type': (row.get('transmission_type') or '').strip(),
                'person_from': (row.get('person_from') or '').strip(),
                'person_to': (row.get('person_to') or '').strip(),
                'message': (row.get('message') or '').strip(),
                'location': (row.get('location') or '').strip(),
                'priority': (row.get('priority') or '').strip(),
                'request_category': (row.get('request_category') or '').strip(),
                'task_status': (row.get('task_status') or '').strip(),
                'role_from': (row.get('role_from') or '').strip(),
                'role_to': (row.get('role_to') or '').strip(),
            })
    return out


class SimpleFeedHandler(BaseHTTPRequestHandler):

    def send_json(self, data, status=200):
        """Helper to send JSON response"""
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.end_headers()
        self.wfile.write(json.dumps(data).encode())

    def send_file(self, filepath):
        """Serve a file with appropriate Content-Type."""
        try:
            with open(filepath, 'rb') as f:
                data = f.read()
        except OSError:
            self.send_response(404)
            self.end_headers()
            return
        content_type, _ = mimetypes.guess_type(str(filepath))
        content_type = content_type or 'application/octet-stream'
        self.send_response(200)
        self.send_header('Content-Type', content_type)
        self.send_header('Content-Length', len(data))
        self.end_headers()
        self.wfile.write(data)

    def do_POST(self):
        """Handle POST requests"""
        if self.path == '/api/config':
            try:
                length = int(self.headers.get('Content-Length', 0))
                body = self.rfile.read(length)
                updates = json.loads(body.decode('utf-8'))

                # Load language_voices mapping
                language_voices = {}
                if (CONFIG_DIR / 'language_voices.json').exists():
                    try:
                        with open(CONFIG_DIR / 'language_voices.json', 'r') as f:
                            language_voices = json.load(f)
                    except Exception:
                        pass

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

                        # Also update main config voice path when language changes
                        if CONFIG_PATH.exists() and updates['language'] in language_voices:
                            try:
                                with open(CONFIG_PATH, 'r') as f:
                                    config = json.load(f)
                                voice_filename = language_voices[updates['language']]
                                voice_models_dir = config.get('tts', {}).get('voice_models_dir', '/home/oliver/models/piper')
                                if voice_models_dir:
                                    config['tts']['voice_path'] = str(Path(voice_models_dir) / voice_filename)
                                else:
                                    config['tts']['voice_path'] = f"/home/oliver/models/piper/{voice_filename}"
                                config['llm']['response_language'] = updates['language']
                                with open(CONFIG_PATH, 'w') as f:
                                    json.dump(config, f, indent=2)
                                print(f"Updated voice path to: {config['tts']['voice_path']}")
                            except Exception as e:
                                print(f"Warning: Failed to update voice path: {e}")

                    if 'input_language' in updates:
                        if CONFIG_PATH.exists():
                            try:
                                with open(CONFIG_PATH, 'r') as f:
                                    config = json.load(f)
                                config['stt']['language'] = updates['input_language']
                                with open(CONFIG_PATH, 'w') as f:
                                    json.dump(config, f, indent=2)
                                print(f"Updated STT language to: {updates['input_language']}")
                            except Exception as e:
                                print(f"Warning: Failed to update STT language: {e}")

                    with open(ACTIVE_PATH, 'w') as f:
                        json.dump(active_data, f, indent=2)
                    print(f"Active updated: {active_data.get('active', '')}, language: {active_data.get('response_language', '')}")
                    self.send_json({'status': 'ok', 'message': 'Language updated. Restart the memo-rf agent (e.g. ./run.sh or systemctl restart memo-rf) to apply changes.'})
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

                        if updates['language'] in language_voices:
                            voice_filename = language_voices[updates['language']]
                            voice_models_dir = config.get('tts', {}).get('voice_models_dir', '/home/oliver/models/piper')
                            if voice_models_dir:
                                config['tts']['voice_path'] = str(Path(voice_models_dir) / voice_filename)
                            else:
                                config['tts']['voice_path'] = f"/home/oliver/models/piper/{voice_filename}"
                            print(f"Updated voice path to: {config['tts']['voice_path']}")

                    if 'input_language' in updates:
                        config['stt']['language'] = updates['input_language']
                        print(f"Updated STT language to: {updates['input_language']}")

                    with open(CONFIG_PATH, 'w') as f:
                        json.dump(config, f, indent=2)
                    print(f"Config updated: persona={updates.get('persona', 'unchanged')}, language={updates.get('language', 'unchanged')}, input_language={updates.get('input_language', 'unchanged')}")
                    self.send_json({'status': 'ok', 'message': 'Config updated. Restart agent to apply.'})
            except Exception as e:
                self.send_json({'error': str(e)}, 500)

        elif self.path == '/api/feed/notify':
            try:
                length = int(self.headers.get('Content-Length', 0))
                body = self.rfile.read(length)
                data = json.loads(body.decode('utf-8'))

                print(f"[{data.get('event_type')}] Session: {data.get('session_id')} | {data.get('data', '')[:50]}...")

                events.insert(0, data)
                if len(events) > MAX_EVENTS:
                    events.pop()

                print(f"Total events in memory: {len(events)}")

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
        path = self.path.split('?')[0]

        if path == '/' or path == '/index.html':
            # Serve dashboard SPA entry
            index_path = DASHBOARD_DIR / 'index.html'
            if index_path.is_file():
                self.send_file(index_path)
            else:
                self.send_response(404)
                self.send_header('Content-Type', 'text/plain')
                self.end_headers()
                self.wfile.write(b'Dashboard not built. Run: cd frontend && npm run build')
            return

        if path.startswith('/api/'):
            # API routes
            if path == '/api/robots':
                try:
                    robots = scan_identity_dir(ROBOTS_DIR)
                    self.send_json({'robots': robots})
                except Exception as e:
                    self.send_json({'error': str(e)}, 500)
                return
            if path == '/api/agents':
                try:
                    agents = scan_identity_dir(AGENTS_DIR)
                    self.send_json({'agents': agents})
                except Exception as e:
                    self.send_json({'error': str(e)}, 500)
                return
            if path == '/api/personas':
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
                return
            if path == '/api/config':
                try:
                    input_lang = ''
                    if CONFIG_PATH.exists():
                        with open(CONFIG_PATH, 'r') as f:
                            config_data = json.load(f)
                        input_lang = config_data.get('stt', {}).get('language', '')

                    if ACTIVE_PATH.exists():
                        with open(ACTIVE_PATH, 'r') as f:
                            active_data = json.load(f)
                        self.send_json({
                            'active': active_data.get('active', ''),
                            'language': active_data.get('response_language', ''),
                            'response_language': active_data.get('response_language', ''),
                            'input_language': input_lang,
                        })
                    else:
                        with open(CONFIG_PATH, 'r') as f:
                            config = json.load(f)
                        self.send_json({
                            'persona': config.get('llm', {}).get('agent_persona', ''),
                            'language': config.get('llm', {}).get('response_language', ''),
                            'input_language': input_lang,
                        })
                except Exception as e:
                    self.send_json({'error': str(e)}, 500)
                return
            if path == '/api/feed':
                exchanges = []
                for event in reversed(events):
                    event_type = event.get('event_type')

                    if event_type == 'transcript':
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
                        for ex in reversed(exchanges):
                            if not ex['response']:
                                ex['response'] = event.get('data', '')
                                break

                exchanges.reverse()
                self.send_json({'exchanges': exchanges})
                return
            if path == '/api/simulated/feed':
                try:
                    items = load_simulated_feed()
                    self.send_json({'items': items})
                except Exception as e:
                    self.send_json({'error': str(e)}, 500)
                return

            self.send_response(404)
            self.end_headers()
            return

        # Static file: resolve under DASHBOARD_DIR
        if not DASHBOARD_DIR.is_dir():
            self.send_response(404)
            self.end_headers()
            return
        # Strip leading slash and resolve (no path traversal)
        rel = path.lstrip('/')
        if '..' in rel or rel.startswith('/'):
            self.send_response(404)
            self.end_headers()
            return
        filepath = (DASHBOARD_DIR / rel).resolve()
        if not str(filepath).startswith(str(DASHBOARD_DIR.resolve())):
            self.send_response(404)
            self.end_headers()
            return
        if filepath.is_file():
            self.send_file(filepath)
        else:
            # SPA fallback: serve index.html for client-side routes
            index_path = DASHBOARD_DIR / 'index.html'
            if index_path.is_file():
                self.send_file(index_path)
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
    print(f"Dashboard:  http://localhost:{port}")
    print(f"API:       http://localhost:{port}/api/feed/notify")
    if not DASHBOARD_DIR.is_dir():
        print(f"Note:      Build dashboard first: cd frontend && npm run build")
    print(f"{'='*60}\n")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down...")
        server.shutdown()
