#include <iostream>
#include <cassert>
#include "book/limit_order_book.hpp"

using namespace trading;

void testBasicOrderSubmission() {
    LimitOrderBook book;

    auto order = std::make_shared<OrderEntry>(
        OrderType::GTC, 1, Side::BUY, 100, 10
    );

    book.submitOrder(order);
    assert(book.orderCount() == 1);

    std::cout << "✓ Basic order submission test passed\n";
}

void testSimpleMatch() {
    LimitOrderBook book;

    auto buy = std::make_shared<OrderEntry>(
        OrderType::GTC, 1, Side::BUY, 100, 10
    );
    auto sell = std::make_shared<OrderEntry>(
        OrderType::GTC, 2, Side::SELL, 100, 10
    );

    book.submitOrder(buy);
    Executions trades = book.submitOrder(sell);

    assert(trades.size() == 1);
    assert(book.orderCount() == 0);  // Both orders fully filled

    std::cout << "✓ Simple match test passed\n";
}

void testPartialFill() {
    LimitOrderBook book;

    auto buy = std::make_shared<OrderEntry>(
        OrderType::GTC, 1, Side::BUY, 100, 50
    );
    auto sell = std::make_shared<OrderEntry>(
        OrderType::GTC, 2, Side::SELL, 100, 30
    );

    book.submitOrder(buy);
    Executions trades = book.submitOrder(sell);

    assert(trades.size() == 1);
    assert(book.orderCount() == 1);  // Buy order partially filled

    std::cout << "✓ Partial fill test passed\n";
}

void testCancellation() {
    LimitOrderBook book;

    auto order = std::make_shared<OrderEntry>(
        OrderType::GTC, 1, Side::BUY, 100, 10
    );

    book.submitOrder(order);
    assert(book.orderCount() == 1);

    book.cancelOrder(1);
    assert(book.orderCount() == 0);

    std::cout << "✓ Cancellation test passed\n";
}

void testFOKRejection() {
    LimitOrderBook book;

    // Add small sell order
    auto sell = std::make_shared<OrderEntry>(
        OrderType::GTC, 1, Side::SELL, 100, 10
    );
    book.submitOrder(sell);

    // Try to fill 50 with FOK (should reject)
    auto buy = std::make_shared<OrderEntry>(
        OrderType::FOK, 2, Side::BUY, 100, 50
    );
    Executions trades = book.submitOrder(buy);

    assert(trades.size() == 0);      // No trade
    assert(book.orderCount() == 1);  // Original sell still there

    std::cout << "✓ FOK rejection test passed\n";
}

void testMarketOrder() {
    LimitOrderBook book;

    // Add resting sell order
    auto sell = std::make_shared<OrderEntry>(
        OrderType::GTC, 1, Side::SELL, 105, 100
    );
    book.submitOrder(sell);

    // Market buy should match at 105
    auto buy = std::make_shared<OrderEntry>(2, Side::BUY, 50);
    Executions trades = book.submitOrder(buy);

    assert(trades.size() == 1);
    assert(trades[0].bidExecution().price == 105);

    std::cout << "✓ Market order test passed\n";
}

int main() {
    std::cout << "Running Limit Order Book Tests\n";
    std::cout << "==============================\n\n";

    testBasicOrderSubmission();
    testSimpleMatch();
    testPartialFill();
    testCancellation();
    testFOKRejection();
    testMarketOrder();

    std::cout << "\n✓ All tests passed!\n";

    return 0;
}
