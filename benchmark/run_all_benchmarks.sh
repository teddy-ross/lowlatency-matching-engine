#!/bin/bash
# run_all_benchmarks.sh - Run all benchmarks and generate report

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
RESULTS_DIR="$SCRIPT_DIR/results"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "========================================"
echo "Running Complete Benchmark Suite"
echo "========================================"
echo "Timestamp: $TIMESTAMP"
echo "Build dir: $BUILD_DIR"
echo ""

# Check if built
if [ ! -f "$BUILD_DIR/benchmark/benchmark_suite" ]; then
    echo -e "${RED}Error: Benchmarks not built${NC}"
    echo "Please run:"
    echo "  cd $PROJECT_ROOT"
    echo "  mkdir -p build && cd build"
    echo "  cmake -DBUILD_BENCHMARKS=ON .."
    echo "  cmake --build ."
    exit 1
fi

# Create results directory
mkdir -p "$RESULTS_DIR"

cd "$BUILD_DIR/benchmark"

echo "========================================"
echo "1. Running Unit Benchmarks"
echo "========================================"
echo ""

./benchmark_suite | tee "$RESULTS_DIR/unit_${TIMESTAMP}.txt"
UNIT_RESULT=${PIPESTATUS[0]}

echo ""
echo "========================================"
echo "2. Running Stress Tests"
echo "========================================"
echo ""

./stress_test | tee "$RESULTS_DIR/stress_${TIMESTAMP}.txt"
STRESS_RESULT=${PIPESTATUS[0]}

echo ""
echo "========================================"
echo "3. Integration Tests"
echo "========================================"

# Check if server is already running
if lsof -Pi :6767 -sTCP:LISTEN -t >/dev/null 2>&1; then
    echo -e "${YELLOW}Warning: Server already running on port 6767${NC}"
    echo "Using existing server for integration tests..."
    SERVER_PID=""
else
    echo "Starting server for integration tests..."
    "$BUILD_DIR/marketDataHandlerLL" &
    SERVER_PID=$!
    sleep 2
    
    if ! kill -0 $SERVER_PID 2>/dev/null; then
        echo -e "${RED}Error: Failed to start server${NC}"
        INTEGRATION_RESULT=1
    else
        echo -e "${GREEN}Server started (PID: $SERVER_PID)${NC}"
    fi
fi

if [ -z "$INTEGRATION_RESULT" ]; then
    echo ""
    echo "Running integration test..."
    cd "$SCRIPT_DIR/integration"
    python3 generator.py 50000 | tee "$RESULTS_DIR/integration_${TIMESTAMP}.txt"
    INTEGRATION_RESULT=${PIPESTATUS[0]}
    
    # Stop server if we started it
    if [ -n "$SERVER_PID" ]; then
        echo "Stopping server..."
        kill $SERVER_PID 2>/dev/null || true
        wait $SERVER_PID 2>/dev/null || true
    fi
fi

# Generate summary
echo ""
echo "========================================"
echo "Benchmark Summary"
echo "========================================"
echo ""

if [ $UNIT_RESULT -eq 0 ]; then
    echo -e "${GREEN}✓${NC} Unit benchmarks: PASSED"
else
    echo -e "${RED}✗${NC} Unit benchmarks: FAILED"
fi

if [ $STRESS_RESULT -eq 0 ]; then
    echo -e "${GREEN}✓${NC} Stress tests: PASSED"
else
    echo -e "${RED}✗${NC} Stress tests: FAILED"
fi

if [ $INTEGRATION_RESULT -eq 0 ]; then
    echo -e "${GREEN}✓${NC} Integration tests: PASSED"
else
    echo -e "${RED}✗${NC} Integration tests: FAILED"
fi

echo ""
echo "Results saved to: $RESULTS_DIR"
echo "  - unit_${TIMESTAMP}.txt"
echo "  - stress_${TIMESTAMP}.txt"
echo "  - integration_${TIMESTAMP}.txt"

# Compare with baseline if it exists
BASELINE="$RESULTS_DIR/baseline_unit.txt"
if [ -f "$BASELINE" ]; then
    echo ""
    echo "========================================"
    echo "Comparing with Baseline"
    echo "========================================"
    python3 "$SCRIPT_DIR/tools/results_analyzer.py" \
        "$BASELINE" \
        "$RESULTS_DIR/unit_${TIMESTAMP}.txt"
else
    echo ""
    echo "No baseline found. To create one, run:"
    echo "  cp $RESULTS_DIR/unit_${TIMESTAMP}.txt $BASELINE"
fi

echo ""
echo "========================================"

# Exit with error if any test failed
if [ $UNIT_RESULT -ne 0 ] || [ $STRESS_RESULT -ne 0 ] || [ $INTEGRATION_RESULT -ne 0 ]; then
    echo -e "${RED}Some benchmarks failed${NC}"
    exit 1
else
    echo -e "${GREEN}All benchmarks passed${NC}"
    exit 0
fi
