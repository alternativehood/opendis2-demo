#include <d2adventure_render/map_geometry.hpp>
#include <d2adventure_render/map_preparer.hpp>
#include <d2adventure_render/ruin_contributor.hpp>
#include <d2engine/assets/stack_banner_asset_catalog_builder.hpp>
#include <d2adventure_render/terrain/ruin_asset_catalog.hpp>
#include <d2runtime/AdventureWorldState.hpp>

#include <gtest/gtest.h>

#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>

namespace d2ar = d2engine::adventure_render;

namespace {

d2ar::RuinAssetCatalog make_catalog() {
    d2ar::RuinAssetCatalog catalog;

    auto add = [&](d2runtime::AdventureSurfacePlacement placement, int image,
                   const char* logical_sprite) {
        d2ar::StaticRuinVisual visual;
        visual.container_path = "Imgs/IsoCmon.ff";
        visual.logical_sprite = logical_sprite;
        visual.canvas_foot_x = 12 + image;
        visual.canvas_foot_y = 18 + image;
        visual.canvas_width = 48 + image;
        visual.canvas_height = 36 + image;
        visual.content_bounds = {0, 0, visual.canvas_width, visual.canvas_height};
        const auto placement_index = placement == d2runtime::AdventureSurfacePlacement::Water
                                         ? std::size_t{1}
                                         : std::size_t{0};
        catalog.visuals[placement_index][static_cast<std::size_t>(image)] = std::move(visual);
    };

    add(d2runtime::AdventureSurfacePlacement::Land, 7, "SYNTHETIC_LAND_RUIN_7");
    add(d2runtime::AdventureSurfacePlacement::Water, 7, "SYNTHETIC_WATER_RUIN_7");
    add(d2runtime::AdventureSurfacePlacement::Land, 1, "SYNTHETIC_LAND_RUIN_1");
    d2ar::AnimatedRuinVisual animation;
    animation.container_path = "Imgs/IsoAnim.ff";
    animation.logical_animation = "SYNTHETIC_RUIN_ANIMATION";
    animation.canvas_foot_x = 4;
    animation.canvas_foot_y = 6;
    animation.animation.animation_name = animation.logical_animation;
    animation.animation.native_canvas_w = 64;
    animation.animation.native_canvas_h = 48;
    animation.animation.is_looping = true;
    animation.animation.frames = {{"FRAME_0", 100, 32, 24, {0, 0, 32, 24}},
                                  {"FRAME_1", 100, 56, 40, {0, 0, 56, 40}}};
    animation.animation.timing_source = d2ar::AdventureAnimationTimingSource::ProvisionalFallback;
    catalog.visuals[0][9] = std::move(animation);
    return catalog;
}

d2ar::StackBannerAssetCatalog make_banner_catalog() {
    d2ar::StackBannerAssetCatalog catalog;
    catalog.frames.resize(5);
    for (std::size_t i = 0; i < catalog.frames.size(); ++i) {
        catalog.frames[i] = {.container_path = "Imgs/IsoCmon.ff",
                             .record_name = "TI",
                             .canvas_width = 300,
                             .canvas_height = 200,
                             .canvas_foot_x = 10,
                             .canvas_foot_y = 20,
                             .content_bounds = {1, 2, 3, 4}};
    }
    return catalog;
}

} // namespace

template <typename Variant>
void expect_static_logical_sprite(const Variant& visual, std::string_view expected) {
    std::visit(
        [&](const auto& candidate) {
            using Visual = std::decay_t<decltype(candidate)>;
            if constexpr (std::is_same_v<Visual, d2ar::StaticRuinVisual>) {
                EXPECT_EQ(candidate.logical_sprite, expected);
            } else {
                FAIL() << "expected static ruin visual";
            }
        },
        visual);
}

