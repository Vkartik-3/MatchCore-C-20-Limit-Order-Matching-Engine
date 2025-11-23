#pragma once

#include "core/order_entry.hpp"

namespace trading {

// Request to modify an existing order
class ModifyRequest {
public:
    ModifyRequest(OrderId id, Side side, Price price, Quantity qty)
        : orderId(id)
        , price(price)
        , orderSide(side)
        , qty(qty)
    {}

    OrderId id() const { return orderId; }
    Price modifiedPrice() const { return price; }
    Side modifiedSide() const { return orderSide; }
    Quantity modifiedQty() const { return qty; }

    // Convert this modification request to a new order
    OrderPtr toOrderPtr(OrderType type) const {
        return std::make_shared<OrderEntry>(
            type, id(), modifiedSide(), modifiedPrice(), modifiedQty()
        );
    }

private:
    OrderId orderId;
    Price price;
    Side orderSide;
    Quantity qty;
};

} // namespace trading
