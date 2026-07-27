#include <gtest/gtest.h>

#include "d2engine/assets/asset_runtime.hpp"
#include "d2engine/assets/ff_asset_store.hpp"
#include "d2engine/assets/game_data_registry.hpp"
#include "d2engine/assets/asset_runtime_catalog_adapter.hpp"
#include "d2engine/assets/iso_actor_visual_resolver.hpp"
#include "d2engine/assets/adventure_stack_actor_request_resolver.hpp"
#include "d2engine/app/adventure_animation_helpers.hpp"
#include "d2engine/app/adventure_interaction_mask.hpp"
#include "d2engine/assets/render_graph_asset_collector.hpp"
#include "d2engine/assets/image_asset_key.hpp"

#include <d2adventure_render/map_geometry.hpp>
#include <d2adventure_render/map_preparer.hpp>
#include <d2adventure_render/prepared_adventure_map.hpp>
#include <d2runtime/AdventureWorldState.hpp>
#include <d2runtime/AdventureTerrainDecoder.hpp>
#include <d2runtime/AdventureStackPresentationResolver.hpp>
#include <d2runtime/MovementCapabilities.hpp>

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#ifndef DISCIPLES2_GAME_ROOT
#define DISCIPLES2_GAME_ROOT ""
#endif

namespace {

namespace ar = d2engine::adventure_render;

static std::filesystem::path game_root() {
    const char* env = std::getenv("DISCIPLES2_GAME_ROOT"); // NOLINT
    return (env != nullptr && env[0] != '\0') ? std::filesystem::path(env)
                                              : std::filesystem::path(DISCIPLES2_GAME_ROOT);
}

std::optional<ar::AdventureActorVisual>
iso_to_adventure_visual(const std::optional<d2engine::IsoActorVisual>& iso) {
    if (!iso.has_value())
        return std::nullopt;
    ar::AdventureActorVisual result;
    result.presentation_kind = iso->presentation_kind;
    result.resolved_owner_id = iso->resolved_owner_id;
    result.body.container_path = iso->body.container_path;
    result.body.logical_animation_name = iso->body.animation_name;
    for (const auto& f : iso->body.frames) {
        result.body.frames.push_back({f.record_name, f.canvas_width, f.canvas_height});
    }
    result.body.native_canvas_w = iso->body.native_canvas_w;
    result.body.native_canvas_h = iso->body.native_canvas_h;
    result.body.canvas_foot_x = iso->body.canvas_foot_x;
    result.body.canvas_foot_y = iso->body.canvas_foot_y;
    if (iso->shadow.has_value()) {
        ar::AdventureActorVisualLayer sl;
        sl.container_path = iso->shadow->container_path;
        sl.logical_animation_name = iso->shadow->animation_name;
        for (const auto& f : iso->shadow->frames) {
            sl.frames.push_back({f.record_name, f.canvas_width, f.canvas_height});
        }
        sl.native_canvas_w = iso->shadow->native_canvas_w;
        sl.native_canvas_h = iso->shadow->native_canvas_h;
        sl.canvas_foot_x = iso->shadow->canvas_foot_x;
        sl.canvas_foot_y = iso->shadow->canvas_foot_y;
        result.shadow = std::move(sl);
    }
    return result;
}

// Scan ground-bound units to find a race meeting a predicate on BOAT/SBOA availability.
// Returns the first UnitDef whose race qualifies, or nullptr.
// Does NOT depend on Gunits.dbf iteration order — scans all units.
struct BoatRaceScanResult {
    const d2engine::UnitDef* candidate = nullptr;
    std::string              race_upper;
    std::string              sboa_anim_name; // SBOA0 for this race
};

BoatRaceScanResult find_boat_race(const d2engine::GameDataRegistry&      game_data,
                                  const std::unordered_set<std::string>& anim_set,
                                  bool                                   require_sboa) {
    BoatRaceScanResult result;
    for (const auto& u : game_data.all_units()) {
        if (u.water_only)
            continue;
        if (u.race_id.empty())
            continue;
        const auto movement =
            d2runtime::MovementCapabilities::from_native_ability_ids(u.native_ability_ids);
        if (movement.can_natively_traverse(d2runtime::AdventureGroundType::Water))
            continue;
        std::string race_upper = u.race_id;
        for (auto& c : race_upper)
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if (!anim_set.contains(race_upper + "BOAT0"))
            continue;
        const bool has_sboa = anim_set.contains(race_upper + "SBOA0");
        if (require_sboa && !has_sboa)
            continue;
        if (!require_sboa && has_sboa)
            continue;
        result.candidate = &u;
        result.race_upper = std::move(race_upper);
        result.sboa_anim_name = race_upper + "SBOA0";
        break;
    }
    return result;
}

// Build a minimal world with one stack on water at (5,5).
struct WaterStackFixture {
    d2runtime::AdventureWorldState   world;
    std::string                      stack_id;
    d2runtime::AdventureIsoDirection direction = d2runtime::AdventureIsoDirection::D0;
};

WaterStackFixture make_water_stack(const d2engine::UnitDef& candidate) {
    WaterStackFixture f;
    f.stack_id = "TEST_BOAT_STACK";
    f.world.map_width = 10;
    f.world.map_height = 10;
    f.world.terrain.width = 10;
    f.world.terrain.height = 10;
    f.world.terrain.tiles.resize(100);
    // tile (5,5) water: low_byte=7 maps to AdventureTerrainMaterial::Water
    f.world.terrain.tiles[5 * 10 + 5].raw_value = 7;

    const std::string unit_id = "TEST_BOAT_UNIT";
    const std::string subrace_id = "TEST_BOAT_SUBRACE";

    d2runtime::AdventureUnitInstance inst;
    inst.id = unit_id;
    inst.type_id = candidate.unit_id;
    f.world.units.push_back(inst);

    d2runtime::AdventureSubraceRef subrace;
    subrace.id = subrace_id;
    subrace.race_id = candidate.race_id;
    f.world.subraces.push_back(std::move(subrace));

    d2runtime::AdventureStack stack;
    stack.id = f.stack_id;
    stack.leader_id = unit_id;
    stack.subrace = subrace_id;
    stack.facing = f.direction;
    stack.position.x = 5;
    stack.position.y = 5;
    f.world.stacks.push_back(stack);

    return f;
}

} // namespace

