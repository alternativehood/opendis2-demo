#pragma once

#include <d2adventure_render/terrain/road_asset_catalog.hpp>

namespace d2engine {

class FfAssetStore;

adventure_render::RoadAssetCatalog build_road_asset_catalog(const FfAssetStore& store);

} // namespace d2engine
