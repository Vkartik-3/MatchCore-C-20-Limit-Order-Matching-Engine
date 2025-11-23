# Limit Order Book Engine

A C++20 implementation of a limit order book with price-time priority matching. I built this to understand how exchanges actually match orders under the hood.

## What is this?

An order book is the core data structure every exchange uses. It keeps track of buy and sell orders, matches them when prices cross, and makes sure earlier orders get filled first (FIFO within each price level).

This implementation supports:
- Multiple order types (GTC, IOC, FOK, Market, Day)
- Thread-safe operations
- Automatic expiry of day orders at market close
- Order modification (cancel/replace)

## Why I built it

After reading about market microstructure, I wanted to see the mechanics myself. The matching algorithm is simple conceptually but has a lot of edge cases:
- What happens when an order partially fills?
- How do you handle market orders when there's no liquidity?
- How do you keep track of order priority efficiently?

Building this helped me understand the performance trade-offs between different data structures (map vs unordered_map, list vs deque).

## What I learned

- `std::map` is surprisingly fast for ordered data (bids/asks sorted by price)
- The hardest part was getting the thread synchronization right for the day order expiry
- Partial fills complicate everything - you need careful bookkeeping
- Cross-platform time handling is annoying (Windows vs POSIX)

## Build instructions

You'll need a C++20 compiler (GCC 10+, Clang 12+, or MSVC 2019+).

```bash
mkdir build && cd build
cmake ..
cmake --build .
./orderbook_demo
```

For tests:
```bash
cmake --build . --target orderbook_tests
./orderbook_tests
```

## Project structure

```
include/    # Header files organized by component
  ├── core/        # Order entry, types, sides
  ├── book/        # Order book and price levels
  ├── execution/   # Trade execution reports
  └── requests/    # Modify requests, common types
src/        # Implementation files
tests/      # Unit tests
examples/   # Sample usage code
```

## How to use

```cpp
#include "book/limit_order_book.hpp"

using namespace trading;

LimitOrderBook book;

// Submit a buy order
auto buy = std::make_shared<OrderEntry>(
    OrderType::GTC, 1, Side::BUY, 100, 50
);
book.submitOrder(buy);

// Submit a matching sell order
auto sell = std::make_shared<OrderEntry>(
    OrderType::GTC, 2, Side::SELL, 100, 50
);
Executions trades = book.submitOrder(sell);

// trades now contains the executed trade details
```

## Future ideas

Things I might add later:
- Benchmark suite to measure order submission latency
- CSV export for trade history
- Simple market data snapshot API
- Support for iceberg orders
- Maybe a visualization tool to see the book live

## Notes

This is a learning project, not production code. Real exchange matching engines use much more sophisticated data structures (custom memory allocators, lock-free queues, etc.) to hit microsecond latencies.

If you're studying this code, pay attention to:
- How the `std::list` provides O(1) removal when you have an iterator
- Why bids use `std::greater` and asks use `std::less` as comparators
- The trade-off between the `activeOrders` map and the per-level lists

## License

MIT - feel free to use this for learning or your own projects.
