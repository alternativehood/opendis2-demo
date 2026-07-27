#include <d2adventure_render/map_geometry.hpp>
#include <d2adventure_render/map_preparer.hpp>
#include <d2adventure_render/site_contributor.hpp>
#include <d2adventure_render/terrain/site_asset_catalog.hpp>
#include <d2engine/assets/site_asset_catalog_builder.hpp>
#include <d2engine/assets/stack_banner_asset_catalog_builder.hpp>
#include <d2engine/assets/ff_asset_store.hpp>
#include <d2runtime/AdventureWorldBuilder.hpp>
#include <d2scenario/SgParser.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace fs = std::filesystem;
namespace d2ar = d2engine::adventure_render;

namespace {

std::vector<std::uint8_t> read_file(const fs::path& path) {
    if (!fs::is_regular_file(path)) {
        return {};
    }
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) {
        return {};
    }
    const auto size = in.tellg();
    if (size <= 0) {
        return {};
    }
    std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
    in.seekg(0);
    if (!in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size))) {
        return {};
    }
    return data;
}

fs::path sg_path(std::string_view name) {
    return fs::path(OPENDIS2_SOURCE_DIR) / "Downloads" / std::string(name);
}

void expect_static_site(const d2ar::SiteAssetCatalog& catalog, d2runtime::AdventureSiteKind kind,
                        int index, std::string_view expected_sprite) {
    const auto* visual = catalog.find(kind, index);
    ASSERT_NE(visual, nullptr);
    ASSERT_TRUE(std::holds_alternative<d2ar::StaticSiteVisual>(*visual));
    EXPECT_EQ(std::get<d2ar::StaticSiteVisual>(*visual).logical_sprite, expected_sprite);
}

void expect_animated_site(const d2ar::SiteAssetCatalog& catalog, d2runtime::AdventureSiteKind kind,
                          int index, std::string_view expected_animation, std::size_t frame_count,
                          bool has_shadow) {
    const auto* visual = catalog.find(kind, index);
    ASSERT_NE(visual, nullptr);
    ASSERT_TRUE(std::holds_alternative<d2ar::AnimatedSiteVisual>(*visual));
    const auto& animated = std::get<d2ar::AnimatedSiteVisual>(*visual);
    EXPECT_EQ(animated.body.logical_animation, expected_animation);
    EXPECT_EQ(animated.body.animation.frames.size(), frame_count);
    EXPECT_EQ(animated.shadow.has_value(), has_shadow);
    if (animated.shadow.has_value()) {
        EXPECT_EQ(animated.shadow->animation.frames.size(), frame_count);
    }
}

d2ar::StackBannerAssetCatalog make_banner_catalog() {
    return d2engine::detail::build_stack_banner_asset_catalog_from_metadata(
        [](std::string_view, std::string_view) {
            d2engine::AnimationSequence sequence;
            sequence.name = "STACK_BANNER_1400";
            sequence.container_path = "Imgs/IsoCmon.ff";
            sequence.native_canvas_w = 300;
            sequence.native_canvas_h = 200;
            sequence.canvas_foot_x = 10;
            sequence.canvas_foot_y = 20;
            sequence.frames.push_back({.image_name = "TI", .index = 0, .duration_ms = 100});
            return sequence;
        },
        [](std::string_view, std::string_view) {
            struct SpriteMeta {
                int                       canvas_width;
                int                       canvas_height;
                int                       canvas_foot_x;
                int                       canvas_foot_y;
                d2ar::CanvasContentBounds content_bounds;
            };
            return SpriteMeta{300, 200, 10, 20, {1, 2, 3, 4}};
        });
}

} // namespace

