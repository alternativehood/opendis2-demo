#pragma once

#include <d2runtime/AdventureTerrain.hpp>

#include <optional>

namespace d2engine {

struct ResolvedAdventureTerrainBorder {
    d2runtime::AdventureTerrainBorderDescriptor descriptor;
    bool                                        asset_found = false;
};

struct ResolvedAdventureTerrainTile {
    d2runtime::AdventureTerrainTileDescriptor     descriptor;
    bool                                          ground_asset_found = false;
    std::optional<ResolvedAdventureTerrainBorder> border;
};

} // namespace d2engine
