#include "AdventureMovementProfile.hpp"

#include <d2engine/assets/unit_def.hpp>
#include <d2runtime/AdventureGroundType.hpp>
#include <d2runtime/MovementCapabilities.hpp>

namespace d2adventure {

AdventureMovementProfile resolve_adventure_movement_profile(const d2engine::UnitDef& leader) {
    if (leader.water_only)
        return AdventureMovementProfile::Swimming;

    const auto capabilities =
        d2runtime::MovementCapabilities::from_native_ability_ids(leader.native_ability_ids);
    if (capabilities.can_natively_traverse(d2runtime::AdventureGroundType::Plain) &&
        capabilities.can_natively_traverse(d2runtime::AdventureGroundType::Forest) &&
        capabilities.can_natively_traverse(d2runtime::AdventureGroundType::Water))
        return AdventureMovementProfile::Flying;

    return AdventureMovementProfile::Walking;
}

} // namespace d2adventure
