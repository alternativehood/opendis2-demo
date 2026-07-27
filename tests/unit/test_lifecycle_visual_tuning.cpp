#include <gtest/gtest.h>

#include "render_batch_test_helpers.hpp"

#include "d2engine/app/battle_tuning_state.hpp"
#include "d2engine/battle_view/battle_renderer.hpp"
#include "d2engine/battle_view/battle_render_snapshot.hpp"
#include "d2engine/battle_view/battle_render_tree_contract.hpp"
#include "d2engine/battle_view/debug_renderable_item.hpp"
#include "d2engine/battle_view/render_placement.hpp"
#include "d2engine/render/render_tree.hpp"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

namespace d2engine {
namespace {

class FakeProvider final : public IBattleTextureProvider {
public:
    BackendTextureRef get_texture(const std::string& /*container_path*/,
                                  const std::string& /*image_name*/) override {
        return BackendTextureRef{.native = reinterpret_cast<void*>(static_cast<std::uintptr_t>(1))};
    }
    [[nodiscard]] std::pair<float, float>
    texture_size(BackendTextureRef /*texture*/) const override {
        return {32.f, 48.f};
    }
};

SnapshotTrack lifecycle_track(TrackKind kind, std::string profile_id,
                              BindingRole role = BindingRole::Global) {
    SnapshotTrack t;
    t.kind = kind;
    t.layer = (kind == TrackKind::DeathBody) ? TrackRenderLayer::Base : TrackRenderLayer::Overlay;
    t.placement = render_placement_for(t.layer, AnchorPolicy::UnitFoot);
    t.lifecycle_profile_id = std::move(profile_id);
    t.effect_role = role;
    t.current_frame_name = "frame";
    t.container_path = "Imgs/Test.ff";
    return t;
}

SnapshotEntity entity_with(SnapshotTrack t) {
    return {.id = VisualEntityId{1},
            .coord = {.side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Front},
            .tracks = {std::move(t)}};
}

BattleRenderOptions no_bg() {
    static const TreeLayout tree = [] {
        TreeLayout t;
        for (const BattleSlotCoord coord : kBattlefieldLayoutCoords) {
            t.set_node(battlefield_slot_tree_path(coord),
                       TreeNode{.kind = "slot", .x = 100.0f + coord.lane * 20.0f, .y = 200.0f});
            t.set_node(battlefield_unit_tree_path(coord), TreeNode{.kind = "mount"});
        }
        return t;
    }();
    return {.draw_background = false, .draw_frame = false, .tree_layout = &tree};
}

ConfigBinding lc_binding(std::string owner_id, BindingRole role) {
    return {.config_file = kBattleScreenConfigPath,
            .owner_kind = BindingOwnerKind::LifecycleProfile,
            .tree_path = std::move(owner_id),
            .role = role};
}

// ──── Binding tests ──────────────────────────────────────────────────────────

TEST(LifecycleBinding, DeathBodyResolvesToCorpseBinding) {
    BattleRenderSnapshot const snapshot{
        .entities = {entity_with(lifecycle_track(TrackKind::DeathBody, "UNDEAD_SMALL"))}};
    FakeProvider provider;
    const auto   batch = BattleRenderer::build_render_batch(snapshot, provider, no_bg());

    const auto& cmds = test::commands_from(batch);
    ASSERT_EQ(cmds.size(), 1u);
    ASSERT_TRUE(test::tunable_item(cmds[0]).has_value() &&
                test::tunable_item(cmds[0])->binding.has_value());
    EXPECT_EQ(test::tunable_item(cmds[0])->binding->owner_kind, BindingOwnerKind::LifecycleProfile);
    EXPECT_EQ(test::tunable_item(cmds[0])->binding->tree_path, "UNDEAD_SMALL");
    EXPECT_EQ(test::tunable_item(cmds[0])->binding->role, BindingRole::Corpse);
}

TEST(LifecycleBinding, DeathFxResolvesToDeathFxBinding) {
    BattleRenderSnapshot const snapshot{
        .entities = {entity_with(lifecycle_track(TrackKind::DeathFx, "UNDEAD_SMALL"))}};
    FakeProvider provider;
    const auto   batch = BattleRenderer::build_render_batch(snapshot, provider, no_bg());

    const auto& cmds = test::commands_from(batch);
    ASSERT_EQ(cmds.size(), 1u);
    ASSERT_TRUE(test::tunable_item(cmds[0]).has_value() &&
                test::tunable_item(cmds[0])->binding.has_value());
    EXPECT_EQ(test::tunable_item(cmds[0])->binding->tree_path, "UNDEAD_SMALL");
    EXPECT_EQ(test::tunable_item(cmds[0])->binding->role, BindingRole::DeathFx);
}

TEST(LifecycleBinding, ReviveFxSmallResolvesToReviveSmallBinding) {
    BattleRenderSnapshot const snapshot{
        .entities = {entity_with(
            lifecycle_track(TrackKind::ReviveFx, "UNDEAD_SMALL", BindingRole::ReviveSmall))}};
    FakeProvider provider;
    const auto   batch = BattleRenderer::build_render_batch(snapshot, provider, no_bg());

    const auto& cmds = test::commands_from(batch);
    ASSERT_EQ(cmds.size(), 1u);
    ASSERT_TRUE(test::tunable_item(cmds[0]).has_value() &&
                test::tunable_item(cmds[0])->binding.has_value());
    EXPECT_EQ(test::tunable_item(cmds[0])->binding->tree_path, "UNDEAD_SMALL");
    EXPECT_EQ(test::tunable_item(cmds[0])->binding->role, BindingRole::ReviveSmall);
}

TEST(LifecycleBinding, ReviveFxLargeResolvesToReviveLargeBinding) {
    BattleRenderSnapshot const snapshot{
        .entities = {entity_with(
            lifecycle_track(TrackKind::ReviveFx, "UNDEAD_SMALL", BindingRole::ReviveLarge))}};
    FakeProvider provider;
    const auto   batch = BattleRenderer::build_render_batch(snapshot, provider, no_bg());

    const auto& cmds = test::commands_from(batch);
    ASSERT_EQ(cmds.size(), 1u);
    ASSERT_TRUE(test::tunable_item(cmds[0]).has_value() &&
                test::tunable_item(cmds[0])->binding.has_value());
    EXPECT_EQ(test::tunable_item(cmds[0])->binding->role, BindingRole::ReviveLarge);
}

TEST(LifecycleBinding, DeathFxWithoutSequenceNameIsEditableIfProfileIdExists) {
    auto t = lifecycle_track(TrackKind::DeathFx, "UNDEAD_SMALL");
    t.sequence_name = "";
    BattleRenderSnapshot const snapshot{.entities = {entity_with(t)}};
    FakeProvider               provider;
    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, no_bg());

