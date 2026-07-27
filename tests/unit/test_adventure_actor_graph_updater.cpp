#include <d2engine/app/adventure_actor_graph_updater.hpp>

#include <d2adventure_render/adventure_actor_primitive_builder.hpp>
#include <d2adventure_render/adventure_banner_primitive_builder.hpp>
#include <d2adventure_render/adventure_banner_placement.hpp>
#include <d2adventure_render/map_stack_presentation_ids.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <memory>

namespace {
using namespace d2engine;
using namespace d2engine::adventure_render;

AdventureActorVisual visual() {
    AdventureActorVisual result;
    result.body.container_path = "Imgs/Synthetic.ff";
    result.body.logical_animation_name = "STOP0";
    result.body.native_canvas_w = 16;
    result.body.native_canvas_h = 20;
    result.body.canvas_foot_x = 4;
    result.body.canvas_foot_y = 18;
    result.body.content_bounds = {0, 0, 16, 20};
    result.body.frames.push_back({.record_name = "IDLE0", .canvas_width = 16, .canvas_height = 20});
    return result;
}

StackBannerAsset banner() {
    return {.container_path = "Imgs/IsoCmon.ff",
            .record_name = "STACK_BANNER_0400",
            .canvas_width = 16,
            .canvas_height = 20,
            .canvas_foot_x = 4,
            .canvas_foot_y = 18,
            .content_bounds = {0, 0, 16, 20}};
}

std::shared_ptr<const InteractionMask> mask(int width = 16, int height = 20) {
    auto result = std::make_shared<InteractionMask>();
    result->width = width;
    result->height = height;
    result->bits.resize(static_cast<std::size_t>(result->stride() * height));
    result->bits[0] = 0x80U;
    return result;
}

d2runtime::AdventureStack stack_at(d2runtime::MapCellCoord position = {3, 4}) {
    d2runtime::AdventureStack result;
    result.id = "STACK";
    result.position = position;
    return result;
}

struct StaticStackPresentation {
    AdventureActorPrimitiveSet                      actor;
    PreparedAdventureRenderPrimitive                banner;
    std::optional<PreparedAdventureRenderPrimitive> shadow;
};

StaticStackPresentation install_static_stack_presentation(PreparedAdventureRenderGraph&    graph,
                                                          const AdventureMapGeometry&      geometry,
                                                          const d2runtime::AdventureStack& stack,
                                                          const AdventureActorVisual&      visual,
                                                          const StackBannerAsset& banner_asset,
                                                          int banner_index = 4) {
    const auto ids = map_stack_presentation_render_ids(stack.id);
    const auto foot = geometry.cell_foot_anchor(stack.position);
    auto actor = build_adventure_actor_primitives(stack, visual, geometry, foot, stack.position,
                                                  ids.body, ids.shadow, "body", "shadow", {});
    graph.world.push_back(actor.body);
    if (actor.shadow.has_value()) {
        graph.world.push_back(*actor.shadow);
    }

    const auto banner = build_adventure_banner_primitive(
        actor.body, visual.body.content_bounds, banner_asset,
        AdventureBannerDockSide::RightOfReference, ids.banner,
        "StackBanner:" + stack.id + ":" + std::to_string(banner_index),
        AdventurePrimitiveRole::MapStackBanner, stack.id, WorldRenderLevel::Actor, 1,
        {stack.position}, stack.position);
    graph.world.push_back(banner);

    auto shadow = actor.shadow;
    return {std::move(actor), std::move(banner), std::move(shadow)};
}

std::size_t
count_by_stable_id_and_draw_origin(const std::vector<PreparedAdventureRenderPrimitive>& world,
                                   StableRenderId stable_id, ScreenPoint draw_origin,
                                   AdventurePrimitiveRole role) {
    return static_cast<std::size_t>(
        std::count_if(world.begin(), world.end(), [&](const auto& prim) {
            return prim.stable_id == stable_id && prim.draw_origin.x == draw_origin.x &&
                   prim.draw_origin.y == draw_origin.y && prim.semantic_role == role;
        }));
}

std::size_t count_by_stable_id(const std::vector<PreparedAdventureRenderPrimitive>& world,
                               StableRenderId                                       stable_id) {
    return static_cast<std::size_t>(std::count_if(
        world.begin(), world.end(), [&](const auto& prim) { return prim.stable_id == stable_id; }));
}
} // namespace

