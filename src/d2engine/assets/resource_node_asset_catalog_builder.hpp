#pragma once

#include <d2adventure_render/terrain/resource_node_asset_catalog.hpp>

namespace d2engine {

class FfAssetStore;

adventure_render::ResourceNodeAssetCatalog
build_resource_node_asset_catalog(const FfAssetStore& store);

} // namespace d2engine
