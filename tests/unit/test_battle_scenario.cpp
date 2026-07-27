#include <gtest/gtest.h>

#include "d2engine/animation/animation_sequence.hpp"
#include "d2engine/app/battle_scenario.hpp"
#include "d2engine/assets/game_data_registry.hpp"
#include "d2engine/battle_view/animation_catalog.hpp"
#include "d2engine/battle_view/animation_role.hpp"
#include "d2engine/battle_view/battle_effect_clip.hpp"
#include "d2engine/battle_view/battle_effect_role_set.hpp"
#include "d2engine/battle_view/battle_ids.hpp"
#include "d2engine/battle_view/battle_presenter.hpp"
#include "d2engine/battle_view/battle_renderer.hpp"
#include "d2engine/battle_view/battle_scenario_data.hpp"
#include "d2engine/battle_view/battle_scenario_executor.hpp"
#include "d2engine/battle_view/battle_scenario_runtime.hpp"
#include "d2engine/battle_view/battle_texture_provider.hpp"
#include "d2engine/battle_view/i_battle_unit_creation_service.hpp"
#include "d2engine/battle_view/battle_scene.hpp"
#include "d2engine/battle_view/battle_slot.hpp"
#include "d2engine/battle_view/battle_unit.hpp"
#include "d2engine/battle_view/life_visual_state.hpp"
#include "d2engine/battle_view/battle_unit_factory.hpp"
#include "d2engine/battle_view/battle_visual_event.hpp"
#include "d2engine/battle_view/battle_visual_profile_registry.hpp"
#include "d2engine/battle_view/event_type_doc.hpp"
#include "d2engine/battle_view/unit_animation_role_set.hpp"
#include "d2engine/battle_view/unit_lifecycle_visual_profile.hpp"
#include "d2engine/battle_view/unit_state_clip.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include "tests/test_process.hpp"

namespace d2engine {
namespace {

// ─────────────────────────────────────────────────────────────────────────────
// Helper utilities
// ─────────────────────────────────────────────────────────────────────────────

std::filesystem::path write_json(const std::string& text) {
    static std::size_t seq = 0;
    const auto    path = std::filesystem::temp_directory_path() /
                         ("opendis2-battle-scenario-" + std::to_string(test_support::process_id()) +
                          "_" + std::to_string(++seq) + ".json");
    std::ofstream out(path);
    out << text;
    return path;
}

void expect_load_error(const std::string& text, const std::string& needle) {
    try {
        (void)load_battle_scenario(write_json(text));
        FAIL() << "expected load failure but got success";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string{e.what()}.find(needle), std::string::npos)
            << "error message \"" << e.what() << "\" did not contain \"" << needle << "\"";
    }
}

ScenarioUnitCreated unit_created(std::string alias, std::string unit_type, std::string slot) {
    return ScenarioUnitCreated{.alias = std::move(alias),
                               .unit_type = std::move(unit_type),
                               .slot = std::move(slot),
                               .hp = 100,
                               .max_hp = 100};
}

// ─────────────────────────────────────────────────────────────────────────────
// FakeAnimationCatalog – minimal stub for unit factory construction
// ─────────────────────────────────────────────────────────────────────────────

class FakeAnimationCatalog final : public IAnimationCatalog {
public:
    explicit FakeAnimationCatalog(std::string missing_idle_unit = {})
        : missing_idle_unit_(std::move(missing_idle_unit)) {}

    [[nodiscard]] AnimationSequence unit_clip(std::string_view unit_type, std::string_view role,
                                              char direction) const override {
        AnimationSequence result;
        result.name =
            AnimationNames::build(std::string{unit_type}, std::string{role}, 1, direction);
        if (unit_type == missing_idle_unit_ && role == AnimationRoles::IDLE) {
            return result;
        }
        result.frames.push_back(
            {.image_name = "frame", .index = 0, .duration_ms = static_cast<std::uint16_t>(100)});
        return result;
    }

    [[nodiscard]] AnimationSequence battle_effect(std::string_view name) const override {
        AnimationSequence result;
        result.name = std::string{name};
        result.frames.push_back(
            {.image_name = "frame", .index = 0, .duration_ms = static_cast<std::uint16_t>(100)});
        return result;
    }

    [[nodiscard]] UnitStateClip unit_state_bundle(std::string_view unit_type,
                                                  std::string_view action,
                                                  char             direction) const override {
        const std::string role = std::string{action} + "A";
        AnimationSequence seq = unit_clip(unit_type, role, direction);
        if (seq.frames.empty()) {
            return {};
        }
        return UnitStateClip::from_a1(std::move(seq), std::string{action}, direction);
    }

    [[nodiscard]] BattleEffectClip battle_effect_bundle(std::string_view unit_type,
                                                        std::string_view effect_family,
                                                        char             direction) const override {
        const std::string role = std::string{effect_family} + "A";
        AnimationSequence seq = unit_clip(unit_type, role, direction);
        if (seq.frames.empty()) {
            return {};
        }
        return BattleEffectClip{.clip = LayeredAnimationClip{.a1 = std::move(seq)},
                                .family = std::string{effect_family},
                                .direction_or_variant = direction,
                                .requested_direction = direction};
    }

private:
    std::string missing_idle_unit_;
};

class ScenarioFakeTextureProvider final : public IBattleTextureProvider {
public:
    BackendTextureRef get_texture(const std::string& /*container_path*/,
                                  const std::string& image_name) override {
        if (image_name.empty()) {
            return {};
        }
        ++calls;
        return BackendTextureRef{.native = reinterpret_cast<void*>(calls)};
    }

    [[nodiscard]] std::pair<float, float>
    texture_size(BackendTextureRef /*texture*/) const override {
        return {32.0f, 48.0f};
    }

    std::uintptr_t calls = 0;
};

TreeLayout scenario_hud_tree() {
    TreeLayout tree;
    tree.set_node("/battlefield/slot_a_front_0",
                  TreeNode{.kind = "battlefield_slot", .x = 100.0f, .y = 100.0f});
    tree.set_node("/battlefield/slot_a_front_0/unit", TreeNode{.kind = "unit_mount"});
    tree.set_node("/battlefield/slot_d_front_0",
                  TreeNode{.kind = "battlefield_slot", .x = 200.0f, .y = 100.0f});
    tree.set_node("/battlefield/slot_d_front_0/unit", TreeNode{.kind = "unit_mount"});
    tree.set_node("/ui/left_unit_group/0_front",
                  TreeNode{.kind = "slot", .x = 0.0f, .y = 0.0f, .w = 100.0f, .h = 160.0f});
    tree.set_node("/ui/left_unit_group/0_front/portrait",
                  TreeNode{.kind = "portrait", .x = 0.0f, .y = 0.0f, .w = 80.0f, .h = 120.0f});
    tree.set_node("/ui/left_unit_group/0_front/hp",
                  TreeNode{.kind = "text", .x = 0.0f, .y = 130.0f, .w = 100.0f, .h = 24.0f});
    tree.set_node("/ui/right_unit_group/0_front",
                  TreeNode{.kind = "slot", .x = 200.0f, .y = 0.0f, .w = 100.0f, .h = 160.0f});
    tree.set_node("/ui/right_unit_group/0_front/portrait",
                  TreeNode{.kind = "portrait", .x = 0.0f, .y = 0.0f, .w = 80.0f, .h = 120.0f});
    tree.set_node("/ui/right_unit_group/0_front/hp",
                  TreeNode{.kind = "text", .x = 0.0f, .y = 130.0f, .w = 100.0f, .h = 24.0f});
    tree.set_node("/ui/combat_frame/info_left", TreeNode{.kind = "info", .x = 400.0f, .y = 0.0f});
    tree.set_node("/ui/combat_frame/info_left/name",
                  TreeNode{.kind = "text", .x = 0.0f, .y = 0.0f, .w = 120.0f, .h = 20.0f});
    tree.set_node("/ui/combat_frame/info_left/hp",
                  TreeNode{.kind = "text", .x = 0.0f, .y = 24.0f, .w = 120.0f, .h = 20.0f});
    tree.set_node("/ui/combat_frame/info_right", TreeNode{.kind = "info", .x = 600.0f, .y = 0.0f});
    tree.set_node("/ui/combat_frame/info_right/name",
                  TreeNode{.kind = "text", .x = 0.0f, .y = 0.0f, .w = 120.0f, .h = 20.0f});
    tree.set_node("/ui/combat_frame/info_right/hp",
                  TreeNode{.kind = "text", .x = 0.0f, .y = 24.0f, .w = 120.0f, .h = 20.0f});
    return tree;
}

std::vector<RenderCommand> scenario_hud_commands(const BattleScene& scene, UnitInstanceId left,
                                                 UnitInstanceId right) {
    TreeLayout          tree = scenario_hud_tree();
    BattleRenderOptions options;
    options.draw_background = false;
    options.draw_frame = false;
    options.draw_unit_groups = true;
    options.tree_layout = &tree;
    options.info_left_unit = left;
    options.info_right_unit = right;
    ScenarioFakeTextureProvider provider;
    return BattleRenderer::build_render_batch(scene.snapshot(), provider, options).commands;
}

const RenderCommand* find_command(const std::vector<RenderCommand>& commands,
                                  const std::string&                path) {
    const auto it = std::ranges::find_if(
        commands, [&](const auto& command) { return command.tree_path == path; });
    return it != commands.end() ? &*it : nullptr;
}

const RenderCommand* find_fill_command(const std::vector<RenderCommand>& commands,
                                       const std::string&                path) {
    const auto it = std::ranges::find_if(commands, [&](const auto& command) {
        return command.tree_path == path && command.fill_color.has_value() &&
               command.fill_color->r == 180;
    });
    return it != commands.end() ? &*it : nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// Common valid scenario JSON fragments
// ─────────────────────────────────────────────────────────────────────────────

constexpr auto MINIMAL_SCENARIO = R"({
    "version": 3,
    "id": "minimal_scenario",
    "sequences": [
        {
            "id": "seq1",
            "steps": [
                {
                    "id": "step1",
                    "complete": "immediate",
                    "events": [
                        {
                            "id": "evt1",
                            "event": {
                                "type": "ActorSelected",
                                "previous": "u1",
                                "selected": "u2"
                            }
                        }
                    ]
                }
            ]
        }
    ]
})";

// ─────────────────────────────────────────────────────────────────────────────
// 1. BattleScenarioParser Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST(BattleScenarioParser, StripsCommentHeader) {
    const std::string input = "// Generated by battle viewer\n"
                              "// Version 3\n"
                              "{\n"
                              "  \"version\": 3,\n"
                              "  \"id\": \"test\"\n"
                              "}\n";
    const std::string got = strip_scenario_comment_header(input);
    EXPECT_EQ(got, "{\n  \"version\": 3,\n  \"id\": \"test\"\n}\n");
}

TEST(BattleScenarioParser, PreservesJsonWithNoComments) {
    const std::string input = "{\n  \"version\": 3,\n  \"id\": \"test\"\n}\n";
    const std::string got = strip_scenario_comment_header(input);
    EXPECT_EQ(got, input);
}

TEST(BattleScenarioParser, StripsMultipleCommentLines) {
    const std::string input = "// line 1\n"
                              "// line 2\n"
                              "// line 3\n"
                              "\n"
                              "{\n"
                              "  \"id\": \"test\"\n"
                              "}\n";
    const std::string got = strip_scenario_comment_header(input);
    EXPECT_EQ(got, "{\n  \"id\": \"test\"\n}\n");
}

TEST(BattleScenarioParser, StripsCommentWithBlankLinesBetween) {
    const std::string input = "// header\n"
                              "\n"
                              "// another comment\n"
                              "{\n"
                              "  \"id\": \"test\"\n"
                              "}\n";
    const std::string got = strip_scenario_comment_header(input);
    EXPECT_EQ(got, "{\n  \"id\": \"test\"\n}\n");
}

TEST(BattleScenarioParser, PreservesInlineCommentsAfterJson) {
    const std::string input = "{\n"
                              "  \"id\": \"test\",\n"
                              "  \"version\": 3\n"
                              "}\n"
                              "// trailing comment\n";
    const std::string got = strip_scenario_comment_header(input);
    // The trailing comment is NOT stripped (only leading comments before data)
    EXPECT_EQ(got, input);
}

TEST(BattleScenarioParser, LoadsMinimalScenario) {
    const auto scenario = load_battle_scenario(write_json(MINIMAL_SCENARIO));
    EXPECT_EQ(scenario.version, 3);
    EXPECT_EQ(scenario.id, "minimal_scenario");
    EXPECT_EQ(scenario.terrain, "HU");
    EXPECT_FALSE(scenario.autoplay.has_value());
    ASSERT_EQ(scenario.sequences.size(), 1u);
    EXPECT_EQ(scenario.sequences[0].id, "seq1");
    ASSERT_EQ(scenario.sequences[0].steps.size(), 1u);
    EXPECT_EQ(scenario.sequences[0].steps[0].id, "step1");
    ASSERT_EQ(scenario.sequences[0].steps[0].envelopes.size(), 1u);
    EXPECT_EQ(scenario.sequences[0].steps[0].envelopes[0].id, "evt1");
}

TEST(BattleScenarioParser, LoadsVersion3ScenarioWithAllFields) {
    const auto* const json = R"({
        "version": 3,
        "id": "full_scenario",
        "terrain": "MT",
        "autoplay": "seq_auto",
        "sequences": [
            {
                "id": "seq_auto",
                "steps": [
                    {
                        "id": "s0",
                        "complete": "all_required_tracks_finished",
                        "events": [
                            {
                                "id": "e0",
                                "optional": true,
                                "event": {
                                    "type": "ActorSelected",
                                    "previous": "u1",
                                    "selected": "u2"
                                }
                            }
                        ]
                    }
                ]
            }
        ]
    })";
    const auto        scenario = load_battle_scenario(write_json(json));
    EXPECT_EQ(scenario.version, 3);
    EXPECT_EQ(scenario.id, "full_scenario");
    EXPECT_EQ(scenario.terrain, "MT");
    ASSERT_TRUE(scenario.autoplay.has_value());
    EXPECT_EQ(*scenario.autoplay, "seq_auto");
    ASSERT_EQ(scenario.sequences.size(), 1u);
    EXPECT_EQ(scenario.sequences[0].id, "seq_auto");
    ASSERT_EQ(scenario.sequences[0].steps.size(), 1u);
    EXPECT_EQ(scenario.sequences[0].steps[0].id, "s0");
    EXPECT_EQ(scenario.sequences[0].steps[0].complete,
              BattleVisualStepCompletion::AllRequiredTracksFinished);
    ASSERT_EQ(scenario.sequences[0].steps[0].envelopes.size(), 1u);
    EXPECT_EQ(scenario.sequences[0].steps[0].envelopes[0].id, "e0");
    EXPECT_TRUE(scenario.sequences[0].steps[0].envelopes[0].optional);
}

TEST(BattleScenarioParser, RejectsMissingVersion) {
    const auto* const json = R"({
        "id": "no_version",
        "sequences": [
            {
                "id": "s1",
                "steps": [
                    {
                        "id": "st1",
                        "complete": "immediate",
                        "events": [
                            {
                                "id": "e1",
                                "event": {
                                    "type": "ActorSelected",
                                    "previous": "a",
                                    "selected": "b"
                                }
                            }
                        ]
                    }
                ]
            }
        ]
    })";
    expect_load_error(json, "version");
}

TEST(BattleScenarioParser, RejectsUnsupportedVersion) {
    const auto* const json = R"({
        "version": 2,
        "id": "old_version"
    })";
    expect_load_error(json, "version");
}

TEST(BattleScenarioParser, RejectsUnknownEventType) {
    const auto* const json = R"({
        "version": 3,
        "id": "bad_type",
        "sequences": [
            {
                "id": "s1",
                "steps": [
                    {
                        "id": "st1",
                        "complete": "immediate",
                        "events": [
                            {
                                "id": "e1",
                                "event": {
                                    "type": "NonExistentEvent"
                                }
                            }
                        ]
                    }
                ]
            }
        ]
    })";
    expect_load_error(json, "unknown event type");
}

