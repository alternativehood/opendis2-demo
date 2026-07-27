#include <d2adventure_render/adventure_render_types.hpp>
#include <d2adventure_render/render_graph.hpp>
#include <d2adventure_render/terrain/capital_asset_catalog.hpp>
#include <d2engine/assets/capital_asset_catalog_builder.hpp>
#include <d2engine/assets/ff_asset_store.hpp>
#include <d2engine/assets/render_graph_asset_collector.hpp>

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace {

std::filesystem::path get_game_root() {
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    const char* env = std::getenv("DISCIPLES2_GAME_ROOT");
    if (env == nullptr || env[0] == '\0')
        return {};
    return env;
}

struct CapitalExpectation {
    const char* race_id;
    const char* animation_name;
    int         frame_count;
    int         canvas_width;
    int         canvas_height;
    int         foot_x;
    int         foot_y;
};

const std::vector<CapitalExpectation> kExpectations = {
    {"g000rr0000", "G000FT0000HU0", 15, 320, 320, 151, 252},
    {"g000rr0001", "G000FT0000DWC0", 15, 320, 320, 161, 249},
    {"g000rr0002", "G000FT0000HE0", 30, 320, 400, 160, 288},
    {"g000rr0003", "G000FT0000UN0", 30, 320, 320, 159, 246},
    {"g000rr0005", "G000FT0000EL0", 30, 320, 320, 160, 254},
};

const std::vector<CapitalExpectation> kRuinedExpectations = {
    {"g000rr0000", "G000FT0000HUC0", 15, 320, 320, 151, 252},
    {"g000rr0002", "G000FT0000HEC0", 15, 320, 400, 160, 288},
    {"g000rr0003", "G000FT0000UNC0", 15, 320, 320, 159, 246},
    {"g000rr0005", "G000FT0000ELC0", 30, 320, 320, 160, 254},
};

} // namespace

TEST(CapitalAssetCatalogIntegration, BuildsCatalogFromRealGameData) {
    const auto game_root = get_game_root();
    if (game_root.empty())
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";

    d2engine::FfAssetStore store(game_root);
    const auto             catalog = d2engine::build_capital_asset_catalog(store);

    EXPECT_EQ(catalog.visuals.size(), kExpectations.size());

    for (const auto& expected : kExpectations) {
        const auto& visual = catalog.resolve(
            expected.race_id, d2engine::adventure_render::CapitalVisualState::Active);
        EXPECT_EQ(visual.container_path, "Imgs/IsoAnim.ff");
        EXPECT_EQ(visual.logical_animation_name, expected.animation_name);
        EXPECT_TRUE(visual.animation_data.is_looping);
        EXPECT_EQ(visual.animation_data.timing_source,
                  d2engine::adventure_render::AdventureAnimationTimingSource::ProvisionalFallback);
        EXPECT_EQ(visual.animation_data.frames.size(),
                  static_cast<std::size_t>(expected.frame_count));
        EXPECT_EQ(visual.canvas_foot_x, expected.foot_x);
        EXPECT_EQ(visual.canvas_foot_y, expected.foot_y);

        ASSERT_FALSE(visual.animation_data.frames.empty());
        const auto& first = visual.animation_data.frames.front();
        EXPECT_EQ(first.canvas_width, expected.canvas_width);
        EXPECT_EQ(first.canvas_height, expected.canvas_height);

        for (const auto& frame : visual.animation_data.frames) {
            EXPECT_GT(frame.canvas_width, 0) << expected.animation_name << " " << frame.record_name;
            EXPECT_GT(frame.canvas_height, 0)
                << expected.animation_name << " " << frame.record_name;
            EXPECT_EQ(frame.duration_ms, 100)
                << expected.animation_name << " " << frame.record_name;
        }
    }
}

TEST(CapitalAssetCatalogIntegration, CapitalAnimationFramesEnterRenderAssetCollector) {
    const auto game_root = get_game_root();
    if (game_root.empty())
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";

    d2engine::FfAssetStore store(game_root);
    const auto             catalog = d2engine::build_capital_asset_catalog(store);

    d2engine::adventure_render::PreparedAdventureRenderGraph graph;
    for (const auto& expected : kExpectations) {
        const auto& visual = catalog.resolve(
            expected.race_id, d2engine::adventure_render::CapitalVisualState::Active);
        d2engine::adventure_render::PreparedAdventureRenderPrimitive prim;
        prim.phase = d2engine::adventure_render::AdventureRenderPhase::World;
        prim.container_path = visual.container_path;
        prim.record_name = visual.animation_data.frames.front().record_name;
        prim.animation = visual.animation_data;
        prim.footprint.push_back({0, 0});
        prim.depth_anchor = {0, 0};
        graph.world.push_back(std::move(prim));
    }

    const auto            keys = d2engine::collect_adventure_render_asset_keys(graph);
    std::set<std::string> asset_ids;
    for (const auto& key : keys) {
        asset_ids.insert(key.container_path + "/" + key.image_name);
    }

    std::size_t expected_key_count = 0;
    for (const auto& expected : kExpectations) {
        const auto& visual = catalog.resolve(
            expected.race_id, d2engine::adventure_render::CapitalVisualState::Active);
        expected_key_count += visual.animation_data.frames.size();
        for (const auto& frame : visual.animation_data.frames) {
            EXPECT_TRUE(asset_ids.contains(visual.container_path + "/" + frame.record_name))
                << visual.logical_animation_name << " missing frame " << frame.record_name;
        }
    }

    EXPECT_EQ(keys.size(), expected_key_count);
}

TEST(CapitalAssetCatalogIntegration, BuildsRuinedCapitalAnimationsFromRealGameData) {
    const auto game_root = get_game_root();
    if (game_root.empty())
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";

    d2engine::FfAssetStore store(game_root);
    const auto             catalog = d2engine::build_capital_asset_catalog(store);

    for (const auto& expected : kRuinedExpectations) {
        const auto& visual = catalog.resolve(
            expected.race_id, d2engine::adventure_render::CapitalVisualState::Ruined);
        EXPECT_EQ(visual.logical_animation_name, expected.animation_name);
        EXPECT_EQ(visual.animation_data.frames.size(),
                  static_cast<std::size_t>(expected.frame_count));
    }

    EXPECT_THROW(static_cast<void>(catalog.resolve(
                     "g000rr0001", d2engine::adventure_render::CapitalVisualState::Ruined)),
                 std::runtime_error);
}
