#!/bin/bash
# Start the mock Muni API server and memo-rf for testing

echo "Starting Mock Muni API Server on port 4890..."
python3 test_muni_api.py &
MOCK_PID=$!

# Wait for server to start
sleep 1

# Check if server is running
if curl -s http://localhost:4890/health > /dev/null 2>&1; then
    echo "✓ Mock API server running (PID: $MOCK_PID)"
    echo ""
    echo "=============================================="
    echo "Ready to test! Run memo-rf in another terminal:"
    echo "  cd build && ./memo-rf"
    echo ""
    echo "Try these voice commands:"
    echo "  - 'stop the robot'"
    echo "  - 'go to position five three'"
    echo "  - 'set mode to autonomous'"
    echo "  - 'release'"
    echo ""
    echo "Press Ctrl+C to stop the mock server"
    echo "=============================================="
    echo ""

    # Wait for user interrupt
    trap "echo 'Stopping mock server...'; kill $MOCK_PID 2>/dev/null; exit 0" INT TERM
    wait $MOCK_PID
else
    echo "✗ Failed to start mock API server"
    kill $MOCK_PID 2>/dev/null
    exit 1
fi
