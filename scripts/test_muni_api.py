#!/usr/bin/env python3
"""Mock Muni rover API server for testing the plugin system."""

from http.server import HTTPServer, BaseHTTPRequestHandler
import json
import sys

class MuniAPIHandler(BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        """Custom logging to make output clearer."""
        sys.stdout.write(f"[API] {format % args}\n")

    def do_POST(self):
        """Handle POST requests to /rovers/{rover_id}/command"""
        # Parse path
        if not self.path.startswith('/rovers/'):
            self.send_error(404, "Not Found")
            return

        # Extract rover_id from path
        parts = self.path.split('/')
        if len(parts) < 4 or parts[3] != 'command':
            self.send_error(404, "Invalid endpoint")
            return

        rover_id = parts[2]

        # Read request body
        content_length = int(self.headers.get('Content-Length', 0))
        body = self.rfile.read(content_length).decode('utf-8')

        try:
            command = json.loads(body)
        except json.JSONDecodeError:
            self.send_error(400, "Invalid JSON")
            return

        # Log the command
        print(f"\n{'='*60}")
        print(f"Rover: {rover_id}")
        print(f"Command: {json.dumps(command, indent=2)}")
        print(f"{'='*60}\n")

        # Check for API key
        api_key = self.headers.get('X-API-Key')
        if api_key:
            print(f"[API] Received API Key: {api_key}")

        # Simulate command processing
        cmd_type = command.get('type', 'unknown')

        response = {
            "status": "success",
            "rover_id": rover_id,
            "command": cmd_type,
            "message": f"Command {cmd_type} executed successfully"
        }

        # Add command-specific responses
        if cmd_type == "estop":
            response["message"] = "Emergency stop activated"
        elif cmd_type == "estop_release":
            response["message"] = "E-stop released, rover idle"
        elif cmd_type == "set_goal":
            x = command.get('x', 0)
            y = command.get('y', 0)
            response["message"] = f"Navigating to ({x}, {y})"
            response["goal"] = {"x": x, "y": y}
        elif cmd_type == "set_mode":
            mode = command.get('mode', 'unknown')
            response["message"] = f"Mode changed to {mode}"
            response["mode"] = mode

        # Send response
        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.end_headers()
        self.wfile.write(json.dumps(response).encode('utf-8'))

    def do_GET(self):
        """Handle GET requests for health checks."""
        if self.path == '/health':
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps({"status": "ok", "service": "muni-mock"}).encode('utf-8'))
        else:
            self.send_error(404, "Not Found")

def run_server(port=4890):
    """Run the mock API server."""
    server_address = ('', port)
    httpd = HTTPServer(server_address, MuniAPIHandler)
    print(f"\n{'='*60}")
    print(f"Mock Muni API Server running on http://localhost:{port}")
    print(f"Ready to receive commands!")
    print(f"{'='*60}\n")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down server...")
        httpd.shutdown()

if __name__ == '__main__':
    port = 4890
    if len(sys.argv) > 1:
        port = int(sys.argv[1])
    run_server(port)