    const auto& cmds = test::commands_from(batch);
    ASSERT_EQ(cmds.size(), 1u);
    ASSERT_TRUE(test::tunable_item(cmds[0]).has_value() &&
                test::tunable_item(cmds[0])->binding.has_value());
    EXPECT_TRUE(test::tunable_item(cmds[0])->binding->writable());
    EXPECT_EQ(test::tunable_item(cmds[0])->binding->role, BindingRole::DeathFx);
}

TEST(LifecycleBinding, DeathBodyWithoutProfileIdHasNoBinding) {
    BattleRenderSnapshot const snapshot{
        .entities = {entity_with(lifecycle_track(TrackKind::DeathBody, ""))}};
    FakeProvider provider;
    const auto   batch = BattleRenderer::build_render_batch(snapshot, provider, no_bg());

    const auto& cmds = test::commands_from(batch);
    ASSERT_EQ(cmds.size(), 1u);
    ASSERT_TRUE(test::tunable_item(cmds[0]).has_value());
    EXPECT_FALSE(test::tunable_item(cmds[0])->binding.has_value());
}

TEST(LifecycleBinding, ReviveFxWithoutProfileIdHasNoBinding) {
    BattleRenderSnapshot const snapshot{.entities = {entity_with(lifecycle_track(
                                            TrackKind::ReviveFx, "", BindingRole::ReviveSmall))}};
    FakeProvider               provider;
    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, no_bg());

    const auto& cmds = test::commands_from(batch);
    ASSERT_EQ(cmds.size(), 1u);
    ASSERT_TRUE(test::tunable_item(cmds[0]).has_value());
    EXPECT_FALSE(test::tunable_item(cmds[0])->binding.has_value());
}

// ──── Storage key tests ──────────────────────────────────────────────────────

