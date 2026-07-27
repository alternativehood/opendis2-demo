#include <d2adventure_render/city_contributor.hpp>
#include <d2adventure_render/map_geometry.hpp>
#include <d2adventure_render/map_preparer.hpp>
#include <d2adventure_render/terrain/city_asset_catalog.hpp>
#include <d2engine/assets/render_graph_asset_collector.hpp>
#include <d2runtime/AdventureWorldState.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <initializer_list>
#include <set>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace d2ar = d2engine::adventure_render;

namespace {

const auto kGeo = d2ar::AdventureMapGeometry::from_source(20, 20);

struct FrameSpec {
    std::string record;
    int         duration_ms = 100;
    int         width = 0;
    int         height = 0;
};

d2ar::AnimatedCityLayer make_layer(std::string animation_name, int foot_x, int foot_y, int native_w,
                                   int native_h, std::initializer_list<FrameSpec> frames) {
    d2ar::AnimatedCityLayer layer;
    layer.container_path = "Imgs/IsoAnim.ff";
    layer.logical_animation = std::move(animation_name);
    layer.canvas_foot_x = foot_x;
    layer.canvas_foot_y = foot_y;
    layer.animation.animation_name = layer.logical_animation;
    layer.animation.native_canvas_w = native_w;
    layer.animation.native_canvas_h = native_h;
    layer.animation.is_looping = true;
    for (const auto& frame : frames) {
        layer.animation.frames.push_back({.record_name = frame.record,
                                          .duration_ms = frame.duration_ms,
                                          .canvas_width = frame.width,
                                          .canvas_height = frame.height});
    }
    return layer;
}

d2ar::CityVisual make_visual(d2ar::AnimatedCityLayer                body,
                             std::optional<d2ar::AnimatedCityLayer> shadow = std::nullopt) {
    d2ar::CityVisual visual;
    visual.body = std::move(body);
    visual.shadow = std::move(shadow);
    return visual;
}

} // namespace

TEST(CityAssetCatalog, ResolveForSizeMapsAndRejectsInvalidSizes) {
    d2ar::CityAssetCatalog catalog;
    catalog.visuals[0] = make_visual(make_layer("SMALL", 10, 20, 320, 320, {{"S1", 100, 64, 64}}));
    catalog.visuals[1] =
        make_visual(make_layer("MEDIUM", 11, 21, 320, 320, {{"M1", 90, 70, 70}}),
                    make_layer("MEDIUMS", 12, 22, 320, 320, {{"MS1", 90, 70, 70}}));
    catalog.visuals[2] = make_visual(make_layer("LARGE", 13, 23, 320, 320, {{"L1", 110, 80, 80}}));

    EXPECT_EQ(catalog.resolve_for_size(1).body.logical_animation, "SMALL");
    EXPECT_EQ(catalog.resolve_for_size(2).body.logical_animation, "MEDIUM");
    EXPECT_EQ(catalog.resolve_for_size(3).body.logical_animation, "LARGE");
    EXPECT_EQ(catalog.resolve_for_size(4).body.logical_animation, "LARGE");
    EXPECT_EQ(catalog.resolve_for_size(5).body.logical_animation, "LARGE");
    EXPECT_EQ(catalog.resolve_for_size(2).shadow->logical_animation, "MEDIUMS");
    EXPECT_FALSE(catalog.resolve_for_size(1).shadow.has_value());
    EXPECT_FALSE(catalog.resolve_for_size(3).shadow.has_value());

    for (int size : {0, 6}) {
        EXPECT_THROW({ static_cast<void>(catalog.resolve_for_size(size)); }, std::runtime_error);
    }

    EXPECT_EQ(catalog.resolve_for_size(1).body.container_path, "Imgs/IsoAnim.ff");
    EXPECT_EQ(catalog.resolve_for_size(2).shadow->container_path, "Imgs/IsoAnim.ff");
}