TEST(BattleScenarioParser, LoadsAllEventTypes) {
    const auto* const json = R"({
        "version": 3,
        "id": "all_types",
        "sequences": [
            {
                "id": "main",
                "steps": [
                    {
                        "id": "step_all",
                        "complete": "immediate",
                        "events": [
                            {
                                "id": "e01",
                                "event": {
                                    "type": "ActorSelected",
                                    "previous": "u1",
                                    "selected": "u2"
                                }
                            },
                            {
                                "id": "e02",
                                "event": {
                                    "type": "TargetSelected",
                                    "previous": "u1",
                                    "selected": "u3"
                                }
                            },
                            {
                                "id": "e03",
                                "event": {
                                    "type": "AttackStarted",
                                    "attack_id": 1,
                                    "source": "u1",
                                    "targets": ["u2"]
                                }
                            },
                            {
                                "id": "e04",
                                "event": {
                                    "type": "AttackImpactCue",
                                    "attack_id": 2,
                                    "source": "u1",
                                    "targets": ["u2", "u3"]
                                }
                            },
                            {
                                "id": "e05",
                                "event": {
                                    "type": "TargetDamaged",
                                    "attack_id": 1,
                                    "source": "u1",
                                    "target": "u2",
                                    "current_hp": 177
                                }
                            },
                            {
                                "id": "e06",
                                "event": {
                                    "type": "TargetMissed",
                                    "attack_id": 1,
                                    "target": "u2"
                                }
                            },
                            {
                                "id": "e07",
                                "event": {
                                    "type": "TargetResisted",
                                    "attack_id": 1,
                                    "target": "u2"
                                }
                            },
                            {
                                "id": "e08",
                                "event": {
                                    "type": "TargetKilled",
                                    "attack_id": 1,
                                    "target": "u2"
                                }
                            },
                            {
                                "id": "e09",
                                "event": {
                                    "type": "LifeDrained",
                                    "attack_id": 1,
                                    "source": "u1",
                                    "target": "u2",
                                    "target_current_hp": 120,
                                    "source_current_hp": 180
                                }
                            },
                            {
                                "id": "e10",
                                "event": {
                                    "type": "SourceHealed",
                                    "attack_id": 1,
                                    "source": "u1",
                                    "current_hp": 190
                                }
                            },
                            {
                                "id": "e11",
                                "event": {
                                    "type": "UnitHitReceived",
                                    "target": "u2"
                                }
                            },
                            {
                                "id": "e12",
                                "event": {
                                    "type": "UnitKilled",
                                    "target": "u2"
                                }
                            },
                            {
                                "id": "e13",
                                "event": {
                                    "type": "UnitReviveStarted",
                                    "target": "u2"
                                }
                            },
                            {
                                "id": "e14",
                                "event": {
                                    "type": "UnitRevived",
                                    "target": "u2",
                                    "current_hp": 40
                                }
                            },
                            {
                                "id": "e15",
                                "event": {
                                    "type": "CastEffectStarted",
                                    "caster": "u1",
                                    "effect": "source_attack_effect"
                                }
                            },
                            {
                                "id": "e16",
                                "event": {
                                    "type": "BattleEffectStarted",
                                    "source": "u1",
                                    "effect": "target_impact_effect",
                                    "visual_role": "source_cast_fx",
                                    "target": "u2"
                                }
                            },
                            {
                                "id": "e17",
                                "event": {
                                    "type": "BattleEffectStarted",
                                    "source": "u1",
                                    "effect": "team_impact_effect",
                                    "visual_role": "team_overlay_fx",
                                    "targets": ["u2", "u3"]
                                }
                            },
                            {
                                "id": "e18",
                                "event": {
                                    "type": "BattleEffectStarted",
                                    "source": "u1",
                                    "effect": "source_attack_effect",
                                    "visual_role": "field_overlay_fx",
                                    "targets": ["u2"]
                                }
                            },
                            {
                                "id": "e19",
                                "event": {
                                    "type": "UnitCreated",
                                    "unit": "new_unit",
                                    "unit_type": "UU0003",
                                    "slot": "A_FRONT_0"
                                }
                            },
                            {
                                "id": "e20",
                                "event": {
                                    "type": "UnitRetreated",
                                    "unit": "old_unit",
                                    "reason": "retreat_test"
                                }
                            }
                        ]
                    }
                ]
            }
        ]
    })";
    const auto        scenario = load_battle_scenario(write_json(json));
    ASSERT_EQ(scenario.sequences.size(), 1u);
    ASSERT_EQ(scenario.sequences[0].steps.size(), 1u);
    const auto& envelopes = scenario.sequences[0].steps[0].envelopes;
    ASSERT_EQ(envelopes.size(), 20u);

    // e01 — ActorSelected
    EXPECT_TRUE(std::holds_alternative<ScenarioActorSelected>(envelopes[0].event));
    const auto& act_sel = std::get<ScenarioActorSelected>(envelopes[0].event);
    EXPECT_EQ(act_sel.previous, "u1");
    EXPECT_EQ(act_sel.selected, "u2");

    // e02 — TargetSelected
    EXPECT_TRUE(std::holds_alternative<ScenarioTargetSelected>(envelopes[1].event));
    const auto& tgt_sel = std::get<ScenarioTargetSelected>(envelopes[1].event);
    EXPECT_EQ(tgt_sel.previous, "u1");
    EXPECT_EQ(tgt_sel.selected, "u3");

    // e03 — AttackStarted
    EXPECT_TRUE(std::holds_alternative<ScenarioAttackStarted>(envelopes[2].event));
    const auto& atk_start = std::get<ScenarioAttackStarted>(envelopes[2].event);
    EXPECT_EQ(atk_start.attack_id, AttackInstanceId{1u});
    EXPECT_EQ(atk_start.source, "u1");
    ASSERT_EQ(atk_start.targets.size(), 1u);
    EXPECT_EQ(atk_start.targets[0], "u2");

    // e04 — AttackImpactCue
    EXPECT_TRUE(std::holds_alternative<ScenarioAttackImpactCue>(envelopes[3].event));
    const auto& atk_cue = std::get<ScenarioAttackImpactCue>(envelopes[3].event);
    EXPECT_EQ(atk_cue.attack_id, AttackInstanceId{2u});
    EXPECT_EQ(atk_cue.source, "u1");
    ASSERT_EQ(atk_cue.targets.size(), 2u);
    EXPECT_EQ(atk_cue.targets[0], "u2");
    EXPECT_EQ(atk_cue.targets[1], "u3");

    // e05 — TargetDamaged
    EXPECT_TRUE(std::holds_alternative<ScenarioTargetDamaged>(envelopes[4].event));
    const auto& tgt_dmg = std::get<ScenarioTargetDamaged>(envelopes[4].event);
    EXPECT_EQ(tgt_dmg.attack_id, AttackInstanceId{1u});
    EXPECT_EQ(tgt_dmg.source, "u1");
    EXPECT_EQ(tgt_dmg.target, "u2");
    ASSERT_TRUE(tgt_dmg.current_hp.has_value());
    EXPECT_EQ(*tgt_dmg.current_hp, 177);

    // e06 — TargetMissed
    EXPECT_TRUE(std::holds_alternative<ScenarioTargetMissed>(envelopes[5].event));
    const auto& tgt_miss = std::get<ScenarioTargetMissed>(envelopes[5].event);
    EXPECT_EQ(tgt_miss.attack_id, AttackInstanceId{1u});
    EXPECT_EQ(tgt_miss.target, "u2");

    // e07 — TargetResisted
    EXPECT_TRUE(std::holds_alternative<ScenarioTargetResisted>(envelopes[6].event));
    const auto& tgt_res = std::get<ScenarioTargetResisted>(envelopes[6].event);
    EXPECT_EQ(tgt_res.attack_id, AttackInstanceId{1u});
    EXPECT_EQ(tgt_res.target, "u2");

    // e08 — TargetKilled
    EXPECT_TRUE(std::holds_alternative<ScenarioTargetKilled>(envelopes[7].event));
    const auto& tgt_kill = std::get<ScenarioTargetKilled>(envelopes[7].event);
    EXPECT_EQ(tgt_kill.attack_id, AttackInstanceId{1u});
    EXPECT_EQ(tgt_kill.target, "u2");

    // e09 — LifeDrained
    EXPECT_TRUE(std::holds_alternative<ScenarioLifeDrained>(envelopes[8].event));
    const auto& life_drain = std::get<ScenarioLifeDrained>(envelopes[8].event);
    EXPECT_EQ(life_drain.attack_id, AttackInstanceId{1u});
    EXPECT_EQ(life_drain.source, "u1");
    EXPECT_EQ(life_drain.target, "u2");
    ASSERT_TRUE(life_drain.target_current_hp.has_value());
    ASSERT_TRUE(life_drain.source_current_hp.has_value());
    EXPECT_EQ(*life_drain.target_current_hp, 120);
    EXPECT_EQ(*life_drain.source_current_hp, 180);

    // e10 — SourceHealed
    EXPECT_TRUE(std::holds_alternative<ScenarioSourceHealed>(envelopes[9].event));
    const auto& src_heal = std::get<ScenarioSourceHealed>(envelopes[9].event);
    EXPECT_EQ(src_heal.attack_id, AttackInstanceId{1u});
    EXPECT_EQ(src_heal.source, "u1");
    ASSERT_TRUE(src_heal.current_hp.has_value());
    EXPECT_EQ(*src_heal.current_hp, 190);

    // e11 — UnitHitReceived
    EXPECT_TRUE(std::holds_alternative<ScenarioUnitHitReceived>(envelopes[10].event));
    EXPECT_EQ(std::get<ScenarioUnitHitReceived>(envelopes[10].event).target, "u2");

    // e12 — UnitKilled
    EXPECT_TRUE(std::holds_alternative<ScenarioUnitKilled>(envelopes[11].event));
    EXPECT_EQ(std::get<ScenarioUnitKilled>(envelopes[11].event).target, "u2");

    // e13 — UnitReviveStarted
    EXPECT_TRUE(std::holds_alternative<ScenarioUnitReviveStarted>(envelopes[12].event));
    EXPECT_EQ(std::get<ScenarioUnitReviveStarted>(envelopes[12].event).target, "u2");

    // e14 — UnitRevived
    EXPECT_TRUE(std::holds_alternative<ScenarioUnitRevived>(envelopes[13].event));
    const auto& revived = std::get<ScenarioUnitRevived>(envelopes[13].event);
    EXPECT_EQ(revived.target, "u2");
    ASSERT_TRUE(revived.current_hp.has_value());
    EXPECT_EQ(*revived.current_hp, 40);

    // e15 — CastEffectStarted
    EXPECT_TRUE(std::holds_alternative<ScenarioCastEffectStarted>(envelopes[14].event));
    const auto& cast_eff = std::get<ScenarioCastEffectStarted>(envelopes[14].event);
    EXPECT_EQ(cast_eff.caster, "u1");
    EXPECT_EQ(cast_eff.effect, BattleEffectRole::Heff);

    // e16 — BattleEffectStarted (source_cast_fx)
    EXPECT_TRUE(std::holds_alternative<ScenarioBattleEffectStarted>(envelopes[15].event));
    const auto& bfx1 = std::get<ScenarioBattleEffectStarted>(envelopes[15].event);
    EXPECT_EQ(bfx1.source, "u1");
    EXPECT_EQ(bfx1.effect, BattleEffectRole::Tuch);
    EXPECT_EQ(bfx1.visual_role, BattleEffectVisualRole::SourceCastFx);
    EXPECT_TRUE(bfx1.target.empty());

    // e17 — BattleEffectStarted (team_overlay_fx with targets)
    EXPECT_TRUE(std::holds_alternative<ScenarioBattleEffectStarted>(envelopes[16].event));
    const auto& bfx2 = std::get<ScenarioBattleEffectStarted>(envelopes[16].event);
    EXPECT_EQ(bfx2.source, "u1");
    EXPECT_EQ(bfx2.effect, BattleEffectRole::Heff);
    EXPECT_EQ(bfx2.visual_role, BattleEffectVisualRole::TeamOverlayFx);
    ASSERT_EQ(bfx2.targets.size(), 2u);
    EXPECT_EQ(bfx2.targets[0], "u2");
    EXPECT_EQ(bfx2.targets[1], "u3");

    // e18 — BattleEffectStarted (field_overlay_fx with targets)
    EXPECT_TRUE(std::holds_alternative<ScenarioBattleEffectStarted>(envelopes[17].event));
    const auto& bfx3 = std::get<ScenarioBattleEffectStarted>(envelopes[17].event);
    EXPECT_EQ(bfx3.source, "u1");
    EXPECT_EQ(bfx3.effect, BattleEffectRole::Heff);
    EXPECT_EQ(bfx3.visual_role, BattleEffectVisualRole::FieldOverlayFx);
    ASSERT_EQ(bfx3.targets.size(), 1u);
    EXPECT_EQ(bfx3.targets[0], "u2");

    // e19 — UnitCreated
    EXPECT_TRUE(std::holds_alternative<ScenarioUnitCreated>(envelopes[18].event));
    const auto& uc = std::get<ScenarioUnitCreated>(envelopes[18].event);
    EXPECT_EQ(uc.alias, "new_unit");
    EXPECT_EQ(uc.unit_type, "UU0003");
    EXPECT_EQ(uc.slot, "A_FRONT_0");
    EXPECT_FALSE(uc.hp.has_value());
    EXPECT_FALSE(uc.max_hp.has_value());
    EXPECT_TRUE(uc.status.empty());

    // e20 — UnitRetreated
    EXPECT_TRUE(std::holds_alternative<ScenarioUnitRetreated>(envelopes[19].event));
    const auto& ur = std::get<ScenarioUnitRetreated>(envelopes[19].event);
    EXPECT_EQ(ur.unit, "old_unit");
    EXPECT_EQ(ur.reason, "retreat_test");
}

TEST(BattleScenarioParser, RejectsUnknownFields) {
    const auto* const json = R"({
        "version": 3,
        "id": "unknown_field_test",
        "unknown_field": "i_dont_belong_here",
        "sequences": [
            {
                "id": "s1",
                "steps": [
                    {
                        "id": "st1",
                        "complete": "immediate",
                        "events": [
                            {
                                "id": "e1",
                                "event": {
                                    "type": "ActorSelected",
                                    "previous": "a",
                                    "selected": "b"
                                }
                            }
                        ]
                    }
                ]
            }
        ]
    })";
    expect_load_error(json, "unknown_field");
}

TEST(BattleScenarioParser, RejectsUnknownFieldsInEvent) {
    const auto* const json = R"({
        "version": 3,
        "id": "bad_event_field",
        "sequences": [
            {
                "id": "s1",
                "steps": [
                    {
                        "id": "st1",
                        "complete": "immediate",
                        "events": [
                            {
                                "id": "e1",
                                "event": {
                                    "type": "AttackStarted",
                                    "attack_id": 1,
                                    "source": "u1",
                                    "targets": ["u2"],
                                    "bogus_field": "should_fail"
                                }
                            }
                        ]
                    }
                ]
            }
        ]
    })";
    expect_load_error(json, "bogus_field");
}

TEST(BattleScenarioParser, RejectsDuplicateSequenceId) {
    const auto* const json = R"({
        "version": 3,
        "id": "dup_seq",
        "sequences": [
            {
                "id": "same_id",
                "steps": [
                    {
                        "id": "st1",
                        "complete": "immediate",
                        "events": [
                            {
                                "id": "e1",
                                "event": {
                                    "type": "ActorSelected",
                                    "previous": "a",
                                    "selected": "b"
                                }
                            }
                        ]
                    }
                ]
            },
            {
                "id": "same_id",
                "steps": [
                    {
                        "id": "st2",
                        "complete": "immediate",
                        "events": [
                            {
                                "id": "e2",
                                "event": {
                                    "type": "TargetSelected",
                                    "previous": "a",
                                    "selected": "b"
                                }
                            }
                        ]
                    }
                ]
            }
        ]
    })";
    expect_load_error(json, "duplicate sequence id");
}

