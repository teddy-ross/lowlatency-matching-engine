# Order Book Benchmark Suite

This directory contains comprehensive performance benchmarks and tests for the matching engine.

## Directory Structure

```
benchmark/
├── CMakeLists.txt              # Build configuration
├── README.md                   # This file
│
├── unit/                       # Unit benchmarks (direct MatchingEngine)
│   ├── benchmark_suite.cpp     # Latency/throughput benchmarks
│   └── stress_test.cpp         # Memory and stress tests
│
├── integration/                # Integration tests (full system)
│   ├── generator.py            # Network RTT test client
│   └── run_integration.sh      # Automated test runner
│
└── tools/                      # Analysis tools
    └── results_analyzer.py     # Result parsing and comparison
```

## Two Types of Benchmarks

### 1. Unit Benchmarks (unit/)

**Direct C++ tests of the MatchingEngine class**

These benchmarks test the matching engine in isolation without network overhead:

- **benchmark_suite.cpp**: Measures pure matching engine performance
  - Submit-only throughput
  - Mixed workload (70% submit, 30% cancel)
  - Cancel-only performance
  - Worst-case scenarios (deep book crossing)

- **stress_test.cpp**: Tests system limits
  - Memory stress with 100k+ orders
  - Sustained high throughput
  - Deep book with many price levels

**Use these for:**
- Finding algorithmic bottlenecks
- Comparing data structure implementations
- Verifying O(1) cancel operations
- Measuring P50/P95/P99 latencies

**Expected Performance:**
- Submit latency: 200-500 ns average
- Cancel latency: 100-300 ns average
- Throughput: 1-5M ops/sec
- P99 latency: < 2 µs

### 2. Integration Tests (integration/)

**End-to-end system tests via TCP socket**

These tests exercise the complete system including network, protocol parsing, and matching:

- **generator.py**: Your existing network RTT measurement tool
  - Sends orders over TCP to running server
  - Measures round-trip time (RTT)
  - Tests realistic client-server interaction

**Use these for:**
- Realistic performance testing
- Measuring network + protocol overhead
- Validating full system behavior
- Integration testing

**Expected Performance:**
- RTT: 50-200 µs average (includes network + parsing)
- Network overhead: ~40-100 µs over unit benchmarks

## Building

### Build with benchmarks enabled

From the project root:

```bash
# Create build directory
mkdir build && cd build

# Configure with benchmarks enabled
cmake -DBUILD_BENCHMARKS=ON ..

# Build everything (main project + benchmarks)
cmake --build .

# Benchmarks will be in build/benchmark/
```

### Build options

```bash
# Enable native optimizations (default: ON)
cmake -DENABLE_NATIVE=ON -DBUILD_BENCHMARKS=ON ..

# Debug build for benchmarks
cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_BENCHMARKS=ON ..

# Release build (default for benchmarks)
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCHMARKS=ON ..
```

## Running Benchmarks

All commands assume you're in the `build` directory after building with `-DBUILD_BENCHMARKS=ON`.

### Unit Benchmarks

```bash
# Run benchmark suite directly
cd benchmark
./benchmark_suite

# Or using CMake target
cd ..
cmake --build . --target run_unit_benchmarks

# Run stress tests
cd benchmark
./stress_test

# Or using target
cd ..
cmake --build . --target run_stress

# Run all unit benchmarks
cmake --build . --target run_all_benchmarks
```

### Integration Tests

**Method 1: Manual (two terminals)**

```bash
# Terminal 1: Start server
./marketDataHandlerLL

# Terminal 2: Run client
cd ../benchmark/integration
python3 generator.py 100000
```

**Method 2: Automated script**

```bash
# From benchmark/integration directory
cd ../benchmark/integration
chmod +x run_integration.sh
./run_integration.sh --num-orders 50000

# Options:
# -n, --num-orders NUM   Number of orders (default: 100000)
# -p, --port PORT        Server port (default: 6767)
```

**Method 3: CMake target** (requires manual server start)

```bash
# Terminal 1: Start server
./marketDataHandlerLL

# Terminal 2: Run integration test
cmake --build . --target run_integration_test
```

### Performance Analysis (Linux)

```bash
# Run with perf counters
cmake --build . --target perf_benchmarks

# Memory profiling
cmake --build . --target memcheck

# Callgrind profiling
cmake --build . --target profile
```

### Save Results for Comparison

```bash
cd benchmark

# Save baseline
./benchmark_suite > results_baseline.txt

# After making changes and rebuilding
./benchmark_suite > results_current.txt

# Compare
python3 tools/results_analyzer.py results_baseline.txt results_current.txt
```

## Understanding the Results

### Unit Benchmark Output

```
=== Submit Only ===
Total operations:  100000
Throughput:        2500000.00 ops/sec
Average latency:   400.00 ns
P50 latency:       380.00 ns
P95 latency:       650.00 ns
P99 latency:       1200.00 ns
```

- **Throughput**: Operations per second (higher is better)
- **Latencies**: Time to complete one operation (lower is better)
- **P50/P95/P99**: Percentile latencies (P99 = 99% of operations complete within this time)