TEST(SiteAssetCatalogIntegration, BuildsCatalogFromRealGameData) {
    const char* root_env = std::getenv("DISCIPLES2_GAME_ROOT");
    if (root_env == nullptr || root_env[0] == '\0') {
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";
    }

    d2engine::FfAssetStore store(root_env);
    const auto             catalog = d2engine::build_site_asset_catalog(store);

    expect_static_site(catalog, d2runtime::AdventureSiteKind::Mage, 0, "G000SI0000MAGE00");
    expect_static_site(catalog, d2runtime::AdventureSiteKind::Mage, 1, "G000SI0000MAGE01");
    expect_static_site(catalog, d2runtime::AdventureSiteKind::Mage, 2, "G000SI0000MAGE02");
    expect_static_site(catalog, d2runtime::AdventureSiteKind::Mage, 3, "G000SI0000MAGE03");

    expect_animated_site(catalog, d2runtime::AdventureSiteKind::Merchant, 0, "G000SI0000MERH00", 20,
                         true);
    EXPECT_EQ(
        std::get<d2ar::AnimatedSiteVisual>(*catalog.find(d2runtime::AdventureSiteKind::Merchant, 0))
            .body.container_path,
        "Imgs/IsoAnim.ff");
    expect_static_site(catalog, d2runtime::AdventureSiteKind::Merchant, 1, "G000SI0000MERH01");
    expect_static_site(catalog, d2runtime::AdventureSiteKind::Merchant, 2, "G000SI0000MERH02");
    expect_static_site(catalog, d2runtime::AdventureSiteKind::Merchant, 3, "G000SI0000MERH03");
    expect_static_site(catalog, d2runtime::AdventureSiteKind::Merchant, 4, "G000SI0000MERH04");
    expect_static_site(catalog, d2runtime::AdventureSiteKind::Merchant, 5, "G000SI0000MERH05");
    expect_static_site(catalog, d2runtime::AdventureSiteKind::Merchant, 6, "G000SI0000MERH06");
    expect_animated_site(catalog, d2runtime::AdventureSiteKind::Merchant, 7, "G000SI0000MERH07", 7,
                         false);

    expect_static_site(catalog, d2runtime::AdventureSiteKind::Mercenary, 0, "G000SI0000MERC00");
    expect_static_site(catalog, d2runtime::AdventureSiteKind::Mercenary, 1, "G000SI0000MERC01");
    expect_static_site(catalog, d2runtime::AdventureSiteKind::Mercenary, 2, "G000SI0000MERC02");
    expect_static_site(catalog, d2runtime::AdventureSiteKind::Mercenary, 3, "G000SI0000MERC03");
    expect_static_site(catalog, d2runtime::AdventureSiteKind::Mercenary, 4, "G000SI0000MERC04");
    for (int image = 0; image <= 3; ++image) {
        const auto* visual = catalog.find(d2runtime::AdventureSiteKind::Trainer, image);
        ASSERT_NE(visual, nullptr);
        ASSERT_TRUE(std::holds_alternative<d2ar::StaticSiteVisual>(*visual));
        const auto& static_visual = std::get<d2ar::StaticSiteVisual>(*visual);
        EXPECT_EQ(static_visual.container_path, "Imgs/IsoCmon.ff");
        EXPECT_EQ(static_visual.logical_sprite, "G000SI0000TRAI0" + std::to_string(image));
        EXPECT_GT(static_visual.canvas_width, 0);
        EXPECT_GT(static_visual.canvas_height, 0);
        EXPECT_GE(static_visual.canvas_foot_x, 0);
        EXPECT_GE(static_visual.canvas_foot_y, 0);
    }
    EXPECT_EQ(catalog.find(d2runtime::AdventureSiteKind::Trainer, -1), nullptr);
    EXPECT_EQ(catalog.find(d2runtime::AdventureSiteKind::Trainer, 4), nullptr);
}

