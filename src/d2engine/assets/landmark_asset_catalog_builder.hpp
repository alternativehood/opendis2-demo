#pragma once

#include <d2adventure_render/terrain/landmark_asset_catalog.hpp>

namespace d2engine {

class FfAssetStore;

adventure_render::LandmarkAssetCatalog build_landmark_asset_catalog(const FfAssetStore& store);

} // namespace d2engine
