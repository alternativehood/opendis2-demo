#pragma once

#include "map_preparer.hpp"

#include <d2adventure_render/stack_banner_asset_catalog.hpp>
#include <d2adventure_render/terrain/site_asset_catalog.hpp>

namespace d2engine::adventure_render {

RenderContributor make_site_contributor(const SiteAssetCatalog&        catalog,
                                        const StackBannerAssetCatalog& banner_catalog);

} // namespace d2engine::adventure_render
