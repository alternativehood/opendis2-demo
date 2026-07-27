#pragma once

#include <d2adventure_render/terrain/site_asset_catalog.hpp>

namespace d2engine {

class FfAssetStore;

[[nodiscard]] d2engine::adventure_render::SiteAssetCatalog
build_site_asset_catalog(const FfAssetStore& store);

} // namespace d2engine
