#pragma once

#include <cstdint>
#include <vector>
#include <limits>

namespace trading {

using Price = std::int32_t;
using Quantity = std::uint32_t;
using OrderId = std::uint64_t;
using OrderIdList = std::vector<OrderId>;

// Sentinel value for invalid prices (used in market orders)
inline constexpr Price INVALID_PRICE = std::numeric_limits<Price>::max();

} // namespace trading