TEST(SitePipelineIntegration, TrainerScenarioProducesOneStaticBody) {
    const char* root_env = std::getenv("DISCIPLES2_GAME_ROOT");
    if (root_env == nullptr || root_env[0] == '\0')
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";
    const auto path = sg_path("test_all_terrain_with_bulgaria_half_map.sg");
    if (!fs::is_regular_file(path))
        GTEST_SKIP() << "reference SG file not available: " << path;

    const auto data = read_file(path);
    ASSERT_FALSE(data.empty());
    const auto  scenario = d2scenario::SgParser(data).parse().scenario;
    const auto  result = d2runtime::AdventureWorldBuilder().build(scenario);
    const auto* trainer = result.world.find_site("S143SI0003");
    ASSERT_NE(trainer, nullptr);
    ASSERT_EQ(trainer->kind, d2runtime::AdventureSiteKind::Trainer);
    ASSERT_EQ(trainer->position, (d2runtime::MapCellCoord{21, 14}));
    ASSERT_EQ(trainer->image_iso, 0);

    const auto catalog = d2engine::build_site_asset_catalog(d2engine::FfAssetStore(root_env));
    const auto geometry =
        d2ar::AdventureMapGeometry::from_source(result.world.map_width, result.world.map_height);
    d2ar::AdventureMapPreparer preparer(geometry);
    preparer.add_contributor(d2ar::make_site_contributor(catalog, make_banner_catalog()));
    const auto prepared = preparer.prepare(result.world);

    const auto bodies = std::count_if(
        prepared.graph.world.begin(), prepared.graph.world.end(),
        [](const auto& primitive) { return primitive.debug_label == "Site:S143SI0003:body"; });
    ASSERT_EQ(bodies, 1);
    const auto body = std::find_if(
        prepared.graph.world.begin(), prepared.graph.world.end(),
        [](const auto& primitive) { return primitive.debug_label == "Site:S143SI0003:body"; });
    ASSERT_NE(body, prepared.graph.world.end());
    EXPECT_EQ(body->record_name, "G000SI0000TRAI00");
    EXPECT_FALSE(body->animation.has_value());
    EXPECT_TRUE(std::all_of(prepared.graph.world.begin(), prepared.graph.world.end(),
                            [](const auto& primitive) {
                                return primitive.record_name != "SITE_ICON_TRAI" &&
                                       primitive.record_name != "SITE_ICON_TRAINER";
                            }));
    EXPECT_FALSE(std::any_of(prepared.diagnostics.begin(), prepared.diagnostics.end(),
                             [](const auto& diagnostic) {
                                 return diagnostic.object_id == "S143SI0003" &&
                                        diagnostic.kind != d2ar::PrepareDiagnosticKind::Resolved;
                             }));
}