TEST(RuinRendering, LandAndWaterRuinsUsePlacementSpecificSprites) {
    const auto                 geometry = d2ar::AdventureMapGeometry::from_source(10, 10);
    d2ar::AdventureMapPreparer preparer(geometry);
    const auto                 catalog = make_catalog();
    const auto                 banner_catalog = make_banner_catalog();
    preparer.add_contributor(d2ar::make_ruin_contributor(catalog, banner_catalog));

    d2runtime::AdventureWorldState world;
    world.ruins.push_back({.id = "S143RU0000",
                           .title = "",
                           .description = "",
                           .cash = "",
                           .item_id = "",
                           .looter_id = "",
                           .ai_priority = 0,
                           .image = 7,
                           .position = {3, 4},
                           .placement = d2runtime::AdventureSurfacePlacement::Land,
                           .defender_unit_ids = {},
                           .formation_positions = {},
                           .footprint = {{3, 4}}});
    world.ruins.push_back({.id = "S143RU0001",
                           .title = "",
                           .description = "",
                           .cash = "",
                           .item_id = "",
                           .looter_id = "",
                           .ai_priority = 0,
                           .image = 7,
                           .position = {4, 4},
                           .placement = d2runtime::AdventureSurfacePlacement::Water,
                           .defender_unit_ids = {},
                           .formation_positions = {},
                           .footprint = {{4, 4}}});

    auto result = preparer.prepare(world);

    ASSERT_EQ(result.graph.world.size(), 4u);
    ASSERT_EQ(result.diagnostics.size(), 2u);

    std::unordered_map<std::string, std::string> record_names;
    for (const auto& prim : result.graph.world) {
        record_names[prim.debug_label] = prim.record_name;
        EXPECT_EQ(prim.phase, d2ar::AdventureRenderPhase::World);
        EXPECT_EQ(prim.level, d2ar::WorldRenderLevel::Structure);
    }

    EXPECT_EQ(record_names.at("Ruin:S143RU0000"), "SYNTHETIC_LAND_RUIN_7");
    EXPECT_EQ(record_names.at("Ruin:S143RU0001"), "SYNTHETIC_WATER_RUIN_7");
}

TEST(RuinRendering, EmptyFootprintEmitsDiagnosticAndNoPrimitive) {
    const auto                 geometry = d2ar::AdventureMapGeometry::from_source(10, 10);
    d2ar::AdventureMapPreparer preparer(geometry);
    const auto                 catalog = make_catalog();
    const auto                 banner_catalog = make_banner_catalog();
    preparer.add_contributor(d2ar::make_ruin_contributor(catalog, banner_catalog));

    d2runtime::AdventureWorldState world;
    world.ruins.push_back({.id = "S143RU0002",
                           .title = "",
                           .description = "",
                           .cash = "",
                           .item_id = "",
                           .looter_id = "",
                           .ai_priority = 0,
                           .image = 7,
                           .position = {5, 6},
                           .placement = d2runtime::AdventureSurfacePlacement::Land,
                           .defender_unit_ids = {},
                           .formation_positions = {},
                           .footprint = {}});

    auto result = preparer.prepare(world);

    EXPECT_TRUE(result.graph.world.empty());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics.front().kind, d2ar::PrepareDiagnosticKind::UnresolvedNoSprite);
    EXPECT_NE(result.diagnostics.front().message.find("reason=empty_footprint"), std::string::npos);
}

TEST(RuinRendering, AnimatedRuinOwnsClockAndUsesAllFrameBounds) {
    const auto                 geometry = d2ar::AdventureMapGeometry::from_source(10, 10);
    d2ar::AdventureMapPreparer preparer(geometry);
    const auto                 catalog = make_catalog();
    const auto                 banner_catalog = make_banner_catalog();
    preparer.add_contributor(d2ar::make_ruin_contributor(catalog, banner_catalog));

    d2runtime::AdventureWorldState world;
    world.ruins.push_back({.id = "ANIMATED",
                           .image = 9,
                           .position = {3, 4},
                           .placement = d2runtime::AdventureSurfacePlacement::Land,
                           .footprint = {{3, 4}}});
    const auto result = preparer.prepare(world);

    ASSERT_EQ(result.graph.world.size(), 2u);
    const auto body = std::find_if(
        result.graph.world.begin(), result.graph.world.end(),
        [](const auto& primitive) { return primitive.debug_label == "Ruin:ANIMATED"; });
    ASSERT_NE(body, result.graph.world.end());
    const auto& primitive = *body;
    ASSERT_TRUE(primitive.animation.has_value());
    EXPECT_FALSE(primitive.animation_sync_source_id.has_value());
    EXPECT_EQ(primitive.record_name, "FRAME_0");
    EXPECT_EQ(primitive.src_width, 32);
    EXPECT_EQ(primitive.src_height, 24);
    EXPECT_EQ(primitive.visual_bounds.max_x - primitive.visual_bounds.min_x, 56);
    EXPECT_EQ(primitive.visual_bounds.max_y - primitive.visual_bounds.min_y, 40);
}

TEST(RuinRendering, GenericRuinIsNotRendered) {
    const auto                 geometry = d2ar::AdventureMapGeometry::from_source(10, 10);
    d2ar::AdventureMapPreparer preparer(geometry);
    const auto                 catalog = make_catalog();
    const auto                 banner_catalog = make_banner_catalog();
    preparer.add_contributor(d2ar::make_ruin_contributor(catalog, banner_catalog));

    d2runtime::AdventureWorldState world;
    world.map_objects.push_back({.id = "GENERIC_RUIN",
                                 .kind = d2runtime::AdventureMapObjectKind::Ruin,
                                 .position = {3, 4}});
    const auto result = preparer.prepare(world);
    EXPECT_TRUE(result.graph.world.empty());
}
