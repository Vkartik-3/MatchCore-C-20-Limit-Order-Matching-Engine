#pragma once

#include "requests/common_types.hpp"

namespace trading {

// Single side of an execution (bid or ask)
struct ExecutionReport {
    OrderId orderId;
    Price price;
    Quantity qty;
};

} // namespace trading