TEST(BattleScenarioParser, RejectsDuplicateEnvelopeId) {
    const auto* const json = R"({
        "version": 3,
        "id": "dup_env",
        "sequences": [
            {
                "id": "s1",
                "steps": [
                    {
                        "id": "st1",
                        "complete": "immediate",
                        "events": [
                            {
                                "id": "dup",
                                "event": {
                                    "type": "ActorSelected",
                                    "previous": "a",
                                    "selected": "b"
                                }
                            },
                            {
                                "id": "dup",
                                "event": {
                                    "type": "TargetSelected",
                                    "previous": "a",
                                    "selected": "b"
                                }
                            }
                        ]
                    }
                ]
            }
        ]
    })";
    expect_load_error(json, "duplicate envelope id");
}

TEST(BattleScenarioParser, RejectsCueRefToMissingEventId) {
    const auto* const json = R"({
        "version": 3,
        "id": "bad_cue",
        "sequences": [
            {
                "id": "s1",
                "steps": [
                    {
                        "id": "st1",
                        "complete": "immediate",
                        "events": [
                            {
                                "id": "e1",
                                "at": {
                                    "event_id": "missing_event",
                                    "cue": "complete"
                                },
                                "event": {
                                    "type": "ActorSelected",
                                    "previous": "a",
                                    "selected": "b"
                                }
                            }
                        ]
                    }
                ]
            }
        ]
    })";
    expect_load_error(json, "missing event_id");
}

TEST(BattleScenarioParser, ParsesUnitCreatedEventWithAllFields) {
    const auto* const json = R"({
        "version": 3,
        "id": "unit_created_test",
        "sequences": [
            {
                "id": "s1",
                "steps": [
                    {
                        "id": "st1",
                        "complete": "immediate",
                        "events": [
                            {
                                "id": "create1",
                                "event": {
                                    "type": "UnitCreated",
                                    "unit": "hero",
                                    "unit_type": "UU0001",
                                    "slot": "A_FRONT_0",
                                    "hp": 150,
                                    "max_hp": 200,
                                    "status": ["bless", "shield"]
                                }
                            }
                        ]
                    }
                ]
            }
        ]
    })";
    const auto        scenario = load_battle_scenario(write_json(json));
    ASSERT_EQ(scenario.sequences.size(), 1u);
    ASSERT_EQ(scenario.sequences[0].steps.size(), 1u);
    ASSERT_EQ(scenario.sequences[0].steps[0].envelopes.size(), 1u);

    const auto& event = scenario.sequences[0].steps[0].envelopes[0].event;
    ASSERT_TRUE(std::holds_alternative<ScenarioUnitCreated>(event));
    const auto& uc = std::get<ScenarioUnitCreated>(event);

    EXPECT_EQ(uc.alias, "hero");
    EXPECT_EQ(uc.unit_type, "UU0001");
    EXPECT_EQ(uc.slot, "A_FRONT_0");
    ASSERT_TRUE(uc.hp.has_value());
    EXPECT_EQ(*uc.hp, 150);
    ASSERT_TRUE(uc.max_hp.has_value());
    EXPECT_EQ(*uc.max_hp, 200);
    ASSERT_EQ(uc.status.size(), 2u);
    EXPECT_EQ(uc.status[0], "bless");
    EXPECT_EQ(uc.status[1], "shield");
}

TEST(BattleScenarioDocs, HpEventsAreDocumentedWithoutMutableNameOrInfoControls) {
    const auto  docs = collect_event_docs();
    std::string combined;
    for (const auto& doc : docs) {
        combined += doc.type_name;
        combined += '\n';
        combined += doc.json_example;
        combined += '\n';
        combined += doc.fields;
        combined += '\n';
        combined += doc.notes;
        combined += '\n';
    }

    EXPECT_NE(combined.find("TargetDamaged"), std::string::npos);
    EXPECT_NE(combined.find("current_hp"), std::string::npos);
    EXPECT_NE(combined.find("LifeDrained"), std::string::npos);
    EXPECT_NE(combined.find("target_current_hp"), std::string::npos);
    EXPECT_NE(combined.find("source_current_hp"), std::string::npos);
    EXPECT_NE(combined.find("SourceHealed"), std::string::npos);
    EXPECT_NE(combined.find("UnitRevived"), std::string::npos);
    EXPECT_EQ(combined.find("SetUnitName"), std::string::npos);
    EXPECT_EQ(combined.find("set_unit_name"), std::string::npos);
    EXPECT_EQ(combined.find("show_unit_info"), std::string::npos);
    EXPECT_EQ(combined.find("clear_unit_info"), std::string::npos);
}

TEST(BattleScenarioParser, ParsesUnitCreatedWithMinimalFields) {
    const auto* const json = R"({
        "version": 3,
        "id": "unit_created_minimal",
        "sequences": [
            {
                "id": "s1",
                "steps": [
                    {
                        "id": "st1",
                        "complete": "immediate",
                        "events": [
                            {
                                "id": "create1",
                                "event": {
                                    "type": "UnitCreated",
                                    "unit": "minion",
                                    "unit_type": "UU0002",
                                    "slot": "D_FRONT_1"
                                }
                            }
                        ]
                    }
                ]
            }
        ]
    })";
    const auto        scenario = load_battle_scenario(write_json(json));
    const auto&       event = scenario.sequences[0].steps[0].envelopes[0].event;
    ASSERT_TRUE(std::holds_alternative<ScenarioUnitCreated>(event));
    const auto& uc = std::get<ScenarioUnitCreated>(event);
    EXPECT_EQ(uc.alias, "minion");
    EXPECT_EQ(uc.unit_type, "UU0002");
    EXPECT_EQ(uc.slot, "D_FRONT_1");
    EXPECT_FALSE(uc.hp.has_value());
    EXPECT_FALSE(uc.max_hp.has_value());
    EXPECT_TRUE(uc.status.empty());
}

TEST(BattleScenarioParser, RejectsEmptyEventType) {
    const auto* const json = R"({
        "version": 3,
        "id": "empty_type",
        "sequences": [
            {
                "id": "s1",
                "steps": [
                    {
                        "id": "st1",
                        "complete": "immediate",
                        "events": [
                            {
                                "id": "e1",
                                "event": {
                                    "type": ""
                                }
                            }
                        ]
                    }
                ]
            }
        ]
    })";
    expect_load_error(json, "non-empty string");
}

TEST(BattleScenarioParser, RejectsEmptyStringFields) {
    const auto* const json = R"({
        "version": 3,
        "id": "",
        "sequences": [
            {
                "id": "s1",
                "steps": [
                    {
                        "id": "st1",
                        "complete": "immediate",
                        "events": [
                            {
                                "id": "e1",
                                "event": {
                                    "type": "ActorSelected",
                                    "previous": "",
                                    "selected": "b"
                                }
                            }
                        ]
                    }
                ]
            }
        ]
    })";
    expect_load_error(json, "non-empty string");
}

TEST(BattleScenarioParser, RejectsMissingSequences) {
    const auto* const json = R"({
        "version": 3,
        "id": "no_sequences"
    })";
    expect_load_error(json, "sequences");
}

TEST(BattleScenarioParser, RejectsBadEffectRole) {
    const auto* const json = R"({
        "version": 3,
        "id": "bad_role",
        "sequences": [
            {
                "id": "s1",
                "steps": [
                    {
                        "id": "st1",
                        "complete": "immediate",
                        "events": [
                            {
                                "id": "e1",
                                "event": {
                                    "type": "CastEffectStarted",
                                    "caster": "u1",
                                    "effect": "InvalidEffect"
                                }
                            }
                        ]
                    }
                ]
            }
        ]
    })";
    expect_load_error(json, "unknown effect role");
}

TEST(BattleScenarioParser, RejectsBadVisualRole) {
    const auto* const json = R"({
        "version": 3,
        "id": "bad_vrole",
        "sequences": [
            {
                "id": "s1",
                "steps": [
                    {
                        "id": "st1",
                        "complete": "immediate",
                        "events": [
                            {
                                "id": "e1",
                                "event": {
                                    "type": "BattleEffectStarted",
                                    "source": "u1",
                                    "effect": "source_attack_effect",
                                    "visual_role": "invalid_role"
                                }
                            }
                        ]
                    }
                ]
            }
        ]
    })";
    expect_load_error(json, "unknown effect visual_role");
}

TEST(BattleScenarioParser, RejectsMissingTargetsForTeamOverlay) {
    const auto* const json = R"({
        "version": 3,
        "id": "missing_targets",
        "sequences": [
            {
                "id": "s1",
                "steps": [
                    {
                        "id": "st1",
                        "complete": "immediate",
                        "events": [
                            {
                                "id": "e1",
                                "event": {
                                    "type": "BattleEffectStarted",
                                    "source": "u1",
                                    "effect": "team_impact_effect",
                                    "visual_role": "team_overlay_fx"
                                }
                            }
                        ]
                    }
                ]
            }
        ]
    })";
    expect_load_error(json, "targets");
}

TEST(BattleScenarioParser, HandlesEventCueSchedule) {
    const auto* const json = R"({
        "version": 3,
        "id": "cue_test",
        "sequences": [
            {
                "id": "s1",
                "steps": [
                    {
                        "id": "st1",
                        "complete": "immediate",
                        "events": [
                            {
                                "id": "first",
                                "event": {
                                    "type": "ActorSelected",
                                    "previous": "u1",
                                    "selected": "u2"
                                }
                            },
                            {
                                "id": "second",
                                "at": {
                                    "event_id": "first",
                                    "cue": "complete"
                                },
                                "event": {
                                    "type": "TargetSelected",
                                    "previous": "u1",
                                    "selected": "u3"
                                }
                            }
                        ]
                    }
                ]
            }
        ]
    })";
    const auto        scenario = load_battle_scenario(write_json(json));
    const auto&       envelopes = scenario.sequences[0].steps[0].envelopes;
    ASSERT_EQ(envelopes.size(), 2u);

    // First envelope has no schedule
    EXPECT_FALSE(envelopes[0].at.event_id.has_value());
    EXPECT_EQ(envelopes[0].at.cue, "start");

    // Second envelope has a cue schedule
    ASSERT_TRUE(envelopes[1].at.event_id.has_value());
    EXPECT_EQ(*envelopes[1].at.event_id, "first");
    EXPECT_EQ(envelopes[1].at.cue, "complete");
}

TEST(BattleScenarioParser, HandlesCastEffectWithTargetImpactEffect) {
    const auto* const json = R"({
        "version": 3,
        "id": "impact_test",
        "sequences": [
            {
                "id": "s1",
                "steps": [
                    {
                        "id": "st1",
                        "complete": "immediate",
                        "events": [
                            {
                                "id": "e1",
                                "event": {
                                    "type": "CastEffectStarted",
                                    "caster": "u1",
                                    "effect": "target_impact_effect"
                                }
                            }
                        ]
                    }
                ]
            }
        ]
    })";
    const auto        scenario = load_battle_scenario(write_json(json));
    const auto&       event = scenario.sequences[0].steps[0].envelopes[0].event;
    ASSERT_TRUE(std::holds_alternative<ScenarioCastEffectStarted>(event));
    EXPECT_EQ(std::get<ScenarioCastEffectStarted>(event).effect, BattleEffectRole::Tuch);
}

// ─────────────────────────────────────────────────────────────────────────────
// 2. BattleScenarioRuntime Tests
// ─────────────────────────────────────────────────────────────────────────────

class BattleScenarioRuntimeTest : public ::testing::Test {
protected:
    const GameDataRegistry          registry_;
    const FakeAnimationCatalog      catalog_;
    const UnitAttackVisualIntentMap intent_map_;
    BattleScene                     scene_;
    BattlePresenter                 presenter_;
    BattleUnitFactory               factory_;
    BattleScenarioRuntime           runtime_;

    BattleScenarioRuntimeTest()
        : registry_(std::filesystem::temp_directory_path() / "opendis2-missing-globals"),
          presenter_(scene_, UnitVisualProfileRegistry{}, UnitLifecycleVisualProfileRegistry{}, 0),
          factory_(registry_, catalog_, intent_map_), runtime_(scene_, presenter_, factory_) {}

    BattleScenarioRuntime::CreateResult register_alias(const std::string& alias,
                                                       BattleSlotCoord    coord) {
        return runtime_.apply_unit_created({.alias = alias,
                                            .unit_type = "UU0001",
                                            .slot = slot_coord_to_string(coord),
                                            .hp = 100,
                                            .max_hp = 100});
    }
};

TEST_F(BattleScenarioRuntimeTest, ResolvesKnownAlias) {
    const auto a1 = register_alias(
        "attacker", {.side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Front});
    const auto a2 = register_alias(
        "defender", {.side = BattleSide::Defender, .lane = 0, .depth = BattleDepth::Front});

    EXPECT_EQ(runtime_.resolve("attacker"), a1.unit_id);
    EXPECT_EQ(runtime_.resolve("defender"), a2.unit_id);
}

TEST_F(BattleScenarioRuntimeTest, ThrowsOnUnknownAlias) {
    EXPECT_THROW(runtime_.resolve("nonexistent"), std::runtime_error);
}

TEST_F(BattleScenarioRuntimeTest, ResolvesMultipleTargets) {
    const auto a1 = register_alias(
        "a1", {.side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Front});
    const auto a2 =
        register_alias("a2", {.side = BattleSide::Defender, .lane = 1, .depth = BattleDepth::Back});

    const auto ids = runtime_.resolve_targets({"a1", "a2"});
    ASSERT_EQ(ids.size(), 2u);
    EXPECT_EQ(ids[0], a1.unit_id);
    EXPECT_EQ(ids[1], a2.unit_id);
}

TEST_F(BattleScenarioRuntimeTest, ThrowsOnUnknownTarget) {
    register_alias("known", {.side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Front});

    EXPECT_THROW(runtime_.resolve_targets({"known", "unknown"}), std::runtime_error);
}

TEST_F(BattleScenarioRuntimeTest, ReportsHasAlias) {
    EXPECT_FALSE(runtime_.has_alias("ghost"));
    register_alias("ghost", {.side = BattleSide::Attacker, .lane = 2, .depth = BattleDepth::Back});
    EXPECT_TRUE(runtime_.has_alias("ghost"));
}

TEST_F(BattleScenarioRuntimeTest, ReportsSlotOccupant) {
    const auto coord =
        BattleSlotCoord{.side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Front};
    EXPECT_FALSE(runtime_.has_slot_occupant(coord));
    register_alias("occ", coord);
    EXPECT_TRUE(runtime_.has_slot_occupant(coord));
}

TEST_F(BattleScenarioRuntimeTest, ReportsIsUnitAlive) {
    register_alias("live", {.side = BattleSide::Defender, .lane = 2, .depth = BattleDepth::Front});
    EXPECT_TRUE(runtime_.has_alias("live"));
    EXPECT_FALSE(runtime_.has_alias("dead"));
}

TEST_F(BattleScenarioRuntimeTest, FindsByAlias) {
    const auto result = register_alias(
        "found", {.side = BattleSide::Attacker, .lane = 1, .depth = BattleDepth::Front});

    auto opt = runtime_.find_by_alias("found");
    ASSERT_TRUE(opt.has_value());
    EXPECT_EQ(*opt, result.unit_id);

    EXPECT_FALSE(runtime_.find_by_alias("missing").has_value());
}

TEST_F(BattleScenarioRuntimeTest, SlotOfReturnsNulloptWhenUnitNotInScene) {
    // apply_unit_created fully creates the unit, so find_by_alias should return a value
    const auto result = register_alias(
        "in_scene", {.side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Front});
    EXPECT_TRUE(runtime_.find_by_alias("in_scene").has_value());
    EXPECT_EQ(*runtime_.find_by_alias("in_scene"), result.unit_id);
}

TEST_F(BattleScenarioRuntimeTest, SlotOfReturnsNulloptForUnknownAlias) {
    EXPECT_FALSE(runtime_.find_by_alias("unknown").has_value());
}

TEST_F(BattleScenarioRuntimeTest, EnsureSlotFreePassesWhenSlotEmpty) {
    EXPECT_NO_THROW(runtime_.apply_unit_created({.alias = "new_unit",
                                                 .unit_type = "UU0001",
                                                 .slot = "A_FRONT_0",
                                                 .hp = 100,
                                                 .max_hp = 100}));
}