TEST(CityRendering, SmallAndLargeProduceSingleBodyPrimitiveAndIgnoreGenericCapital) {
    d2ar::CityAssetCatalog catalog;
    catalog.visuals[0] = make_visual(make_layer("SMALL", 10, 20, 320, 320, {{"S1", 100, 64, 64}}));
    catalog.visuals[1] =
        make_visual(make_layer("MEDIUM", 11, 21, 320, 320, {{"M1", 90, 70, 70}}),
                    make_layer("MEDIUMS", 12, 22, 320, 320, {{"MS1", 90, 70, 70}}));
    catalog.visuals[2] = make_visual(make_layer("LARGE", 13, 23, 320, 320, {{"L1", 110, 80, 80}}));

    d2ar::AdventureMapPreparer preparer(kGeo);
    preparer.add_contributor(d2ar::make_city_contributor(catalog));

    d2runtime::AdventureWorldState world;
    world.map_width = 20;
    world.map_height = 20;
    world.cities.push_back({.id = "CITY1", .position = {2, 2}, .size = 1, .footprint = {{2, 2}}});
    world.cities.push_back(
        {.id = "CITY5", .position = {5, 5}, .size = 5, .footprint = {{5, 5}, {6, 5}, {6, 6}}});
    world.map_objects.push_back({.id = "CAP",
                                 .kind = d2runtime::AdventureMapObjectKind::Capital,
                                 .position = {10, 10},
                                 .footprint = {{10, 10}}});

    const auto result = preparer.prepare(world);

    ASSERT_EQ(result.graph.world.size(), 2u);
    EXPECT_EQ(result.diagnostics.size(), 2u);

    auto small =
        std::find_if(result.graph.world.begin(), result.graph.world.end(),
                     [](const auto& prim) { return prim.debug_label == "City:CITY1:body"; });
    ASSERT_NE(small, result.graph.world.end());
    EXPECT_EQ(small->phase, d2ar::AdventureRenderPhase::World);
    EXPECT_EQ(small->level, d2ar::WorldRenderLevel::Structure);
    EXPECT_EQ(small->local_suborder, 0);
    ASSERT_TRUE(small->animation.has_value());
    ASSERT_FALSE(small->animation->frames.empty());
    EXPECT_EQ(small->animation->frames[0].record_name, "S1");
    EXPECT_FALSE(small->animation_sync_source_id.has_value());
    ASSERT_EQ(small->footprint.size(), 1u);
    EXPECT_EQ(small->footprint[0], (d2runtime::MapCellCoord{2, 2}));

    auto large =
        std::find_if(result.graph.world.begin(), result.graph.world.end(),
                     [](const auto& prim) { return prim.debug_label == "City:CITY5:body"; });
    ASSERT_NE(large, result.graph.world.end());
    EXPECT_EQ(large->level, d2ar::WorldRenderLevel::Structure);
    EXPECT_EQ(large->local_suborder, 0);
    ASSERT_TRUE(large->animation.has_value());
    ASSERT_EQ(large->footprint.size(), 3u);
    EXPECT_EQ(large->depth_anchor, (d2runtime::MapCellCoord{6, 6}));

    EXPECT_EQ(result.graph.pick_entries.size(), 0u);
    EXPECT_TRUE(
        std::none_of(result.graph.world.begin(), result.graph.world.end(), [](const auto& prim) {
            return prim.debug_label.find("CAP") != std::string::npos;
        }));
}

