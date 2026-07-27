#include <d2engine/app/adventure_route_preview_presentation.hpp>

#include <gtest/gtest.h>

namespace {

d2engine::AdventureAnimatedVisual visual(std::string name, int width, int height, int anchor_x,
                                         int anchor_y) {
    d2engine::AdventureAnimatedVisual result;
    result.container_path = "Imgs/IsoCmon.ff";
    result.semantic_anchor_x = anchor_x;
    result.semantic_anchor_y = anchor_y;
    result.src_width = width;
    result.src_height = height;
    result.animation.animation_name = std::move(name);
    result.animation.native_canvas_w = width;
    result.animation.native_canvas_h = height;
    result.animation.frames.push_back({"frame", 100, width, height});
    return result;
}

d2adventure::AdventureRoutePreview preview() {
    d2adventure::AdventureRoutePreview result;
    result.start = {0, 0};
    result.destination = {2, 1};
    result.steps.push_back({0, {1, 0}, d2adventure::AdventureRouteMarkerKind::Normal, 3});
    result.steps.push_back(
        {1, {2, 1}, d2adventure::AdventureRouteMarkerKind::ActionLimit, std::nullopt});
    return result;
}

d2engine::AdventureRoutePreviewVisualResources visuals() {
    d2engine::AdventureRoutePreviewVisualResources result;
    result.normal = visual("MOVENORMAL", 72, 72, 35, 40);
    result.action_limit = visual("MOVEACTION", 72, 72, 35, 40);
    result.destination_highlight = visual("TILE_HIGHLIGHT", 480, 480, 240, 240);
    return result;
}

} // namespace

TEST(AdventureRoutePreviewPresentation, MapsSemanticMarkersAndAnchors) {
    const auto geometry = d2engine::adventure_render::AdventureMapGeometry::from_source(4, 3);
    const auto result = d2engine::AdventureRoutePreviewPresentationBuilder{}.build(
        preview(), "STACK", visuals(), geometry);
    ASSERT_EQ(result.world_primitives.size(), 2u);
    EXPECT_EQ(result.world_primitives[0].animation->animation_name, "MOVENORMAL");
    EXPECT_EQ(result.world_primitives[1].animation->animation_name, "MOVEACTION");
    for (const auto& primitive : result.world_primitives) {
        EXPECT_EQ(primitive.phase, d2engine::adventure_render::AdventureRenderPhase::World);
        EXPECT_EQ(primitive.level, d2engine::adventure_render::WorldRenderLevel::ActorUnderlay);
        EXPECT_EQ(primitive.local_suborder, d2engine::adventure_render::kActorRouteMarkerSuborder);
        ASSERT_EQ(primitive.footprint.size(), 1u);
        EXPECT_EQ(primitive.footprint.front(), primitive.depth_anchor);
        const auto step = primitive.footprint.front();
        const auto foot = geometry.cell_foot_anchor(step);
        EXPECT_EQ(primitive.draw_origin.x, foot.x - 35);
        EXPECT_EQ(primitive.draw_origin.y, foot.y - 40);
        EXPECT_EQ(primitive.src_width, 72);
        EXPECT_EQ(primitive.src_height, 72);
        EXPECT_EQ(primitive.visual_bounds.max_x - primitive.visual_bounds.min_x, 72);
        EXPECT_EQ(primitive.visual_bounds.max_y - primitive.visual_bounds.min_y, 72);
        EXPECT_FALSE(primitive.interaction_mask);
    }
    EXPECT_NE(result.world_primitives[0].stable_id, result.world_primitives[1].stable_id);
}

