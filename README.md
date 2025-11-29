# Low-Latency Matching Engine

READ ME UPDATED: 11/28/25

This project is a **single-threaded limit order book and matching engine**, written in modern C++17. It implements realistic exchange-style behavior including:

- Price/time priority matching  
- Persistent order book for both bids and asks  
- Partial fills, full fills, and leftover order insertion  
- O(1) cancel using iterator-based indexing  
- Text-based order entry protocol (SUBMIT / CANCEL)  
- Clean price-level and order-level memory management  

This is designed as a foundation for a full market data handler or exchange simulator. Uses C++17 standard.

---

## Current Features

### Limit Order Matching
Implements strict **price/time priority**:

- Buy orders match the best available sell prices  
- Sell orders match the best available buy prices  
- Supports partial fills  
- Generates `FILL` messages for every trade

### Persistent Order Book
Unfilled (or partially filled) orders are inserted into either:

- `buyBook` — highest price first  
- `sellBook` — lowest price first  

Each price level stores a FIFO queue of orders.

### O(1) Cancel Support
Cancellation is implemented using an `unordered_map` that maps order IDs to:

- The side (B/S)  
- The price level  
- A deque iterator pointing to the exact resting order  

This enables **instant lookup and deletion** of an order without scanning the book.

### Command-Line Interface
The engine reads newline-terminated commands:

SUBMIT <id> <B|S> <price> <quantity>
CANCEL <id>
PRINT

Produces responses:

ACK <id>
FILL <incoming_id> <matched_id> <price> <qty>
CANCEL_ACK <id>
CANCEL_REJECT <id>


---

## Architecture Overview

### Order Books

```cpp
std::map<double, std::deque<Order>, std::greater<double>> buyBook;
std::map<double, std::deque<Order>> sellBook;

```
Books are sorted by:

Descending price for buys

Ascending price for sells

Price levels store FIFO deques of orders.

---
## Building

### Requirements

CMake 3.13+ 
C++17 compiler

### Build Steps

```
mkdir build
cd build
cmake ..
make
```

This produces 
```
lowlatency-matching-engine
```

---
## Running

Launch engine with:
```
./lowlatency-matching-engine
```

## Example


Input:

```
SUBMIT O1 B 100 10
SUBMIT O2 S 100 5
SUBMIT O3 S 101 5
CANCEL O3
PRINT
```

Output:

```
ACK O1
FILL O1 O2 100 5
ACK O3
CANCEL_ACK O3

BUY BOOK:
100: O1(5)

SELL BOOK:
101:
```

## TO DO

### Short Term
Top-of-book and full-depth snapshots

TCP order-entry gateway

UDP multicast market data feed

IOC/FOK order types

Market orders

### Medium Term

Historical replay engine

Logging and instrumentation

Memory pool and preallocation


### Long Term

Multi-threaded matching engine

Lock-free message queues

Python API for strategy testing

Simulation or ingestion of real market data feeds

## License

MIT License

Copyright (c) 2025 Edward Ross

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