TEST(LifecycleStorage, DeathFxAndCorpseForSameProfileHaveDifferentKeys) {
    EXPECT_NE(lc_binding("UNDEAD_SMALL", BindingRole::DeathFx).key(),
              lc_binding("UNDEAD_SMALL", BindingRole::Corpse).key());
}

TEST(LifecycleStorage, EditingDeathFxDoesNotChangeCorpse) {
    BattleTuningState state;
    auto              fx = lc_binding("UNDEAD_SMALL", BindingRole::DeathFx);
    auto              cps = lc_binding("UNDEAD_SMALL", BindingRole::Corpse);
    ASSERT_TRUE(state.set_placement(fx, {.x = 10.f, .y = 20.f}));
    EXPECT_FLOAT_EQ(state.placement(cps).x, 0.f);
    EXPECT_FLOAT_EQ(state.placement(cps).y, 0.f);
}

TEST(LifecycleStorage, EditingCorpseDoesNotChangeDeathFx) {
    BattleTuningState state;
    auto              fx = lc_binding("UNDEAD_SMALL", BindingRole::DeathFx);
    auto              cps = lc_binding("UNDEAD_SMALL", BindingRole::Corpse);
    ASSERT_TRUE(state.set_placement(cps, {.x = 50.f, .y = 70.f}));
    EXPECT_FLOAT_EQ(state.placement(fx).x, 0.f);
    EXPECT_FLOAT_EQ(state.placement(fx).y, 0.f);
}

TEST(LifecycleStorage, EditingOneProfileCorpseDoesNotAffectOtherProfile) {
    BattleTuningState state;
    auto              undead = lc_binding("UNDEAD_SMALL", BindingRole::Corpse);
    auto              dwarf = lc_binding("DWARF_SMALL", BindingRole::Corpse);
    ASSERT_TRUE(state.set_placement(undead, {.x = 10.f}));
    EXPECT_FLOAT_EQ(state.placement(dwarf).x, 0.f);
}

TEST(LifecycleStorage, KeyContainsBothProfileIdAndRole) {
    const auto k1 = lc_binding("UNDEAD_SMALL", BindingRole::Corpse).key();
    const auto k2 = lc_binding("UNDEAD_SMALL", BindingRole::DeathFx).key();
    const auto k3 = lc_binding("DWARF_SMALL", BindingRole::Corpse).key();
    EXPECT_NE(k1, k2); // same profile, different role
    EXPECT_NE(k1, k3); // same role, different profile
    EXPECT_NE(k2, k3); // different both
}

// ──── Renderer tests ─────────────────────────────────────────────────────────

TEST(LifecycleRenderer, ReadsDeathFxPlacementByProfileAndRole) {
    auto                       t = lifecycle_track(TrackKind::DeathFx, "UNDEAD_SMALL");
    BattleRenderSnapshot const snapshot{.entities = {entity_with(t)}};
    FakeProvider               provider;

    const auto cmd_default_batch = BattleRenderer::build_render_batch(snapshot, provider, no_bg());

    const auto&         cmd_default = test::commands_from(cmd_default_batch);
    BattleRenderOptions opts = no_bg();
    opts.placements[lc_binding("UNDEAD_SMALL", BindingRole::DeathFx).key()] = {.x = 100.f,
                                                                               .y = 200.f};
    const auto cmd_placed_batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmd_placed = test::commands_from(cmd_placed_batch);

    ASSERT_EQ(cmd_default.size(), 1u);
    ASSERT_EQ(cmd_placed.size(), 1u);
    EXPECT_NE(cmd_placed[0].destination.x, cmd_default[0].destination.x);
}

TEST(LifecycleRenderer, ReadsCorpsePlacementByProfileAndRole) {
    auto                       t = lifecycle_track(TrackKind::DeathBody, "UNDEAD_SMALL");
    BattleRenderSnapshot const snapshot{.entities = {entity_with(t)}};
    FakeProvider               provider;

    const auto cmd_default_batch = BattleRenderer::build_render_batch(snapshot, provider, no_bg());

    const auto&         cmd_default = test::commands_from(cmd_default_batch);
    BattleRenderOptions opts = no_bg();
    opts.placements[lc_binding("UNDEAD_SMALL", BindingRole::Corpse).key()] = {.scale_x = 3.f,
                                                                              .scale_y = 3.f};
    const auto cmd_placed_batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmd_placed = test::commands_from(cmd_placed_batch);

    ASSERT_EQ(cmd_default.size(), 1u);
    ASSERT_EQ(cmd_placed.size(), 1u);
    EXPECT_NE(cmd_placed[0].destination.w, cmd_default[0].destination.w);
}

