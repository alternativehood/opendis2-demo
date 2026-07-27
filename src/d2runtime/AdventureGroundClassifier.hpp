#pragma once

#include "AdventureTerrain.hpp"
#include "AdventureGroundType.hpp"

namespace d2runtime {

[[nodiscard]] AdventureGroundType
classify_adventure_ground(const AdventureTerrainTileDescriptor& descriptor);

} // namespace d2runtime
