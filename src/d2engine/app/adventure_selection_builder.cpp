#include "adventure_selection_builder.hpp"

#include <d2adventure_render/adventure_render_types.hpp>
#include <d2adventure_render/map_geometry.hpp>

namespace d2engine {

std::optional<adventure_render::MapCell>
resolve_stack_selection_cell(const d2runtime::AdventureWorldState& world,
                             const d2runtime::AdventureStack&      stack) {
    if (d2runtime::is_stack_on_adventure_map(stack)) {
        return stack.position;
    }

    const auto location = world.find_contained_stack_location(stack);
    if (!location.has_value() || location->footprint == nullptr || location->footprint->empty()) {
        return std::nullopt;
    }

    return adventure_render::AdventureMapGeometry::derive_depth_anchor(*location->footprint);
}

adventure_render::PreparedAdventureRenderPrimitive
build_selection_primitive(const AdventureStackRef& stack, const adventure_render::MapCell& cell,
                          const SelectVisual& visual, adventure_render::StableRenderId stable_id,
                          const adventure_render::AdventureMapGeometry& geo) {
    adventure_render::PreparedAdventureRenderPrimitive prim;
    prim.stable_id = stable_id;
    prim.debug_label = "Select:" + stack.object_id;
    prim.phase = adventure_render::AdventureRenderPhase::World;
    prim.level = adventure_render::WorldRenderLevel::ActorUnderlay;
    prim.local_suborder = adventure_render::kActorSelectionSuborder;
    prim.container_path = visual.key.container_path;
    prim.record_name = visual.key.image_name;
    prim.src_width = visual.src_width;
    prim.src_height = visual.src_height;

    const auto foot = geo.cell_foot_anchor(cell);
    prim.draw_origin = {foot.x - visual.canvas_foot_x, foot.y - visual.canvas_foot_y};
    prim.visual_bounds = {prim.draw_origin.x, prim.draw_origin.y,
                          prim.draw_origin.x + visual.src_width,
                          prim.draw_origin.y + visual.src_height};
    prim.footprint.push_back(cell);
    prim.depth_anchor = cell;
    return prim;
}

} // namespace d2engine
