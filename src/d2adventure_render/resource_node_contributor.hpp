#pragma once

#include <d2adventure_render/map_preparer.hpp>
#include <d2adventure_render/terrain/resource_node_asset_catalog.hpp>

namespace d2engine::adventure_render {

RenderContributor make_resource_node_contributor(const ResourceNodeAssetCatalog& catalog);

} // namespace d2engine::adventure_render