TEST(LifecycleRenderer, DeathBodyWithoutPerItemUsesIdentityPlacement) {
    auto                       t = lifecycle_track(TrackKind::DeathBody, "UNDEAD_SMALL");
    BattleRenderSnapshot const snapshot{.entities = {entity_with(t)}};
    FakeProvider               provider;

    // death_offset/death_scale were removed — lifecycle uses typed placements only
    const auto cmd_batch = BattleRenderer::build_render_batch(snapshot, provider, no_bg());

    const auto& cmd = test::commands_from(cmd_batch);
    ASSERT_EQ(cmd.size(), 1u);
    EXPECT_GT(cmd[0].destination.w, 0.0f);
}

TEST(LifecycleRenderer, LifecycleProfileHasNoGlobalFallback) {
    BattleTuningState const state;
    const auto              val = state.placement(lc_binding("UNDEAD_SMALL", BindingRole::Corpse));
    EXPECT_FLOAT_EQ(val.x, 0.f);
    EXPECT_FLOAT_EQ(val.y, 0.f);
    EXPECT_FLOAT_EQ(val.scale_x, 1.f);
    EXPECT_FLOAT_EQ(val.scale_y, 1.f);
}

// ──── Save/reload tests ──────────────────────────────────────────────────────

TEST(LifecycleSaveReload, EditingDeathFxWritesToCorrectJsonPath) {
    ConfigBinding const b = lc_binding("UNDEAD_SMALL", BindingRole::DeathFx);
    nlohmann::json      json;
    write_placement_to_json(json, b, {.x = 1.f, .y = 2.f});
    ASSERT_TRUE(json.contains("unit_lifecycle_profiles"));
    EXPECT_TRUE(json["unit_lifecycle_profiles"]["UNDEAD_SMALL"].contains("death_fx"));
    EXPECT_FALSE(json["unit_lifecycle_profiles"]["UNDEAD_SMALL"].contains("corpse"));
}

TEST(LifecycleSaveReload, EditingCorpseWritesToCorrectJsonPath) {
    ConfigBinding const b = lc_binding("UNDEAD_SMALL", BindingRole::Corpse);
    nlohmann::json      json;
    write_placement_to_json(json, b, {.x = 5.f, .y = 10.f});
    ASSERT_TRUE(json.contains("unit_lifecycle_profiles"));
    EXPECT_TRUE(json["unit_lifecycle_profiles"]["UNDEAD_SMALL"].contains("corpse"));
    EXPECT_FALSE(json["unit_lifecycle_profiles"]["UNDEAD_SMALL"].contains("death_fx"));
}

TEST(LifecycleSaveReload, EditingSpriteProfileWritesFlatSpritePath) {
    ConfigBinding const b{.config_file = kBattleScreenConfigPath,
                          .owner_kind = BindingOwnerKind::SpriteProfile,
                          .tree_path = "DEAD_HUMAN_LA",
                          .role = BindingRole::Global,
                          .display_path = "sprite_profiles.DEAD_HUMAN_LA"};
    nlohmann::json      json;
    write_placement_to_json(json, b, {.x = 5.f, .y = 10.f, .scale_x = 3.f, .scale_y = 3.f});
    ASSERT_TRUE(json.contains("sprite_profiles"));
    ASSERT_TRUE(json["sprite_profiles"].contains("DEAD_HUMAN_LA"));
    EXPECT_FLOAT_EQ(json["sprite_profiles"]["DEAD_HUMAN_LA"]["scale_x"].get<float>(), 3.f);
    EXPECT_FALSE(json["sprite_profiles"]["DEAD_HUMAN_LA"].contains("corpse"));
}