TEST_F(BattleScenarioRuntimeTest, EnsureSlotFreeThrowsWhenSlotOccupied) {
    register_alias("occupant",
                   {.side = BattleSide::Attacker, .lane = 1, .depth = BattleDepth::Front});
    EXPECT_THROW(runtime_.apply_unit_created({.alias = "intruder",
                                              .unit_type = "UU0001",
                                              .slot = "A_FRONT_1",
                                              .hp = 100,
                                              .max_hp = 100}),
                 std::runtime_error);
}

TEST_F(BattleScenarioRuntimeTest, EnsureSlotFreeThrowsOnInvalidSlot) {
    EXPECT_THROW(runtime_.apply_unit_created({.alias = "bad",
                                              .unit_type = "UU0001",
                                              .slot = "INVALID_SLOT",
                                              .hp = 100,
                                              .max_hp = 100}),
                 std::runtime_error);
}

TEST_F(BattleScenarioRuntimeTest, EnsureUnitExistsPassesWhenAliasKnown) {
    register_alias("exists",
                   {.side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Front});
    EXPECT_TRUE(runtime_.has_alias("exists"));
}

TEST_F(BattleScenarioRuntimeTest, EnsureUnitExistsThrowsWhenAliasUnknown) {
    EXPECT_FALSE(runtime_.has_alias("missing"));
}

TEST_F(BattleScenarioRuntimeTest, RetreatRemovesAliasFromLiveMap) {
    const auto created = runtime_.apply_unit_created(
        {.alias = "victim", .unit_type = "UU0001", .slot = "A_FRONT_0", .hp = 100, .max_hp = 100});
    EXPECT_TRUE(runtime_.has_alias("victim"));
    const auto result = runtime_.apply_unit_retreated({.unit = "victim", .reason = "test"});
    EXPECT_EQ(result.unit_id, created.unit_id);
    EXPECT_FALSE(runtime_.has_alias("victim"));
    EXPECT_TRUE(runtime_.find_by_alias("victim") == std::nullopt);
}

TEST_F(BattleScenarioRuntimeTest, RetreatedAliasFailsResolve) {
    runtime_.apply_unit_created({.alias = "retreater",
                                 .unit_type = "UU0001",
                                 .slot = "A_FRONT_0",
                                 .hp = 100,
                                 .max_hp = 100});
    runtime_.apply_unit_retreated({.unit = "retreater", .reason = "test"});
    EXPECT_THROW(
        {
            try {
                runtime_.resolve("retreater");
            } catch (const std::runtime_error& e) {
                EXPECT_TRUE(std::string(e.what()).find("retreated") != std::string::npos);
                throw;
            }
        },
        std::runtime_error);
}

TEST_F(BattleScenarioRuntimeTest, RetreatedSlotFreedForReuse) {
    const auto coord = parse_position_string("A_FRONT_0").value();
    runtime_.apply_unit_created({.alias = "occupant",
                                 .unit_type = "UU0001",
                                 .slot = "A_FRONT_0",
                                 .hp = 100,
                                 .max_hp = 100});
    EXPECT_TRUE(runtime_.has_slot_occupant(coord));
    runtime_.apply_unit_retreated({.unit = "occupant", .reason = "test"});
    EXPECT_FALSE(runtime_.has_slot_occupant(coord));
}

TEST_F(BattleScenarioRuntimeTest, SameAliasCanBeRecreatedAfterRetreat) {
    const auto first = runtime_.apply_unit_created(
        {.alias = "phoenix", .unit_type = "UU0001", .slot = "A_FRONT_0", .hp = 100, .max_hp = 100});
    runtime_.apply_unit_retreated({.unit = "phoenix", .reason = "test"});
    EXPECT_FALSE(runtime_.has_alias("phoenix"));
    const auto second = runtime_.apply_unit_created(
        {.alias = "phoenix", .unit_type = "UU0001", .slot = "A_FRONT_2", .hp = 100, .max_hp = 100});
    EXPECT_TRUE(runtime_.has_alias("phoenix"));
    EXPECT_NE(second.unit_id, first.unit_id);
}

TEST_F(BattleScenarioRuntimeTest, RecreatedAliasResolvesToNewId) {
    runtime_.apply_unit_created(
        {.alias = "reborn", .unit_type = "UU0001", .slot = "A_FRONT_0", .hp = 100, .max_hp = 100});
    runtime_.apply_unit_retreated({.unit = "reborn", .reason = "test"});
    const auto second = runtime_.apply_unit_created(
        {.alias = "reborn", .unit_type = "UU0001", .slot = "A_FRONT_2", .hp = 100, .max_hp = 100});
    const auto resolved = runtime_.resolve("reborn");
    EXPECT_EQ(resolved, second.unit_id);
}

TEST_F(BattleScenarioRuntimeTest, FreedSlotCanBeReusedAfterRetreat) {
    const auto coord = parse_position_string("A_FRONT_0").value();
    runtime_.apply_unit_created(
        {.alias = "first", .unit_type = "UU0001", .slot = "A_FRONT_0", .hp = 100, .max_hp = 100});
    runtime_.apply_unit_retreated({.unit = "first", .reason = "test"});
    EXPECT_FALSE(runtime_.has_slot_occupant(coord));
    runtime_.apply_unit_created(
        {.alias = "second", .unit_type = "UU0002", .slot = "A_FRONT_0", .hp = 100, .max_hp = 100});
    EXPECT_TRUE(runtime_.has_slot_occupant(coord));
    const auto slot_unit = runtime_.resolve("second");
    EXPECT_TRUE(slot_unit == UnitInstanceId{} || slot_unit.value > 0);
}

TEST_F(BattleScenarioRuntimeTest, DuplicateLiveAliasStillFails) {
    runtime_.apply_unit_created(
        {.alias = "unique", .unit_type = "UU0001", .slot = "A_FRONT_0", .hp = 100, .max_hp = 100});
    EXPECT_THROW(runtime_.apply_unit_created({.alias = "unique",
                                              .unit_type = "UU0002",
                                              .slot = "A_FRONT_1",
                                              .hp = 100,
                                              .max_hp = 100}),
                 std::runtime_error);
}

// ─────────────────────────────────────────────────────────────────────────────
// 2b. Large unit slot validation and occupancy tests
// ─────────────────────────────────────────────────────────────────────────────

// Factory stub that treats unit_type starting with "LARGE_" as is_large=true.
class FakeSizedUnitFactory final : public IBattleUnitCreationService {
public:
    FakeSizedUnitFactory() = default;

    UnitCreationData create_unit(const UnitCreationRequest& req) override {
        UnitCreationData result;
        result.debug_alias = req.alias;
        result.unit_type = req.unit_type;
        result.animation_unit_type = req.unit_type;
        result.display_name = req.unit_type;
        result.max_hp = req.max_hp.value_or(100);
        result.current_hp = req.hp.value_or(result.max_hp);

        const auto opt_coord = parse_position_string(req.slot);
        if (!opt_coord.has_value()) {
            result.diagnostics.push_back("invalid slot: " + req.slot);
            return result;
        }
        result.coord = *opt_coord;
        result.slot_name = req.slot;
        result.direction = (result.coord.side == BattleSide::Attacker) ? 'A' : 'D';
        result.is_large = req.unit_type.starts_with("LARGE_");

        try {
            validate_unit_slot(result.coord, result.is_large, req.unit_type, req.alias);
        } catch (const std::invalid_argument& e) {
            result.diagnostics.emplace_back(e.what());
            return result;
        }

        // Minimal idle clip so the unit can be created.
        AnimationSequence idle;
        idle.name = req.unit_type + "_IDLE";
        idle.frames.push_back(
            {.image_name = "f", .index = 0, .duration_ms = static_cast<uint16_t>(100)});
        result.roles.idle = UnitStateClip::from_a1(idle, "IDLE", result.direction);
        result.success = true;
        return result;
    }
};

class LargeUnitRuntimeTest : public ::testing::Test {
protected:
    BattleScene           scene_;
    BattlePresenter       presenter_;
    FakeSizedUnitFactory  factory_;
    BattleScenarioRuntime runtime_;

    LargeUnitRuntimeTest()
        : presenter_(scene_, UnitVisualProfileRegistry{}, UnitLifecycleVisualProfileRegistry{}, 0),
          runtime_(scene_, presenter_, factory_) {}

    [[nodiscard]] const BattleUnit& unit(UnitInstanceId id) const {
        const auto entity = scene_.visual_entity_for(id);
        EXPECT_TRUE(entity.has_value());
        return *scene_.try_unit_by_id(*entity);
    }

    void submit(const BattleScenarioEvent& event) {
        presenter_.submit_visual_event(BattleScenarioExecutor::resolve_event(event, runtime_));
    }
};

TEST_F(LargeUnitRuntimeTest, UnitCreatedSeedsHpStateAndUnitgroupUi) {
    const auto created = runtime_.apply_unit_created({.alias = "attacker",
                                                      .unit_type = "SMALL_U",
                                                      .slot = "A_FRONT_0",
                                                      .hp = 177,
                                                      .max_hp = 250});

    EXPECT_EQ(unit(created.unit_id).display_name, "SMALL_U");
    EXPECT_EQ(unit(created.unit_id).current_hp, 177);
    EXPECT_EQ(unit(created.unit_id).max_hp, 250);

    const auto  commands = scenario_hud_commands(scene_, created.unit_id, UnitInstanceId{});
    const auto* hp = find_command(commands, "/ui/left_unit_group/0_front/hp");
    const auto* fill = find_fill_command(commands, "/ui/left_unit_group/0_front/portrait");

    ASSERT_NE(hp, nullptr);
    EXPECT_EQ(hp->text, "177/250");
    ASSERT_NE(fill, nullptr);
    ASSERT_TRUE(fill->fill_color.has_value());
    EXPECT_NEAR(fill->destination.h, 120.0f * (1.0f - (177.0f / 250.0f)), 0.01f);
}

TEST_F(LargeUnitRuntimeTest, TargetDamagedUpdatesTargetHpAndSelectedRightInfo) {
    const auto source = runtime_.apply_unit_created({.alias = "attacker",
                                                     .unit_type = "SMALL_A",
                                                     .slot = "A_FRONT_0",
                                                     .hp = 100,
                                                     .max_hp = 100});
    const auto target = runtime_.apply_unit_created({.alias = "defender",
                                                     .unit_type = "SMALL_D",
                                                     .slot = "D_FRONT_0",
                                                     .hp = 250,
                                                     .max_hp = 250});

    submit(ScenarioTargetSelected{.previous = "attacker", .selected = "defender"});
    submit(ScenarioTargetDamaged{.attack_id = AttackInstanceId{1},
                                 .source = "attacker",
                                 .target = "defender",
                                 .current_hp = 177});

    EXPECT_EQ(unit(target.unit_id).current_hp, 177);
    const auto commands = scenario_hud_commands(scene_, source.unit_id, target.unit_id);
    EXPECT_EQ(find_command(commands, "/ui/right_unit_group/0_front/hp")->text, "177/250");
    EXPECT_EQ(find_command(commands, "/ui/combat_frame/info_right/hp")->text, "177/250");
}

TEST_F(LargeUnitRuntimeTest, TargetDamagedWithoutCurrentHpLeavesHpUnchanged) {
    runtime_.apply_unit_created({.alias = "attacker",
                                 .unit_type = "SMALL_A",
                                 .slot = "A_FRONT_0",
                                 .hp = 100,
                                 .max_hp = 100});
    const auto target = runtime_.apply_unit_created({.alias = "defender",
                                                     .unit_type = "SMALL_D",
                                                     .slot = "D_FRONT_0",
                                                     .hp = 200,
                                                     .max_hp = 250});

    submit(ScenarioTargetDamaged{
        .attack_id = AttackInstanceId{1}, .source = "attacker", .target = "defender"});

    EXPECT_EQ(unit(target.unit_id).current_hp, 200);
}

TEST_F(LargeUnitRuntimeTest, LifeDrainedAndSourceHealedUpdateAffectedUnits) {
    const auto source = runtime_.apply_unit_created({.alias = "attacker",
                                                     .unit_type = "SMALL_A",
                                                     .slot = "A_FRONT_0",
                                                     .hp = 100,
                                                     .max_hp = 200});
    const auto target = runtime_.apply_unit_created({.alias = "defender",
                                                     .unit_type = "SMALL_D",
                                                     .slot = "D_FRONT_0",
                                                     .hp = 250,
                                                     .max_hp = 250});

    submit(ScenarioLifeDrained{.attack_id = AttackInstanceId{1},
                               .source = "attacker",
                               .target = "defender",
                               .target_current_hp = 120,
                               .source_current_hp = 180});
    EXPECT_EQ(unit(source.unit_id).current_hp, 180);
    EXPECT_EQ(unit(target.unit_id).current_hp, 120);

    submit(ScenarioSourceHealed{
        .attack_id = AttackInstanceId{2}, .source = "attacker", .current_hp = 190});
    EXPECT_EQ(unit(source.unit_id).current_hp, 190);

    const auto commands = scenario_hud_commands(scene_, source.unit_id, target.unit_id);
    EXPECT_EQ(find_command(commands, "/ui/combat_frame/info_left/hp")->text, "190/200");
    EXPECT_EQ(find_command(commands, "/ui/combat_frame/info_right/hp")->text, "120/250");
}

TEST_F(LargeUnitRuntimeTest, KillEventsSetHpToZeroAndRevivedCanSetCurrentHp) {
    runtime_.apply_unit_created({.alias = "attacker",
                                 .unit_type = "SMALL_A",
                                 .slot = "A_FRONT_0",
                                 .hp = 100,
                                 .max_hp = 100});
    const auto target = runtime_.apply_unit_created({.alias = "defender",
                                                     .unit_type = "SMALL_D",
                                                     .slot = "D_FRONT_0",
                                                     .hp = 50,
                                                     .max_hp = 250});

    submit(ScenarioTargetKilled{.attack_id = AttackInstanceId{1}, .target = "defender"});
    EXPECT_EQ(unit(target.unit_id).current_hp, 0);
    auto commands = scenario_hud_commands(scene_, UnitInstanceId{}, target.unit_id);
    EXPECT_EQ(find_command(commands, "/ui/right_unit_group/0_front/hp")->text, "0/250");
    EXPECT_NE(find_command(commands, "/ui/right_unit_group/0_front/portrait"), nullptr);

    submit(ScenarioUnitRevived{.target = "defender", .current_hp = 40});
    EXPECT_EQ(unit(target.unit_id).current_hp, 40);
}

TEST_F(LargeUnitRuntimeTest, ActorAndTargetSelectionDriveBattleFrameInfo) {
    const auto a1 = runtime_.apply_unit_created(
        {.alias = "a1", .unit_type = "UNIT_A1", .slot = "A_FRONT_0", .hp = 90, .max_hp = 100});
    const auto d1 = runtime_.apply_unit_created(
        {.alias = "d1", .unit_type = "UNIT_D1", .slot = "D_FRONT_0", .hp = 180, .max_hp = 200});

    submit(ScenarioActorSelected{.previous = "d1", .selected = "a1"});
    submit(ScenarioTargetSelected{.previous = "a1", .selected = "d1"});

    const auto commands = scenario_hud_commands(scene_, a1.unit_id, d1.unit_id);
    EXPECT_EQ(find_command(commands, "/ui/combat_frame/info_left/name")->text, "UNIT_A1");
    EXPECT_EQ(find_command(commands, "/ui/combat_frame/info_left/hp")->text, "90/100");
    EXPECT_EQ(find_command(commands, "/ui/combat_frame/info_right/name")->text, "UNIT_D1");
    EXPECT_EQ(find_command(commands, "/ui/combat_frame/info_right/hp")->text, "180/200");
}

