#include <gtest/gtest.h>

#include <d2adventure_render/city_contributor.hpp>
#include <d2adventure_render/map_geometry.hpp>
#include <d2adventure_render/map_preparer.hpp>
#include <d2adventure_render/terrain/city_asset_catalog.hpp>
#include <d2engine/assets/city_asset_catalog_builder.hpp>
#include <d2engine/assets/ff_asset_store.hpp>
#include <d2engine/assets/render_graph_asset_collector.hpp>
#include <d2runtime/AdventureWorldBuilder.hpp>
#include <d2scenario/SgParser.hpp>

#include <cstdlib>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
namespace d2ar = d2engine::adventure_render;

namespace {

fs::path find_sg_file() {
    const char* env = std::getenv("OPENDIS2_ADVENTURE_TEST_SG");
    if (env != nullptr && env[0] != '\0') {
        fs::path p(env);
        if (fs::is_regular_file(p)) {
            return p;
        }
    }
    const auto repo = fs::path(OPENDIS2_SOURCE_DIR) / "testdata" / "test_map.sg";
    if (fs::is_regular_file(repo)) {
        return repo;
    }
    return {};
}

d2ar::AnimatedCityLayer make_layer(std::string animation_name, int foot_x, int foot_y, int native_w,
                                   int                                                  native_h,
                                   std::initializer_list<d2ar::AdventureAnimationFrame> frames) {
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
        layer.animation.frames.push_back(frame);
    }
    return layer;
}

} // namespace

TEST(CityPipelineIntegration, TestMapRegressionRendersOnlyMidVillage) {
    const auto sg_path = find_sg_file();
    if (sg_path.empty()) {
        GTEST_SKIP() << "test_map.sg not found";
    }

    std::ifstream in(sg_path, std::ios::binary | std::ios::ate);
    if (!in) {
        GTEST_SKIP() << "cannot open SG file: " << sg_path;
    }

    const auto           size = in.tellg();
    std::vector<uint8_t> data(static_cast<std::size_t>(size));
    in.seekg(0);
    in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));

    d2scenario::SgParser parser(data);
    const auto           parse_result = parser.parse();

    d2runtime::AdventureWorldBuilder builder;
    const auto                       build_result = builder.build(parse_result.scenario);

    auto parsed_village =
        std::find_if(parse_result.scenario.cities.begin(), parse_result.scenario.cities.end(),
                     [](const auto& city) { return city.id == "S143FT0001"; });
    ASSERT_NE(parsed_village, parse_result.scenario.cities.end());
    EXPECT_EQ(parsed_village->kind, "MidVillage");
    EXPECT_EQ(parsed_village->size, 1);

    const auto* village = build_result.world.find_city("S143FT0001");
    ASSERT_NE(village, nullptr);
    EXPECT_EQ(village->size, 1);

    auto city_object =
        std::find_if(build_result.world.map_objects.begin(), build_result.world.map_objects.end(),
                     [](const auto& mo) { return mo.id == "S143FT0001"; });
    ASSERT_NE(city_object, build_result.world.map_objects.end());
    EXPECT_EQ(city_object->kind, d2runtime::AdventureMapObjectKind::City);

    auto capital_object =
        std::find_if(build_result.world.map_objects.begin(), build_result.world.map_objects.end(),
                     [](const auto& mo) { return mo.id == "S143FT0000"; });
    ASSERT_NE(capital_object, build_result.world.map_objects.end());
    EXPECT_EQ(capital_object->kind, d2runtime::AdventureMapObjectKind::Capital);

    const auto* capital = build_result.world.find_city("S143FT0000");
    EXPECT_EQ(capital, nullptr);

    d2ar::CityAssetCatalog catalog;
    catalog.visuals[0] = d2ar::CityVisual{
        .body = make_layer("G000FT0000NE1", 10, 20, 320, 320, {{"NE1F0", 100, 64, 64}})};
    catalog.visuals[1] =
        d2ar::CityVisual{.body = make_layer("G000FT0000NE2", 14, 24, 320, 320,
                                            {{"NE2F0", 120, 80, 72}, {"NE2F1", 100, 80, 72}}),
                         .shadow = make_layer("G000FT0000NE2S", 18, 28, 320, 320,
                                              {{"NE2SF0", 120, 80, 40}, {"NE2SF1", 100, 80, 40}})};
    catalog.visuals[2] = d2ar::CityVisual{
        .body = make_layer("G000FT0000NE3", 13, 23, 320, 320, {{"NE3F0", 110, 90, 90}})};

    const auto geometry = d2ar::AdventureMapGeometry::from_source(build_result.world.map_width,
                                                                  build_result.world.map_height);
    d2ar::AdventureMapPreparer preparer(geometry);
    preparer.add_contributor(d2ar::make_city_contributor(catalog));

    const auto render_result = preparer.prepare(build_result.world);

    ASSERT_EQ(render_result.graph.world.size(), 1u);
    const auto& prim = render_result.graph.world.front();
    EXPECT_EQ(prim.debug_label, "City:S143FT0001:body");
    ASSERT_TRUE(prim.animation.has_value());
    ASSERT_FALSE(prim.animation->frames.empty());
    EXPECT_EQ(prim.animation->frames[0].record_name, "NE1F0");
    EXPECT_EQ(prim.level, d2ar::WorldRenderLevel::Structure);
    EXPECT_EQ(prim.phase, d2ar::AdventureRenderPhase::World);
    EXPECT_EQ(prim.footprint, village->footprint);
}

TEST(CityAssetCatalogIntegration, BuildsCatalogFromRealGameData) {
    const char* root_env = std::getenv("DISCIPLES2_GAME_ROOT");
    if (root_env == nullptr || root_env[0] == '\0') {
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";
    }

    d2engine::FfAssetStore store(root_env);
    const auto             catalog = d2engine::build_city_asset_catalog(store);

    EXPECT_EQ(catalog.resolve_for_size(1).body.container_path, "Imgs/IsoAnim.ff");
    EXPECT_EQ(catalog.resolve_for_size(1).body.logical_animation, "G000FT0000NE1");
    EXPECT_EQ(catalog.resolve_for_size(2).body.logical_animation, "G000FT0000NE2");
    ASSERT_TRUE(catalog.resolve_for_size(2).shadow.has_value());
    EXPECT_EQ(catalog.resolve_for_size(2).shadow->logical_animation, "G000FT0000NE2S");
    EXPECT_EQ(catalog.resolve_for_size(3).body.logical_animation, "G000FT0000NE3");
    EXPECT_FALSE(catalog.resolve_for_size(1).shadow.has_value());
    EXPECT_FALSE(catalog.resolve_for_size(3).shadow.has_value());
    EXPECT_THROW({ static_cast<void>(catalog.resolve_for_size(0)); }, std::runtime_error);
    EXPECT_THROW({ static_cast<void>(catalog.resolve_for_size(6)); }, std::runtime_error);
}
