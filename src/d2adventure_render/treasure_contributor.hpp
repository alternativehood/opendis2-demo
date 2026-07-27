#pragma once

#include <d2adventure_render/map_preparer.hpp>
#include <d2adventure_render/terrain/treasure_asset_catalog.hpp>

namespace d2engine::adventure_render {

RenderContributor make_treasure_contributor(const TreasureAssetCatalog& catalog);

} // namespace d2engine::adventure_render
