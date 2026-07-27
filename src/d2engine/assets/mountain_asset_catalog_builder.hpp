#pragma once

#include <d2adventure_render/terrain/mountain_asset_catalog.hpp>

namespace d2engine {

class FfAssetStore;

adventure_render::MountainAssetCatalog build_mountain_asset_catalog(const FfAssetStore& store);

} // namespace d2engine
