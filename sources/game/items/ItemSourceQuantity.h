#pragma once

#include <cstdint>
#include <string>

namespace game {

// Ordered quantities from original placements. An empty ID is anonymous stock.
struct ItemSourceQuantity {
    std::string instanceId;
    std::uint64_t quantity = 0;
};

} // namespace game