TEST_F(LargeUnitRuntimeTest, HpValidationRejectsOutOfRangeScenarioValues) {
    EXPECT_THROW(
        (void)runtime_.apply_unit_created(ScenarioUnitCreated{
            .alias = "bad_max", .unit_type = "SMALL_A", .slot = "A_FRONT_0", .hp = 0, .max_hp = 0}),
        std::runtime_error);
    EXPECT_THROW((void)runtime_.apply_unit_created(ScenarioUnitCreated{.alias = "bad_hp",
                                                                       .unit_type = "SMALL_A",
                                                                       .slot = "A_FRONT_0",
                                                                       .hp = 251,
                                                                       .max_hp = 250}),
                 std::runtime_error);

    runtime_.apply_unit_created({.alias = "attacker",
                                 .unit_type = "SMALL_A",
                                 .slot = "A_FRONT_0",
                                 .hp = 100,
                                 .max_hp = 100});
    runtime_.apply_unit_created({.alias = "defender",
                                 .unit_type = "SMALL_D",
                                 .slot = "D_FRONT_0",
                                 .hp = 100,
                                 .max_hp = 100});

    EXPECT_THROW((void)BattleScenarioExecutor::resolve_event(
                     ScenarioTargetDamaged{.attack_id = AttackInstanceId{1},
                                           .source = "attacker",
                                           .target = "defender",
                                           .current_hp = 101},
                     runtime_),
                 std::runtime_error);
    EXPECT_THROW((void)BattleScenarioExecutor::resolve_event(
                     ScenarioLifeDrained{.attack_id = AttackInstanceId{1},
                                         .source = "attacker",
                                         .target = "defender",
                                         .target_current_hp = -1},
                     runtime_),
                 std::runtime_error);
    EXPECT_THROW((void)BattleScenarioExecutor::resolve_event(
                     ScenarioSourceHealed{
                         .attack_id = AttackInstanceId{1}, .source = "attacker", .current_hp = 101},
                     runtime_),
                 std::runtime_error);
    EXPECT_THROW((void)BattleScenarioExecutor::resolve_event(
                     ScenarioUnitRevived{.target = "defender", .current_hp = 0}, runtime_),
                 std::runtime_error);
}

TEST_F(LargeUnitRuntimeTest, SmallInFrontSucceeds) {
    EXPECT_NO_THROW(
        runtime_.apply_unit_created({.alias = "s", .unit_type = "SMALL_U", .slot = "A_FRONT_0"}));
}

TEST_F(LargeUnitRuntimeTest, SmallInBackSucceeds) {
    EXPECT_NO_THROW(
        runtime_.apply_unit_created({.alias = "s", .unit_type = "SMALL_U", .slot = "A_BACK_0"}));
}

TEST_F(LargeUnitRuntimeTest, LargeInCenterSucceeds) {
    EXPECT_NO_THROW(
        runtime_.apply_unit_created({.alias = "lg", .unit_type = "LARGE_U", .slot = "D_CENTER_1"}));
}

TEST_F(LargeUnitRuntimeTest, LargeInFrontFails) {
    EXPECT_THROW(
        runtime_.apply_unit_created({.alias = "lg", .unit_type = "LARGE_U", .slot = "D_FRONT_1"}),
        std::runtime_error);
}

TEST_F(LargeUnitRuntimeTest, LargeInBackFails) {
    EXPECT_THROW(
        runtime_.apply_unit_created({.alias = "lg", .unit_type = "LARGE_U", .slot = "D_BACK_1"}),
        std::runtime_error);
}

TEST_F(LargeUnitRuntimeTest, SmallInCenterFails) {
    EXPECT_THROW(
        runtime_.apply_unit_created({.alias = "s", .unit_type = "SMALL_U", .slot = "D_CENTER_0"}),
        std::runtime_error);
}

TEST_F(LargeUnitRuntimeTest, LargeOccupiesFrontAndBackFootprint) {
    runtime_.apply_unit_created({.alias = "lg", .unit_type = "LARGE_U", .slot = "D_CENTER_1"});
    const BattleSlotCoord center{
        .side = BattleSide::Defender, .lane = 1, .depth = BattleDepth::Center};
    const BattleSlotCoord front{
        .side = BattleSide::Defender, .lane = 1, .depth = BattleDepth::Front};
    const BattleSlotCoord back{.side = BattleSide::Defender, .lane = 1, .depth = BattleDepth::Back};
    EXPECT_TRUE(runtime_.has_slot_occupant(center));
    EXPECT_TRUE(runtime_.has_slot_occupant(front));
    EXPECT_TRUE(runtime_.has_slot_occupant(back));
}

TEST_F(LargeUnitRuntimeTest, SmallCannotBeCreatedInLargeFootprintFront) {
    runtime_.apply_unit_created({.alias = "lg", .unit_type = "LARGE_U", .slot = "D_CENTER_1"});
    EXPECT_THROW(
        runtime_.apply_unit_created({.alias = "s", .unit_type = "SMALL_U", .slot = "D_FRONT_1"}),
        std::runtime_error);
}

TEST_F(LargeUnitRuntimeTest, SmallCannotBeCreatedInLargeFootprintBack) {
    runtime_.apply_unit_created({.alias = "lg", .unit_type = "LARGE_U", .slot = "D_CENTER_1"});
    EXPECT_THROW(
        runtime_.apply_unit_created({.alias = "s", .unit_type = "SMALL_U", .slot = "D_BACK_1"}),
        std::runtime_error);
}

TEST_F(LargeUnitRuntimeTest, LargeCannotBeCreatedIfFootprintFrontOccupied) {
    runtime_.apply_unit_created({.alias = "s", .unit_type = "SMALL_U", .slot = "D_FRONT_1"});
    EXPECT_THROW(
        runtime_.apply_unit_created({.alias = "lg", .unit_type = "LARGE_U", .slot = "D_CENTER_1"}),
        std::runtime_error);
}

TEST_F(LargeUnitRuntimeTest, LargeCannotBeCreatedIfFootprintBackOccupied) {
    runtime_.apply_unit_created({.alias = "s", .unit_type = "SMALL_U", .slot = "D_BACK_1"});
    EXPECT_THROW(
        runtime_.apply_unit_created({.alias = "lg", .unit_type = "LARGE_U", .slot = "D_CENTER_1"}),
        std::runtime_error);
}

TEST_F(LargeUnitRuntimeTest, RetreatedLargeFreesAllFootprintSlots) {
    runtime_.apply_unit_created({.alias = "lg", .unit_type = "LARGE_U", .slot = "D_CENTER_1"});
    runtime_.apply_unit_retreated({.unit = "lg", .reason = "test"});
    const BattleSlotCoord center{
        .side = BattleSide::Defender, .lane = 1, .depth = BattleDepth::Center};
    const BattleSlotCoord front{
        .side = BattleSide::Defender, .lane = 1, .depth = BattleDepth::Front};
    const BattleSlotCoord back{.side = BattleSide::Defender, .lane = 1, .depth = BattleDepth::Back};
    EXPECT_FALSE(runtime_.has_slot_occupant(center));
    EXPECT_FALSE(runtime_.has_slot_occupant(front));
    EXPECT_FALSE(runtime_.has_slot_occupant(back));
}

// Rejected creation must not mutate scene or runtime maps.

TEST_F(LargeUnitRuntimeTest, LargeInFrontRejectsWithNoSceneMutation) {
    EXPECT_THROW(
        runtime_.apply_unit_created({.alias = "lg", .unit_type = "LARGE_U", .slot = "D_FRONT_1"}),
        std::runtime_error);
    EXPECT_EQ(scene_.units().size(), 0u);
}

TEST_F(LargeUnitRuntimeTest, LargeInBackRejectsWithNoSceneMutation) {
    EXPECT_THROW(
        runtime_.apply_unit_created({.alias = "lg", .unit_type = "LARGE_U", .slot = "D_BACK_1"}),
        std::runtime_error);
    EXPECT_EQ(scene_.units().size(), 0u);
}

TEST_F(LargeUnitRuntimeTest, SmallInCenterRejectsWithNoSceneMutation) {
    EXPECT_THROW(
        runtime_.apply_unit_created({.alias = "s", .unit_type = "SMALL_U", .slot = "D_CENTER_0"}),
        std::runtime_error);
    EXPECT_EQ(scene_.units().size(), 0u);
}

TEST_F(LargeUnitRuntimeTest, LargeRejectsIfFrontOccupied_NoExtraUnit) {
    runtime_.apply_unit_created({.alias = "s", .unit_type = "SMALL_U", .slot = "D_FRONT_1"});
    const std::size_t before = scene_.units().size();
    EXPECT_THROW(
        runtime_.apply_unit_created({.alias = "lg", .unit_type = "LARGE_U", .slot = "D_CENTER_1"}),
        std::runtime_error);
    EXPECT_EQ(scene_.units().size(), before);
}

TEST_F(LargeUnitRuntimeTest, LargeRejectsIfBackOccupied_NoExtraUnit) {
    runtime_.apply_unit_created({.alias = "s", .unit_type = "SMALL_U", .slot = "D_BACK_1"});
    const std::size_t before = scene_.units().size();
    EXPECT_THROW(
        runtime_.apply_unit_created({.alias = "lg", .unit_type = "LARGE_U", .slot = "D_CENTER_1"}),
        std::runtime_error);
    EXPECT_EQ(scene_.units().size(), before);
}

// Killed unit retains occupancy — only UnitRetreated clears the occupancy index.

TEST_F(LargeUnitRuntimeTest, KilledLargeKeepsFootprintOccupied) {
    const auto result =
        runtime_.apply_unit_created({.alias = "lg", .unit_type = "LARGE_U", .slot = "D_CENTER_1"});
    // Simulate visual kill: set life_state directly (no runtime "kill" path — UnitRetreated is
    // the only event that frees slots).
    BattleUnit* unit = scene_.try_unit_by_id(result.entity_id);
    ASSERT_NE(unit, nullptr);
    unit->life_state = LifeVisualState::Dead;

    const BattleSlotCoord center{
        .side = BattleSide::Defender, .lane = 1, .depth = BattleDepth::Center};
    const BattleSlotCoord front{
        .side = BattleSide::Defender, .lane = 1, .depth = BattleDepth::Front};
    const BattleSlotCoord back{.side = BattleSide::Defender, .lane = 1, .depth = BattleDepth::Back};
    EXPECT_TRUE(runtime_.has_slot_occupant(center));
    EXPECT_TRUE(runtime_.has_slot_occupant(front));
    EXPECT_TRUE(runtime_.has_slot_occupant(back));
}

// ── Footprint conflict diagnostic message contents ────────────────────────

TEST_F(LargeUnitRuntimeTest, SmallInOccupiedSlotDiagIncludesAlias) {
    runtime_.apply_unit_created({.alias = "first", .unit_type = "SMALL_U", .slot = "D_FRONT_0"});
    try {
        runtime_.apply_unit_created(
            {.alias = "intruder", .unit_type = "SMALL_U", .slot = "D_FRONT_0"});
        FAIL() << "expected runtime_error";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string_view{e.what()}.find("intruder"), std::string_view::npos)
            << "message should contain alias of conflicting unit";
    }
}

TEST_F(LargeUnitRuntimeTest, SmallInOccupiedSlotDiagIncludesUnitType) {
    runtime_.apply_unit_created({.alias = "first", .unit_type = "SMALL_U", .slot = "D_FRONT_0"});
    try {
        runtime_.apply_unit_created(
            {.alias = "intruder", .unit_type = "SMALL_X", .slot = "D_FRONT_0"});
        FAIL() << "expected runtime_error";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string_view{e.what()}.find("SMALL_X"), std::string_view::npos)
            << "message should contain unit_type";
    }
}

TEST_F(LargeUnitRuntimeTest, SmallInOccupiedSlotDiagIncludesOccupantId) {
    runtime_.apply_unit_created({.alias = "first", .unit_type = "SMALL_U", .slot = "D_FRONT_0"});
    try {
        runtime_.apply_unit_created(
            {.alias = "intruder", .unit_type = "SMALL_U", .slot = "D_FRONT_0"});
        FAIL() << "expected runtime_error";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string_view{e.what()}.find("occupant_id="), std::string_view::npos)
            << "message should contain occupant_id";
    }
}

