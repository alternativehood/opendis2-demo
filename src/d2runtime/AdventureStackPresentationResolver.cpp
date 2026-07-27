#include "AdventureStackPresentationResolver.hpp"

namespace d2runtime {

AdventureActorPresentation
AdventureStackPresentationResolver::resolve(const AdventureStackPresentationInput& input) const {
    AdventureActorPresentation result;

    const bool can_traverse_water =
        input.leader_movement.can_natively_traverse(AdventureGroundType::Water) ||
        input.leader_water_only;

    if (input.ground == AdventureGroundType::Water && !can_traverse_water) {
        result.kind = AdventureActorPresentationKind::Boat;
    }

    return result;
}

} // namespace d2runtime