TEST(LifecycleSaveReload, AfterReloadDeathFxAndCorpseRemainDistinct) {
    BattleTuningState state;
    auto              fx = lc_binding("UNDEAD_SMALL", BindingRole::DeathFx);
    auto              cps = lc_binding("UNDEAD_SMALL", BindingRole::Corpse);
    state.set_placement(fx, {.x = 11.f, .y = 22.f});
    state.set_placement(cps, {.x = 33.f, .y = 44.f});
    EXPECT_FLOAT_EQ(state.placement(fx).x, 11.f);
    EXPECT_FLOAT_EQ(state.placement(cps).x, 33.f);
    EXPECT_NE(state.placement(fx).x, state.placement(cps).x);
}

TEST(LifecycleSaveReload, ReviveSmallAndReviveLargeAreSavedDistinctly) {
    ConfigBinding const small_b = lc_binding("UNDEAD_SMALL", BindingRole::ReviveSmall);
    ConfigBinding const large_b = lc_binding("UNDEAD_SMALL", BindingRole::ReviveLarge);
    nlohmann::json      json;
    write_placement_to_json(json, small_b, {.x = 1.f});
    write_placement_to_json(json, large_b, {.x = 2.f});
    EXPECT_TRUE(json["unit_lifecycle_profiles"]["UNDEAD_SMALL"].contains("revive_small"));
    EXPECT_TRUE(json["unit_lifecycle_profiles"]["UNDEAD_SMALL"].contains("revive_large"));
    EXPECT_NE(json["unit_lifecycle_profiles"]["UNDEAD_SMALL"]["revive_small"]["x"].get<float>(),
              json["unit_lifecycle_profiles"]["UNDEAD_SMALL"]["revive_large"]["x"].get<float>());
}

TEST(LifecycleSaveReload, LoadedRolesAreRecognized) {
    // Check that revive_small and revive_large survive a load round-trip
    BattleTuningState state;
    auto              small_b = lc_binding("UNDEAD_SMALL", BindingRole::ReviveSmall);
    auto              large_b = lc_binding("UNDEAD_SMALL", BindingRole::ReviveLarge);
    state.set_placement(small_b, {.x = 7.f});
    state.set_placement(large_b, {.x = 9.f});
    EXPECT_FLOAT_EQ(state.placement(small_b).x, 7.f);
    EXPECT_FLOAT_EQ(state.placement(large_b).x, 9.f);
    EXPECT_NE(state.placement(small_b).x, state.placement(large_b).x);
}

// ──── No fallback tests ──────────────────────────────────────────────────────

TEST(LifecycleNoFallback, ConcreteCorpseEditStoresOnlyInTypedPlacements) {
    BattleTuningState state;
    auto              b = lc_binding("UNDEAD_SMALL", BindingRole::Corpse);
    ASSERT_TRUE(state.set_placement(b, {.x = 99.f, .y = 88.f, .scale_x = 2.f, .scale_y = 2.f}));
    // Only the typed placement for this binding should exist
    EXPECT_FLOAT_EQ(state.placement(b).x, 99.f);
    EXPECT_EQ(state.placements.size(), 1u);
}

TEST(LifecycleNoFallback, DeathBodyWithEmptyProfileIdHasNoBinding) {
    BattleRenderSnapshot const snapshot{
        .entities = {entity_with(lifecycle_track(TrackKind::DeathBody, ""))}};
    FakeProvider provider;
    const auto   batch = BattleRenderer::build_render_batch(snapshot, provider, no_bg());

    const auto& cmds = test::commands_from(batch);
    ASSERT_EQ(cmds.size(), 1u);
    ASSERT_TRUE(test::tunable_item(cmds[0]).has_value());
    EXPECT_FALSE(test::tunable_item(cmds[0])->binding.has_value());
}

TEST(LifecycleNoFallback, AllLifecycleRolesHaveDistinctKeys) {
    const std::string pid = "PROFILE";
    const auto        k_corpse = lc_binding(pid, BindingRole::Corpse).key();
    const auto        k_death = lc_binding(pid, BindingRole::DeathFx).key();
    const auto        k_rs = lc_binding(pid, BindingRole::ReviveSmall).key();
    const auto        k_rl = lc_binding(pid, BindingRole::ReviveLarge).key();
    EXPECT_NE(k_corpse, k_death);
    EXPECT_NE(k_corpse, k_rs);
    EXPECT_NE(k_corpse, k_rl);
    EXPECT_NE(k_death, k_rs);
    EXPECT_NE(k_death, k_rl);
    EXPECT_NE(k_rs, k_rl);
}

