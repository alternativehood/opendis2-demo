#include <gtest/gtest.h>

#include <d2adventure_render/map_geometry.hpp>
#include <d2adventure_render/map_preparer.hpp>
#include <d2adventure_render/resource_node_contributor.hpp>
#include <d2adventure_render/terrain/resource_node_asset_catalog.hpp>
#include <d2engine/assets/image_asset_key.hpp>
#include <d2engine/assets/render_graph_asset_collector.hpp>
#include <d2engine/assets/resource_node_asset_catalog_builder.hpp>
#include <d2engine/assets/ff_asset_store.hpp>
#include <d2runtime/AdventureWorldBuilder.hpp>
#include <d2runtime/AdventureWorldState.hpp>
#include <d2scenario/SgParser.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

using namespace d2runtime;
namespace fs = std::filesystem;

static fs::path find_sg_file() {
    const char* adventure_env = std::getenv("OPENDIS2_ADVENTURE_TEST_SG");
    if (adventure_env != nullptr && adventure_env[0] != '\0') {
        fs::path p(adventure_env);
        if (fs::is_regular_file(p))
            return p;
    }
    const char* game_root_env = std::getenv("DISCIPLES2_GAME_ROOT");
    if (game_root_env != nullptr && game_root_env[0] != '\0') {
        const auto from_game =
            fs::path(game_root_env) / "Exports" / "test_all_terrain_with_bulgaria_half_map.sg";
        if (fs::is_regular_file(from_game))
            return from_game;
    }
    const auto from_repo = fs::path(OPENDIS2_SOURCE_DIR) / "testdata" / "test_map.sg";
    if (fs::is_regular_file(from_repo))
        return from_repo;
    return {};
}

// ── Real SG integration test ────────────────────────────────────────────

struct ExpectedResourceNode {
    const char*           id;
    int                   pos_x;
    int                   pos_y;
    int                   raw_resource;
    AdventureResourceKind kind;
    int                   ai_priority;
    const char*           raw_type;
    const char*           owner;
    bool                  is_static_visual;
    const char*           expected_container;
    const char*           expected_logical_id;
};

static const ExpectedResourceNode kExpectedNodes[] = {
    {"S143CR0005", 19, 1, 0, AdventureResourceKind::GoldMine, 3, "", "", true, "Imgs/IsoCmon.ff",
     "G000CR0000GL"},
    {"S143CR0000", 19, 4, 1, AdventureResourceKind::RedMana, 3, "", "", false, "Imgs/IsoAnim.ff",
     "G000CR0000RD"},
    {"S143CR0003", 22, 7, 2, AdventureResourceKind::YellowMana, 3, "", "", false, "Imgs/IsoAnim.ff",
     "G000CR0000YE"},
    {"S143CR0002", 25, 7, 3, AdventureResourceKind::OrangeMana, 3, "", "", false, "Imgs/IsoAnim.ff",
     "G000CR0000RG"},
    {"S143CR0001", 23, 3, 4, AdventureResourceKind::WhiteMana, 3, "", "", false, "Imgs/IsoAnim.ff",
     "G000CR0000WH"},
    {"S143CR0004", 25, 0, 5, AdventureResourceKind::BlueMana, 3, "", "", false, "Imgs/IsoAnim.ff",
     "G000CR0000GR"},
};