TEST(AdventureActorGraphUpdater, InstallsBodyMaskAndKeepsShadowMaskless) {
    auto prepared = adventure_render::PreparedAdventureMap{};
    prepared.geometry = AdventureMapGeometry::from_source(12, 12);
    const auto initial_stack = stack_at({1, 1});
    const auto initial = install_static_stack_presentation(prepared.world_graph, prepared.geometry,
                                                           initial_stack, visual(), banner());
    const auto supplied = mask();
    AdventureAnimationPlayerMap players;

    auto settled_stack = stack_at({3, 4});
    AdventureActorGraphUpdater::settle_stack_actor(settled_stack, visual(), supplied, banner(), 4,
                                                   prepared, players);

    const auto ids = map_stack_presentation_render_ids(settled_stack.id);
    EXPECT_EQ(count_by_stable_id(prepared.world_graph.world, ids.body), 1u);
    EXPECT_EQ(count_by_stable_id(prepared.world_graph.world, ids.banner), 1u);
    EXPECT_EQ(count_by_stable_id(prepared.world_graph.world, ids.shadow), 0u);
    EXPECT_EQ(count_by_stable_id_and_draw_origin(prepared.world_graph.world, ids.body,
                                                 initial.actor.body.draw_origin,
                                                 AdventurePrimitiveRole::MapStackBody),
              0u);
    EXPECT_EQ(count_by_stable_id_and_draw_origin(prepared.world_graph.world, ids.banner,
                                                 initial.banner.draw_origin,
                                                 AdventurePrimitiveRole::MapStackBanner),
              0u);

    const auto body =
        std::find_if(prepared.world_graph.world.begin(), prepared.world_graph.world.end(),
                     [ids](const auto& primitive) { return primitive.stable_id == ids.body; });
    ASSERT_NE(body, prepared.world_graph.world.end());
    EXPECT_EQ(body->interaction_mask, supplied);
    EXPECT_EQ(body->footprint, (std::vector<d2runtime::MapCellCoord>{{3, 4}}));
    EXPECT_EQ(body->depth_anchor, (d2runtime::MapCellCoord{3, 4}));

    const auto expected_banner = build_adventure_banner_primitive(
        *body, visual().body.content_bounds, banner(), AdventureBannerDockSide::RightOfReference,
        ids.banner, "StackBanner:STACK:4", AdventurePrimitiveRole::MapStackBanner, settled_stack.id,
        WorldRenderLevel::Actor, 1, {{3, 4}}, {3, 4});
    const auto banner_prim =
        std::find_if(prepared.world_graph.world.begin(), prepared.world_graph.world.end(),
                     [ids](const auto& primitive) { return primitive.stable_id == ids.banner; });
    ASSERT_NE(banner_prim, prepared.world_graph.world.end());
    EXPECT_EQ(banner_prim->draw_origin.x, expected_banner.draw_origin.x);
    EXPECT_EQ(banner_prim->draw_origin.y, expected_banner.draw_origin.y);
    EXPECT_EQ(banner_prim->footprint, (std::vector<d2runtime::MapCellCoord>{{3, 4}}));
    EXPECT_EQ(banner_prim->depth_anchor, (d2runtime::MapCellCoord{3, 4}));
    EXPECT_EQ(banner_prim->semantic_role, AdventurePrimitiveRole::MapStackBanner);
    EXPECT_EQ(banner_prim->semantic_object_id, settled_stack.id);
    EXPECT_EQ(banner_prim->visibility_group, AdventureRenderVisibilityGroup::Banners);
    EXPECT_FALSE(banner_prim->animation.has_value());
    EXPECT_EQ(banner_prim->interaction_mask, nullptr);
    EXPECT_NE(banner_prim->stable_id, body->stable_id);

    EXPECT_FALSE(players.contains(ids.banner));
}

TEST(AdventureActorGraphUpdater, RepeatedSettlementRemainsIdempotentInCardinality) {
    auto prepared = adventure_render::PreparedAdventureMap{};
    prepared.geometry = AdventureMapGeometry::from_source(12, 12);
    const auto initial_stack = stack_at({1, 1});
    const auto initial = install_static_stack_presentation(prepared.world_graph, prepared.geometry,
                                                           initial_stack, visual(), banner());
    AdventureAnimationPlayerMap players;

    auto settled_stack = stack_at({3, 4});
    AdventureActorGraphUpdater::settle_stack_actor(settled_stack, visual(), mask(), banner(), 4,
                                                   prepared, players);

    const auto ids = map_stack_presentation_render_ids(settled_stack.id);

    const auto first_banner =
        std::find_if(prepared.world_graph.world.begin(), prepared.world_graph.world.end(),
                     [&](const auto& primitive) { return primitive.stable_id == ids.banner; });
    ASSERT_NE(first_banner, prepared.world_graph.world.end());
    const auto first_banner_draw_origin = first_banner->draw_origin;

    settled_stack.position = {5, 6};
    AdventureActorGraphUpdater::settle_stack_actor(settled_stack, visual(), mask(), banner(), 4,
                                                   prepared, players);

    EXPECT_EQ(count_by_stable_id(prepared.world_graph.world, ids.body), 1u);
    EXPECT_EQ(count_by_stable_id(prepared.world_graph.world, ids.banner), 1u);
    EXPECT_EQ(count_by_stable_id(prepared.world_graph.world, ids.shadow), 0u);
    EXPECT_EQ(count_by_stable_id_and_draw_origin(prepared.world_graph.world, ids.body,
                                                 initial.actor.body.draw_origin,
                                                 AdventurePrimitiveRole::MapStackBody),
              0u);
    EXPECT_EQ(count_by_stable_id_and_draw_origin(prepared.world_graph.world, ids.banner,
                                                 initial.banner.draw_origin,
                                                 AdventurePrimitiveRole::MapStackBanner),
              0u);
    EXPECT_EQ(count_by_stable_id_and_draw_origin(prepared.world_graph.world, ids.banner,
                                                 first_banner_draw_origin,
                                                 AdventurePrimitiveRole::MapStackBanner),
              0u);
    EXPECT_EQ(std::count_if(prepared.world_graph.world.begin(), prepared.world_graph.world.end(),
                            [&](const auto& primitive) {
                                return primitive.stable_id == ids.banner &&
                                       primitive.depth_anchor == (d2runtime::MapCellCoord{5, 6});
                            }),
              1);
    EXPECT_FALSE(players.contains(ids.banner));
}

