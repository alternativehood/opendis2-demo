#pragma once

#include <d2adventure_render/terrain/terrain_asset_catalog.hpp>
#include <d2adventure_render/terrain/adventure_terrain_types.hpp>

#include <d2runtime/AdventureTerrain.hpp>

#include <optional>
#include <vector>

namespace d2engine {

class AdventureTerrainAssetResolver {
public:
    explicit AdventureTerrainAssetResolver(const TerrainAssetCatalog& catalog);

    [[nodiscard]] ResolvedAdventureTerrainTile
    resolve(const d2runtime::AdventureTerrainTileDescriptor& descriptor) const;

    [[nodiscard]] std::vector<ResolvedAdventureTerrainTile>
    resolve_all(const std::vector<d2runtime::AdventureTerrainTileDescriptor>& descriptors) const;

private:
    const TerrainAssetCatalog& catalog_;
};

} // namespace d2engine
