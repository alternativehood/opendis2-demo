#include <gtest/gtest.h>

#include "src/d2engine/app/adventure_movement_presentation_controller.hpp"

#include <d2adventure_render/adventure_actor_primitive_builder.hpp>

namespace {
using namespace d2engine;
using namespace d2engine::adventure_render;

AdventureActorVisualLayer layer(std::string name, std::size_t frame_count) {
    AdventureActorVisualLayer result{.container_path = "Imgs/Test.ff",
                                     .logical_animation_name = std::move(name),
                                     .native_canvas_w = 64,
                                     .native_canvas_h = 64,
                                     .canvas_foot_x = 16,
                                     .canvas_foot_y = 32};
    for (std::size_t i = 0; i < frame_count; ++i)
        result.frames.push_back(
            {.record_name = "FRAME" + std::to_string(i), .canvas_width = 32, .canvas_height = 32});
    return result;
}

AdventureMovementVisualPlan plan_with_segments(std::size_t count) {
    AdventureMovementVisualPlan plan;
    plan.stack_id = "S143KC0012";
    for (std::size_t i = 0; i < count; ++i) {
        AdventureMovementSegmentVisual segment;
        segment.route_step_index = i;
        segment.from = {static_cast<int>(i), 0};
        segment.to = {static_cast<int>(i + 1), 0};
        segment.move_visual.body = layer("MOVE0", 2);
        segment.idle_visual_at_destination.body = layer("STOP0", 1);
        plan.segments.push_back(std::move(segment));
    }
    return plan;
}
} // namespace

TEST(AdventureMovementPresentationController, RepeatedTemporalIdsAreValid) {
    const auto id = stable_render_id("MovingStack:Body:S143KC0012");
    EXPECT_EQ(id, 15137123245555172749ULL);
    EXPECT_NO_THROW(validate_adventure_movement_animation_domains(
        plan_with_segments(5), AdventureMapGeometry::from_source(8, 8), {}, {}));
}

TEST(AdventureMovementPresentationController, ExternalStaticCollisionIsRejected) {
    AdventureAnimationPlayerMap static_players;
    static_players.emplace(stable_render_id("MovingStack:Body:S143KC0012"), AnimationPlayer{});
    EXPECT_THROW(
        validate_adventure_movement_animation_domains(
            plan_with_segments(2), AdventureMapGeometry::from_source(8, 8), static_players, {}),
        std::logic_error);
}

TEST(AdventureMovementPresentationController, ShadowClockTopologyFollowsFrameCounts) {
    const auto                geometry = AdventureMapGeometry::from_source(8, 8);
    d2runtime::AdventureStack stack;
    stack.id = "S143KC0012";
    const auto           body_id = stable_render_id("MovingStack:Body:S143KC0012");
    const auto           shadow_id = stable_render_id("MovingStack:Shadow:S143KC0012");
    AdventureActorVisual visual;
    visual.body = layer("MOVE0", 2);
    visual.shadow = layer("SMOV0", 2);
    auto set =
        build_adventure_actor_primitives(stack, visual, geometry, geometry.cell_foot_anchor({0, 0}),
                                         {0, 0}, body_id, shadow_id, "body", "shadow", {});
    PreparedAdventureRenderGraph graph;
    graph.world = {set.body, *set.shadow};
    EXPECT_EQ(build_adventure_animation_players(graph).size(), 1U);

    visual.shadow = layer("SMOV0", 3);
    set =
        build_adventure_actor_primitives(stack, visual, geometry, geometry.cell_foot_anchor({0, 0}),
                                         {0, 0}, body_id, shadow_id, "body", "shadow", {});
    graph.world = {set.body, *set.shadow};
    EXPECT_EQ(build_adventure_animation_players(graph).size(), 2U);
}
