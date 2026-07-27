#pragma once
#include "adventure_movement_visual_plan.hpp"
#include "adventure_animation_helpers.hpp"
#include "../../d2adventure_render/adventure_actor_primitive_builder.hpp"
#include <d2adventure_render/stack_banner_asset_catalog.hpp>
#include <d2game/GameSession.hpp>
#include <d2adventure_render/map_geometry.hpp>
#include <optional>
#include <memory>

namespace d2engine {
struct AdventureMovementPresentationPolicy {
    static constexpr int  segment_duration_ms = 400;
    static constexpr int  actor_frame_duration_ms = 100;
    static constexpr bool actor_animation_loops = true;
};

struct AdventureMovementActorSettlement {
    std::string                                              stack_id;
    adventure_render::AdventureActorVisual                   idle_visual;
    std::shared_ptr<const adventure_render::InteractionMask> idle_interaction_mask;
    adventure_render::StackBannerAsset                       banner_asset;
    int                                                      banner_index = 0;
    std::size_t                                              last_committed_step_index = 0;
    bool                                                     completed = false;
};

struct AdventureMovementPresentationUpdate {
    std::vector<d2game::GameCommandResult>          command_results;
    std::optional<AdventureMovementActorSettlement> settlement;
};

void validate_adventure_movement_animation_domains(
    const AdventureMovementVisualPlan& plan, const adventure_render::AdventureMapGeometry& geometry,
    const AdventureAnimationPlayerMap& static_players,
    const AdventureAnimationPlayerMap& route_preview_players);

class AdventureMovementPresentationController {
public:
    void begin(AdventureMovementVisualPlan                   plan,
               const adventure_render::AdventureMapGeometry& geometry) {
        if (plan.segments.empty())
            throw std::invalid_argument("movement presentation requires a non-empty plan");
        auto first_primitives = [&] {
            const auto&                         segment = plan.segments.front();
            const auto                          a = geometry.cell_foot_anchor(segment.from);
            const adventure_render::ScreenPoint foot{a.x, a.y};
            d2runtime::AdventureStack           presentation_stack;
            presentation_stack.id = plan.stack_id;
            presentation_stack.position = segment.from;
            const auto set = adventure_render::build_adventure_actor_primitives(
                presentation_stack, segment.move_visual, geometry, foot, segment.from,
                adventure_render::stable_render_id("MovingStack:Body:" + plan.stack_id),
                adventure_render::stable_render_id("MovingStack:Shadow:" + plan.stack_id),
                "MovingStack:Body:" + plan.stack_id, "MovingStack:Shadow:" + plan.stack_id, {});
            std::vector<adventure_render::PreparedAdventureRenderPrimitive> result{set.body};
            if (set.shadow)
                result.push_back(*set.shadow);
            return result;
        }();
        auto first_players = [&] {
            adventure_render::PreparedAdventureRenderGraph graph;
            graph.world = first_primitives;
            return build_adventure_animation_players(graph);
        }();
        plan_ = std::move(plan);
        geometry_ = &geometry;
        segment_index_ = 0;
        elapsed_ms_ = 0;
        active_primitives_ = std::move(first_primitives);
        animation_players_ = std::move(first_players);
        for (auto& [id, player] : animation_players_)
            player.play();
    }
    [[nodiscard]] bool             active() const { return plan_.has_value(); }
    [[nodiscard]] std::string_view stack_id() const {
        return plan_ ? std::string_view(plan_->stack_id) : std::string_view{};
    }
    [[nodiscard]] const AdventureAnimationPlayerMap& animation_players() const {
        return animation_players_;
    }
    [[nodiscard]] std::optional<adventure_render::ScreenPoint> current_actor_foot() const;
    [[nodiscard]] std::vector<adventure_render::PreparedAdventureRenderPrimitive>
    current_world_primitives(const adventure_render::AdventureMapGeometry&) const;
    [[nodiscard]] AdventureMovementPresentationUpdate update(int animation_delta_ms,
                                                             d2game::GameSession& session);
    void                                              clear() {
        plan_.reset();
        segment_index_ = 0;
        elapsed_ms_ = 0;
        animation_players_.clear();
        active_primitives_.clear();
        geometry_ = nullptr;
    }

private:
    std::optional<AdventureMovementVisualPlan>                      plan_;
    std::size_t                                                     segment_index_ = 0;
    int                                                             elapsed_ms_ = 0;
    const adventure_render::AdventureMapGeometry*                   geometry_ = nullptr;
    AdventureAnimationPlayerMap                                     animation_players_;
    std::vector<adventure_render::PreparedAdventureRenderPrimitive> active_primitives_;

    [[nodiscard]] std::vector<adventure_render::PreparedAdventureRenderPrimitive>
    primitives_for(const adventure_render::AdventureMapGeometry&, std::size_t, double) const;
};
} // namespace d2engine