// ======================================================================
// Test A: race with visible BOAT0 + SBOA0 → 2 primitives
// ======================================================================

TEST(BoatWaterStack, VisibleShadowRace) {
    const auto root = game_root();
    const auto globals_path = root / "Globals";
    const auto iso_path = root / "Imgs/Isounit.ff";
    if (root.empty() || !std::filesystem::exists(globals_path) ||
        !std::filesystem::exists(iso_path))
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set or Globals/Isounit.ff not found";

    d2engine::AssetRuntime               assets(root, 1);
    d2engine::GameDataRegistry           game_data(globals_path);
    d2engine::AssetRuntimeCatalogAdapter catalog(assets);

    const auto                            anim_list = catalog.animations_in("Imgs/Isounit.ff");
    const std::unordered_set<std::string> anim_set(anim_list.begin(), anim_list.end());

    // Find a race with BOAT0 + SBOA0
    auto scan = find_boat_race(game_data, anim_set, true);
    ASSERT_NE(scan.candidate, nullptr)
        << "No ground-bound race with visible BOAT0+SBOA0 found in real game data";

    // Build world + resolve once
    auto fix = make_water_stack(*scan.candidate);

    d2engine::AdventureStackActorRequestResolver req_resolver(game_data);
    const auto req = req_resolver.resolve(fix.world, fix.world.stacks[0]);
    ASSERT_EQ(req.presentation.kind, d2runtime::AdventureActorPresentationKind::Boat);

    d2engine::IsoActorVisualResolver iso_resolver(catalog, game_data);
    const auto                       iso_visual = iso_resolver.resolve(req);
    ASSERT_TRUE(iso_visual.has_value());
    ASSERT_TRUE(iso_visual->shadow.has_value())
        << "race " << scan.race_upper << " should have visible SBOA";

    // Store resolved visual by stack id — one resolve per stack
    std::unordered_map<std::string, ar::AdventureActorVisual> visual_cache;
    visual_cache[fix.stack_id] = *iso_to_adventure_visual(iso_visual);

    const auto               geometry = ar::AdventureMapGeometry::from_source(10, 10);
    ar::AdventureMapPreparer preparer(geometry);
    preparer.add_contributor(ar::make_stack_actor_contributor(
        [&](const d2runtime::AdventureStack& s,
            const d2runtime::AdventureUnitInstance&) -> std::optional<ar::AdventureActorVisual> {
            auto it = visual_cache.find(s.id);
            return it != visual_cache.end() ? std::optional(it->second) : std::nullopt;
        }));
    const auto result = preparer.prepare(fix.world);

    // Visible SBOA: 2 primitives, body + shadow
    EXPECT_EQ(result.graph.world.size(), 2u) << "visible SBOA race should have body+shadow";
    EXPECT_EQ(result.pick_entries.size(), 1u);

    const ar::PreparedAdventureRenderPrimitive* body = nullptr;
    const ar::PreparedAdventureRenderPrimitive* shadow = nullptr;
    for (const auto& prim : result.graph.world) {
        if (prim.stable_id == ar::stable_render_id("Stack:" + fix.stack_id))
            body = &prim;
        if (prim.stable_id == ar::stable_render_id("StackShadow:" + fix.stack_id))
            shadow = &prim;
    }
    ASSERT_NE(body, nullptr);
    ASSERT_NE(shadow, nullptr);
    EXPECT_FLOAT_EQ(shadow->alpha, 0.5f);
    EXPECT_TRUE(shadow->animation_sync_source_id.has_value());
    EXPECT_EQ(*shadow->animation_sync_source_id, body->stable_id);

    auto players = d2engine::build_adventure_animation_players(result.graph);
    EXPECT_EQ(players.size(), 1u);
    EXPECT_TRUE(players.contains(body->stable_id));

    GTEST_LOG_(INFO) << "Visible-shadow race: " << scan.race_upper;
}

