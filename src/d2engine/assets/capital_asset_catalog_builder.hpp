#pragma once

#include <d2adventure_render/terrain/capital_asset_catalog.hpp>

namespace d2engine {

class FfAssetStore;

[[nodiscard]] adventure_render::CapitalAssetCatalog
build_capital_asset_catalog(const FfAssetStore& store);

} // namespace d2engine