TEST(LifecycleNoFallback, GlobalFallbackReturnsDefaultForLifecycleCorpse) {
    BattleTuningState const state;
    const auto              val = state.global_fallback(lc_binding("PROFILE", BindingRole::Corpse));
    EXPECT_FLOAT_EQ(val.x, 0.f);
    EXPECT_FLOAT_EQ(val.y, 0.f);
    EXPECT_FLOAT_EQ(val.scale_x, 1.f);
    EXPECT_EQ(val.level, 0);
}

TEST(LifecycleNoFallback, LifecycleStorageKeyAlwaysIncludesRole) {
    // object_id-only key must not exist as accepted lifecycle storage
    const auto key_with_role = lc_binding("UNDEAD_SMALL", BindingRole::Corpse).key();
    EXPECT_NE(key_with_role.find("Corpse"), std::string::npos);
    EXPECT_NE(key_with_role.find("UNDEAD_SMALL"), std::string::npos);
}

// --- unit_visual_layer_profiles save/load roundtrip ---

TEST(UnitVisualLayerProfile, WriteAndReadRoundtrip) {
    // write_placement_to_json for UnitVisualLayerProfile must produce the 4-level key.
    const ConfigBinding b{.config_file = "test.json",
                          .owner_kind = BindingOwnerKind::UnitVisualLayerProfile,
                          .tree_path = "UU0010:a2",
                          .role = BindingRole::UnitIdle};
    nlohmann::json      json;
    write_placement_to_json(json, b, {.x = 3.0f, .y = 7.0f});

    // Should be under unit_visual_layer_profiles.UU0010.idle.a2
    ASSERT_TRUE(json.contains("unit_visual_layer_profiles"));
    ASSERT_TRUE(json["unit_visual_layer_profiles"].contains("UU0010"));
    ASSERT_TRUE(json["unit_visual_layer_profiles"]["UU0010"].contains("idle"));
    ASSERT_TRUE(json["unit_visual_layer_profiles"]["UU0010"]["idle"].contains("a2"));
    EXPECT_FLOAT_EQ(json["unit_visual_layer_profiles"]["UU0010"]["idle"]["a2"]["x"].get<float>(),
                    3.0f);
    EXPECT_FLOAT_EQ(json["unit_visual_layer_profiles"]["UU0010"]["idle"]["a2"]["y"].get<float>(),
                    7.0f);
}

TEST(UnitVisualLayerProfile, LoadRoundtrip) {
    // Load a JSON with unit_visual_layer_profiles and verify the key is in placements.
    nlohmann::json json;
    json["unit_visual_layer_profiles"]["UU0010"]["idle"]["s1"] = {
        {"x", 5.0f},       {"y", 2.0f},  {"scale_x", 1.0f},
        {"scale_y", 1.0f}, {"level", 0}, {"frame_delay", 0}};

    const std::filesystem::path tmp = "test_layer_roundtrip.json";
    BattleTuningState           state;
    load_battle_tuning_config(state, json, tmp);

    const ConfigBinding b{.config_file = tmp.string(),
                          .owner_kind = BindingOwnerKind::UnitVisualLayerProfile,
                          .tree_path = "UU0010:s1",
                          .role = BindingRole::UnitIdle};
    const auto          placed = state.placement(b);
    EXPECT_FLOAT_EQ(placed.x, 5.0f);
    EXPECT_FLOAT_EQ(placed.y, 2.0f);
}

TEST(BattleTuningConfig, RejectsUnsupportedTopLevelKey) {
    nlohmann::json json;
    json["stale_section"] = nlohmann::json::object();
    const std::filesystem::path tmp = "test_unsupported_key.json";

    BattleTuningState state;
    EXPECT_THROW(load_battle_tuning_config(state, json, tmp), std::runtime_error);
}

// ──── Side-specific placement save/load ─────────────────────────────────────

TEST(UnitVisualLayerProfile, WriteSideSpecificLayerProducesNestedKey) {
    // Writing UnitVisualLayerProfile with side="a" must produce <unit>.<role>.<slot>.a in JSON.
    const ConfigBinding b{.config_file = "test.json",
                          .owner_kind = BindingOwnerKind::UnitVisualLayerProfile,
                          .tree_path = "UU0010:a2",
                          .role = BindingRole::UnitIdle,
                          .side = "a"};
    nlohmann::json      json;
    write_placement_to_json(json, b, {.x = 3.0f, .y = 7.0f});

    ASSERT_TRUE(json["unit_visual_layer_profiles"]["UU0010"]["idle"]["a2"].contains("a"));
    EXPECT_FLOAT_EQ(
        json["unit_visual_layer_profiles"]["UU0010"]["idle"]["a2"]["a"]["x"].get<float>(), 3.0f);
}