TEST(ResourceNodeRealScenario, ParsesSixExactNodes) {
    const auto sg_path = find_sg_file();
    if (sg_path.empty())
        GTEST_SKIP() << "test SG fixture not found — set OPENDIS2_ADVENTURE_TEST_SG or "
                        "DISCIPLES2_GAME_ROOT";

    const bool env_set = (std::getenv("OPENDIS2_ADVENTURE_TEST_SG") != nullptr &&
                          std::getenv("OPENDIS2_ADVENTURE_TEST_SG")[0] != '\0');

    std::ifstream in(sg_path, std::ios::binary | std::ios::ate);
    if (!in) {
        if (env_set)
            FAIL() << "OPENDIS2_ADVENTURE_TEST_SG set but cannot open: " << sg_path;
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

    auto& nodes = build_result.world.resource_nodes;

    if (env_set) {
        ASSERT_EQ(nodes.size(), 6u)
            << "OPENDIS2_ADVENTURE_TEST_SG: expected exactly 6 resource nodes, got "
            << nodes.size();
    } else {
        if (nodes.size() < 6)
            GTEST_SKIP() << "SG file has fewer than 6 resource nodes";
        ASSERT_EQ(nodes.size(), 6u) << "expected exactly 6 resource nodes";
    }

    for (const auto& exp : kExpectedNodes) {
        const auto* node = build_result.world.find_resource_node(exp.id);
        ASSERT_NE(node, nullptr) << "missing resource node " << exp.id;
        EXPECT_EQ(node->position.x, exp.pos_x) << exp.id;
        EXPECT_EQ(node->position.y, exp.pos_y) << exp.id;
        EXPECT_EQ(node->raw_resource, exp.raw_resource) << exp.id;
        EXPECT_EQ(node->resource_kind, exp.kind) << exp.id;
        EXPECT_EQ(node->ai_priority, exp.ai_priority) << exp.id;
        EXPECT_EQ(node->raw_type, exp.raw_type) << exp.id << " type";
        EXPECT_EQ(node->owner, exp.owner) << exp.id << " owner";
        ASSERT_EQ(node->footprint.size(), 1u) << exp.id << " footprint";
        EXPECT_EQ(node->footprint[0].x, exp.pos_x) << exp.id;
        EXPECT_EQ(node->footprint[0].y, exp.pos_y) << exp.id;
    }

    for (const auto& d : build_result.diagnostics) {
        if (d.kind == d2runtime::BuildDiagnosticKind::UnsupportedResourceNodeValue ||
            d.kind == d2runtime::BuildDiagnosticKind::ResourceNodePlanFootprintMismatch) {
            ADD_FAILURE() << "unexpected diagnostic: " << d.message;
        }
    }

    // ── Render pipeline test (requires DISCIPLES2_GAME_ROOT) ───────────────
    const char* game_root_env = std::getenv("DISCIPLES2_GAME_ROOT");
    if (game_root_env == nullptr || game_root_env[0] == '\0') {
        if (env_set) {
            // SG loaded but no game root — skip render pipeline
        } else {
            GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set — skipping render pipeline test";
        }
    } else {
        d2engine::FfAssetStore store(game_root_env);
        const auto             catalog = d2engine::build_resource_node_asset_catalog(store);

        const auto geometry = d2engine::adventure_render::AdventureMapGeometry::from_source(
            build_result.world.map_width, build_result.world.map_height);

        d2engine::adventure_render::AdventureMapPreparer preparer(geometry);
        preparer.add_contributor(
            d2engine::adventure_render::make_resource_node_contributor(catalog));
        auto render_result = preparer.prepare(build_result.world);

        // Exactly 6 resource-node primitives
        ASSERT_EQ(render_result.graph.world.size(), 6u);

        // No pick entries
        EXPECT_EQ(render_result.pick_entries.size(), 0u);

        // Verify each expected node's primitive
        int static_count = 0;
        int animated_count = 0;
        for (const auto& exp : kExpectedNodes) {
            auto stable =
                d2engine::adventure_render::stable_render_id(std::string("ResourceNode:") + exp.id);
            bool found = false;
            for (const auto& prim : render_result.graph.world) {
                if (prim.stable_id == stable) {
                    found = true;
                    EXPECT_EQ(prim.level, d2engine::adventure_render::WorldRenderLevel::Structure);
                    EXPECT_EQ(prim.container_path, exp.expected_container) << exp.id;
                    EXPECT_TRUE(prim.src_width > 0) << exp.id;
                    EXPECT_TRUE(prim.src_height > 0) << exp.id;

                    if (exp.is_static_visual) {
                        EXPECT_FALSE(prim.animation.has_value()) << exp.id << " should be static";
                        EXPECT_EQ(prim.record_name, exp.expected_logical_id) << exp.id;
                        ++static_count;
                    } else {
                        ASSERT_TRUE(prim.animation.has_value()) << exp.id << " should be animated";
                        EXPECT_EQ(prim.animation->animation_name, exp.expected_logical_id)
                            << exp.id;
                        EXPECT_TRUE(prim.animation->is_looping) << exp.id;
                        EXPECT_GT(prim.animation->frames.size(), 0u) << exp.id;
                        EXPECT_EQ(prim.container_path, "Imgs/IsoAnim.ff") << exp.id;
                        ++animated_count;
                    }
                    break;
                }
            }
            EXPECT_TRUE(found) << "no primitive for " << exp.id;
        }
        EXPECT_EQ(static_count, 1);
        EXPECT_EQ(animated_count, 5);

        // Preload keys: 1 IsoCmon + sum of all animation frames
        auto keys = d2engine::collect_adventure_render_asset_keys(render_result.graph);
        int  iso_cmon_keys = 0;
        int  iso_anim_keys = 0;
        int  iso_still_keys = 0;
        for (const auto& k : keys) {
            if (k.container_path == "Imgs/IsoCmon.ff")
                ++iso_cmon_keys;
            else if (k.container_path == "Imgs/IsoAnim.ff")
                ++iso_anim_keys;
            else if (k.container_path == "Imgs/IsoStill.ff")
                ++iso_still_keys;
        }
        EXPECT_EQ(iso_cmon_keys, 1) << "exactly 1 IsoCmon key (GoldMine static sprite)";
        EXPECT_EQ(iso_still_keys, 0) << "zero IsoStill keys for resource nodes";

        // No mana identity requested as IsoCmon static
        for (const auto& k : keys) {
            if (k.container_path == "Imgs/IsoCmon.ff") {
                EXPECT_EQ(k.image_name, "G000CR0000GL")
                    << "only GoldMine sprite should be in IsoCmon keys; got " << k.image_name;
            }
        }

        // Known frame counts: 35 + 35 + 35 + 35 + 25 = 165 IsoAnim frames + 1 IsoCmon = 166
        EXPECT_EQ(iso_anim_keys, 165) << "expected 165 IsoAnim frame keys (35+35+35+35+25)";
        EXPECT_EQ(static_cast<int>(keys.size()), 166)
            << "total keys = 1 (IsoCmon) + 165 (IsoAnim) = 166";
    }
}
