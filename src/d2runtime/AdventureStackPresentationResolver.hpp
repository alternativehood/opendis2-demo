#pragma once

#include "AdventureGroundType.hpp"
#include "MovementCapabilities.hpp"

#include <string>

namespace d2runtime {

struct AdventureStackPresentationInput {
    AdventureGroundType  ground;
    MovementCapabilities leader_movement;
    bool                 leader_water_only = false;
};

enum class AdventureActorPresentationKind : uint8_t { Unit, Boat };

struct AdventureActorPresentation {
    AdventureActorPresentationKind kind = AdventureActorPresentationKind::Unit;
};

class AdventureStackPresentationResolver {
public:
    [[nodiscard]] AdventureActorPresentation
    resolve(const AdventureStackPresentationInput& input) const;
};

} // namespace d2runtime
