# Market Data Handler / Low-Latency Matching Engine (C++20)

This project is an evolving **low-latency matching engine** and **market data handler prototype**, written in **C++20**. It is a work in progress, with many features and optimizations to be made.
---

## Project Structure

```text
marketDataHandlerLL/
│
├── CMakeLists.txt
│
├── include/
│   ├── MatchingEngine.hpp      # Matching engine interface + Order struct
│   └── Protocol.hpp            # Line-based text protocol parser
│
├── src/
│   ├── MatchingEngine.cpp      # Order book + matching logic
│   ├── Protocol.cpp            # Command parsing (SUBMIT, CANCEL, DUMP)
│   └── main.cpp                # TCP server + integration layer
│
├── bench/
│   └── generator.py            # Python load tester
│
└── README.md                   # Project documentation
```

---

## Build Requirements

- **C++20 compiler, we used Clang**
- **CMake 3.13+**
- **Linux/macOS** (uses POSIX sockets)
- Optional: Python 3.13.5 (for stress testing)

---

## Build Instructions

```bash
git clone https://github.com/teddy-ross/marketDataHandlerLL.git
cd marketDataHandlerLL

mkdir build
cd build

cmake ..
cmake --build . --config Release
```

The executable will be created as:

```bash
./marketDataHandlerLL
```

---

## Running the TCP Matching Engine

Start the server:

```bash
./marketDataHandlerLL
```

Expected output:

```text
Listening on port 6767...
```

You may now connect via:

### A. Python generator (recommended)

```bash
python3 generator.py 50000
```

### B. Interactive testing via netcat

```bash
nc 127.0.0.1 6767
```

---

## Text Protocol Specification

### SUBMIT

Create a new order:

```text
SUBMIT <id> <B|S> <price> <qty>
```

Example:

```text
SUBMIT 1 B 100 10
```

### CANCEL

Cancel an order:

```text
CANCEL <id>
```

Example:

```text
CANCEL 1
```

### DUMP

Debug command to show the internal state of the book:

```text
DUMP
```

---

## System Architecture

### 1. Matching Engine (`MatchingEngine.hpp/.cpp`)

Responsibilities:

- Maintain bid and ask books
- Enforce price/time priority
- Match incoming orders against resting liquidity
- Generate `FILL` events
- Handle cancels

The engine is completely decoupled from networking and parsing, making it easy to unit test and reuse.

---

### 2. Protocol Layer (`Protocol.hpp/.cpp`)

Responsibilities:

- Parse incoming text lines (`SUBMIT`, `CANCEL`, `DUMP`)
- Validate input and construct `Order` objects
- Invoke `MatchingEngine` methods
- Format responses (ACK/FILL/ERR/DUMP output)

Later, this can be swapped out for:

- A binary protocol
- FIX-style messages
- Market data feed message formats

---

### 3. TCP Server (`src/main.cpp`)

Responsibilities:

- Open a listening socket on port `6767`
- Accept a single client connection (for now)
- Buffer incoming bytes and split on newline boundaries
- Pass each complete line to `process_line(...)`
- Send responses back to the client

Future enhancements could include:

- Multiple client support
- Non-blocking sockets or `epoll`/`kqueue`/`select` loops
- UDP/multicast ingestion for market data feeds
- Graceful shutdown & logging

---

### 4. Python Generator (`generator.py`)

The generator script:

- Connects to `127.0.0.1:6666`
- Sends a large number of `SUBMIT` commands
- Reads and validates responses
- Measures round-trip time (latency)
- Prints progress and timing statistics

This acts as both a **load tester** and a **functional integration test** for the engine + protocol + server stack.

---

## Roadmap

Planned and potential future improvements:

- **Market data normalization layer** producing `MarketUpdate` structs
- **File-based replay mode** for deterministic backtesting
- **Binary wire protocol** with fixed-size message headers
- **UDP multicast feed simulator** for more realistic exchange-like behavior
- **Lock-free queues** for publishing updates to multiple consumers
- **Snapshot endpoints** for top-of-book and depth-of-book
- **Benchmark harness** with profiling (perf, VTune, etc.)
- **Improved logging and stats** (message rates, latencies, book depth)

---

## License


