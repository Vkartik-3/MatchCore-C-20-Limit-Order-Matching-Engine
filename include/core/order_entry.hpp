#pragma once

#include <memory>
#include <list>
#include <stdexcept>
#include <format>

#include "order_types.hpp"
#include "trading_side.hpp"
#include "requests/common_types.hpp"

namespace trading {

class OrderEntry {
public:
    // Standard limit order constructor
    OrderEntry(OrderType type, OrderId id, Side side, Price price, Quantity qty)
        : orderType(type)
        , orderId(id)
        , orderSide(side)
        , price(price)
        , initialQty(qty)
        , remainingQty(qty)
    {}

    // Market order constructor (price is invalid)
    OrderEntry(OrderId id, Side side, Quantity qty)
        : OrderEntry(OrderType::MARKET, id, side, INVALID_PRICE, qty)
    {}

    // Accessors using modern naming
    OrderId id() const { return orderId; }
    Side side() const { return orderSide; }
    Price orderPrice() const { return price; }
    OrderType type() const { return orderType; }
    Quantity initial() const { return initialQty; }
    Quantity remaining() const { return remainingQty; }
    Quantity filled() const { return initial() - remaining(); }
    bool isFilled() const { return remaining() == 0; }

    // Fill this order by specified quantity
    void fill(Quantity qty) {
        if (qty > remaining()) {
            throw std::logic_error(
                std::format("Order {} cannot be filled beyond remaining qty", id())
            );
        }
        remainingQty -= qty;
    }

    // Convert market order to GTC limit order at specified price
    void convertToGTC(Price limitPrice) {
        if (type() != OrderType::MARKET) {
            throw std::logic_error(
                std::format("Only market orders can be converted, order {} is not market", id())
            );
        }
        price = limitPrice;
        orderType = OrderType::GTC;
    }

private:
    OrderType orderType;
    OrderId orderId;
    Side orderSide;
    Price price;
    Quantity initialQty;
    Quantity remainingQty;
};

using OrderPtr = std::shared_ptr<OrderEntry>;
using OrderList = std::list<OrderPtr>;

} // namespace trading
