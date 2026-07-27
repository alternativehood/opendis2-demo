#pragma once

#include <d2adventure_render/terrain/tree_asset_catalog.hpp>

namespace d2engine {

class FfAssetStore;

adventure_render::TreeAssetCatalog build_tree_asset_catalog(const FfAssetStore& store);

} // namespace d2engine