TEST(CityRendering, MediumProducesBodyAndShadowWithSharedDepthAndCollectorFrames) {
    d2ar::CityAssetCatalog catalog;
    catalog.visuals[0] = make_visual(make_layer("SMALL", 10, 20, 320, 320, {{"S1", 100, 64, 64}}));
    catalog.visuals[1] = make_visual(
        make_layer("G000FT0000NE2", 14, 24, 320, 320, {{"M1", 120, 80, 72}, {"M2", 100, 84, 76}}),
        make_layer("G000FT0000NE2S", 18, 28, 320, 320,
                   {{"MS1", 120, 80, 40}, {"MS2", 100, 84, 44}}));
    catalog.visuals[2] = make_visual(make_layer("LARGE", 13, 23, 320, 320, {{"L1", 110, 80, 80}}));

    d2ar::AdventureMapPreparer preparer(kGeo);
    preparer.add_contributor(d2ar::make_city_contributor(catalog));

    d2runtime::AdventureWorldState world;
    world.map_width = 20;
    world.map_height = 20;
    world.cities.push_back({.id = "CITY2",
                            .position = {4, 4},
                            .size = 2,
                            .footprint = {{4, 4}, {5, 4}, {4, 5}, {5, 5}}});
    world.cities.push_back({.id = "EMPTY", .position = {1, 1}, .size = 1});
    world.map_objects.push_back({.id = "CAP",
                                 .kind = d2runtime::AdventureMapObjectKind::Capital,
                                 .position = {10, 10},
                                 .footprint = {{10, 10}}});

    const auto result = preparer.prepare(world);

    ASSERT_EQ(result.graph.world.size(), 2u);

    auto body =
        std::find_if(result.graph.world.begin(), result.graph.world.end(),
                     [](const auto& prim) { return prim.debug_label == "City:CITY2:body"; });
    auto shadow =
        std::find_if(result.graph.world.begin(), result.graph.world.end(),
                     [](const auto& prim) { return prim.debug_label == "City:CITY2:shadow"; });
    ASSERT_NE(body, result.graph.world.end());
    ASSERT_NE(shadow, result.graph.world.end());
    EXPECT_EQ(body->phase, d2ar::AdventureRenderPhase::World);
    EXPECT_EQ(shadow->phase, d2ar::AdventureRenderPhase::World);
    EXPECT_EQ(body->level, d2ar::WorldRenderLevel::Structure);
    EXPECT_EQ(shadow->level, d2ar::WorldRenderLevel::Structure);
    EXPECT_EQ(body->footprint, shadow->footprint);
    EXPECT_EQ(body->depth_anchor, shadow->depth_anchor);
    EXPECT_EQ(body->local_suborder, 0);
    EXPECT_EQ(shadow->local_suborder, -1);
    EXPECT_TRUE(shadow->animation_sync_source_id.has_value());
    EXPECT_EQ(*shadow->animation_sync_source_id, body->stable_id);
    EXPECT_FLOAT_EQ(shadow->alpha, 1.0f);
    EXPECT_FALSE(body->animation_sync_source_id.has_value());
    EXPECT_EQ(body->depth_anchor, (d2runtime::MapCellCoord{5, 5}));
    EXPECT_NE(body->draw_origin.x, shadow->draw_origin.x);
    EXPECT_NE(body->draw_origin.y, shadow->draw_origin.y);
    EXPECT_EQ(body->visual_bounds.max_x - body->visual_bounds.min_x, 84);
    EXPECT_EQ(shadow->visual_bounds.max_x - shadow->visual_bounds.min_x, 84);
    EXPECT_EQ(shadow->visual_bounds.max_y - shadow->visual_bounds.min_y, 44);

    EXPECT_TRUE(
        std::any_of(result.diagnostics.begin(), result.diagnostics.end(), [](const auto& diag) {
            return diag.object_id == "EMPTY" && diag.kind != d2ar::PrepareDiagnosticKind::Resolved;
        }));

    const auto            keys = d2engine::collect_adventure_render_asset_keys(result.graph);
    std::set<std::string> asset_ids;
    for (const auto& key : keys) {
        asset_ids.insert(key.container_path + "/" + key.image_name);
    }
    EXPECT_TRUE(asset_ids.contains("Imgs/IsoAnim.ff/M1"));
    EXPECT_TRUE(asset_ids.contains("Imgs/IsoAnim.ff/M2"));
    EXPECT_TRUE(asset_ids.contains("Imgs/IsoAnim.ff/MS1"));
    EXPECT_TRUE(asset_ids.contains("Imgs/IsoAnim.ff/MS2"));
    EXPECT_EQ(asset_ids.size(), 4u);
}
