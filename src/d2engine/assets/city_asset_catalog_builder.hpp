#pragma once

#include <d2adventure_render/terrain/city_asset_catalog.hpp>

namespace d2engine {

class FfAssetStore;

[[nodiscard]] adventure_render::CityAssetCatalog
build_city_asset_catalog(const FfAssetStore& store);

} // namespace d2engine
