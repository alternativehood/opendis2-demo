#pragma once

#include "adventure_visual_resources.hpp"
#include "adventure_animation_helpers.hpp"

#include <d2adventure_rules/AdventureRoutePreview.hpp>
#include <d2adventure_render/map_geometry.hpp>
#include <d2adventure_render/adventure_render_types.hpp>

#include <string_view>
#include <vector>

namespace d2engine {

struct AdventureRoutePreviewPresentation {
    struct MovementPointLabel {
        std::size_t                      route_step_index = 0;
        d2runtime::MapCellCoord          cell{};
        int                              remaining_movement_points = 0;
        adventure_render::ScreenPoint    canvas_anchor{};
        adventure_render::StableRenderId stable_id = 0;
        bool                             operator==(const MovementPointLabel& other) const {
            return route_step_index == other.route_step_index && cell == other.cell &&
                   remaining_movement_points == other.remaining_movement_points &&
                   canvas_anchor.x == other.canvas_anchor.x &&
                   canvas_anchor.y == other.canvas_anchor.y && stable_id == other.stable_id;
        }
    };
    std::vector<adventure_render::PreparedAdventureRenderPrimitive> ground_overlays;
    std::vector<adventure_render::PreparedAdventureRenderPrimitive> world_primitives;
    std::vector<MovementPointLabel>                                 movement_point_labels;

    [[nodiscard]] bool empty() const { return ground_overlays.empty() && world_primitives.empty(); }
};

class AdventureRoutePreviewPresentationBuilder {
public:
    [[nodiscard]] AdventureRoutePreviewPresentation
    build(const d2adventure::AdventureRoutePreview& preview, std::string_view selected_stack_id,
          const AdventureRoutePreviewVisualResources&   visuals,
          const adventure_render::AdventureMapGeometry& geometry) const;
};

void validate_route_preview_animation_player_ids(
    const AdventureRoutePreviewPresentation& presentation,
    const AdventureAnimationPlayerMap&       static_players);

} // namespace d2engine
