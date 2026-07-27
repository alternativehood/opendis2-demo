#pragma once

#include "AdventureMovementProfile.hpp"

#include <d2runtime/AdventureGroundType.hpp>

#include <cstdint>

namespace d2adventure {

enum class AdventureCellOccupancy : std::uint8_t {
    Free,
    BlockingObject,
    OccupiedByStack,
};

enum class AdventureMovementBlockReason : std::uint8_t {
    None,
    BlockingObject,
    OccupiedByStack,
    UnsupportedGround,
    ProfileRestriction,
};

struct AdventureTraversalCell {
    d2runtime::AdventureGroundType ground = d2runtime::AdventureGroundType::Unknown;
    bool                           has_road = false;
    AdventureCellOccupancy         occupancy = AdventureCellOccupancy::Free;
};

struct AdventureMovementDecision {
    bool                         passable = false;
    int                          movement_cost = 0;
    AdventureMovementBlockReason block_reason = AdventureMovementBlockReason::None;
};

[[nodiscard]] AdventureMovementDecision
evaluate_adventure_movement_cell(AdventureMovementProfile      profile,
                                 const AdventureTraversalCell& cell);

} // namespace d2adventure