TEST(AdventureRoutePreviewPresentation, DestinationHighlightIsGroundOverlay) {
    const auto geometry = d2engine::adventure_render::AdventureMapGeometry::from_source(4, 3);
    const auto result = d2engine::AdventureRoutePreviewPresentationBuilder{}.build(
        preview(), "STACK", visuals(), geometry);
    ASSERT_EQ(result.ground_overlays.size(), 1u);
    const auto& highlight = result.ground_overlays.front();
    EXPECT_EQ(highlight.animation->animation_name, "TILE_HIGHLIGHT");
    EXPECT_EQ(highlight.phase, d2engine::adventure_render::AdventureRenderPhase::GroundOverlay);
    EXPECT_EQ(highlight.depth_anchor, (d2runtime::MapCellCoord{2, 1}));
    EXPECT_EQ(highlight.footprint.front(), (d2runtime::MapCellCoord{2, 1}));
    const auto origin = geometry.cell_canvas_origin({2, 1});
    EXPECT_EQ(highlight.draw_origin.x, origin.x + geometry.half_tile_width - 240);
    EXPECT_EQ(highlight.draw_origin.y, origin.y + geometry.half_tile_height - 240);
    EXPECT_EQ(highlight.src_width, 480);
    EXPECT_EQ(highlight.src_height, 480);
    EXPECT_EQ(highlight.visual_bounds.max_x - highlight.visual_bounds.min_x, 480);
    EXPECT_EQ(highlight.visual_bounds.max_y - highlight.visual_bounds.min_y, 480);
    EXPECT_FALSE(highlight.interaction_mask);
}

TEST(AdventureRoutePreviewPresentation, IDsAreStableAndActionLimitStopsMarkers) {
    const auto geometry = d2engine::adventure_render::AdventureMapGeometry::from_source(4, 3);
    const auto first = d2engine::AdventureRoutePreviewPresentationBuilder{}.build(
        preview(), "STACK", visuals(), geometry);
    const auto second = d2engine::AdventureRoutePreviewPresentationBuilder{}.build(
        preview(), "STACK", visuals(), geometry);
    ASSERT_EQ(first.world_primitives.size(), 2u);
    ASSERT_EQ(first.world_primitives.size(), second.world_primitives.size());
    for (std::size_t i = 0; i < first.world_primitives.size(); ++i) {
        EXPECT_EQ(first.world_primitives[i].stable_id, second.world_primitives[i].stable_id);
        EXPECT_EQ(first.world_primitives[i].draw_origin.x,
                  second.world_primitives[i].draw_origin.x);
        EXPECT_EQ(first.world_primitives[i].draw_origin.y,
                  second.world_primitives[i].draw_origin.y);
    }
    EXPECT_NE(first.world_primitives[0].stable_id, first.world_primitives[1].stable_id);
    EXPECT_NE(first.world_primitives[0].debug_label, first.world_primitives[1].debug_label);
    EXPECT_EQ(first.ground_overlays.front().footprint.front(), preview().destination);
}

TEST(AdventureRoutePreviewPresentation, EmptyRouteStillHighlightsDestination) {
    d2adventure::AdventureRoutePreview empty;
    empty.start = {1, 1};
    empty.destination = empty.start;
    const auto geometry = d2engine::adventure_render::AdventureMapGeometry::from_source(3, 3);
    const auto result = d2engine::AdventureRoutePreviewPresentationBuilder{}.build(
        empty, "STACK", visuals(), geometry);
    EXPECT_TRUE(result.world_primitives.empty());
    EXPECT_EQ(result.ground_overlays.size(), 1u);
}

TEST(AdventureRoutePreviewPresentation, AnimationPlayerIDsRejectDuplicatesAndStaticCollisions) {
    d2engine::AdventureRoutePreviewPresentation                  presentation;
    d2engine::adventure_render::PreparedAdventureRenderPrimitive first;
    first.stable_id = 10;
    first.debug_label = "first-preview";
    first.animation = d2engine::adventure_render::AdventureAnimationData{};
    first.container_path = "Imgs/IsoCmon.ff";
    presentation.ground_overlays.push_back(first);
    EXPECT_NO_THROW(d2engine::validate_route_preview_animation_player_ids(presentation, {}));

    auto duplicate = first;
    duplicate.stable_id = 10;
    duplicate.debug_label = "duplicate-preview";
    presentation.world_primitives.push_back(duplicate);
    try {
        d2engine::validate_route_preview_animation_player_ids(presentation, {});
        FAIL();
    } catch (const std::logic_error& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("duplicate"), std::string::npos);
        EXPECT_NE(message.find("duplicate-preview"), std::string::npos);
    }

    presentation.world_primitives.clear();
    d2engine::AdventureAnimationPlayerMap static_players;
    static_players.emplace(10, d2engine::AnimationPlayer{});
    try {
        d2engine::validate_route_preview_animation_player_ids(presentation, static_players);
        FAIL();
    } catch (const std::logic_error& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("static adventure animation-player map"), std::string::npos);
        EXPECT_NE(message.find("first-preview"), std::string::npos);
    }
}
