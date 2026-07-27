#pragma once

#include "AdventureNavigationMap.hpp"
#include "AdventureRoute.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

namespace d2adventure {

enum class AdventurePathStatus : std::uint8_t {
    Found,
    AlreadyAtDestination,
    EmptyNavigationMap,
    StartOutOfBounds,
    DestinationOutOfBounds,
    StartBlocked,
    DestinationBlocked,
    NoPath,
};

struct AdventurePathResult {
    AdventurePathStatus status = AdventurePathStatus::NoPath;

    std::optional<AdventureRoute> route;

    AdventureMovementBlockReason block_reason = AdventureMovementBlockReason::None;

    [[nodiscard]] bool success() const {
        return status == AdventurePathStatus::Found ||
               status == AdventurePathStatus::AlreadyAtDestination;
    }
};

class AdventurePathfinder {
public:
    [[nodiscard]] AdventurePathResult find_path(const AdventureNavigationMap& map,
                                                d2runtime::MapCellCoord       start,
                                                d2runtime::MapCellCoord       destination,
                                                AdventureMovementProfile      profile,
                                                std::string_view moving_stack_id) const;
};

} // namespace d2adventure
