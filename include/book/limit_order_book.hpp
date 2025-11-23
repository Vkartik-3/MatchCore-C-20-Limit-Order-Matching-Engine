#pragma once

#include <map>
#include <unordered_map>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <atomic>

#include "requests/common_types.hpp"
#include "core/order_entry.hpp"
#include "requests/modify_request.hpp"
#include "price_level_book.hpp"
#include "execution/execution.hpp"

namespace trading {

class LimitOrderBook {
public:
    LimitOrderBook();
    ~LimitOrderBook();

    // Disable copying and moving
    LimitOrderBook(const LimitOrderBook&) = delete;
    LimitOrderBook& operator=(const LimitOrderBook&) = delete;
    LimitOrderBook(LimitOrderBook&&) = delete;
    LimitOrderBook& operator=(LimitOrderBook&&) = delete;

    // Primary order operations
    Executions submitOrder(OrderPtr order);
    void cancelOrder(OrderId id);
    Executions modifyOrder(ModifyRequest request);

    // Query operations
    std::size_t orderCount() const;
    PriceLevelBook getMarketDepth() const;

private:
    // Internal book entry that pairs an order with its location in the level list
    struct BookEntry {
        OrderPtr order;
        OrderList::iterator listIter;
    };

    // Metrics tracked per price level
    struct LevelMetrics {
        Quantity qty{};
        Quantity count{};

        enum class Action {
            Add,
            Remove,
            Match
        };
    };

    // Core data structures
    std::unordered_map<OrderId, BookEntry> activeOrders;
    std::map<Price, OrderList, std::greater<Price>> bids;    // Sorted high to low
    std::map<Price, OrderList, std::less<Price>> asks;       // Sorted low to high
    std::unordered_map<Price, LevelMetrics> metrics;

    // Thread synchronization
    mutable std::mutex mutex;
    std::thread expiryThread;
    std::condition_variable shutdownCV;
    std::atomic<bool> isShuttingDown{false};

    // Internal order management
    void removeOrderInternal(OrderId id);
    void cancelMultiple(OrderIdList ids);

    // Event handlers
    void handleOrderAdded(OrderPtr order);
    void handleOrderRemoved(OrderPtr order);
    void handleOrderMatched(Price price, Quantity qty, bool fullyFilled);
    void updateLevelMetrics(Price price, Quantity qty, LevelMetrics::Action action);

    // Matching logic
    Executions executeMatching();
    bool canMatchPrice(Side side, Price price) const;
    bool canFillCompletely(Side side, Price price, Quantity qty) const;

    // Background expiry thread
    void expireDayOrders();
};

} // namespace trading