// ======================================================================
// Test B: race with BOAT0 but no SBOA0 → 1 primitive, no generated shadow
// ======================================================================

TEST(BoatWaterStack, ShadowlessRace) {
    const auto root = game_root();
    const auto globals_path = root / "Globals";
    const auto iso_path = root / "Imgs/Isounit.ff";
    if (root.empty() || !std::filesystem::exists(globals_path) ||
        !std::filesystem::exists(iso_path))
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set or Globals/Isounit.ff not found";

    d2engine::AssetRuntime               assets(root, 1);
    d2engine::GameDataRegistry           game_data(globals_path);
    d2engine::AssetRuntimeCatalogAdapter catalog(assets);

    const auto                            anim_list = catalog.animations_in("Imgs/Isounit.ff");
    const std::unordered_set<std::string> anim_set(anim_list.begin(), anim_list.end());

    // Find a race with BOAT0 but no SBOA0
    auto scan = find_boat_race(game_data, anim_set, false);
    ASSERT_NE(scan.candidate, nullptr)
        << "No ground-bound race with BOAT0 but no SBOA0 found in real game data";

    // Build world + resolve once
    auto fix = make_water_stack(*scan.candidate);

    d2engine::AdventureStackActorRequestResolver req_resolver(game_data);
    const auto req = req_resolver.resolve(fix.world, fix.world.stacks[0]);
    ASSERT_EQ(req.presentation.kind, d2runtime::AdventureActorPresentationKind::Boat);

    d2engine::IsoActorVisualResolver iso_resolver(catalog, game_data);
    const auto                       iso_visual = iso_resolver.resolve(req);
    ASSERT_TRUE(iso_visual.has_value());
    EXPECT_FALSE(iso_visual->shadow.has_value())
        << "race " << scan.race_upper << " should have no SBOA shadow";

    // Store resolved visual by stack id
    std::unordered_map<std::string, ar::AdventureActorVisual> visual_cache;
    visual_cache[fix.stack_id] = *iso_to_adventure_visual(iso_visual);

    const auto               geometry = ar::AdventureMapGeometry::from_source(10, 10);
    ar::AdventureMapPreparer preparer(geometry);
    preparer.add_contributor(ar::make_stack_actor_contributor(
        [&](const d2runtime::AdventureStack& s,
            const d2runtime::AdventureUnitInstance&) -> std::optional<ar::AdventureActorVisual> {
            auto it = visual_cache.find(s.id);
            return it != visual_cache.end() ? std::optional(it->second) : std::nullopt;
        }));
    const auto result = preparer.prepare(fix.world);

    // No SBOA: 1 primitive, body only, no shadow
    EXPECT_EQ(result.graph.world.size(), 1u)
        << "shadowless race should have exactly 1 primitive (body only)";
    EXPECT_EQ(result.pick_entries.size(), 1u);

    const ar::PreparedAdventureRenderPrimitive* body = nullptr;
    const ar::PreparedAdventureRenderPrimitive* shadow = nullptr;
    for (const auto& prim : result.graph.world) {
        if (prim.stable_id == ar::stable_render_id("Stack:" + fix.stack_id))
            body = &prim;
        if (prim.stable_id == ar::stable_render_id("StackShadow:" + fix.stack_id))
            shadow = &prim;
    }
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(shadow, nullptr) << "shadowless race must not produce a shadow primitive";

    auto players = d2engine::build_adventure_animation_players(result.graph);
    EXPECT_EQ(players.size(), 1u);
    EXPECT_TRUE(players.contains(body->stable_id));

    GTEST_LOG_(INFO) << "Shadowless race: " << scan.race_upper;
}
