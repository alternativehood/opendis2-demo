#include "AdventureMovementPolicy.hpp"

namespace d2adventure {

AdventureMovementDecision evaluate_adventure_movement_cell(AdventureMovementProfile      profile,
                                                           const AdventureTraversalCell& cell) {
    if (cell.occupancy == AdventureCellOccupancy::BlockingObject)
        return {false, 0, AdventureMovementBlockReason::BlockingObject};
    if (cell.occupancy == AdventureCellOccupancy::OccupiedByStack)
        return {false, 0, AdventureMovementBlockReason::OccupiedByStack};

    using d2runtime::AdventureGroundType;
    if (cell.ground != AdventureGroundType::Plain && cell.ground != AdventureGroundType::Forest &&
        cell.ground != AdventureGroundType::Water)
        return {false, 0, AdventureMovementBlockReason::UnsupportedGround};

    if (profile == AdventureMovementProfile::Flying)
        return {true, 2, AdventureMovementBlockReason::None};

    if (profile == AdventureMovementProfile::Swimming) {
        if (cell.ground == AdventureGroundType::Water)
            return {true, 2, AdventureMovementBlockReason::None};
        return {false, 0, AdventureMovementBlockReason::ProfileRestriction};
    }

    if (cell.has_road)
        return {true, 1, AdventureMovementBlockReason::None};
    const int cost = cell.ground == AdventureGroundType::Plain    ? 3
                     : cell.ground == AdventureGroundType::Forest ? 4
                                                                  : 6;
    return {true, cost, AdventureMovementBlockReason::None};
}

} // namespace d2adventure
