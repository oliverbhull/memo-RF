#!/bin/bash
# Run memo-rf with default config and optional feed server
# Usage: ./run.sh [optional-config-path]
# Environment variables:
#   MEMO_RF_NO_FEED_SERVER=1  - Disable the feed server
#   MEMO_RF_FEED_PORT=5050    - Feed server port (default: 5050)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

if [ ! -f "build/memo-rf" ]; then
    echo "Error: build/memo-rf not found. Run ./build.sh first."
    exit 1
fi

# Kill any existing processes
echo "Checking for existing processes..."
KILLED=0
if pkill -9 -f "build/memo-rf" 2>/dev/null; then
    echo "  ✓ Killed existing memo-rf agent"
    KILLED=1
fi
if pkill -9 -f "feed_server.py" 2>/dev/null; then
    echo "  ✓ Killed existing feed server"
    KILLED=1
fi
if [ $KILLED -eq 0 ]; then
    echo "  No existing processes found"
fi
sleep 0.5

# Feed server configuration - DISABLED BY DEFAULT
FEED_SERVER_ENABLED=${MEMO_RF_FEED_SERVER:-0}
FEED_PORT=${MEMO_RF_FEED_PORT:-5050}
FEED_PID=""

# Cleanup function to stop feed server on exit
cleanup() {
    if [ -n "$FEED_PID" ] && kill -0 "$FEED_PID" 2>/dev/null; then
        echo "Stopping feed server (PID $FEED_PID)..."
        kill "$FEED_PID" 2>/dev/null
        wait "$FEED_PID" 2>/dev/null
    fi
}

# Set trap to cleanup on exit
trap cleanup EXIT INT TERM

# Start feed server if enabled
if [ "$FEED_SERVER_ENABLED" = "1" ]; then
    if [ -f "scripts/feed_server.py" ]; then
        echo "Starting feed server on port $FEED_PORT..."
        python3 scripts/feed_server.py --port "$FEED_PORT" --enable-agent-management > /dev/null 2>&1 &
        FEED_PID=$!

        # Give it a moment to start
        sleep 0.5

        # Check if it started successfully
        if kill -0 "$FEED_PID" 2>/dev/null; then
            echo "Feed server running at http://localhost:$FEED_PORT (PID $FEED_PID)"

            # Try to show LAN IP for remote access
            if command -v hostname &> /dev/null; then
                LOCAL_IP=$(hostname -I 2>/dev/null | awk '{print $1}')
                if [ -n "$LOCAL_IP" ] && [ "$LOCAL_IP" != "127.0.0.1" ]; then
                    echo "Access from network: http://$LOCAL_IP:$FEED_PORT"
                fi
            fi
        else
            echo "Warning: Feed server failed to start"
            FEED_PID=""
        fi
    else
        echo "Warning: scripts/feed_server.py not found, skipping feed server"
    fi
else
    echo "Feed server disabled (set MEMO_RF_FEED_SERVER=1 to enable)"
fi

echo "Starting memo-rf..."
echo ""

# Run memo-rf (not using exec so cleanup trap works)
if [ $# -eq 0 ]; then
    ./build/memo-rf config/config.json
else
    ./build/memo-rf "$@"
fi
