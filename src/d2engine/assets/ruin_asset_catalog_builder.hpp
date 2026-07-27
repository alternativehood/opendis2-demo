#pragma once

#include <d2adventure_render/terrain/ruin_asset_catalog.hpp>

namespace d2engine {

class FfAssetStore;

[[nodiscard]] ::d2engine::adventure_render::RuinAssetCatalog
build_ruin_asset_catalog(FfAssetStore& store);

} // namespace d2engine
