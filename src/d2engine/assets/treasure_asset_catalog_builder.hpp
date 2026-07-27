#pragma once

#include <d2adventure_render/terrain/treasure_asset_catalog.hpp>

namespace d2engine {

class FfAssetStore;

[[nodiscard]] adventure_render::TreasureAssetCatalog
build_treasure_asset_catalog(const FfAssetStore& store);

} // namespace d2engine
