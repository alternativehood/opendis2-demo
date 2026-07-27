#pragma once

#include "adventure_banner_placement.hpp"
#include "adventure_render_types.hpp"
#include "stack_banner_asset_catalog.hpp"

namespace d2engine::adventure_render {

[[nodiscard]] PreparedAdventureRenderPrimitive build_adventure_banner_primitive(
    const PreparedAdventureRenderPrimitive& reference_primitive,
    CanvasContentBounds reference_content, const StackBannerAsset& banner_asset,
    AdventureBannerDockSide dock_side, StableRenderId stable_id, std::string debug_label,
    AdventurePrimitiveRole semantic_role, std::string semantic_object_id,
    WorldRenderLevel render_level, int local_suborder, GridFootprint footprint,
    IsoDepthAnchor depth_anchor);

} // namespace d2engine::adventure_render
