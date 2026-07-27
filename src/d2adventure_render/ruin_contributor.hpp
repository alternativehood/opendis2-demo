#pragma once

#include "map_preparer.hpp"

#include <d2adventure_render/stack_banner_asset_catalog.hpp>
#include <d2adventure_render/terrain/ruin_asset_catalog.hpp>

namespace d2engine::adventure_render {

RenderContributor make_ruin_contributor(const RuinAssetCatalog&        catalog,
                                        const StackBannerAssetCatalog& banner_catalog);

} // namespace d2engine::adventure_render