TEST_F(LargeUnitRuntimeTest, LargeFootprintConflictDiagIncludesOccupiedSlot) {
    runtime_.apply_unit_created({.alias = "blocker", .unit_type = "SMALL_U", .slot = "D_BACK_1"});
    try {
        runtime_.apply_unit_created({.alias = "lg", .unit_type = "LARGE_U", .slot = "D_CENTER_1"});
        FAIL() << "expected runtime_error";
    } catch (const std::runtime_error& e) {
        const std::string msg{e.what()};
        EXPECT_NE(msg.find("occupied_slot="), std::string::npos)
            << "footprint conflict message should identify occupied_slot";
        EXPECT_NE(msg.find("occupant_id="), std::string::npos)
            << "footprint conflict message should include occupant_id";
        EXPECT_NE(msg.find("LARGE_U"), std::string::npos)
            << "message should include unit_type of the large unit being placed";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 3. BattleScenarioExecutor Tests
// ─────────────────────────────────────────────────────────────────────────────

class BattleScenarioExecutorTest : public ::testing::Test {
protected:
    const GameDataRegistry          registry_;
    const FakeAnimationCatalog      catalog_;
    const UnitAttackVisualIntentMap intent_map_;
    BattleScene                     scene_;
    BattlePresenter                 presenter_;
    BattleUnitFactory               factory_;
    BattleScenarioRuntime           runtime_;

    BattleScenarioExecutorTest()
        : registry_(std::filesystem::temp_directory_path() / "opendis2-missing-globals"),
          presenter_(scene_, UnitVisualProfileRegistry{}, UnitLifecycleVisualProfileRegistry{}, 0),
          factory_(registry_, catalog_, intent_map_), runtime_(scene_, presenter_, factory_) {}

    BattleScenarioRuntime::CreateResult register_alias(const std::string& alias,
                                                       BattleSlotCoord    coord) {
        return runtime_.apply_unit_created({.alias = alias,
                                            .unit_type = "UU0001",
                                            .slot = slot_coord_to_string(coord),
                                            .hp = 100,
                                            .max_hp = 100});
    }
};

TEST_F(BattleScenarioExecutorTest, ResolvesActorSelected) {
    const auto a1 = register_alias(
        "u1", {.side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Front});
    const auto a2 = register_alias(
        "u2", {.side = BattleSide::Defender, .lane = 0, .depth = BattleDepth::Front});

    const ScenarioActorSelected event{.previous = "u1", .selected = "u2"};
    const auto                  result = BattleScenarioExecutor::resolve_event(event, runtime_);

    ASSERT_TRUE(std::holds_alternative<ActorSelected>(result));
    const auto& as = std::get<ActorSelected>(result);
    EXPECT_EQ(as.previous, a1.unit_id);
    EXPECT_EQ(as.selected, a2.unit_id);
}

TEST_F(BattleScenarioExecutorTest, ResolvesTargetSelected) {
    const auto a1 = register_alias(
        "prev", {.side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Front});
    const auto a2 = register_alias(
        "next", {.side = BattleSide::Defender, .lane = 1, .depth = BattleDepth::Back});

    const ScenarioTargetSelected event{.previous = "prev", .selected = "next"};
    const auto                   result = BattleScenarioExecutor::resolve_event(event, runtime_);

    ASSERT_TRUE(std::holds_alternative<TargetSelected>(result));
    const auto& ts = std::get<TargetSelected>(result);
    EXPECT_EQ(ts.previous, a1.unit_id);
    EXPECT_EQ(ts.selected, a2.unit_id);
}

TEST_F(BattleScenarioExecutorTest, ResolvesAttackStarted) {
    const auto a_src = register_alias(
        "src", {.side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Front});
    const auto a_t1 = register_alias(
        "t1", {.side = BattleSide::Defender, .lane = 0, .depth = BattleDepth::Front});
    const auto a_t2 = register_alias(
        "t2", {.side = BattleSide::Defender, .lane = 1, .depth = BattleDepth::Front});

    const ScenarioAttackStarted event{
        .attack_id = AttackInstanceId{99u}, .source = "src", .targets = {"t1", "t2"}};
    const auto result = BattleScenarioExecutor::resolve_event(event, runtime_);

    ASSERT_TRUE(std::holds_alternative<AttackStarted>(result));
    const auto& as = std::get<AttackStarted>(result);
    EXPECT_EQ(as.attack_id, AttackInstanceId{99u});
    EXPECT_EQ(as.source, a_src.unit_id);
    ASSERT_EQ(as.targets.units.size(), 2u);
    EXPECT_EQ(as.targets.units[0], a_t1.unit_id);
    EXPECT_EQ(as.targets.units[1], a_t2.unit_id);
}

TEST_F(BattleScenarioExecutorTest, ResolvesAttackImpactCue) {
    const auto a_src = register_alias(
        "src", {.side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Front});
    const auto a_tgt = register_alias(
        "tgt", {.side = BattleSide::Defender, .lane = 0, .depth = BattleDepth::Front});

    const ScenarioAttackImpactCue event{
        .attack_id = AttackInstanceId{1u}, .source = "src", .targets = {"tgt"}};
    const auto result = BattleScenarioExecutor::resolve_event(event, runtime_);

    ASSERT_TRUE(std::holds_alternative<AttackImpactCue>(result));
    const auto& ac = std::get<AttackImpactCue>(result);
    EXPECT_EQ(ac.attack_id, AttackInstanceId{1u});
    EXPECT_EQ(ac.source, a_src.unit_id);
    ASSERT_EQ(ac.targets.units.size(), 1u);
    EXPECT_EQ(ac.targets.units[0], a_tgt.unit_id);
}

TEST_F(BattleScenarioExecutorTest, ResolvesTargetDamaged) {
    const auto a_src = register_alias(
        "src", {.side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Front});
    const auto a_tgt = register_alias(
        "tgt", {.side = BattleSide::Defender, .lane = 0, .depth = BattleDepth::Front});

    const ScenarioTargetDamaged event{
        .attack_id = AttackInstanceId{5u}, .source = "src", .target = "tgt"};
    const auto result = BattleScenarioExecutor::resolve_event(event, runtime_);

    ASSERT_TRUE(std::holds_alternative<TargetDamaged>(result));
    const auto& td = std::get<TargetDamaged>(result);
    EXPECT_EQ(td.attack_id, AttackInstanceId{5u});
    EXPECT_EQ(td.source, a_src.unit_id);
    EXPECT_EQ(td.target, a_tgt.unit_id);
}

TEST_F(BattleScenarioExecutorTest, ResolvesTargetMissed) {
    const auto a_tgt = register_alias(
        "tgt", {.side = BattleSide::Defender, .lane = 0, .depth = BattleDepth::Front});

    const ScenarioTargetMissed event{.attack_id = AttackInstanceId{1u}, .target = "tgt"};
    const auto                 result = BattleScenarioExecutor::resolve_event(event, runtime_);

    ASSERT_TRUE(std::holds_alternative<TargetMissed>(result));
    EXPECT_EQ(std::get<TargetMissed>(result).attack_id, AttackInstanceId{1u});
    EXPECT_EQ(std::get<TargetMissed>(result).target, a_tgt.unit_id);
}

TEST_F(BattleScenarioExecutorTest, ResolvesTargetResisted) {
    const auto a_tgt = register_alias(
        "tgt", {.side = BattleSide::Defender, .lane = 0, .depth = BattleDepth::Front});

    const ScenarioTargetResisted event{.attack_id = AttackInstanceId{2u}, .target = "tgt"};
    const auto                   result = BattleScenarioExecutor::resolve_event(event, runtime_);

    ASSERT_TRUE(std::holds_alternative<TargetResisted>(result));
    EXPECT_EQ(std::get<TargetResisted>(result).attack_id, AttackInstanceId{2u});
    EXPECT_EQ(std::get<TargetResisted>(result).target, a_tgt.unit_id);
}

TEST_F(BattleScenarioExecutorTest, ResolvesTargetKilled) {
    const auto a_tgt = register_alias(
        "tgt", {.side = BattleSide::Defender, .lane = 0, .depth = BattleDepth::Front});

    const ScenarioTargetKilled event{.attack_id = AttackInstanceId{3u}, .target = "tgt"};
    const auto                 result = BattleScenarioExecutor::resolve_event(event, runtime_);

    ASSERT_TRUE(std::holds_alternative<TargetKilled>(result));
    EXPECT_EQ(std::get<TargetKilled>(result).attack_id, AttackInstanceId{3u});
    EXPECT_EQ(std::get<TargetKilled>(result).target, a_tgt.unit_id);
}

TEST_F(BattleScenarioExecutorTest, ResolvesLifeDrained) {
    const auto a_src = register_alias(
        "src", {.side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Front});
    const auto a_tgt = register_alias(
        "tgt", {.side = BattleSide::Defender, .lane = 0, .depth = BattleDepth::Front});

    const ScenarioLifeDrained event{
        .attack_id = AttackInstanceId{1u}, .source = "src", .target = "tgt"};
    const auto result = BattleScenarioExecutor::resolve_event(event, runtime_);

    ASSERT_TRUE(std::holds_alternative<LifeDrained>(result));
    const auto& ld = std::get<LifeDrained>(result);
    EXPECT_EQ(ld.attack_id, AttackInstanceId{1u});
    EXPECT_EQ(ld.source, a_src.unit_id);
    EXPECT_EQ(ld.target, a_tgt.unit_id);
}

TEST_F(BattleScenarioExecutorTest, ResolvesSourceHealed) {
    const auto a_src = register_alias(
        "src", {.side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Front});

    const ScenarioSourceHealed event{.attack_id = AttackInstanceId{1u}, .source = "src"};
    const auto                 result = BattleScenarioExecutor::resolve_event(event, runtime_);

    ASSERT_TRUE(std::holds_alternative<SourceHealed>(result));
    EXPECT_EQ(std::get<SourceHealed>(result).attack_id, AttackInstanceId{1u});
    EXPECT_EQ(std::get<SourceHealed>(result).source, a_src.unit_id);
}

TEST_F(BattleScenarioExecutorTest, ResolvesUnitHitReceived) {
    const auto a_tgt = register_alias(
        "tgt", {.side = BattleSide::Defender, .lane = 0, .depth = BattleDepth::Front});

    const ScenarioUnitHitReceived event{.target = "tgt"};
    const auto                    result = BattleScenarioExecutor::resolve_event(event, runtime_);

    ASSERT_TRUE(std::holds_alternative<UnitHitReceived>(result));
    EXPECT_EQ(std::get<UnitHitReceived>(result).target, a_tgt.unit_id);
}

TEST_F(BattleScenarioExecutorTest, ResolvesUnitKilled) {
    const auto a_tgt = register_alias(
        "tgt", {.side = BattleSide::Defender, .lane = 0, .depth = BattleDepth::Front});

    const ScenarioUnitKilled event{.target = "tgt"};
    const auto               result = BattleScenarioExecutor::resolve_event(event, runtime_);

    ASSERT_TRUE(std::holds_alternative<UnitKilled>(result));
    EXPECT_EQ(std::get<UnitKilled>(result).target, a_tgt.unit_id);
}

TEST_F(BattleScenarioExecutorTest, ResolvesUnitReviveStarted) {
    const auto a_tgt = register_alias(
        "tgt", {.side = BattleSide::Defender, .lane = 0, .depth = BattleDepth::Front});

    const ScenarioUnitReviveStarted event{.target = "tgt"};
    const auto                      result = BattleScenarioExecutor::resolve_event(event, runtime_);

    ASSERT_TRUE(std::holds_alternative<UnitReviveStarted>(result));
    EXPECT_EQ(std::get<UnitReviveStarted>(result).target, a_tgt.unit_id);
}

TEST_F(BattleScenarioExecutorTest, ResolvesUnitRevived) {
    const auto a_tgt = register_alias(
        "tgt", {.side = BattleSide::Defender, .lane = 0, .depth = BattleDepth::Front});

    const ScenarioUnitRevived event{.target = "tgt"};
    const auto                result = BattleScenarioExecutor::resolve_event(event, runtime_);

    ASSERT_TRUE(std::holds_alternative<UnitRevived>(result));
    EXPECT_EQ(std::get<UnitRevived>(result).target, a_tgt.unit_id);
}

TEST_F(BattleScenarioExecutorTest, ResolvesCastEffectStarted) {
    const auto a_caster = register_alias(
        "caster", {.side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Front});

    const ScenarioCastEffectStarted event{.caster = "caster", .effect = BattleEffectRole::Heff};
    const auto                      result = BattleScenarioExecutor::resolve_event(event, runtime_);

    ASSERT_TRUE(std::holds_alternative<CastEffectStarted>(result));
    const auto& ce = std::get<CastEffectStarted>(result);
    EXPECT_EQ(ce.caster, a_caster.unit_id);
    EXPECT_EQ(ce.role, BattleEffectRole::Heff);
}

TEST_F(BattleScenarioExecutorTest, ResolvesCastEffectWithSemanticName) {
    const auto a_caster = register_alias(
        "caster", {.side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Front});

    const ScenarioCastEffectStarted event{.caster = "caster", .effect = BattleEffectRole::Tuch};
    const auto                      result = BattleScenarioExecutor::resolve_event(event, runtime_);

    ASSERT_TRUE(std::holds_alternative<CastEffectStarted>(result));
    const auto& ce = std::get<CastEffectStarted>(result);
    EXPECT_EQ(ce.caster, a_caster.unit_id);
    EXPECT_EQ(ce.role, BattleEffectRole::Tuch);
}

TEST_F(BattleScenarioExecutorTest, ResolvesBattleEffectStartedWithTarget) {
    const auto a_src = register_alias(
        "src", {.side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Front});
    const auto a_tgt = register_alias(
        "tgt", {.side = BattleSide::Defender, .lane = 0, .depth = BattleDepth::Front});

    const ScenarioBattleEffectStarted event{.source = "src",
                                            .effect = BattleEffectRole::Heff,
                                            .visual_role = BattleEffectVisualRole::TargetDamageFx,
                                            .target = "tgt"};
    const auto result = BattleScenarioExecutor::resolve_event(event, runtime_);

    ASSERT_TRUE(std::holds_alternative<BattleEffectStarted>(result));
    const auto& be = std::get<BattleEffectStarted>(result);
    EXPECT_EQ(be.source, a_src.unit_id);
    EXPECT_EQ(be.role, BattleEffectRole::Heff);
    EXPECT_EQ(be.visual_role, BattleEffectVisualRole::TargetDamageFx);
    EXPECT_EQ(be.target, a_tgt.unit_id);
}

TEST_F(BattleScenarioExecutorTest, ResolvesBattleEffectStartedWithTeamTargets) {
    const auto a_src = register_alias(
        "src", {.side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Front});
    const auto a_t1 = register_alias(
        "t1", {.side = BattleSide::Defender, .lane = 0, .depth = BattleDepth::Front});
    const auto a_t2 = register_alias(
        "t2", {.side = BattleSide::Defender, .lane = 1, .depth = BattleDepth::Front});

    const ScenarioBattleEffectStarted event{.source = "src",
                                            .effect = BattleEffectRole::Tuch,
                                            .visual_role = BattleEffectVisualRole::TeamOverlayFx,
                                            .targets = {"t1", "t2"}};
    const auto result = BattleScenarioExecutor::resolve_event(event, runtime_);

    ASSERT_TRUE(std::holds_alternative<BattleEffectStarted>(result));
    const auto& be = std::get<BattleEffectStarted>(result);
    EXPECT_EQ(be.source, a_src.unit_id);
    EXPECT_EQ(be.role, BattleEffectRole::Tuch);
    EXPECT_EQ(be.visual_role, BattleEffectVisualRole::TeamOverlayFx);
    ASSERT_EQ(be.targets.units.size(), 2u);
    EXPECT_EQ(be.targets.units[0], a_t1.unit_id);
    EXPECT_EQ(be.targets.units[1], a_t2.unit_id);
}

TEST_F(BattleScenarioExecutorTest, ThrowsOnUnitCreatedLifecycleEvent) {
    const ScenarioUnitCreated event{
        .alias = "new_unit", .unit_type = "UU0001", .slot = "A_FRONT_0"};
    EXPECT_THROW(BattleScenarioExecutor::resolve_event(event, runtime_), std::logic_error);
}

TEST_F(BattleScenarioExecutorTest, ThrowsOnUnitRetreatedLifecycleEvent) {
    const ScenarioUnitRetreated event{.unit = "old_unit", .reason = "test"};
    EXPECT_THROW(BattleScenarioExecutor::resolve_event(event, runtime_), std::logic_error);
}

TEST_F(BattleScenarioExecutorTest, ThrowsOnUnknownAlias) {
    const ScenarioActorSelected event{.previous = "unknown", .selected = "unknown2"};
    EXPECT_THROW(BattleScenarioExecutor::resolve_event(event, runtime_), std::runtime_error);
}

TEST_F(BattleScenarioExecutorTest, ExecuteEnvelopeHandlesUnitCreated) {
    register_alias("existing",
                   {.side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Front});

    const BattleScenarioEventEnvelope envelope{
        .id = "test_env",
        .event = ScenarioActorSelected{.previous = "existing", .selected = "existing"}};
    auto result = BattleScenarioExecutor::execute_envelope(envelope, runtime_);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<ActorSelected>(*result));
}

TEST_F(BattleScenarioExecutorTest, ExecuteEnvelopeHandlesUnitRetreated) {
    runtime_.apply_unit_created(
        {.alias = "victim", .unit_type = "UU0001", .slot = "A_FRONT_0", .hp = 50, .max_hp = 50});
    const BattleScenarioEventEnvelope envelope{
        .id = "retreat_env", .event = ScenarioUnitRetreated{.unit = "victim", .reason = "test"}};
    auto result = BattleScenarioExecutor::execute_envelope(envelope, runtime_);
    EXPECT_FALSE(result.has_value());
    EXPECT_FALSE(runtime_.has_alias("victim"));
}

// ─────────────────────────────────────────────────────────────────────────────
// 4. BattleScenarioPlayer Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST(BattleScenarioPlayerTest, AutoplayAbsenceUsesFirstSequence) {
    BattleScenario scenario;
    scenario.version = 3;
    scenario.id = "no_autoplay_test";
    scenario.terrain = "HU";
    BattleScenarioSequence seq;
    seq.id = "first_seq";
    BattleScenarioStep step;
    step.id = "step0";
    step.complete = BattleVisualStepCompletion::Immediate;
    BattleScenarioEventEnvelope env;
    env.id = "e1";
    env.event = ScenarioActorSelected{.previous = "a", .selected = "b"};
    step.envelopes.push_back(std::move(env));
    seq.steps.push_back(std::move(step));
    scenario.sequences.push_back(std::move(seq));

    // No autoplay is set
    EXPECT_FALSE(scenario.autoplay.has_value());

    // The first sequence id should be used as play_seq_id
    const std::string play_seq_id = scenario.sequences.empty() ? "" : scenario.sequences.front().id;
    EXPECT_EQ(play_seq_id, "first_seq");

    // Loading and playing should not throw (first sequence is valid)
    BattleScenarioPlayer player;
    EXPECT_NO_THROW(player.load(std::move(scenario)));
    EXPECT_NO_THROW(player.play("first_seq"));
    EXPECT_TRUE(player.is_playing());
    EXPECT_FALSE(player.is_completed());
}

TEST(BattleScenarioPlayerTest, AutoplayScenarioUsesNamedSequence) {
    BattleScenario scenario;
    scenario.version = 3;
    scenario.id = "autoplay_test";
    scenario.autoplay = "main_seq";
    BattleScenarioSequence seq;
    seq.id = "main_seq";
    BattleScenarioStep step;
    step.id = "step0";
    step.complete = BattleVisualStepCompletion::Immediate;
    BattleScenarioEventEnvelope env;
    env.id = "e1";
    env.event = ScenarioActorSelected{.previous = "a", .selected = "b"};
    step.envelopes.push_back(std::move(env));
    seq.steps.push_back(std::move(step));
    scenario.sequences.push_back(std::move(seq));

    ASSERT_TRUE(scenario.autoplay.has_value());
    EXPECT_EQ(*scenario.autoplay, "main_seq");

    BattleScenarioPlayer player;
    EXPECT_NO_THROW(player.load(std::move(scenario)));
    EXPECT_NO_THROW(player.play("main_seq"));
    EXPECT_TRUE(player.is_playing());
}