TEST(UnitVisualLayerProfile, WriteSideSpecificRoleProducesNestedKey) {
    // Writing UnitVisualProfile with side="d" must produce <unit>.<role>.d in JSON.
    const ConfigBinding b{.config_file = "test.json",
                          .owner_kind = BindingOwnerKind::UnitVisualProfile,
                          .tree_path = "UU0010",
                          .role = BindingRole::UnitAttack,
                          .side = "d"};
    nlohmann::json      json;
    write_placement_to_json(json, b, {.x = 5.0f});

    ASSERT_TRUE(json["unit_visual_profiles"]["UU0010"]["attack"].contains("d"));
    EXPECT_FLOAT_EQ(json["unit_visual_profiles"]["UU0010"]["attack"]["d"]["x"].get<float>(), 5.0f);
}

TEST(UnitVisualLayerProfile, LoadSideSpecificLayerPopulatesKeyWithSide) {
    // JSON with a "a" sub-object inside slot must load as a key ending in ":a".
    nlohmann::json json;
    json["unit_visual_layer_profiles"]["UU0010"]["idle"]["a2"] = {{"x", 1.0f},
                                                                  {"y", 2.0f},
                                                                  {"scale_x", 1.0f},
                                                                  {"scale_y", 1.0f},
                                                                  {"level", 0},
                                                                  {"frame_delay", 0},
                                                                  {"a",
                                                                   {{"x", 9.0f},
                                                                    {"y", 0.0f},
                                                                    {"scale_x", 1.0f},
                                                                    {"scale_y", 1.0f},
                                                                    {"level", 0},
                                                                    {"frame_delay", 0}}}};

    BattleTuningState           state;
    const std::filesystem::path tmp = "test_side_layer_load.json";
    load_battle_tuning_config(state, json, tmp);

    const ConfigBinding common{.config_file = tmp.string(),
                               .owner_kind = BindingOwnerKind::UnitVisualLayerProfile,
                               .tree_path = "UU0010:a2",
                               .role = BindingRole::UnitIdle};
    const ConfigBinding side_a{.config_file = tmp.string(),
                               .owner_kind = BindingOwnerKind::UnitVisualLayerProfile,
                               .tree_path = "UU0010:a2",
                               .role = BindingRole::UnitIdle,
                               .side = "a"};
    EXPECT_FLOAT_EQ(state.placement(common).x, 1.0f);
    EXPECT_FLOAT_EQ(state.placement(side_a).x, 9.0f);
}

TEST(UnitVisualLayerProfile, SaveLoadRoundtripPreservesCommonAndSideSpecific) {
    // set_placement for common and side-specific bindings; write_placement_to_json and reload
    // must reconstruct both entries.
    BattleTuningState   state;
    const ConfigBinding common{.config_file = "test.json",
                               .owner_kind = BindingOwnerKind::UnitVisualLayerProfile,
                               .tree_path = "UU0077:s1",
                               .role = BindingRole::UnitAttack};
    const ConfigBinding side_d{.config_file = "test.json",
                               .owner_kind = BindingOwnerKind::UnitVisualLayerProfile,
                               .tree_path = "UU0077:s1",
                               .role = BindingRole::UnitAttack,
                               .side = "d"};
    state.set_placement(common, {.x = 3.0f});
    state.set_placement(side_d, {.x = 7.0f});

    nlohmann::json json;
    write_placement_to_json(json, common, state.placement(common));
    write_placement_to_json(json, side_d, state.placement(side_d));

    const std::filesystem::path tmp = "test_side_roundtrip.json";
    BattleTuningState           state2;
    load_battle_tuning_config(state2, json, tmp);

    EXPECT_FLOAT_EQ(state2.placement(common).x, 3.0f);
    EXPECT_FLOAT_EQ(state2.placement(side_d).x, 7.0f);
}

} // namespace
} // namespace d2engine