TEST(SitePipelineIntegration, ManualAcceptanceOnRealSGFiles) {
    const char* root_env = std::getenv("DISCIPLES2_GAME_ROOT");
    if (root_env == nullptr || root_env[0] == '\0') {
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";
    }

    d2engine::FfAssetStore           store(root_env);
    const auto                       catalog = d2engine::build_site_asset_catalog(store);
    d2runtime::AdventureWorldBuilder builder;

    const auto head_path = sg_path("Head of the Beast.sg");
    const auto hellish_path = sg_path("Hellish Invasion.sg");
    const auto hureth_path = sg_path("Hureth Valley.sg");
    if (!fs::is_regular_file(head_path) || !fs::is_regular_file(hellish_path) ||
        !fs::is_regular_file(hureth_path)) {
        GTEST_SKIP() << "reference SG files not available";
    }

    auto load = [&](const fs::path& path) {
        const auto data = read_file(path);
        EXPECT_FALSE(data.empty()) << path.string();
        d2scenario::SgParser parser(data);
        return builder.build(parser.parse().scenario);
    };

    {
        const auto  result = load(head_path);
        const auto* site = result.world.find_site("S139SI0000");
        ASSERT_NE(site, nullptr);
        EXPECT_EQ(site->kind, d2runtime::AdventureSiteKind::Merchant);
        EXPECT_EQ(site->image_iso, 0);
        EXPECT_EQ(std::get<d2ar::AnimatedSiteVisual>(*catalog.find(site->kind, site->image_iso))
                      .body.logical_animation,
                  "G000SI0000MERH00");
        EXPECT_TRUE(std::get<d2ar::AnimatedSiteVisual>(*catalog.find(site->kind, site->image_iso))
                        .shadow.has_value());

        const auto* trainer = result.world.find_site("S139SI0002");
        ASSERT_NE(trainer, nullptr);
        EXPECT_EQ(trainer->kind, d2runtime::AdventureSiteKind::Trainer);
        EXPECT_EQ(trainer->position, (d2runtime::MapCellCoord{16, 2}));
        EXPECT_EQ(trainer->image_iso, 0);
        EXPECT_EQ(std::get<d2ar::StaticSiteVisual>(*catalog.find(trainer->kind, trainer->image_iso))
                      .logical_sprite,
                  "G000SI0000TRAI00");
    }

    {
        const auto  result = load(hellish_path);
        const auto* s1 = result.world.find_site("S001SI0001");
        const auto* s2 = result.world.find_site("S001SI0002");
        const auto* s3 = result.world.find_site("S001SI0003");
        const auto* s4 = result.world.find_site("S001SI0004");
        ASSERT_NE(s1, nullptr);
        ASSERT_NE(s2, nullptr);
        ASSERT_NE(s3, nullptr);
        ASSERT_NE(s4, nullptr);
        const auto* trainer = result.world.find_site("S001SI0000");
        ASSERT_NE(trainer, nullptr);
        EXPECT_EQ(trainer->kind, d2runtime::AdventureSiteKind::Trainer);
        EXPECT_EQ(trainer->position, (d2runtime::MapCellCoord{39, 12}));
        EXPECT_EQ(trainer->image_iso, 2);
        EXPECT_EQ(std::get<d2ar::StaticSiteVisual>(*catalog.find(trainer->kind, trainer->image_iso))
                      .logical_sprite,
                  "G000SI0000TRAI02");
        EXPECT_EQ(
            std::get<d2ar::StaticSiteVisual>(*catalog.find(s1->kind, s1->image_iso)).logical_sprite,
            "G000SI0000MERH04");
        EXPECT_EQ(
            std::get<d2ar::StaticSiteVisual>(*catalog.find(s2->kind, s2->image_iso)).logical_sprite,
            "G000SI0000MERH01");
        EXPECT_EQ(
            std::get<d2ar::StaticSiteVisual>(*catalog.find(s3->kind, s3->image_iso)).logical_sprite,
            "G000SI0000MAGE01");
        EXPECT_EQ(
            std::get<d2ar::StaticSiteVisual>(*catalog.find(s4->kind, s4->image_iso)).logical_sprite,
            "G000SI0000MERH02");
    }

    {
        const auto  result = load(hureth_path);
        const auto* s0 = result.world.find_site("S071SI0000");
        const auto* s1 = result.world.find_site("S071SI0001");
        const auto* s3 = result.world.find_site("S071SI0003");
        const auto* s4 = result.world.find_site("S071SI0004");
        ASSERT_NE(s0, nullptr);
        ASSERT_NE(s1, nullptr);
        ASSERT_NE(s3, nullptr);
        ASSERT_NE(s4, nullptr);
        EXPECT_EQ(
            std::get<d2ar::StaticSiteVisual>(*catalog.find(s0->kind, s0->image_iso)).logical_sprite,
            "G000SI0000MAGE00");
        EXPECT_EQ(
            std::get<d2ar::StaticSiteVisual>(*catalog.find(s1->kind, s1->image_iso)).logical_sprite,
            "G000SI0000MERH01");
        EXPECT_EQ(
            std::get<d2ar::StaticSiteVisual>(*catalog.find(s3->kind, s3->image_iso)).logical_sprite,
            "G000SI0000MERC01");
        EXPECT_EQ(
            std::get<d2ar::StaticSiteVisual>(*catalog.find(s4->kind, s4->image_iso)).logical_sprite,
            "G000SI0000MERH06");
    }
}
