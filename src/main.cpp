#include <iostream>
#include "book/limit_order_book.hpp"

using namespace trading;

int main() {
    LimitOrderBook book;

    std::cout << "Limit Order Book Demo\n";
    std::cout << "====================\n\n";

    // Create a buy order at price 100
    auto buyOrder = std::make_shared<OrderEntry>(
        OrderType::GTC,
        1,  // order ID
        Side::BUY,
        100,  // price
        50    // quantity
    );

    std::cout << "Submitting buy order: ID=" << buyOrder->id()
              << " price=" << buyOrder->orderPrice()
              << " qty=" << buyOrder->initial() << "\n";

    book.submitOrder(buyOrder);
    std::cout << "Order book now has " << book.orderCount() << " orders\n\n";

    // Create a matching sell order
    auto sellOrder = std::make_shared<OrderEntry>(
        OrderType::GTC,
        2,  // order ID
        Side::SELL,
        100,  // price
        50    // quantity
    );

    std::cout << "Submitting sell order: ID=" << sellOrder->id()
              << " price=" << sellOrder->orderPrice()
              << " qty=" << sellOrder->initial() << "\n";

    Executions trades = book.submitOrder(sellOrder);

    std::cout << "\nExecuted " << trades.size() << " trade(s):\n";
    for (const auto& trade : trades) {
        const auto& bid = trade.bidExecution();
        const auto& ask = trade.askExecution();
        std::cout << "  Bid order " << bid.orderId << " matched with ask order "
                  << ask.orderId << " for " << bid.qty << " @ " << bid.price << "\n";
    }

    std::cout << "\nOrder book now has " << book.orderCount() << " orders\n";

    // Get market depth
    auto depth = book.getMarketDepth();
    std::cout << "\nMarket depth:\n";
    std::cout << "  " << depth.bids().size() << " bid levels\n";
    std::cout << "  " << depth.asks().size() << " ask levels\n";

    return 0;
}
