#include <d2adventure_render/map_geometry.hpp>
#include <d2adventure_render/map_preparer.hpp>
#include <d2adventure_render/ruin_contributor.hpp>
#include <d2adventure_render/terrain/ruin_asset_catalog.hpp>
#include <d2adventure_render/stack_banner_asset_catalog.hpp>

#include <gtest/gtest.h>

#include <algorithm>

namespace d2ar = d2engine::adventure_render;

namespace {

d2ar::StackBannerAssetCatalog make_banner_catalog() {
    d2ar::StackBannerAssetCatalog catalog;
    catalog.frames.resize(5);
    for (auto& frame : catalog.frames) {
        frame.container_path = "synthetic-banner";
        frame.record_name = "BANNER";
        frame.canvas_width = 16;
        frame.canvas_height = 16;
        frame.canvas_foot_x = 0;
        frame.canvas_foot_y = 0;
        frame.content_bounds = {0, 0, 16, 16};
    }
    return catalog;
}

} // namespace

TEST(RuinPipeline, SyntheticCatalogControlsResolutionAndPlacement) {
    d2ar::RuinAssetCatalog catalog;
    catalog.visuals[0][0] = d2ar::StaticRuinVisual{.container_path = "synthetic-land",
                                                   .logical_sprite = "LAND_RUIN_0",
                                                   .canvas_foot_x = 2,
                                                   .canvas_foot_y = 3,
                                                   .canvas_width = 20,
                                                   .canvas_height = 30,
                                                   .content_bounds = {0, 0, 20, 30}};
    catalog.visuals[1][0] = d2ar::StaticRuinVisual{.container_path = "synthetic-water",
                                                   .logical_sprite = "WATER_RUIN_0",
                                                   .canvas_foot_x = 4,
                                                   .canvas_foot_y = 5,
                                                   .canvas_width = 24,
                                                   .canvas_height = 34,
                                                   .content_bounds = {0, 0, 24, 34}};

    d2runtime::AdventureWorldState world;
    world.ruins.push_back({.id = "R_LAND",
                           .image = 0,
                           .position = {1, 1},
                           .placement = d2runtime::AdventureSurfacePlacement::Land,
                           .footprint = {{1, 1}, {2, 1}}});
    world.ruins.push_back({.id = "R_WATER",
                           .image = 0,
                           .position = {4, 2},
                           .placement = d2runtime::AdventureSurfacePlacement::Water,
                           .footprint = {{4, 2}}});
    world.ruins.push_back({.id = "R_MISSING",
                           .image = 10,
                           .position = {6, 2},
                           .placement = d2runtime::AdventureSurfacePlacement::Land,
                           .footprint = {{6, 2}}});

    const auto                 geometry = d2ar::AdventureMapGeometry::from_source(20, 20);
    d2ar::AdventureMapPreparer preparer(geometry);
    const auto                 banner_catalog = make_banner_catalog();
    preparer.add_contributor(d2ar::make_ruin_contributor(catalog, banner_catalog));
    const auto result = preparer.prepare(world);

    ASSERT_EQ(result.graph.world.size(), 4u);
    const auto land =
        std::find_if(result.graph.world.begin(), result.graph.world.end(),
                     [](const auto& primitive) { return primitive.debug_label == "Ruin:R_LAND"; });
    ASSERT_NE(land, result.graph.world.end());
    EXPECT_EQ(land->record_name, "LAND_RUIN_0");
    EXPECT_EQ(land->container_path, "synthetic-land");
    EXPECT_EQ(land->depth_anchor, d2runtime::MapCellCoord({2, 1}));
    const auto banner = std::find_if(
        result.graph.world.begin(), result.graph.world.end(),
        [](const auto& primitive) { return primitive.debug_label == "RuinBanner:R_LAND:4"; });
    ASSERT_NE(banner, result.graph.world.end());
    EXPECT_EQ(banner->visibility_group, d2ar::AdventureRenderVisibilityGroup::Banners);
    EXPECT_TRUE(std::any_of(
        result.diagnostics.begin(), result.diagnostics.end(), [](const auto& diagnostic) {
            return diagnostic.object_id == "R_MISSING" &&
                   diagnostic.kind == d2ar::PrepareDiagnosticKind::UnresolvedNoSprite;
        }));
}
