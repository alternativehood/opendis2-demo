#include "adventure_terrain_asset_resolver.hpp"

#include <algorithm>
#include <utility>

namespace d2engine {

namespace {

bool has_ground(const TerrainAssetCatalog&                 catalog,
                const d2runtime::AdventureTerrainAssetRef& expected) {
    return std::ranges::any_of(catalog.ground_textures, [&](const auto& asset) {
        return asset.container_path == expected.container_path &&
               asset.record_name == expected.record_name;
    });
}

bool has_border(const TerrainAssetCatalog&                 catalog,
                const d2runtime::AdventureTerrainAssetRef& expected) {
    return std::ranges::any_of(catalog.border_assets, [&](const auto& asset) {
        return asset.container_path == expected.container_path &&
               asset.record_name == expected.record_name;
    });
}

} // namespace

AdventureTerrainAssetResolver::AdventureTerrainAssetResolver(const TerrainAssetCatalog& catalog)
    : catalog_(catalog) {}

ResolvedAdventureTerrainTile AdventureTerrainAssetResolver::resolve(
    const d2runtime::AdventureTerrainTileDescriptor& descriptor) const {
    ResolvedAdventureTerrainTile resolved;
    resolved.descriptor = descriptor;
    resolved.ground_asset_found = has_ground(catalog_, descriptor.expected_ground_asset);

    if (descriptor.border) {
        ResolvedAdventureTerrainBorder border;
        border.descriptor = *descriptor.border;
        if (descriptor.border->kind == d2runtime::AdventureTerrainBorderKind::NonDrawableShape16) {
            border.asset_found = true;
        } else if (descriptor.border->expected_asset) {
            border.asset_found = has_border(catalog_, *descriptor.border->expected_asset);
        }
        resolved.border = std::move(border);
    }

    return resolved;
}

std::vector<ResolvedAdventureTerrainTile> AdventureTerrainAssetResolver::resolve_all(
    const std::vector<d2runtime::AdventureTerrainTileDescriptor>& descriptors) const {
    std::vector<ResolvedAdventureTerrainTile> resolved;
    resolved.reserve(descriptors.size());
    for (const auto& descriptor : descriptors) {
        resolved.push_back(resolve(descriptor));
    }
    return resolved;
}

} // namespace d2engine
