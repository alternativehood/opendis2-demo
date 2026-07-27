#pragma once

#include <d2adventure_render/map_preparer.hpp>
#include <d2adventure_render/terrain/city_asset_catalog.hpp>

namespace d2engine::adventure_render {

RenderContributor make_city_contributor(const CityAssetCatalog& catalog);

} // namespace d2engine::adventure_render
