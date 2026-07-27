#pragma once

#include <cstdint>

namespace d2engine {
struct UnitDef;
}

namespace d2adventure {

enum class AdventureMovementProfile : std::uint8_t {
    Walking,
    Flying,
    Swimming,
};

[[nodiscard]] AdventureMovementProfile
resolve_adventure_movement_profile(const d2engine::UnitDef& leader);

} // namespace d2adventure