TEST(AdventureActorGraphUpdater, MissingBannerFailsWithActualCount) {
    auto prepared = adventure_render::PreparedAdventureMap{};
    prepared.geometry = AdventureMapGeometry::from_source(12, 12);
    (void)install_static_stack_presentation(prepared.world_graph, prepared.geometry,
                                            stack_at({1, 1}), visual(), banner());
    prepared.world_graph.world.erase(
        std::remove_if(prepared.world_graph.world.begin(), prepared.world_graph.world.end(),
                       [](const auto& primitive) {
                           return primitive.semantic_role == AdventurePrimitiveRole::MapStackBanner;
                       }),
        prepared.world_graph.world.end());

    AdventureAnimationPlayerMap players;
    try {
        AdventureActorGraphUpdater::settle_stack_actor(stack_at({3, 4}), visual(), mask(), banner(),
                                                       4, prepared, players);
        FAIL() << "expected settle_stack_actor to throw";
    } catch (const std::logic_error& e) {
        EXPECT_NE(std::string(e.what()).find(
                      "map_stack_settle_banner_count stack=STACK expected=1 actual=0"),
                  std::string::npos);
    }
}

TEST(AdventureActorGraphUpdater, DuplicateBannerFailsWithActualCount) {
    auto prepared = adventure_render::PreparedAdventureMap{};
    prepared.geometry = AdventureMapGeometry::from_source(12, 12);
    const auto initial = install_static_stack_presentation(prepared.world_graph, prepared.geometry,
                                                           stack_at({1, 1}), visual(), banner());
    prepared.world_graph.world.push_back(initial.banner);

    AdventureAnimationPlayerMap players;
    try {
        AdventureActorGraphUpdater::settle_stack_actor(stack_at({3, 4}), visual(), mask(), banner(),
                                                       4, prepared, players);
        FAIL() << "expected settle_stack_actor to throw";
    } catch (const std::logic_error& e) {
        EXPECT_NE(std::string(e.what()).find(
                      "map_stack_settle_banner_count stack=STACK expected=1 actual=2"),
                  std::string::npos);
    }
}

TEST(AdventureActorGraphUpdater, WrongSemanticBannerFails) {
    auto prepared = adventure_render::PreparedAdventureMap{};
    prepared.geometry = AdventureMapGeometry::from_source(12, 12);
    const auto initial = install_static_stack_presentation(prepared.world_graph, prepared.geometry,
                                                           stack_at({1, 1}), visual(), banner());
    auto       wrong_banner = initial.banner;
    wrong_banner.semantic_role = AdventurePrimitiveRole::SiteBanner;
    prepared.world_graph.world.back() = wrong_banner;

    AdventureAnimationPlayerMap players;
    try {
        AdventureActorGraphUpdater::settle_stack_actor(stack_at({3, 4}), visual(), mask(), banner(),
                                                       4, prepared, players);
        FAIL() << "expected settle_stack_actor to throw";
    } catch (const std::logic_error& e) {
        EXPECT_NE(
            std::string(e.what()).find("map_stack_settle_banner_semantic_mismatch stack=STACK"),
            std::string::npos);
    }
}

TEST(AdventureActorGraphUpdater, RejectsNullAndWrongSizedMasks) {
    auto prepared = adventure_render::PreparedAdventureMap{};
    prepared.geometry = AdventureMapGeometry::from_source(12, 12);
    (void)install_static_stack_presentation(prepared.world_graph, prepared.geometry,
                                            stack_at({1, 1}), visual(), banner());
    AdventureAnimationPlayerMap players;
    EXPECT_THROW(AdventureActorGraphUpdater::settle_stack_actor(stack_at({3, 4}), visual(), nullptr,
                                                                banner(), 4, prepared, players),
                 std::logic_error);
    EXPECT_THROW(AdventureActorGraphUpdater::settle_stack_actor(
                     stack_at({3, 4}), visual(), mask(2, 2), banner(), 4, prepared, players),
                 std::logic_error);
}