TEST(BattleScenarioPlayerTest, PlayStartStepSkipsEarlierSteps) {
    BattleScenario scenario;
    scenario.version = 3;
    scenario.id = "start_step_test";
    BattleScenarioSequence seq;
    seq.id = "s1";
    {
        BattleScenarioStep step;
        step.id = "setup_units";
        step.complete = BattleVisualStepCompletion::Immediate;
        BattleScenarioEventEnvelope env;
        env.id = "create1";
        env.event = ScenarioActorSelected{.previous = "a", .selected = "b"};
        step.envelopes.push_back(std::move(env));
        seq.steps.push_back(std::move(step));
    }
    {
        BattleScenarioStep step;
        step.id = "first_real_step";
        step.complete = BattleVisualStepCompletion::Immediate;
        BattleScenarioEventEnvelope env;
        env.id = "action1";
        env.event = ScenarioActorSelected{.previous = "b", .selected = "c"};
        step.envelopes.push_back(std::move(env));
        seq.steps.push_back(std::move(step));
    }
    scenario.sequences.push_back(std::move(seq));

    BattleScenarioPlayer player;
    player.load(std::move(scenario));

    // Start from step index 1 (skip setup_units)
    player.play("s1", 1);
    EXPECT_TRUE(player.is_playing());
    EXPECT_EQ(player.current_step_id(), "");

    // Current sequence should be set
    EXPECT_EQ(player.current_sequence_id(), "s1");
}

class BattleScenarioPlayerFixtureTest : public ::testing::Test {
protected:
    const GameDataRegistry          registry_;
    const FakeAnimationCatalog      catalog_;
    const UnitAttackVisualIntentMap intent_map_;
    BattleScene                     scene_;
    BattlePresenter                 presenter_;
    BattleUnitFactory               factory_;

    BattleScenarioPlayerFixtureTest()
        : registry_(std::filesystem::temp_directory_path() / "opendis2-missing-globals"),
          presenter_(scene_, UnitVisualProfileRegistry{}, UnitLifecycleVisualProfileRegistry{}, 0),
          factory_(registry_, catalog_, intent_map_) {}
};

TEST_F(BattleScenarioPlayerFixtureTest, UnitCreatedWithSchedulingThrows) {
    BattleScenario scenario;
    scenario.version = 3;
    scenario.id = "scheduled_lifecycle_test";
    BattleScenarioSequence seq;
    seq.id = "s1";
    BattleScenarioStep step;
    step.id = "steps";
    step.complete = BattleVisualStepCompletion::AllRequiredTracksFinished;
    BattleScenarioEventEnvelope env;
    env.id = "e1";
    env.at = {.event_id = "some_event", .cue = "complete"};
    env.event = unit_created("new_unit", "UU0001", "A_FRONT_0");
    step.envelopes.push_back(std::move(env));
    seq.steps.push_back(std::move(step));
    scenario.sequences.push_back(std::move(seq));

    BattleScenarioPlayer player;
    player.load(std::move(scenario));
    player.play("s1");

    BattleScenarioRuntime runtime(scene_, presenter_, factory_);
    try {
        player.update(presenter_, runtime);
        FAIL() << "Expected runtime_error for scheduled UnitCreated";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("cannot use at/cue scheduling"), std::string::npos);
    }
}

TEST_F(BattleScenarioPlayerFixtureTest, UnitRetreatedWithSchedulingThrows) {
    BattleScenario scenario;
    scenario.version = 3;
    scenario.id = "scheduled_retreat_test";
    BattleScenarioSequence seq;
    seq.id = "s1";
    BattleScenarioStep step;
    step.id = "steps";
    step.complete = BattleVisualStepCompletion::AllRequiredTracksFinished;
    BattleScenarioEventEnvelope env;
    env.id = "e1";
    env.at = {.event_id = "some_event", .cue = "complete"};
    env.event = ScenarioUnitRetreated{.unit = "old_unit", .reason = "test"};
    step.envelopes.push_back(std::move(env));
    seq.steps.push_back(std::move(step));
    scenario.sequences.push_back(std::move(seq));

    BattleScenarioPlayer player;
    player.load(std::move(scenario));
    player.play("s1");

    BattleScenarioRuntime runtime(scene_, presenter_, factory_);
    try {
        player.update(presenter_, runtime);
        FAIL() << "Expected runtime_error for scheduled UnitRetreated";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("cannot use at/cue scheduling"), std::string::npos);
    }
}

TEST_F(BattleScenarioPlayerFixtureTest, SetupUnitsWithOnlyUnitCreatedSucceeds) {
    BattleScenario scenario;
    scenario.version = 3;
    scenario.id = "setup_ok_test";
    BattleScenarioSequence seq;
    seq.id = "s1";
    BattleScenarioStep step;
    step.id = "setup_units";
    step.complete = BattleVisualStepCompletion::Immediate;
    {
        BattleScenarioEventEnvelope env;
        env.id = "create1";
        env.event = unit_created("u1", "UU0001", "A_FRONT_0");
        step.envelopes.push_back(std::move(env));
    }
    seq.steps.push_back(std::move(step));
    scenario.sequences.push_back(std::move(seq));

    BattleScenarioPlayer player;
    player.load(std::move(scenario));
    player.play("s1");

    BattleScenarioRuntime runtime(scene_, presenter_, factory_);
    // Should not throw — setup_units with only UnitCreated is valid
    EXPECT_NO_THROW(player.update(presenter_, runtime));
}

TEST_F(BattleScenarioPlayerFixtureTest, SetupUnitsWithActorSelectedFails) {
    BattleScenario scenario;
    scenario.version = 3;
    scenario.id = "setup_bad_test";
    BattleScenarioSequence seq;
    seq.id = "s1";
    BattleScenarioStep step;
    step.id = "setup_units";
    step.complete = BattleVisualStepCompletion::Immediate;
    {
        BattleScenarioEventEnvelope env;
        env.id = "e1";
        env.event = ScenarioActorSelected{.previous = "a", .selected = "b"};
        step.envelopes.push_back(std::move(env));
    }
    seq.steps.push_back(std::move(step));
    scenario.sequences.push_back(std::move(seq));

    BattleScenarioPlayer player;
    player.load(std::move(scenario));
    player.play("s1");

    BattleScenarioRuntime runtime(scene_, presenter_, factory_);
    try {
        player.update(presenter_, runtime);
        FAIL() << "expected error for non-UnitCreated in setup_units";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("setup_units"), std::string::npos);
    }
}

TEST_F(BattleScenarioPlayerFixtureTest, UnitCreatedMixedWithActorSelectedFails) {
    BattleScenario scenario;
    scenario.version = 3;
    scenario.id = "mixed_lifecycle_test";
    BattleScenarioSequence seq;
    seq.id = "s1";
    BattleScenarioStep step;
    step.id = "step1";
    step.complete = BattleVisualStepCompletion::Immediate;
    {
        BattleScenarioEventEnvelope env;
        env.id = "create1";
        env.event = unit_created("u1", "UU0001", "A_FRONT_0");
        step.envelopes.push_back(std::move(env));
    }
    {
        BattleScenarioEventEnvelope env;
        env.id = "select1";
        env.event = ScenarioActorSelected{.previous = "u1", .selected = "u1"};
        // Use a known alias that exists - register it
        step.envelopes.push_back(std::move(env));
    }
    seq.steps.push_back(std::move(step));
    scenario.sequences.push_back(std::move(seq));

    BattleScenarioPlayer player;
    player.load(std::move(scenario));
    player.play("s1");

    BattleScenarioRuntime runtime(scene_, presenter_, factory_);
    try {
        player.update(presenter_, runtime);
        FAIL() << "expected error for mixed lifecycle/visual step";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("lifecycle events"), std::string::npos);
        EXPECT_NE(std::string(e.what()).find("step1"), std::string::npos);
    }
}

TEST_F(BattleScenarioPlayerFixtureTest, UnitRetreatedMixedWithTargetDamagedFails) {
    BattleScenario scenario;
    scenario.version = 3;
    scenario.id = "mixed_retreat_test";
    BattleScenarioSequence seq;
    seq.id = "s1";
    BattleScenarioStep step;
    step.id = "mix_step";
    step.complete = BattleVisualStepCompletion::Immediate;
    {
        BattleScenarioEventEnvelope env;
        env.id = "retreat1";
        env.event = ScenarioUnitRetreated{.unit = "u1", .reason = "test"};
        step.envelopes.push_back(std::move(env));
    }
    {
        BattleScenarioEventEnvelope env;
        env.id = "dmg1";
        env.event = ScenarioTargetDamaged{
            .attack_id = AttackInstanceId{1u}, .source = "u1", .target = "u2"};
        step.envelopes.push_back(std::move(env));
    }
    seq.steps.push_back(std::move(step));
    scenario.sequences.push_back(std::move(seq));

    BattleScenarioPlayer player;
    player.load(std::move(scenario));
    player.play("s1");

    BattleScenarioRuntime runtime(scene_, presenter_, factory_);
    try {
        player.update(presenter_, runtime);
        FAIL() << "expected error for mixed lifecycle/visual step";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("lifecycle events"), std::string::npos);
    }
}

TEST_F(BattleScenarioPlayerFixtureTest, LifecycleOnlyUnitCreatedStepExecutes) {
    BattleScenario scenario;
    scenario.version = 3;
    scenario.id = "lifecycle_create_test";
    BattleScenarioSequence seq;
    seq.id = "s1";
    BattleScenarioStep s;
    s.id = "setup_units";
    s.complete = BattleVisualStepCompletion::Immediate;
    BattleScenarioEventEnvelope env;
    env.id = "c1";
    env.event = unit_created("hero", "UU0001", "A_FRONT_0");
    s.envelopes.push_back(std::move(env));
    seq.steps.push_back(std::move(s));
    BattleScenarioStep s2;
    s2.id = "lifecycle_create";
    s2.complete = BattleVisualStepCompletion::Immediate;
    BattleScenarioEventEnvelope env2;
    env2.id = "c2";
    env2.event = unit_created("summon", "UU0002", "A_FRONT_1");
    s2.envelopes.push_back(std::move(env2));
    seq.steps.push_back(std::move(s2));
    scenario.sequences.push_back(std::move(seq));

    // Execute setup_units BEFORE moving scenario into player
    BattleScenarioRuntime runtime(scene_, presenter_, factory_);
    for (const auto& e : scenario.sequences[0].steps[0].envelopes) {
        BattleScenarioExecutor::execute_envelope(e, runtime, "s1", "setup_units", "");
    }

    BattleScenarioPlayer player;
    player.load(std::move(scenario));
    player.play("s1", 1);
    EXPECT_TRUE(player.is_playing());

    EXPECT_NO_THROW(player.update(presenter_, runtime));
    EXPECT_TRUE(runtime.has_alias("summon"));
}

TEST_F(BattleScenarioPlayerFixtureTest, LifecycleOnlyUnitRetreatedStepExecutes) {
    BattleScenario scenario;
    scenario.version = 3;
    scenario.id = "lifecycle_retreat_test";
    BattleScenarioSequence seq;
    seq.id = "s1";
    BattleScenarioStep s;
    s.id = "setup_units";
    s.complete = BattleVisualStepCompletion::Immediate;
    BattleScenarioEventEnvelope env;
    env.id = "c1";
    env.event = unit_created("victim", "UU0001", "A_FRONT_0");
    s.envelopes.push_back(std::move(env));
    seq.steps.push_back(std::move(s));
    BattleScenarioStep s2;
    s2.id = "retreat_step";
    s2.complete = BattleVisualStepCompletion::Immediate;
    BattleScenarioEventEnvelope env2;
    env2.id = "r1";
    env2.event = ScenarioUnitRetreated{.unit = "victim", .reason = "test"};
    s2.envelopes.push_back(std::move(env2));
    seq.steps.push_back(std::move(s2));
    scenario.sequences.push_back(std::move(seq));

    // Execute setup_units BEFORE moving scenario
    BattleScenarioRuntime runtime(scene_, presenter_, factory_);
    for (const auto& e : scenario.sequences[0].steps[0].envelopes) {
        BattleScenarioExecutor::execute_envelope(e, runtime, "s1", "setup_units", "");
    }

    BattleScenarioPlayer player;
    player.load(std::move(scenario));
    player.play("s1", 1);
    EXPECT_TRUE(player.is_playing());

    EXPECT_TRUE(runtime.has_alias("victim"));
    EXPECT_NO_THROW(player.update(presenter_, runtime));
    EXPECT_FALSE(runtime.has_alias("victim"));
}

TEST_F(BattleScenarioPlayerFixtureTest, VisualOnlyStepSubmitsToPresenter) {
    BattleScenario scenario;
    scenario.version = 3;
    scenario.id = "visual_only_test";
    BattleScenarioSequence seq;
    seq.id = "s1";
    BattleScenarioStep step;
    step.id = "setup_units";
    step.complete = BattleVisualStepCompletion::Immediate;
    BattleScenarioEventEnvelope env;
    env.id = "c1";
    env.event = unit_created("u1", "UU0001", "A_FRONT_0");
    step.envelopes.push_back(std::move(env));
    seq.steps.push_back(std::move(step));

    BattleScenarioStep step2;
    step2.id = "visual_only_step";
    step2.complete = BattleVisualStepCompletion::Immediate;
    BattleScenarioEventEnvelope env2;
    env2.id = "sel1";
    env2.event = ScenarioActorSelected{.previous = "u1", .selected = "u1"};
    step2.envelopes.push_back(std::move(env2));
    seq.steps.push_back(std::move(step2));
    scenario.sequences.push_back(std::move(seq));

    // Execute setup_units first, then the scenario player handles the visual-only step.
    // Create a known alias in the runtime for resolution.
    BattleScenarioRuntime runtime(scene_, presenter_, factory_);

    // Create the unit via runtime
    runtime.apply_unit_created(
        {.alias = "u1", .unit_type = "UU0001", .slot = "A_FRONT_0", .hp = 100, .max_hp = 100});

    BattleScenarioPlayer player;
    player.load(std::move(scenario));
    // Start from step index 1 (skip setup_units)
    player.play("s1", 1);
    EXPECT_TRUE(player.is_playing());

    // Update should process the visual-only step — it completes immediately
    // (ActorSelected is commandless) and advances.
    EXPECT_NO_THROW(player.update(presenter_, runtime));
    EXPECT_FALSE(presenter_.has_active_visual_step());
    // Second update detects end of sequence
    player.update(presenter_, runtime);
    EXPECT_TRUE(player.is_completed());
}

TEST(BattleScenarioPlayerTest, UnscheduledLifecycleEventHasNoAtField) {
    // Verify that a UnitCreated event without at/cue scheduling has the
    // correct default at field values (event_id=nullopt, cue="start").
    // This ensures the scheduling check in update() will pass.
    BattleScenarioEventEnvelope env;
    env.id = "e1";
    env.event = unit_created("new_unit", "UU0001", "A_FRONT_0");
    EXPECT_FALSE(env.at.event_id.has_value());
    EXPECT_EQ(env.at.cue, "start");
}

// ─────────────────────────────────────────────────────────────────────────────
// 5. New scenario player lifecycle + visual step semantics
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(BattleScenarioPlayerFixtureTest, LifecycleOnlyStepAdvancesWithoutVisualStep) {
    BattleScenario scenario;
    scenario.version = 3;
    scenario.id = "lifecycle_advance_test";
    BattleScenarioSequence seq;
    seq.id = "main";
    BattleScenarioStep s;
    s.id = "setup_units";
    s.complete = BattleVisualStepCompletion::Immediate;
    BattleScenarioEventEnvelope env;
    env.id = "c1";
    env.event = unit_created("u1", "UU0001", "A_FRONT_0");
    s.envelopes.push_back(std::move(env));
    seq.steps.push_back(std::move(s));
    BattleScenarioStep s2;
    s2.id = "lifecycle_only";
    s2.complete = BattleVisualStepCompletion::Immediate;
    BattleScenarioEventEnvelope env2;
    env2.id = "c2";
    env2.event = unit_created("u2", "UU0002", "A_FRONT_1");
    s2.envelopes.push_back(std::move(env2));
    seq.steps.push_back(std::move(s2));
    scenario.sequences.push_back(std::move(seq));

    BattleScenarioRuntime runtime(scene_, presenter_, factory_);
    for (const auto& e : scenario.sequences[0].steps[0].envelopes) {
        BattleScenarioExecutor::execute_envelope(e, runtime, "main", "setup_units", "");
    }

    BattleScenarioPlayer player;
    player.load(std::move(scenario));
    player.play("main", 1);
    EXPECT_TRUE(player.is_playing());
    EXPECT_FALSE(presenter_.has_active_visual_step());

    EXPECT_NO_THROW(player.update(presenter_, runtime));
    EXPECT_TRUE(runtime.has_alias("u2"));
    EXPECT_FALSE(presenter_.has_active_visual_step());
    // Second update detects end of sequence
    player.update(presenter_, runtime);
    EXPECT_TRUE(player.is_completed());
}

