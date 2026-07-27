#include "contained_stack_presentation_builder.hpp"

#include <stdexcept>
#include <string>

namespace d2engine::adventure_render {

ContainedStackPresentationBuilder::ContainedStackPresentationBuilder(
    const ContainedStackShieldAssetCatalog& shield_catalog, const AdventureMapGeometry& geometry)
    : shield_catalog_(shield_catalog), geometry_(geometry) {}

ContainedStackPresentation
ContainedStackPresentationBuilder::build(const d2runtime::AdventureWorldState& /*world*/,
                                         const d2runtime::AdventureStack&                  stack,
                                         const d2runtime::AdventureContainedStackLocation& location,
                                         const d2runtime::AdventureSubraceRef& subrace) const {
    if (location.footprint == nullptr || location.footprint->empty()) {
        throw std::runtime_error("contained_stack_shield_missing_footprint settlement=" +
                                 std::string(location.settlement_id) + " stack_id=" + stack.id);
    }

    const auto& shield_asset = shield_catalog_.resolve(subrace.race_id, location.kind);
    const auto  depth_anchor = AdventureMapGeometry::derive_depth_anchor(*location.footprint);
    const auto  foot = geometry_.cell_foot_anchor(depth_anchor);

    ContainedStackPresentation presentation;
    presentation.pick_entry =
        PickEntry{.stable_id = stable_render_id("ContainedStackShield:" + stack.id),
                  .kind = PickEntryKind::Stack,
                  .object_id = stack.id};

    presentation.shield.stable_id = presentation.pick_entry.stable_id;
    presentation.shield.debug_label =
        "ContainedStackShield:" + std::string(location.settlement_id) + ":" + stack.id + ":" +
        std::string(subrace.race_id);
    presentation.shield.semantic_role = AdventurePrimitiveRole::ContainedStackShield;
    presentation.shield.semantic_object_id = stack.id;
    presentation.shield.phase = AdventureRenderPhase::World;
    presentation.shield.level = WorldRenderLevel::Actor;
    presentation.shield.local_suborder = 0;
    presentation.shield.container_path = shield_asset.container_path;
    presentation.shield.record_name = shield_asset.sprite_name;
    presentation.shield.draw_origin = {foot.x - shield_asset.canvas_foot_x,
                                       foot.y - shield_asset.canvas_foot_y};
    presentation.shield.visual_bounds = {
        presentation.shield.draw_origin.x, presentation.shield.draw_origin.y,
        presentation.shield.draw_origin.x + shield_asset.canvas_width,
        presentation.shield.draw_origin.y + shield_asset.canvas_height};
    presentation.shield.footprint = *location.footprint;
    presentation.shield.depth_anchor = depth_anchor;
    presentation.shield.src_width = shield_asset.canvas_width;
    presentation.shield.src_height = shield_asset.canvas_height;
    presentation.shield.alpha = 1.0f;
    presentation.shield_outer_logical_name = shield_asset.outer_logical_name;
    presentation.shield_sprite_name = shield_asset.sprite_name;

    return presentation;
}

} // namespace d2engine::adventure_render
