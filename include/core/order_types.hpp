#pragma once

namespace trading {

enum class OrderType {
    GTC,        // Good Till Cancel
    IOC,        // Immediate or Cancel (FillAndKill)
    FOK,        // Fill or Kill
    DAY,        // Good For Day
    MARKET      // Market order
};

} // namespace trading