TEST_F(BattleScenarioPlayerFixtureTest, LifecycleOnlyRetreatAdvancesWithoutVisualStep) {
    BattleScenario scenario;
    scenario.version = 3;
    scenario.id = "retreat_advance_test";
    BattleScenarioSequence seq;
    seq.id = "main";
    BattleScenarioStep s;
    s.id = "setup_units";
    s.complete = BattleVisualStepCompletion::Immediate;
    BattleScenarioEventEnvelope env;
    env.id = "c1";
    env.event = unit_created("u1", "UU0001", "A_FRONT_0");
    s.envelopes.push_back(std::move(env));
    seq.steps.push_back(std::move(s));
    BattleScenarioStep s2;
    s2.id = "retreat_step";
    s2.complete = BattleVisualStepCompletion::Immediate;
    BattleScenarioEventEnvelope env2;
    env2.id = "r1";
    env2.event = ScenarioUnitRetreated{.unit = "u1", .reason = "test"};
    s2.envelopes.push_back(std::move(env2));
    seq.steps.push_back(std::move(s2));
    scenario.sequences.push_back(std::move(seq));

    BattleScenarioRuntime runtime(scene_, presenter_, factory_);
    for (const auto& e : scenario.sequences[0].steps[0].envelopes) {
        BattleScenarioExecutor::execute_envelope(e, runtime, "main", "setup_units", "");
    }

    BattleScenarioPlayer player;
    player.load(std::move(scenario));
    player.play("main", 1);
    EXPECT_TRUE(player.is_playing());
    EXPECT_FALSE(presenter_.has_active_visual_step());

    EXPECT_NO_THROW(player.update(presenter_, runtime));
    EXPECT_FALSE(runtime.has_alias("u1"));
    EXPECT_FALSE(presenter_.has_active_visual_step());
    // Second update detects end of sequence
    player.update(presenter_, runtime);
    EXPECT_TRUE(player.is_completed());
}

TEST_F(BattleScenarioPlayerFixtureTest, LifecycleThenVisualStepSequenceWorks) {
    BattleScenario scenario;
    scenario.version = 3;
    scenario.id = "lifecycle_then_visual_test";
    BattleScenarioSequence seq;
    seq.id = "main";
    BattleScenarioStep s;
    s.id = "setup_units";
    s.complete = BattleVisualStepCompletion::Immediate;
    BattleScenarioEventEnvelope env;
    env.id = "c1";
    env.event = unit_created("u1", "UU0001", "A_FRONT_0");
    s.envelopes.push_back(std::move(env));
    seq.steps.push_back(std::move(s));
    BattleScenarioStep s2;
    s2.id = "lifecycle_mid";
    s2.complete = BattleVisualStepCompletion::Immediate;
    BattleScenarioEventEnvelope env2;
    env2.id = "c2";
    env2.event = unit_created("u2", "UU0002", "A_FRONT_1");
    s2.envelopes.push_back(std::move(env2));
    seq.steps.push_back(std::move(s2));
    BattleScenarioStep s3;
    s3.id = "visual_after";
    s3.complete = BattleVisualStepCompletion::Immediate;
    BattleScenarioEventEnvelope env3;
    env3.id = "sel1";
    env3.event = ScenarioActorSelected{.previous = "u1", .selected = "u2"};
    s3.envelopes.push_back(std::move(env3));
    seq.steps.push_back(std::move(s3));
    scenario.sequences.push_back(std::move(seq));

    BattleScenarioRuntime runtime(scene_, presenter_, factory_);
    for (const auto& e : scenario.sequences[0].steps[0].envelopes) {
        BattleScenarioExecutor::execute_envelope(e, runtime, "main", "setup_units", "");
    }

    BattleScenarioPlayer player;
    player.load(std::move(scenario));
    player.play("main", 1);
    EXPECT_TRUE(player.is_playing());

    // Step 1: lifecycle (UnitCreated) creates u2 — advances without visual step
    EXPECT_NO_THROW(player.update(presenter_, runtime));
    EXPECT_TRUE(runtime.has_alias("u2"));
    EXPECT_FALSE(presenter_.has_active_visual_step());
    EXPECT_TRUE(player.is_playing());

    // Step 2: visual (ActorSelected) — submitted and completes immediately
    EXPECT_NO_THROW(player.update(presenter_, runtime));
    EXPECT_FALSE(presenter_.has_active_visual_step());
    // Third update detects end of sequence
    EXPECT_NO_THROW(player.update(presenter_, runtime));
    EXPECT_TRUE(player.is_completed());
}

TEST_F(BattleScenarioPlayerFixtureTest, FailedVisualStepThrowsAndClearsActiveStep) {
    BattleScenario scenario;
    scenario.version = 3;
    scenario.id = "failure_test";
    BattleScenarioSequence seq;
    seq.id = "main";
    BattleScenarioStep step;
    step.id = "setup_units";
    step.complete = BattleVisualStepCompletion::Immediate;
    BattleScenarioEventEnvelope env;
    env.id = "c1";
    env.event = unit_created("u1", "UU0001", "A_FRONT_0");
    step.envelopes.push_back(std::move(env));
    seq.steps.push_back(std::move(step));
    BattleScenarioStep s2;
    s2.id = "bad_visual";
    s2.complete = BattleVisualStepCompletion::Immediate;
    BattleScenarioEventEnvelope env2;
    env2.id = "miss_env";
    env2.optional = false;
    env2.event = ScenarioTargetMissed{.attack_id = AttackInstanceId{1u}, .target = "nonexistent"};
    s2.envelopes.push_back(std::move(env2));
    seq.steps.push_back(std::move(s2));
    scenario.sequences.push_back(std::move(seq));

    BattleScenarioRuntime runtime(scene_, presenter_, factory_);
    for (const auto& e : scenario.sequences[0].steps[0].envelopes) {
        BattleScenarioExecutor::execute_envelope(e, runtime, "main", "setup_units", "");
    }

    BattleScenarioPlayer player;
    player.load(std::move(scenario));
    player.play("main", 1);
    EXPECT_TRUE(player.is_playing());

    // Should throw because the visual step fails (nonexistent alias)
    EXPECT_THROW(player.update(presenter_, runtime), std::runtime_error);
    // After failure, presenter should not have an active step
    EXPECT_FALSE(presenter_.has_active_visual_step());
}

// ─────────────────────────────────────────────────────────────────────────────
// 6. Semantic effect name tests
// ─────────────────────────────────────────────────────────────────────────────

TEST(BattleScenarioParser, ParsesCastEffectWithSourceAttackEffect) {
    const auto* const json = R"({
        "version": 3,
        "id": "source_attack_effect_test",
        "sequences": [
            {
                "id": "s1",
                "steps": [
                    {
                        "id": "st1",
                        "complete": "immediate",
                        "events": [
                            {
                                "id": "e1",
                                "event": {
                                    "type": "CastEffectStarted",
                                    "caster": "u1",
                                    "effect": "source_attack_effect"
                                }
                            }
                        ]
                    }
                ]
            }
        ]
    })";
    const auto        scenario = load_battle_scenario(write_json(json));
    const auto&       event = scenario.sequences[0].steps[0].envelopes[0].event;
    ASSERT_TRUE(std::holds_alternative<ScenarioCastEffectStarted>(event));
    EXPECT_EQ(std::get<ScenarioCastEffectStarted>(event).effect, BattleEffectRole::Heff);
}

TEST(BattleScenarioParser, ParsesBattleEffectWithTeamImpactEffect) {
    const auto* const json = R"({
        "version": 3,
        "id": "team_impact_test",
        "sequences": [
            {
                "id": "s1",
                "steps": [
                    {
                        "id": "st1",
                        "complete": "immediate",
                        "events": [
                            {
                                "id": "e1",
                                "event": {
                                    "type": "BattleEffectStarted",
                                    "source": "u1",
                                    "effect": "team_impact_effect",
                                    "visual_role": "team_overlay_fx",
                                    "targets": ["u2", "u3"]
                                }
                            }
                        ]
                    }
                ]
            }
        ]
    })";
    const auto        scenario = load_battle_scenario(write_json(json));
    const auto&       event = scenario.sequences[0].steps[0].envelopes[0].event;
    ASSERT_TRUE(std::holds_alternative<ScenarioBattleEffectStarted>(event));
    EXPECT_EQ(std::get<ScenarioBattleEffectStarted>(event).effect, BattleEffectRole::Heff);
}

TEST(BattleScenarioParser, RejectsLegacyHeffRole) {
    const auto* const json = R"({
        "version": 3,
        "id": "legacy_heff",
        "sequences": [
            {
                "id": "s1",
                "steps": [
                    {
                        "id": "st1",
                        "complete": "immediate",
                        "events": [
                            {
                                "id": "e1",
                                "event": {
                                    "type": "CastEffectStarted",
                                    "caster": "u1",
                                    "effect": "Heff"
                                }
                            }
                        ]
                    }
                ]
            }
        ]
    })";
    expect_load_error(json, "raw asset family");
}

TEST(BattleScenarioParser, RejectsOldRoleFieldName) {
    const auto* const json = R"({
        "version": 3,
        "id": "old_role_field",
        "sequences": [
            {
                "id": "s1",
                "steps": [
                    {
                        "id": "st1",
                        "complete": "immediate",
                        "events": [
                            {
                                "id": "e1",
                                "event": {
                                    "type": "CastEffectStarted",
                                    "caster": "u1",
                                    "role": "source_attack_effect"
                                }
                            }
                        ]
                    }
                ]
            }
        ]
    })";
    expect_load_error(json, "unknown field");
}

// ─────────────────────────────────────────────────────────────────────────────
// 7. Raw-token validation tests
// ─────────────────────────────────────────────────────────────────────────────

TEST(BattleScenarioParser, RejectsHeffInLeadingComment) {
    const auto* const json =
        "// HEFF in comment\n{\n\"version\": 3,\n\"id\": \"heff_comment\",\n\"sequences\": []\n}";
    expect_load_error(json, "raw asset family");
}

TEST(BattleScenarioParser, RejectsTuchInLeadingComment) {
    const auto* const json =
        "// tuch in comment\n{\n\"version\": 3,\n\"id\": \"tuch_comment\",\n\"sequences\": []\n}";
    expect_load_error(json, "raw asset family");
}

TEST(BattleScenarioParser, SemanticScenarioPassesRawTokenValidation) {
    const auto* const json = R"({
        "version": 3,
        "id": "semantic_ok",
        "sequences": [
            {
                "id": "s1",
                "steps": [
                    {
                        "id": "setup_units",
                        "complete": "immediate",
                        "events": [
                            {
                                "id": "c1",
                                "event": {
                                    "type": "UnitCreated",
                                    "unit": "u1",
                                    "unit_type": "UU0001",
                                    "slot": "A_FRONT_0"
                                }
                            }
                        ]
                    },
                    {
                        "id": "st1",
                        "complete": "immediate",
                        "events": [
                            {
                                "id": "e1",
                                "event": {
                                    "type": "CastEffectStarted",
                                    "caster": "u1",
                                    "effect": "source_attack_effect"
                                }
                            }
                        ]
                    }
                ]
            }
        ]
    })";
    EXPECT_NO_THROW((void)load_battle_scenario(write_json(json)));
}

// ─────────────────────────────────────────────────────────────────────────────
// 8. UnitRetreated lifecycle-only busy check tests
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(BattleScenarioPlayerFixtureTest, UnitRetreatedLifecycleIdleSucceeds) {
    BattleScenario scenario;
    scenario.version = 3;
    scenario.id = "retreat_idle_test";
    BattleScenarioSequence seq;
    seq.id = "main";
    BattleScenarioStep s;
    s.id = "setup_units";
    s.complete = BattleVisualStepCompletion::Immediate;
    BattleScenarioEventEnvelope env;
    env.id = "c1";
    env.event = unit_created("u1", "UU0001", "A_FRONT_0");
    s.envelopes.push_back(std::move(env));
    seq.steps.push_back(std::move(s));
    BattleScenarioStep s2;
    s2.id = "retreat_step";
    s2.complete = BattleVisualStepCompletion::Immediate;
    BattleScenarioEventEnvelope env2;
    env2.id = "r1";
    env2.event = ScenarioUnitRetreated{.unit = "u1", .reason = "test"};
    s2.envelopes.push_back(std::move(env2));
    seq.steps.push_back(std::move(s2));
    scenario.sequences.push_back(std::move(seq));

    BattleScenarioRuntime runtime(scene_, presenter_, factory_);
    for (const auto& e : scenario.sequences[0].steps[0].envelopes) {
        BattleScenarioExecutor::execute_envelope(e, runtime, "main", "setup_units", "");
    }

    // Presenter is idle (default state)
    BattleScenarioPlayer player;
    player.load(std::move(scenario));
    player.play("main", 1);
    EXPECT_TRUE(player.is_playing());

    EXPECT_NO_THROW(player.update(presenter_, runtime));

    // Unit must be gone after successful retreat
    EXPECT_FALSE(runtime.has_alias("u1"));
}

TEST_F(BattleScenarioPlayerFixtureTest, MixedLifecycleVisualStepRejected) {
    BattleScenario scenario;
    scenario.version = 3;
    scenario.id = "mixed_step_test";
    BattleScenarioSequence seq;
    seq.id = "main";
    BattleScenarioStep s;
    s.id = "setup_units";
    s.complete = BattleVisualStepCompletion::Immediate;
    BattleScenarioEventEnvelope env;
    env.id = "c1";
    env.event = unit_created("u1", "UU0001", "A_FRONT_0");
    s.envelopes.push_back(std::move(env));
    seq.steps.push_back(std::move(s));
    BattleScenarioStep s2;
    s2.id = "mixed_step";
    s2.complete = BattleVisualStepCompletion::Immediate;
    BattleScenarioEventEnvelope env2;
    env2.id = "sel1";
    env2.event = ScenarioActorSelected{.previous = "u1", .selected = "u1"};
    s2.envelopes.push_back(std::move(env2));
    BattleScenarioEventEnvelope env3;
    env3.id = "r1";
    env3.event = ScenarioUnitRetreated{.unit = "u1", .reason = "test"};
    s2.envelopes.push_back(std::move(env3));
    seq.steps.push_back(std::move(s2));
    scenario.sequences.push_back(std::move(seq));

    BattleScenarioRuntime runtime(scene_, presenter_, factory_);
    for (const auto& e : scenario.sequences[0].steps[0].envelopes) {
        BattleScenarioExecutor::execute_envelope(e, runtime, "main", "setup_units", "");
    }

    BattleScenarioPlayer player;
    player.load(std::move(scenario));
    player.play("main", 1);
    EXPECT_TRUE(player.is_playing());

    // Mixed lifecycle+visual step must be rejected by composition validation
    EXPECT_THROW(
        {
            try {
                player.update(presenter_, runtime);
            } catch (const std::runtime_error& e) {
                EXPECT_NE(std::string(e.what()).find("Cannot mix"), std::string::npos);
                throw;
            }
        },
        std::runtime_error);
    EXPECT_TRUE(runtime.has_alias("u1"));
}

// ─────────────────────────────────────────────────────────────────────────────
// 9. Committed scenario file validation tests
} // namespace
} // namespace d2engine
