#pragma once

#include <vector>
#include "requests/common_types.hpp"

namespace trading {

// Aggregated view of a single price level
struct PriceLevel {
    Price price;
    Quantity qty;
};

using PriceLevels = std::vector<PriceLevel>;

// Snapshot of the order book (bids and asks)
class PriceLevelBook {
public:
    PriceLevelBook(const PriceLevels& bids, const PriceLevels& asks)
        : bidLevels(bids)
        , askLevels(asks)
    {}

    const PriceLevels& bids() const { return bidLevels; }
    const PriceLevels& asks() const { return askLevels; }

private:
    PriceLevels bidLevels;
    PriceLevels askLevels;
};

} // namespace trading