### Integration Test Output

```
sent 10000, last RTT ≈ 85.3 µs, elapsed 0.85s
sent 20000, last RTT ≈ 92.1 µs, elapsed 1.70s
done 100000 orders in 8.50s
```

- **RTT**: Round-trip time from client send to receiving ACK
- **Includes**: Network latency + protocol parsing + matching + response

### Comparing Unit vs Integration

The difference between unit and integration latencies shows your overhead:

```
Integration RTT: 85 µs
Unit latency:    0.4 µs (400 ns)
Overhead:        ~84.6 µs (network + protocol + system calls)
```

This is normal! The overhead includes:
- TCP/IP stack traversal
- System calls (send/recv)
- Protocol parsing
- String operations
- Context switching

## Benchmark Types Explained

### 1. Submit Only
Tests pure order submission throughput with random buy/sell orders.

**Key Metrics:**
- Raw throughput capability
- Average latency under load
- Latency distribution

### 2. Mixed Workload (70/30)
70% order submissions, 30% cancellations - mimics realistic trading.

**Key Metrics:**
- Combined operation throughput
- Cancel performance under load
- Memory stability

### 3. Cancel Only
Tests cancellation performance on pre-built order book.

**Key Metrics:**
- O(1) cancel verification
- Index lookup efficiency
- Book maintenance overhead

### 4. Worst Case (Deep Book Cross)
Aggressive orders that cross many price levels.

**Key Metrics:**
- Latency under contention
- Matching algorithm efficiency
- List traversal performance

### 5. Memory Stress
Creates 100,000+ resting orders.

**Key Metrics:**
- Memory allocation patterns
- Data structure scalability
- Large book performance

### 6. High Throughput
Sustained load over time.

**Key Metrics:**
- Performance stability
- Memory leaks detection
- Thermal throttling effects

### 7. Deep Book
Many price levels with multiple orders per level.

**Key Metrics:**
- Map/tree traversal costs
- Multi-level matching
- Cache effects

## Continuous Benchmarking

For CI/CD integration:

```bash
# From project root
mkdir build && cd build
cmake -DBUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release ..
cmake --build .

# Run benchmarks and save baseline
cd benchmark
./benchmark_suite > baseline.txt

# After code changes, rebuild and compare
cd ..
cmake --build . 
cd benchmark
./benchmark_suite > current.txt
python3 tools/results_analyzer.py baseline.txt current.txt

# Check for regressions (add to your CI script)
if grep -q "REGRESSION" results.txt; then
    echo "Performance regression detected!"
    exit 1
fi
```

## Optimization Tips

If benchmarks show poor performance:

1. **Check compiler flags**: Ensure `-O3 -march=native` are used
   ```bash
   cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_NATIVE=ON -DBUILD_BENCHMARKS=ON ..
   ```

2. **Profile with perf**: Identify hotspots
   ```bash
   cmake --build . --target perf_benchmarks
   ```

3. **Check data structures**: Profile cache misses
   ```bash
   perf stat -e cache-misses,cache-references ./benchmark_suite
   ```

4. **Disable CPU frequency scaling** (for consistent results):
   ```bash
   # Linux
   sudo cpupower frequency-set --governor performance
   ```

5. **Close other applications**: Ensure exclusive CPU access

6. **Check thermal throttling**: Monitor CPU temperature

## Troubleshooting

### Unit benchmarks run slowly
- Verify Release build: `cmake -DCMAKE_BUILD_TYPE=Release ..`
- Check optimization flags in output
- Ensure no debugger attached

### Integration test fails to connect
- Ensure server is running on port 6767
- Check firewall settings
- Verify server accepts connections

### High variance in results
- Run with CPU frequency scaling disabled
- Close other applications
- Run multiple times and average results
- Check for thermal throttling

### Out of memory in stress tests
- Reduce NUM_OPS in stress_test.cpp
- Check for memory leaks with valgrind
- Monitor with `top` or `htop`

## Adding Custom Benchmarks

### Add to unit benchmarks

Edit `unit/benchmark_suite.cpp`:

```cpp
BenchmarkResult benchmarkMyFeature(int num_ops) {
    MatchingEngine engine;
    std::ostringstream out;
    std::vector<long long> latencies;
    
    auto start = high_resolution_clock::now();
    
    for (int i = 0; i < num_ops; ++i) {
        auto t1 = high_resolution_clock::now();
        // Your test code here
        auto t2 = high_resolution_clock::now();
        latencies.push_back(duration_cast<nanoseconds>(t2 - t1).count());
    }
    
    auto end = high_resolution_clock::now();
    double elapsed = duration_cast<microseconds>(end - start).count() / 1e6;
    
    return computeStats("My Feature", latencies, elapsed);
}
```

### Add to integration tests

Create new Python script in `integration/` following the pattern of `generator.py`.

## Further Reading

- See `BENCHMARK_QUICKSTART.md` for quick reference
- Check individual source files for implementation details
- Review CMakeLists.txt for build configuration
