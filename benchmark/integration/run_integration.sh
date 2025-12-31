#!/bin/bash
# run_integration.sh - Run end-to-end integration tests

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
SERVER_BINARY="$BUILD_DIR/marketDataHandlerLL"

# Default values
NUM_ORDERS=100000
PORT=6767
WAIT_TIME=2

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -n|--num-orders)
            NUM_ORDERS="$2"
            shift 2
            ;;
        -p|--port)
            PORT="$2"
            shift 2
            ;;
        --help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  -n, --num-orders NUM   Number of orders to send (default: 100000)"
            echo "  -p, --port PORT        Server port (default: 6767)"
            echo "  --help                 Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Check if server binary exists
if [ ! -f "$SERVER_BINARY" ]; then
    echo "Error: Server binary not found at $SERVER_BINARY"
    echo "Please build the project first:"
    echo "  cd $PROJECT_ROOT"
    echo "  mkdir -p build && cd build"
    echo "  cmake .."
    echo "  cmake --build ."
    exit 1
fi

# Check if generator.py exists
if [ ! -f "$SCRIPT_DIR/generator.py" ]; then
    echo "Error: generator.py not found at $SCRIPT_DIR/generator.py"
    exit 1
fi

echo "========================================"
echo "Integration Test Configuration"
echo "========================================"
echo "Server binary: $SERVER_BINARY"
echo "Port:          $PORT"
echo "Orders:        $NUM_ORDERS"
echo "========================================"
echo ""

# Check if server is already running
if lsof -Pi :$PORT -sTCP:LISTEN -t >/dev/null 2>&1; then
    echo "Warning: Port $PORT is already in use"
    echo "Using existing server..."
    SERVER_PID=""
else
    # Start the server in background
    echo "Starting server..."
    "$SERVER_BINARY" &
    SERVER_PID=$!
    
    # Wait for server to start
    echo "Waiting for server to start..."
    sleep $WAIT_TIME
    
    # Check if server is running
    if ! kill -0 $SERVER_PID 2>/dev/null; then
        echo "Error: Server failed to start"
        exit 1
    fi
    
    echo "Server started (PID: $SERVER_PID)"
fi

echo ""
echo "Running integration test with $NUM_ORDERS orders..."
echo ""

# Run the generator
cd "$SCRIPT_DIR"
python3 generator.py $NUM_ORDERS
TEST_RESULT=$?

echo ""
echo "========================================"
if [ $TEST_RESULT -eq 0 ]; then
    echo "Integration test PASSED"
else
    echo "Integration test FAILED"
fi
echo "========================================"

# Stop the server if we started it
if [ -n "$SERVER_PID" ]; then
    echo ""
    echo "Stopping server (PID: $SERVER_PID)..."
    kill $SERVER_PID 2>/dev/null || true
    wait $SERVER_PID 2>/dev/null || true
    echo "Server stopped"
fi

exit $TEST_RESULT
