#include "book/limit_order_book.hpp"

#include <numeric>
#include <chrono>
#include <ctime>

namespace trading {

namespace {
    // Constants for day order expiry
    constexpr int MARKET_CLOSE_HOUR = 16;
    constexpr auto SHUTDOWN_GRACE = std::chrono::milliseconds(100);
}

// Constructor launches the expiry thread
LimitOrderBook::LimitOrderBook()
    : expiryThread([this] { expireDayOrders(); })
{}

// Destructor signals shutdown and waits for expiry thread
LimitOrderBook::~LimitOrderBook() {
    isShuttingDown.store(true, std::memory_order_release);
    shutdownCV.notify_one();
    expiryThread.join();
}

// Submit a new order to the book
Executions LimitOrderBook::submitOrder(OrderPtr order) {
    std::scoped_lock lock(mutex);

    // Reject duplicate order IDs
    if (activeOrders.contains(order->id())) {
        return {};
    }

    // Handle market orders by converting to limit at worst available price
    if (order->type() == OrderType::MARKET) {
        if (order->side() == Side::BUY && !asks.empty()) {
            auto [worstAskPrice, _] = *asks.rbegin();
            order->convertToGTC(worstAskPrice);
        } else if (order->side() == Side::SELL && !bids.empty()) {
            auto [worstBidPrice, _] = *bids.rbegin();
            order->convertToGTC(worstBidPrice);
        } else {
            return {};  // No liquidity for market order
        }
    }

    // IOC orders must match immediately or reject
    if (order->type() == OrderType::IOC && !canMatchPrice(order->side(), order->orderPrice())) {
        return {};
    }

    // FOK orders must fill completely or reject
    if (order->type() == OrderType::FOK && !canFillCompletely(order->side(), order->orderPrice(), order->initial())) {
        return {};
    }

    // Insert order into appropriate side of the book
    OrderList::iterator iter;
    if (order->side() == Side::BUY) {
        auto& levelOrders = bids[order->orderPrice()];
        levelOrders.push_back(order);
        iter = std::prev(levelOrders.end());
    } else {
        auto& levelOrders = asks[order->orderPrice()];
        levelOrders.push_back(order);
        iter = std::prev(levelOrders.end());
    }

    // Track the order in our lookup map
    activeOrders.insert({order->id(), BookEntry{order, iter}});

    handleOrderAdded(order);

    // Attempt to match against opposite side
    return executeMatching();
}

// Cancel an existing order
void LimitOrderBook::cancelOrder(OrderId id) {
    std::scoped_lock lock(mutex);
    removeOrderInternal(id);
}

// Modify an existing order (implemented as cancel-replace)
Executions LimitOrderBook::modifyOrder(ModifyRequest request) {
    OrderType originalType;

    {
        std::scoped_lock lock(mutex);

        if (!activeOrders.contains(request.id())) {
            return {};
        }

        auto& [existingOrder, _] = activeOrders.at(request.id());
        originalType = existingOrder->type();
    }

    // Cancel old order and submit new one
    cancelOrder(request.id());
    return submitOrder(request.toOrderPtr(originalType));
}

// Get total number of active orders
std::size_t LimitOrderBook::orderCount() const {
    std::scoped_lock lock(mutex);
    return activeOrders.size();
}

// Get current market depth (bid/ask levels with quantities)
PriceLevelBook LimitOrderBook::getMarketDepth() const {
    PriceLevels bidData, askData;
    bidData.reserve(activeOrders.size());
    askData.reserve(activeOrders.size());

    auto aggregateLevel = [](Price price, const OrderList& orders) {
        Quantity total = std::accumulate(
            orders.begin(), orders.end(), Quantity{0},
            [](Quantity sum, const OrderPtr& order) {
                return sum + order->remaining();
            }
        );
        return PriceLevel{price, total};
    };

    for (const auto& [price, orders] : bids) {
        bidData.push_back(aggregateLevel(price, orders));
    }

    for (const auto& [price, orders] : asks) {
        askData.push_back(aggregateLevel(price, orders));
    }

    return PriceLevelBook{bidData, askData};
}

// Main matching engine - attempts to cross bid/ask spread
Executions LimitOrderBook::executeMatching() {
    Executions results;
    results.reserve(activeOrders.size());

    while (true) {
        // Need both sides to have orders
        if (bids.empty() || asks.empty()) {
            break;
        }

        auto& [bidPrice, bidOrders] = *bids.begin();
        auto& [askPrice, askOrders] = *asks.begin();

        // Check if prices cross
        if (bidPrice < askPrice) {
            break;
        }

        // Match orders at the front of each level (FIFO)
        while (!bidOrders.empty() && !askOrders.empty()) {
            auto bid = bidOrders.front();
            auto ask = askOrders.front();

            // Determine how much can be filled
            Quantity fillQty = std::min(bid->remaining(), ask->remaining());

            // Execute the fill
            bid->fill(fillQty);
            ask->fill(fillQty);

            // Remove filled orders from book
            if (bid->isFilled()) {
                bidOrders.pop_front();
                activeOrders.erase(bid->id());
            }

            if (ask->isFilled()) {
                askOrders.pop_front();
                activeOrders.erase(ask->id());
            }

            // Record the execution
            results.push_back(Execution{
                ExecutionReport{bid->id(), bid->orderPrice(), fillQty},
                ExecutionReport{ask->id(), ask->orderPrice(), fillQty}
            });

            // Update metrics for both sides
            handleOrderMatched(bid->orderPrice(), fillQty, bid->isFilled());
            handleOrderMatched(ask->orderPrice(), fillQty, ask->isFilled());
        }

        // Clean up empty price levels
        if (bidOrders.empty()) {
            bids.erase(bidPrice);
            metrics.erase(bidPrice);
        }

        if (askOrders.empty()) {
            asks.erase(askPrice);
            metrics.erase(askPrice);
        }
    }

    // Handle IOC orders that didn't fully fill
    if (!bids.empty()) {
        auto& [_, bidOrders] = *bids.begin();
        if (!bidOrders.empty()) {
            auto& order = bidOrders.front();
            if (order->type() == OrderType::IOC) {
                cancelOrder(order->id());
            }
        }
    }

    if (!asks.empty()) {
        auto& [_, askOrders] = *asks.begin();
        if (!askOrders.empty()) {
            auto& order = askOrders.front();
            if (order->type() == OrderType::IOC) {
                cancelOrder(order->id());
            }
        }
    }

    return results;
}

// Check if an order at given price can match against opposite side
bool LimitOrderBook::canMatchPrice(Side side, Price price) const {
    if (side == Side::BUY) {
        if (asks.empty()) return false;
        auto [bestAsk, _] = *asks.begin();
        return price >= bestAsk;
    } else {
        if (bids.empty()) return false;
        auto [bestBid, _] = *bids.begin();
        return price <= bestBid;
    }
}

// Check if an order can be completely filled at given price
bool LimitOrderBook::canFillCompletely(Side side, Price price, Quantity qty) const {
    if (!canMatchPrice(side, price)) {
        return false;
    }

    // Find the best available price on opposite side
    std::optional<Price> threshold;
    if (side == Side::BUY) {
        auto [askPrice, _] = *asks.begin();
        threshold = askPrice;
    } else {
        auto [bidPrice, _] = *bids.begin();
        threshold = bidPrice;
    }

    // Sum up available liquidity across all matchable levels
    Quantity available = 0;
    for (const auto& [levelPrice, levelMetrics] : metrics) {
        // Skip levels on the wrong side
        if (threshold.has_value()) {
            bool wrongSide = (side == Side::BUY && threshold.value() > levelPrice) ||
                           (side == Side::SELL && threshold.value() < levelPrice);
            if (wrongSide) continue;
        }

        // Skip levels beyond our limit price
        bool beyondLimit = (side == Side::BUY && levelPrice > price) ||
                          (side == Side::SELL && levelPrice < price);
        if (beyondLimit) continue;

        available += levelMetrics.qty;

        if (available >= qty) {
            return true;
        }
    }

    return false;
}

// Internal cancel without locking (assumes lock is held)
void LimitOrderBook::removeOrderInternal(OrderId id) {
    if (!activeOrders.contains(id)) {
        return;
    }

    auto [order, iter] = activeOrders.at(id);
    activeOrders.erase(id);

    // Remove from the appropriate side's level list
    Price orderPrice = order->orderPrice();
    if (order->side() == Side::SELL) {
        auto& levelOrders = asks.at(orderPrice);
        levelOrders.erase(iter);
        if (levelOrders.empty()) {
            asks.erase(orderPrice);
        }
    } else {
        auto& levelOrders = bids.at(orderPrice);
        levelOrders.erase(iter);
        if (levelOrders.empty()) {
            bids.erase(orderPrice);
        }
    }

    handleOrderRemoved(order);
}

// Cancel multiple orders (used by expiry thread)
void LimitOrderBook::cancelMultiple(OrderIdList ids) {
    std::scoped_lock lock(mutex);
    for (OrderId id : ids) {
        removeOrderInternal(id);
    }
}

// Update metrics when order is added to book
void LimitOrderBook::handleOrderAdded(OrderPtr order) {
    updateLevelMetrics(order->orderPrice(), order->initial(), LevelMetrics::Action::Add);
}

// Update metrics when order is cancelled
void LimitOrderBook::handleOrderRemoved(OrderPtr order) {
    updateLevelMetrics(order->orderPrice(), order->remaining(), LevelMetrics::Action::Remove);
}

// Update metrics when order is matched
void LimitOrderBook::handleOrderMatched(Price price, Quantity qty, bool fullyFilled) {
    auto action = fullyFilled ? LevelMetrics::Action::Remove : LevelMetrics::Action::Match;
    updateLevelMetrics(price, qty, action);
}

// Core metrics update logic
void LimitOrderBook::updateLevelMetrics(Price price, Quantity qty, LevelMetrics::Action action) {
    auto& data = metrics[price];

    // Update order count
    if (action == LevelMetrics::Action::Add) {
        data.count += 1;
    } else if (action == LevelMetrics::Action::Remove) {
        data.count -= 1;
    }

    // Update total quantity
    if (action == LevelMetrics::Action::Remove || action == LevelMetrics::Action::Match) {
        data.qty -= qty;
    } else {
        data.qty += qty;
    }

    // Clean up empty levels
    if (data.count == 0) {
        metrics.erase(price);
    }
}

// Background thread that expires DAY orders at market close
void LimitOrderBook::expireDayOrders() {
    using namespace std::chrono;

    while (true) {
        // Calculate next expiry time (16:00)
        auto now = system_clock::now();
        auto nowTimeT = system_clock::to_time_t(now);
        std::tm timeParts;

        // Platform-specific time conversion
        #ifdef _WIN32
            localtime_s(&timeParts, &nowTimeT);
        #else
            localtime_r(&nowTimeT, &timeParts);
        #endif

        // If past market close, move to next day
        if (timeParts.tm_hour >= MARKET_CLOSE_HOUR) {
            timeParts.tm_mday += 1;
        }

        // Set to exactly 16:00:00
        timeParts.tm_hour = MARKET_CLOSE_HOUR;
        timeParts.tm_min = 0;
        timeParts.tm_sec = 0;

        auto nextExpiry = system_clock::from_time_t(mktime(&timeParts));
        auto waitDuration = nextExpiry - now + SHUTDOWN_GRACE;

        // Wait until expiry time or shutdown signal
        {
            std::unique_lock lock(mutex);
            if (isShuttingDown.load(std::memory_order_acquire) ||
                shutdownCV.wait_for(lock, waitDuration) == std::cv_status::no_timeout) {
                return;  // Shutting down
            }
        }

        // Collect all DAY orders for cancellation
        OrderIdList expiredIds;
        {
            std::scoped_lock lock(mutex);
            for (const auto& [id, entry] : activeOrders) {
                if (entry.order->type() == OrderType::DAY) {
                    expiredIds.push_back(id);
                }
            }
        }

        // Cancel all expired orders
        cancelMultiple(expiredIds);
    }
}

} // namespace trading
