#include "adventure_route_preview_presentation.hpp"

#include <stdexcept>
#include <string>
#include <unordered_set>

namespace d2engine {

namespace {

adventure_render::PreparedAdventureRenderPrimitive
make_marker(const d2adventure::AdventureRoutePreviewStep& step, std::string_view selected_stack_id,
            const AdventureAnimatedVisual& visual, std::string_view marker_name,
            const adventure_render::AdventureMapGeometry& geometry) {
    const auto        foot = geometry.cell_foot_anchor(step.cell);
    const std::string id_source = "RoutePreview:Marker:" + std::string(selected_stack_id) + ":" +
                                  std::to_string(step.route_step_index) + ":" +
                                  std::string(marker_name);
    adventure_render::PreparedAdventureRenderPrimitive primitive;
    primitive.stable_id = adventure_render::stable_render_id(id_source);
    primitive.debug_label = id_source;
    primitive.phase = adventure_render::AdventureRenderPhase::World;
    primitive.level = adventure_render::WorldRenderLevel::ActorUnderlay;
    primitive.local_suborder = adventure_render::kActorRouteMarkerSuborder;
    primitive.animation = visual.animation;
    primitive.container_path = visual.container_path;
    primitive.draw_origin = {foot.x - visual.semantic_anchor_x, foot.y - visual.semantic_anchor_y};
    primitive.src_width = visual.src_width;
    primitive.src_height = visual.src_height;
    primitive.visual_bounds = {primitive.draw_origin.x, primitive.draw_origin.y,
                               primitive.draw_origin.x + primitive.src_width,
                               primitive.draw_origin.y + primitive.src_height};
    primitive.footprint = {step.cell};
    primitive.depth_anchor = step.cell;
    return primitive;
}

} // namespace

AdventureRoutePreviewPresentation AdventureRoutePreviewPresentationBuilder::build(
    const d2adventure::AdventureRoutePreview& preview, std::string_view selected_stack_id,
    const AdventureRoutePreviewVisualResources&   visuals,
    const adventure_render::AdventureMapGeometry& geometry) const {
    AdventureRoutePreviewPresentation result;
    result.world_primitives.reserve(preview.steps.size());
    for (const auto& step : preview.steps) {
        const bool action = step.marker == d2adventure::AdventureRouteMarkerKind::ActionLimit;
        result.world_primitives.push_back(
            make_marker(step, selected_stack_id, action ? visuals.action_limit : visuals.normal,
                        action ? "ActionLimit" : "Normal", geometry));
        if (!action && step.remaining_movement_points.has_value()) {
            const auto        foot = geometry.cell_foot_anchor(step.cell);
            const std::string source =
                "RoutePreview:MovementPoints:" + std::string(selected_stack_id) + ":" +
                std::to_string(step.route_step_index);
            result.movement_point_labels.push_back({step.route_step_index,
                                                    step.cell,
                                                    *step.remaining_movement_points,
                                                    {foot.x, foot.y - 48},
                                                    adventure_render::stable_render_id(source)});
        }
    }

    const auto        origin = geometry.cell_canvas_origin(preview.destination);
    const auto&       visual = visuals.destination_highlight;
    const std::string id_source = "RoutePreview:Destination:" + std::string(selected_stack_id) +
                                  ":" + std::to_string(preview.destination.x) + ":" +
                                  std::to_string(preview.destination.y);
    adventure_render::PreparedAdventureRenderPrimitive destination;
    destination.stable_id = adventure_render::stable_render_id(id_source);
    destination.debug_label = id_source;
    destination.phase = adventure_render::AdventureRenderPhase::GroundOverlay;
    destination.level = adventure_render::WorldRenderLevel::GroundObject;
    destination.animation = visual.animation;
    destination.container_path = visual.container_path;
    const int center_x = origin.x + geometry.half_tile_width;
    const int center_y = origin.y + geometry.half_tile_height;
    destination.draw_origin = {center_x - visual.semantic_anchor_x,
                               center_y - visual.semantic_anchor_y};
    destination.src_width = visual.src_width;
    destination.src_height = visual.src_height;
    destination.visual_bounds = {destination.draw_origin.x, destination.draw_origin.y,
                                 destination.draw_origin.x + destination.src_width,
                                 destination.draw_origin.y + destination.src_height};
    destination.footprint = {preview.destination};
    destination.depth_anchor = preview.destination;
    result.ground_overlays.push_back(std::move(destination));
    return result;
}

void validate_route_preview_animation_player_ids(
    const AdventureRoutePreviewPresentation& presentation,
    const AdventureAnimationPlayerMap&       static_players) {
    std::unordered_set<adventure_render::StableRenderId> preview_ids;
    const auto                                           validate = [&](const auto& primitives) {
        for (const auto& primitive : primitives) {
            if (!primitive.animation.has_value())
                continue;
            const auto player_id = adventure_animation_player_id(primitive);
            if (!preview_ids.insert(player_id).second) {
                throw std::logic_error(
                    "duplicate route-preview animation player id=" + std::to_string(player_id) +
                    " primitive=" + std::to_string(primitive.stable_id) +
                    " label=" + primitive.debug_label);
            }
            if (static_players.contains(player_id)) {
                throw std::logic_error(
                    "route-preview animation player id collides with static adventure "
                    "animation-player map id=" +
                    std::to_string(player_id) + " primitive=" +
                    std::to_string(primitive.stable_id) + " label=" + primitive.debug_label);
            }
        }
    };
    validate(presentation.ground_overlays);
    validate(presentation.world_primitives);
}

} // namespace d2engine
