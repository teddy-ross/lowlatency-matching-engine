# Low-Latency Matching Engine (C++23)

A **low-latency limit order book and matching engine** with a text protocol and TCP front end, written in **modern C++23**. I'm building this to learn (and demonstrate) low-latency programming in C++.

The core design:

- **Event-driven engine.** The matching engine performs no formatting or I/O. It reports results through small, strongly-typed events (`AckEvent`, `FillEvent`, `CancelAckEvent`, `RejectEvent`) delivered to a caller-supplied sink constrained by an `EventSink` concept. The protocol layer turns events into wire messages; benchmarks can drop them; tests can record them.
- **Price/time priority book.** Price levels live in `std::pmr::map` (bids descending, asks ascending) with FIFO `std::pmr::list` queues per level, and an `id -> {level, queue position}` locator index for **O(1) cancels**.
- **Pooled allocation.** All book/index nodes are served from a `std::pmr::unsynchronized_pool_resource` owned by the engine, so steady-state submit/cancel traffic recycles fixed-size blocks instead of hitting the global allocator.
- **Allocation-free parsing.** `std::string_view` tokenization + `std::from_chars`, with errors reported via `std::expected<Command, ParseError>`.

### Modern C++ features used

`std::expected`, `std::print`/`std::println` (with a `std::format` fallback on pre-GCC-14 stdlibs), `std::unreachable`, static `operator()`, `std::to_underlying`, concepts (`EventSink`, `OrderRange`), `std::pmr` pooled containers, designated initializers, `std::from_chars`, `std::format_to`.

---

## Build Requirements

- **GCC 13+ or Clang 17+** (GCC 14+ / Clang 18+ recommended for `std::print`; older stdlibs automatically fall back to `std::format` + `cout`)
- **CMake 3.20+**
- **Unix/Linux system**
- **Python 3** (integration generator only)
- **GoogleTest** for the unit tests — downloaded and built automatically by
  CMake (`FetchContent`) when `BUILD_TESTS=ON`; nothing to install

---

## Build Instructions

```bash
git clone https://github.com/teddy-ross/lowlatency-matching-engine.git
cd lowlatency-matching-engine

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Options (all `cmake -D...`):

| Option | Default | Effect |
|---|---|---|
| `ENABLE_NATIVE` | `ON` | `-march=native` |
| `BUILD_TESTS` | `ON` | unit tests + CTest |
| `BUILD_BENCHMARKS` | `OFF` | benchmark + stress binaries |

The server executable is `build/marketDataHandlerLL`.

---

## Running the TCP Matching Engine

```bash
./build/marketDataHandlerLL          # default port 6767
./build/marketDataHandlerLL 7000     # or pass a port
```

Expected output:

```text
Listening on port 6767...
```

The server handles one client at a time but keeps accepting new connections; **book state persists across reconnects**. Client sockets run with `TCP_NODELAY`, and responses for each received chunk are batched into a single `send()`.

Connect via:

```bash
# A. Python generator (load test + integration test)
python3 benchmark/integration/generator.py 50000

# B. Interactive testing
nc 127.0.0.1 6767
```

---

## Text Protocol Specification

### SUBMIT — create a new order

```text
SUBMIT <id> <B|S> <price> <qty>
```

Responses: zero or more `FILL <taker> <maker> <price> <qty>` lines (fills print at the **maker's** price), then `ACK <id>`; or `ERR DUPLICATE_ID <id>` / `ERR BAD_QTY`.

### CANCEL — cancel a resting order

```text
CANCEL <id>
```

Responses: `ACK <id>` on success, `ACK <id> NOT_FOUND` otherwise.

### DUMP — debug view of the book

```text
DUMP
```

Prints `BIDS:` and `ASKS:` sections, one line per price level, best level first, orders in FIFO order as `id(qty)`.

Malformed input yields `ERR BAD_SUBMIT`, `ERR BAD_SIDE`, `ERR BAD_CANCEL`, or `ERR UNKNOWN_CMD`. The wire format is unchanged from the C++20 version; parsing is slightly **stricter** (numeric fields must be whole tokens, and trailing junk after a complete command is rejected).

---

## System Architecture

### 1. Matching Engine (`include/MatchingEngine.hpp`, header-only)

- Maintains bid/ask books with price/time priority
- Matches incoming orders against resting liquidity, best level first, FIFO within a level
- Emits typed events through any `EventSink`; never touches strings or sockets
- O(1) cancels via the locator index
- One deduplicated `matchAgainst` serves both sides by reusing the book's own ordering predicate
- Single-threaded by design; neither copyable nor movable (containers point at the member arena)

### 2. Protocol Layer (`include/Protocol.hpp`, `src/Protocol.cpp`)

- `parse_command`: line → `std::expected<Command, ParseError>`, zero allocations
- `FormattingSink`: engine events → wire text, appended to a reused response buffer
- `process_line`: parse + dispatch + format, the single entry point the server uses

Swappable later for a binary protocol or FIX-style messages without touching the engine.

### 3. TCP Server (`src/main.cpp`)

- Accept loop on port 6767 (or `argv[1]`); the book outlives individual clients
- `TCP_NODELAY`, `MSG_NOSIGNAL` + `SIGPIPE` ignored, `EINTR`-safe send/recv
- O(n) newline framing (offset scan, one buffer compaction per chunk)
- One batched `send()` per received chunk

### 4. Python Generator (`benchmark/integration/generator.py`)

Connects, streams `SUBMIT`s, validates responses, and measures round-trip time — a load tester and end-to-end integration test in one.

---

## Tests

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```

`tests/engine_tests.cpp` pins down matching semantics (maker-price execution, FIFO time priority, level sweeping, cancel paths, rejects), the parser's error taxonomy, the exact `DUMP` format, and the custom-sink API — written with **GoogleTest** (a `MatchingEngineTest` fixture drives the full parse → match → format pipeline), with each test case discovered individually by CTest.

---

## Benchmarks

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCHMARKS=ON
cmake --build build -j
./build/benchmark/benchmark_suite     # latency/throughput scenarios
./build/benchmark/stress_test        # sustained load, deep books
```

Each scenario runs twice: **[wire-format]** (events formatted into a reused response buffer — what the server pays per message) and **[engine-only]** (`NullSink` — pure matching cost). Indicative numbers from a containerized Linux box (GCC 14, `-O3 -march=native`):

| Scenario (avg latency) | wire-format | engine-only |
|---|---|---|
| Submit only | ~340–460 ns | ~122 ns |
| Mixed 70/30 submit/cancel | ~480 ns | ~330 ns |
| Cancel only | ~120 ns | ~50 ns |

A pipelined client (50k orders blasted in one write) sees **~2.4M msgs/sec** end-to-end through the TCP server, ~4x the previous single-send-per-line server.

`benchmark/` also keeps convenience targets: `run_all_benchmarks`, `perf_benchmarks`, `memcheck`, `profile`.

---

## Roadmap

- **Flat/intrusive price levels** (vector-backed or intrusive lists over the pool) to cut pointer chasing on the hot path
- **`epoll` multi-client event loop** with per-connection buffers
- **Binary wire protocol** with fixed-size headers (`std::byteswap` for endianness)
- **Top-of-book / depth snapshots** as engine events
- **Market data normalization layer** and file-based replay for deterministic backtesting
- **Lock-free queues** for publishing updates to multiple consumers
- **Improved logging and stats** (message rates, latencies, book depth)

---

## License

