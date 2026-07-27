#include "adventure_banner_primitive_builder.hpp"

#include <stdexcept>
#include <utility>

namespace d2engine::adventure_render {

PreparedAdventureRenderPrimitive build_adventure_banner_primitive(
    const PreparedAdventureRenderPrimitive& reference_primitive,
    CanvasContentBounds reference_content, const StackBannerAsset& banner_asset,
    AdventureBannerDockSide dock_side, StableRenderId stable_id, std::string debug_label,
    AdventurePrimitiveRole semantic_role, std::string semantic_object_id,
    WorldRenderLevel render_level, int local_suborder, GridFootprint footprint,
    IsoDepthAnchor depth_anchor) {
    if (!is_banner_primitive_role(semantic_role)) {
        throw std::runtime_error("adventure_banner_invalid_semantic_role role=" +
                                 std::string(adventure_primitive_role_name(semantic_role)));
    }
    if (!reference_content.valid() || !banner_asset.content_bounds.valid()) {
        throw std::runtime_error("banner primitive requires valid content bounds");
    }

    PreparedAdventureRenderPrimitive prim;
    prim.stable_id = stable_id;
    prim.debug_label = std::move(debug_label);
    prim.semantic_role = semantic_role;
    prim.semantic_object_id = std::move(semantic_object_id);
    prim.phase = AdventureRenderPhase::World;
    prim.level = render_level;
    prim.visibility_group = AdventureRenderVisibilityGroup::Banners;
    prim.local_suborder = local_suborder;
    prim.container_path = banner_asset.container_path;
    prim.record_name = banner_asset.record_name;
    prim.draw_origin = dock_adventure_banner(reference_primitive.draw_origin, reference_content,
                                             banner_asset.content_bounds, dock_side);
    prim.visual_bounds = {prim.draw_origin.x, prim.draw_origin.y,
                          prim.draw_origin.x + banner_asset.canvas_width,
                          prim.draw_origin.y + banner_asset.canvas_height};
    prim.footprint = std::move(footprint);
    prim.depth_anchor = depth_anchor;
    prim.src_width = banner_asset.canvas_width;
    prim.src_height = banner_asset.canvas_height;
    prim.alpha = 1.0f;
    return prim;
}

} // namespace d2engine::adventure_render
