#include <gtest/gtest.h>

#include "render_batch_test_helpers.hpp"

#include "d2engine/app/battle_tuning_state.hpp"
#include "d2engine/battle_view/battle_render_tree_contract.hpp"
#include "d2engine/battle_view/battle_renderer.hpp"
#include "d2engine/battle_view/render_placement.hpp"
#include "d2engine/render/render_tree.hpp"

#include <nlohmann/json.hpp>
#include <string>

namespace d2engine {
namespace {

// ---- helpers ---------------------------------------------------------------

BattleTuningState make_state(std::string_view json_text) {
    auto              doc = nlohmann::json::parse(json_text);
    const auto        tmp = std::filesystem::path{"test_default_profiles.json"};
    BattleTuningState state;
    load_battle_tuning_config(state, doc, tmp);
    return state;
}

nlohmann::json make_document(std::string_view json_text) {
    return nlohmann::json::parse(json_text);
}

// Save state into a JSON document, then reload a new state from that document.
BattleTuningState save_and_reload(const BattleTuningState& src, nlohmann::json& doc,
                                  const std::filesystem::path& config_path) {
    write_battle_tuning_config(doc, src);
    BattleTuningState reloaded;
    load_battle_tuning_config(reloaded, doc, config_path);
    return reloaded;
}

BattleRenderOptions make_options(const BattleTuningState& state) {
    static const TreeLayout tree = [] {
        TreeLayout t;
        for (const BattleSlotCoord coord : kBattlefieldLayoutCoords) {
            t.set_node(battlefield_slot_tree_path(coord),
                       TreeNode{.kind = "slot", .x = 100.0f + coord.lane * 20.0f, .y = 200.0f});
            t.set_node(battlefield_unit_tree_path(coord), TreeNode{.kind = "mount"});
        }
        return t;
    }();
    BattleRenderOptions opts;
    opts.placements = state.placements;
    opts.tree_layout = &tree;
    return opts;
}

// ---- load / save tests ------------------------------------------------------

TEST(LayerDefaultProfiles, LoadAttackA2SideSpecific) {
    const auto state = make_state(R"({
        "unit_visual_layer_default_profiles": {
            "attack": {
                "a2": {
                    "a": {"x": -10.0, "y": -155.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0},
                    "d": {"x":  -8.0, "y": -150.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
                }
            }
        }
    })");

    const std::string key_a = "UnitVisualLayerDefaultProfile:a2:UnitAttack:a";
    const std::string key_d = "UnitVisualLayerDefaultProfile:a2:UnitAttack:d";
    ASSERT_TRUE(state.placements.contains(key_a));
    ASSERT_TRUE(state.placements.contains(key_d));
    EXPECT_FLOAT_EQ(state.placements.at(key_a).x, -10.0f);
    EXPECT_FLOAT_EQ(state.placements.at(key_d).x, -8.0f);
}

TEST(LayerDefaultProfiles, HitA2BothSidesLoadable) {
    const auto state = make_state(R"({
        "unit_visual_layer_default_profiles": {
            "hit": {
                "a2": {
                    "a": {"x": 7.0, "y": -145.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0},
                    "d": {"x": 7.0, "y": -145.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
                }
            }
        }
    })");

    EXPECT_TRUE(state.placements.contains("UnitVisualLayerDefaultProfile:a2:UnitHit:a"));
    EXPECT_TRUE(state.placements.contains("UnitVisualLayerDefaultProfile:a2:UnitHit:d"));
}

TEST(LayerDefaultProfiles, NoCommonBindingLoadedWhenOnlySidesPresent) {
    const auto state = make_state(R"({
        "unit_visual_layer_default_profiles": {
            "attack": {
                "a2": {
                    "a": {"x": -10.0, "y": -155.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
                }
            }
        }
    })");

    EXPECT_FALSE(state.placements.contains("UnitVisualLayerDefaultProfile:a2:UnitAttack"));
}

TEST(LayerDefaultProfiles, AttackerDefenderIndependent) {
    const auto state = make_state(R"({
        "unit_visual_layer_default_profiles": {
            "attack": {
                "a2": {
                    "a": {"x": -10.0, "y": -155.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0},
                    "d": {"x":  -8.0, "y": -120.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
                }
            }
        }
    })");

    const auto a = state.placements.at("UnitVisualLayerDefaultProfile:a2:UnitAttack:a");
    const auto d = state.placements.at("UnitVisualLayerDefaultProfile:a2:UnitAttack:d");
    EXPECT_NE(a.y, d.y); // a=-155 vs d=-120
}

TEST(LayerDefaultProfiles, HitNotAffectedByAttackDefault) {
    const auto state = make_state(R"({
        "unit_visual_layer_default_profiles": {
            "attack": {
                "a2": { "a": {"x": -99.0, "y": -99.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0} }
            }
        }
    })");

    EXPECT_FALSE(state.placements.contains("UnitVisualLayerDefaultProfile:a2:UnitHit:a"));
}

TEST(LayerDefaultProfiles, ADefaultDoesNotAffectDKey) {
    const auto state = make_state(R"({
        "unit_visual_layer_default_profiles": {
            "attack": {
                "a2": { "a": {"x": -10.0, "y": -155.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0} }
            }
        }
    })");

    EXPECT_FALSE(state.placements.contains("UnitVisualLayerDefaultProfile:a2:UnitAttack:d"));
}

TEST(LayerDefaultProfiles, SaveRoundTrip) {
    const auto tmp = std::filesystem::path{"test_default_rt.json"};
    auto       doc = make_document(R"({
        "unit_visual_layer_default_profiles": {
            "attack": {
                "a2": {
                    "a": {"x": -10.0, "y": -155.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0},
                    "d": {"x": -10.0, "y": -155.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
                }
            }
        }
    })");

    BattleTuningState state;
    load_battle_tuning_config(state, doc, tmp);

    BattleTuningState reloaded = save_and_reload(state, doc, tmp);

    const std::string key_a = "UnitVisualLayerDefaultProfile:a2:UnitAttack:a";
    ASSERT_TRUE(reloaded.placements.contains(key_a));
    EXPECT_FLOAT_EQ(reloaded.placements.at(key_a).x, -10.0f);
    EXPECT_FLOAT_EQ(reloaded.placements.at(key_a).y, -155.0f);
}

TEST(LayerDefaultProfiles, IdentityExactLayerPrunedOnSave) {
    const auto tmp = std::filesystem::path{"test_layer_prune.json"};
    auto       doc = make_document(R"({
        "unit_visual_layer_profiles": {
            "UU0014": {
                "attack": {
                    "a2": {"x": 0.0, "y": 0.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
                }
            }
        }
    })");

    BattleTuningState state;
    load_battle_tuning_config(state, doc, tmp);
    write_battle_tuning_config(doc, state);

    const bool pruned = !doc.contains("unit_visual_layer_profiles") ||
                        !doc["unit_visual_layer_profiles"].contains("UU0014") ||
                        doc["unit_visual_layer_profiles"]["UU0014"].empty();
    EXPECT_TRUE(pruned) << "Identity exact layer override was not pruned on save";
}

// ---- effect defaults (visual_role dispatch) ---------------------------------

TEST(LayerDefaultProfiles, EffectDefaultByVisualRole_SourceAttackOverlay) {
    const auto state = make_state(R"({
        "battle_effect_default_profiles": {
            "source_attack_overlay": {
                "a": {"x": 93.0, "y": 3.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
            }
        }
    })");

    const std::string key_a = "EffectDefaultProfile:source_attack_overlay:Source:a";
    ASSERT_TRUE(state.placements.contains(key_a));
    EXPECT_FLOAT_EQ(state.placements.at(key_a).x, 93.0f);
}

TEST(LayerDefaultProfiles, EffectDefaultByVisualRole_TargetDamage) {
    const auto state = make_state(R"({
        "battle_effect_default_profiles": {
            "target_damage": {
                "d": {"x": 5.0, "y": -10.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
            }
        }
    })");

    EXPECT_TRUE(state.placements.contains("EffectDefaultProfile:target_damage:Target:d"));
}

TEST(LayerDefaultProfiles, EffectDefaultByVisualRole_TeamAttack) {
    const auto state = make_state(R"({
        "battle_effect_default_profiles": {
            "team_attack": {
                "a": {"x": -15.0, "y": 35.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
            }
        }
    })");

    EXPECT_TRUE(state.placements.contains("EffectDefaultProfile:team_attack:TargetTeam:a"));
}

TEST(LayerDefaultProfiles, EffectDefaultRoundTrip) {
    // frame_delay in effect defaults is not applied at render time and is dropped on save.
    const auto tmp = std::filesystem::path{"test_effect_default.json"};
    auto       doc = make_document(R"({
        "battle_effect_default_profiles": {
            "source_attack_overlay": {
                "a": {"x": 93.0, "y": 3.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0}
            }
        }
    })");

    BattleTuningState state;
    load_battle_tuning_config(state, doc, tmp);
    BattleTuningState reloaded = save_and_reload(state, doc, tmp);

    const std::string key_a = "EffectDefaultProfile:source_attack_overlay:Source:a";
    ASSERT_TRUE(reloaded.placements.contains(key_a));
    EXPECT_FLOAT_EQ(reloaded.placements.at(key_a).x, 93.0f);
    EXPECT_EQ(reloaded.placements.at(key_a).frame_delay,
              0); // dropped on save — not used for defaults
}

// ---- visual_role propagation tests ------------------------------------------

TEST(LayerDefaultProfiles, VisualRoleDistinguishesSourceAttackFromSourceCast) {
    // SourceAttackOverlayFx and SourceCastFx both have effect_role=Source,
    // but the config distinguishes them by category name.
    const auto state = make_state(R"({
        "battle_effect_default_profiles": {
            "source_attack_overlay": {
                "a": {"x": 99.0, "y": 0.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
            }
        }
    })");

    const auto opts = make_options(state);

    // source_attack_overlay key present; source_cast key absent (not in config)
    EXPECT_TRUE(opts.placements.contains("EffectDefaultProfile:source_attack_overlay:Source:a"));
    EXPECT_FALSE(opts.placements.contains("EffectDefaultProfile:source_cast:Source:a"));
}

TEST(LayerDefaultProfiles, BDirectionAssetUsesRenderSideNotB) {
    // TUCHA1B sequence is TargetDamageFx; placement side is attacker/defender of TARGET, not "b".
    const auto state = make_state(R"({
        "battle_effect_default_profiles": {
            "target_damage": {
                "a": {"x": 10.0, "y": -20.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0},
                "d": {"x": 20.0, "y": -30.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
            }
        }
    })");

    // No "b" key should exist in placements
    EXPECT_FALSE(state.placements.contains("EffectDefaultProfile:target_damage:Target:b"));
    EXPECT_TRUE(state.placements.contains("EffectDefaultProfile:target_damage:Target:a"));
    EXPECT_TRUE(state.placements.contains("EffectDefaultProfile:target_damage:Target:d"));
}

TEST(LayerDefaultProfiles, TeamAttackNotAppliedToTeamOverlay) {
    // TeamAttackOverlayFx and TeamOverlayFx must use different categories.
    const auto state = make_state(R"({
        "battle_effect_default_profiles": {
            "team_attack": {
                "a": {"x": -15.0, "y": 35.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
            },
            "target_team": {
                "a": {"x": -5.0, "y": 10.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
            }
        }
    })");

    EXPECT_TRUE(state.placements.contains("EffectDefaultProfile:team_attack:TargetTeam:a"));
    EXPECT_TRUE(state.placements.contains("EffectDefaultProfile:target_team:TargetTeam:a"));
}

// ---- UU0014 hit.a2 promotion test -------------------------------------------

TEST(LayerDefaultProfiles, UU0014HitA2APromotedToDefault) {
    // A-side hit a2 canonical value is (-11, -155) from Angel sprite calibration.
    // D-side is (7, -145) — different because defenders face opposite direction.
    const auto state = make_state(R"({
        "unit_visual_layer_default_profiles": {
            "hit": {
                "a2": {
                    "a": {"x": -11.0, "y": -155.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0},
                    "d": {"x":   7.0, "y": -145.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
                }
            }
        }
    })");

    ASSERT_TRUE(state.placements.contains("UnitVisualLayerDefaultProfile:a2:UnitHit:a"));
    EXPECT_FLOAT_EQ(state.placements.at("UnitVisualLayerDefaultProfile:a2:UnitHit:a").x, -11.0f);
    EXPECT_FLOAT_EQ(state.placements.at("UnitVisualLayerDefaultProfile:a2:UnitHit:a").y, -155.0f);
}

// ---- UU0095 stays as exact override when different from default --------------

TEST(LayerDefaultProfiles, UU0095ExactOverrideKeptWhenDifferentFromDefault) {
    const auto state = make_state(R"({
        "unit_visual_layer_default_profiles": {
            "attack": {
                "a2": {
                    "d": {"x": -10.0, "y": -155.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
                }
            }
        },
        "unit_visual_layer_profiles": {
            "UU0095": {
                "attack": {
                    "a2": {
                        "d": {"x": -255.0, "y": 76.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
                    }
                }
            }
        }
    })");

    // Default key present
    EXPECT_TRUE(state.placements.contains("UnitVisualLayerDefaultProfile:a2:UnitAttack:d"));
    // UU0095 exact override present (non-identity delta)
    EXPECT_TRUE(state.placements.contains("UnitVisualLayerProfile:UU0095:a2:UnitAttack:d"));
}

// ---- TUCHA1B00 migration test -----------------------------------------------

TEST(LayerDefaultProfiles, TUCHA1B00TargetRoleLoads) {
    const auto state = make_state(R"({
        "battle_effect_profiles": {
            "G000UU0103TUCHA1B00": {
                "target": {
                    "frame_delay": -301,
                    "level": 0,
                    "scale_x": 1.3,
                    "scale_y": 1.3,
                    "x": -15.0,
                    "y": 35.0
                }
            }
        }
    })");

    const std::string key = "EffectProfile:G000UU0103TUCHA1B00:Target";
    ASSERT_TRUE(state.placements.contains(key));
    EXPECT_EQ(state.placements.at(key).frame_delay, -301);
}

TEST(LayerDefaultProfiles, TUCHA1B00NoStaleTargetTeamKey) {
    // After migration, target_team key must not exist for G000UU0103TUCHA1B00.
    const auto state = make_state(R"({
        "battle_effect_profiles": {
            "G000UU0103TUCHA1B00": {
                "target": {
                    "frame_delay": -301,
                    "level": 0,
                    "scale_x": 1.3,
                    "scale_y": 1.3,
                    "x": -15.0,
                    "y": 35.0
                }
            }
        }
    })");

    EXPECT_FALSE(state.placements.contains("EffectProfile:G000UU0103TUCHA1B00:TargetTeam"));
}

// ---- idle A2 defaults -------------------------------------------------------

TEST(LayerDefaultProfiles, IdleA2BothSidesLoadable) {
    const auto state = make_state(R"({
        "unit_visual_layer_default_profiles": {
            "idle": {
                "a2": {
                    "a": {"x": -10.0, "y": -154.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0},
                    "d": {"x":   7.0, "y": -146.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
                }
            }
        }
    })");

    ASSERT_TRUE(state.placements.contains("UnitVisualLayerDefaultProfile:a2:UnitIdle:a"));
    ASSERT_TRUE(state.placements.contains("UnitVisualLayerDefaultProfile:a2:UnitIdle:d"));
    EXPECT_FLOAT_EQ(state.placements.at("UnitVisualLayerDefaultProfile:a2:UnitIdle:d").x, 7.0f);
    EXPECT_FLOAT_EQ(state.placements.at("UnitVisualLayerDefaultProfile:a2:UnitIdle:d").y, -146.0f);
}

TEST(LayerDefaultProfiles, IdleA2DDefaultHasCorrectCalibratedValues) {
    const auto state = make_state(R"({
        "unit_visual_layer_default_profiles": {
            "idle": {
                "a2": {
                    "d": {"x": 7.0, "y": -146.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
                }
            }
        }
    })");

    ASSERT_TRUE(state.placements.contains("UnitVisualLayerDefaultProfile:a2:UnitIdle:d"));
    auto d = state.placements.at("UnitVisualLayerDefaultProfile:a2:UnitIdle:d");
    EXPECT_FLOAT_EQ(d.x, 7.0f);
    EXPECT_FLOAT_EQ(d.y, -146.0f);
    EXPECT_FLOAT_EQ(d.scale_x, 1.0f);
    EXPECT_EQ(d.level, 0);
}

TEST(LayerDefaultProfiles, IdleA2DNotAffectedByAttackOrHitA2Defaults) {
    const auto state = make_state(R"({
        "unit_visual_layer_default_profiles": {
            "attack": {
                "a2": {
                    "a": {"x": -10.0, "y": -155.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0},
                    "d": {"x": -10.0, "y": -155.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
                }
            },
            "hit": {
                "a2": {
                    "a": {"x": -11.0, "y": -155.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0},
                    "d": {"x":   7.0, "y": -145.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
                }
            }
        }
    })");

    EXPECT_FALSE(state.placements.contains("UnitVisualLayerDefaultProfile:a2:UnitIdle:a"));
    EXPECT_FALSE(state.placements.contains("UnitVisualLayerDefaultProfile:a2:UnitIdle:d"));
}

TEST(LayerDefaultProfiles, IdleA2DNotAppliedToASide) {
    const auto state = make_state(R"({
        "unit_visual_layer_default_profiles": {
            "idle": {
                "a2": {
                    "a": {"x": -10.0, "y": -154.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0},
                    "d": {"x":   7.0, "y": -146.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
                }
            }
        }
    })");

    ASSERT_TRUE(state.placements.contains("UnitVisualLayerDefaultProfile:a2:UnitIdle:a"));
    ASSERT_TRUE(state.placements.contains("UnitVisualLayerDefaultProfile:a2:UnitIdle:d"));
    EXPECT_NE(state.placements.at("UnitVisualLayerDefaultProfile:a2:UnitIdle:a").x,
              state.placements.at("UnitVisualLayerDefaultProfile:a2:UnitIdle:d").x);
    EXPECT_NE(state.placements.at("UnitVisualLayerDefaultProfile:a2:UnitIdle:a").y,
              state.placements.at("UnitVisualLayerDefaultProfile:a2:UnitIdle:d").y);
}

TEST(LayerDefaultProfiles, IdleA2NoBCommonSide) {
    const auto state = make_state(R"({
        "unit_visual_layer_default_profiles": {
            "idle": {
                "a2": {
                    "a": {"x": -10.0, "y": -154.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0},
                    "d": {"x":   7.0, "y": -146.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
                }
            }
        }
    })");

    EXPECT_FALSE(state.placements.contains("UnitVisualLayerDefaultProfile:a2:UnitIdle:b"));
    EXPECT_FALSE(state.placements.contains("UnitVisualLayerDefaultProfile:a2:UnitIdle"));
}

TEST(LayerDefaultProfiles, IdleA2NotAffectedByAttackA2Default) {
    const auto state = make_state(R"({
        "unit_visual_layer_default_profiles": {
            "attack": {
                "a2": {
                    "a": {"x": -99.0, "y": -99.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
                }
            }
        }
    })");

    EXPECT_FALSE(state.placements.contains("UnitVisualLayerDefaultProfile:a2:UnitIdle:a"));
}

// ---- hit.a2 migration correctness -------------------------------------------

TEST(LayerDefaultProfiles, HitA2ASideIsActualAngel) {
    const auto state = make_state(R"({
        "unit_visual_layer_default_profiles": {
            "hit": {
                "a2": {
                    "a": {"x": -11.0, "y": -155.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0},
                    "d": {"x":   7.0, "y": -145.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
                }
            }
        }
    })");

    ASSERT_TRUE(state.placements.contains("UnitVisualLayerDefaultProfile:a2:UnitHit:a"));
    EXPECT_FLOAT_EQ(state.placements.at("UnitVisualLayerDefaultProfile:a2:UnitHit:a").x, -11.0f);
    EXPECT_FLOAT_EQ(state.placements.at("UnitVisualLayerDefaultProfile:a2:UnitHit:a").y, -155.0f);
    ASSERT_TRUE(state.placements.contains("UnitVisualLayerDefaultProfile:a2:UnitHit:d"));
    EXPECT_FLOAT_EQ(state.placements.at("UnitVisualLayerDefaultProfile:a2:UnitHit:d").x, 7.0f);
    EXPECT_FLOAT_EQ(state.placements.at("UnitVisualLayerDefaultProfile:a2:UnitHit:d").y, -145.0f);
}

TEST(LayerDefaultProfiles, EffectDefaultLevelRoundTrip) {
    // frame_delay must not appear in saved effect default JSON.
    const auto tmp = std::filesystem::path{"test_effect_level_rt.json"};
    auto       doc = make_document(R"({
        "battle_effect_default_profiles": {
            "team_attack": {
                "d": {"x": -15.0, "y": 35.0, "scale_x": 1.0, "scale_y": 1.0, "level": 3}
            }
        }
    })");

    BattleTuningState state;
    load_battle_tuning_config(state, doc, tmp);
    write_battle_tuning_config(doc, state);

    const auto& saved = doc;
    ASSERT_TRUE(saved.contains("battle_effect_default_profiles"));
    const auto& ta = saved["battle_effect_default_profiles"]["team_attack"]["d"];
    EXPECT_EQ(ta.value("level", 0), 3);
    EXPECT_FALSE(ta.contains("frame_delay")); // must be omitted from effect default saves
}

// ---- render-command regression tests ----------------------------------------

class FakeTexProvider final : public IBattleTextureProvider {
    static int s_dummy;

public:
    BackendTextureRef get_texture(const std::string& /*container_path*/,
                                  const std::string& /*image_name*/) override {
        return BackendTextureRef{.native = &s_dummy};
    }
    [[nodiscard]] std::pair<float, float>
    texture_size(BackendTextureRef /*texture*/) const override {
        return {64.0f, 64.0f};
    }
};
int FakeTexProvider::s_dummy = 1;

BattleRenderSnapshot make_unit_snapshot(BattleSide side, const std::string& unit_type,
                                        BindingRole effect_role, LayerSlot slot,
                                        const std::string& seq_name) {
    SnapshotTrack t;
    t.id = TrackId{1};
    t.kind = TrackKind::Base;
    t.layer = TrackRenderLayer::Base;
    t.anchor = AnchorPolicy::UnitFoot;
    t.placement = render_placement_for(t.layer, t.anchor);
    t.effect_role = effect_role;
    t.layer_slot = slot;
    t.sequence_name = seq_name;
    t.current_frame_name = "frame";
    t.container_path = "test.ff";
    t.native_canvas_w = 1600;
    t.native_canvas_h = 945;
    t.canvas_foot_x = 800;
    t.canvas_foot_y = 945;

    SnapshotEntity e;
    e.id = VisualEntityId{1};
    e.unit_instance_id = UnitInstanceId{1};
    e.coord = BattleSlotCoord{.side = side};
    e.unit_type = unit_type;
    e.animation_unit_type = unit_type;
    e.tracks.push_back(std::move(t));

    BattleRenderSnapshot snap;
    snap.entities.push_back(std::move(e));
    return snap;
}

BattleRenderSnapshot make_effect_snapshot(BattleSide side, const std::string& unit_type,
                                          BindingRole            effect_role,
                                          BattleEffectVisualRole visual_role,
                                          const std::string&     seq_name) {
    SnapshotTrack t;
    t.id = TrackId{2};
    t.kind = TrackKind::Effect;
    t.layer = TrackRenderLayer::Effect;
    t.anchor = AnchorPolicy::UnitFoot;
    t.placement = render_placement_for(t.layer, t.anchor);
    t.effect_role = effect_role;
    t.visual_role = visual_role;
    t.sequence_name = seq_name;
    t.current_frame_name = "frame";
    t.container_path = "test.ff";

    SnapshotEntity e;
    e.id = VisualEntityId{2};
    e.unit_instance_id = UnitInstanceId{2};
    e.coord = BattleSlotCoord{.side = side};
    e.unit_type = unit_type;
    e.animation_unit_type = unit_type;
    e.tracks.push_back(std::move(t));

    BattleRenderSnapshot snap;
    snap.entities.push_back(std::move(e));
    return snap;
}

const DebugRenderableItem* find_tunable_item(const RenderBatch& batch) {
    for (const auto& cmd : test::commands_from(batch)) {
        if (test::tunable_item(cmd).has_value() && test::tunable_item(cmd)->selectable) {
            return &*test::tunable_item(cmd);
        }
    }
    return nullptr;
}

// Helper: build commands and return the first selectable tunable item
const DebugRenderableItem* render_and_get_item(const BattleRenderSnapshot& snap,
                                               const BattleRenderOptions&  opts,
                                               FakeTexProvider& tex, RenderBatch& storage) {
    storage = BattleRenderer::build_render_batch(snap, tex, opts);
    return find_tunable_item(storage);
}

// ---- unit-layer binding tests -----------------------------------------------

TEST(RenderCommand, AHitA2ReceivesHitA2ADefault) {
    const auto      state = make_state(R"({
        "unit_visual_layer_default_profiles": {
            "hit": {
                "a2": {
                    "a": {"x": -11.0, "y": -155.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0},
                    "d": {"x":   7.0, "y": -145.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
                }
            }
        }
    })");
    FakeTexProvider tex;
    RenderBatch     cmds;
    const auto      snap = make_unit_snapshot(BattleSide::Attacker, "UU0014", BindingRole::UnitHit,
                                              LayerSlot::A2, "G000UU0014HHITA2A00");
    const auto*     item = render_and_get_item(snap, make_options(state), tex, cmds);
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->render_side, "a");
    EXPECT_EQ(item->visual_category, "UnitHitA2");
    ASSERT_TRUE(item->default_binding.has_value());
    EXPECT_NE(item->default_binding->display_path.find("hit.a2.a"), std::string::npos);
}

TEST(RenderCommand, DHitA2ReceivesHitA2DDefault) {
    const auto      state = make_state(R"({
        "unit_visual_layer_default_profiles": {
            "hit": {
                "a2": {
                    "a": {"x": -11.0, "y": -155.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0},
                    "d": {"x":   7.0, "y": -145.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
                }
            }
        }
    })");
    FakeTexProvider tex;
    RenderBatch     cmds;
    const auto      snap = make_unit_snapshot(BattleSide::Defender, "UU0014", BindingRole::UnitHit,
                                              LayerSlot::A2, "G000UU0014HHITA2D00");
    const auto*     item = render_and_get_item(snap, make_options(state), tex, cmds);
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->render_side, "d");
    ASSERT_TRUE(item->default_binding.has_value());
    EXPECT_NE(item->default_binding->display_path.find("hit.a2.d"), std::string::npos);
}

TEST(RenderCommand, AAttackA2ReceivesAttackA2ADefault) {
    const auto      state = make_state(R"({
        "unit_visual_layer_default_profiles": {
            "attack": {
                "a2": {
                    "a": {"x": -10.0, "y": -155.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0},
                    "d": {"x": -10.0, "y": -155.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
                }
            }
        }
    })");
    FakeTexProvider tex;
    RenderBatch     cmds;
    const auto  snap = make_unit_snapshot(BattleSide::Attacker, "UU0014", BindingRole::UnitAttack,
                                          LayerSlot::A2, "G000UU0014HMOVA2A00");
    const auto* item = render_and_get_item(snap, make_options(state), tex, cmds);
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->render_side, "a");
    EXPECT_EQ(item->visual_category, "UnitAttackA2");
    ASSERT_TRUE(item->default_binding.has_value());
    EXPECT_NE(item->default_binding->display_path.find("attack.a2.a"), std::string::npos);
}

TEST(RenderCommand, DAttackA2NightmareReceivesAttackA2DDefault) {
    const auto      state = make_state(R"({
        "unit_visual_layer_default_profiles": {
            "attack": {
                "a2": {
                    "a": {"x": -10.0, "y": -155.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0},
                    "d": {"x":    5.0, "y": -218.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
                }
            }
        }
    })");
    FakeTexProvider tex;
    RenderBatch     cmds;
    const auto  snap = make_unit_snapshot(BattleSide::Defender, "UU0103", BindingRole::UnitAttack,
                                          LayerSlot::A2, "G000UU0103HMOVA2D00");
    const auto* item = render_and_get_item(snap, make_options(state), tex, cmds);
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->render_side, "d");
    EXPECT_EQ(item->visual_category, "UnitAttackA2");
    ASSERT_TRUE(item->default_binding.has_value());
    EXPECT_NE(item->default_binding->display_path.find("attack.a2.d"), std::string::npos);
    EXPECT_EQ(item->default_binding->display_path.find("UU0103"), std::string::npos);
}

TEST(RenderCommand, DAttackA2UU0163ReceivesSameAttackA2DDefault) {
    const auto      state = make_state(R"({
        "unit_visual_layer_default_profiles": {
            "attack": {
                "a2": {
                    "a": {"x": -10.0, "y": -155.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0},
                    "d": {"x":    5.0, "y": -218.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
                }
            }
        }
    })");
    FakeTexProvider tex;
    RenderBatch     cmds;
    const auto  snap = make_unit_snapshot(BattleSide::Defender, "UU0163", BindingRole::UnitAttack,
                                          LayerSlot::A2, "G000UU0163HMOVA2D00");
    const auto* item = render_and_get_item(snap, make_options(state), tex, cmds);
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->render_side, "d");
    ASSERT_TRUE(item->default_binding.has_value());
    EXPECT_NE(item->default_binding->display_path.find("attack.a2.d"), std::string::npos);
    EXPECT_EQ(item->default_binding->display_path.find("UU0163"), std::string::npos);
}

TEST(RenderCommand, DAttackA2UU0095ReceivesSameAttackA2DDefault) {
    const auto      state = make_state(R"({
        "unit_visual_layer_default_profiles": {
            "attack": {
                "a2": {
                    "a": {"x": -10.0, "y": -155.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0},
                    "d": {"x":    5.0, "y": -218.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
                }
            }
        }
    })");
    FakeTexProvider tex;
    RenderBatch     cmds;
    const auto  snap = make_unit_snapshot(BattleSide::Defender, "UU0095", BindingRole::UnitAttack,
                                          LayerSlot::A2, "G000UU0095HMOVA2D00");
    const auto* item = render_and_get_item(snap, make_options(state), tex, cmds);
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->render_side, "d");
    ASSERT_TRUE(item->default_binding.has_value());
    EXPECT_NE(item->default_binding->display_path.find("attack.a2.d"), std::string::npos);
    EXPECT_EQ(item->default_binding->display_path.find("UU0095"), std::string::npos);
}

TEST(RenderCommand, DAttackA2UU0014ReceivesSameAttackA2DDefault) {
    const auto      state = make_state(R"({
        "unit_visual_layer_default_profiles": {
            "attack": {
                "a2": {
                    "a": {"x": -10.0, "y": -155.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0},
                    "d": {"x":    5.0, "y": -218.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
                }
            }
        }
    })");
    FakeTexProvider tex;
    RenderBatch     cmds;
    const auto  snap = make_unit_snapshot(BattleSide::Defender, "UU0014", BindingRole::UnitAttack,
                                          LayerSlot::A2, "G000UU0014HMOVA2D00");
    const auto* item = render_and_get_item(snap, make_options(state), tex, cmds);
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->render_side, "d");
    ASSERT_TRUE(item->default_binding.has_value());
    EXPECT_NE(item->default_binding->display_path.find("attack.a2.d"), std::string::npos);
    EXPECT_EQ(item->default_binding->display_path.find("UU0014"), std::string::npos);
}

TEST(RenderCommand, AAttackA2UsesAttackA2ANotD) {
    const auto      state = make_state(R"({
        "unit_visual_layer_default_profiles": {
            "attack": {
                "a2": {
                    "a": {"x": -10.0, "y": -155.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0},
                    "d": {"x":    5.0, "y": -218.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
                }
            }
        }
    })");
    FakeTexProvider tex;
    RenderBatch     cmds;
    const auto  snap = make_unit_snapshot(BattleSide::Attacker, "UU0103", BindingRole::UnitAttack,
                                          LayerSlot::A2, "G000UU0103HMOVA2A00");
    const auto* item = render_and_get_item(snap, make_options(state), tex, cmds);
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->render_side, "a");
    ASSERT_TRUE(item->default_binding.has_value());
    EXPECT_NE(item->default_binding->display_path.find("attack.a2.a"), std::string::npos);
    EXPECT_EQ(item->default_binding->display_path.find("attack.a2.d"), std::string::npos);
}

TEST(RenderCommand, SpawnedEffectsDoNotUseUnitVisualLayerDefaults) {
    const auto      state = make_state(R"({
        "unit_visual_layer_default_profiles": {
            "attack": {
                "a2": {
                    "d": {"x": 17.0, "y": -63.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
                }
            }
        }
    })");
    FakeTexProvider tex;
    RenderBatch     cmds;
    const auto      snap =
        make_effect_snapshot(BattleSide::Defender, "UU0103", BindingRole::Source,
                             BattleEffectVisualRole::SourceAttackOverlayFx, "G000UU0103HEFFA1D00");
    const auto* item = render_and_get_item(snap, make_options(state), tex, cmds);
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->visual_category, "SourceAttackOverlayFx");
    if (item->default_binding.has_value()) {
        EXPECT_EQ(item->default_binding->display_path.find("attack.a2.d"), std::string::npos);
    }
}

// ---- effect default binding tests -------------------------------------------

TEST(RenderCommand, SourceAttackOverlayReceivesCorrectDefault) {
    const auto      state = make_state(R"({
        "battle_effect_default_profiles": {
            "source_attack_overlay": {
                "a": {"x": 93.0, "y": 3.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0}
            }
        }
    })");
    FakeTexProvider tex;
    RenderBatch     cmds;
    const auto      snap =
        make_effect_snapshot(BattleSide::Attacker, "UU0001", BindingRole::Source,
                             BattleEffectVisualRole::SourceAttackOverlayFx, "G000UU0001HEFFA1A00");
    const auto* item = render_and_get_item(snap, make_options(state), tex, cmds);
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->visual_category, "SourceAttackOverlayFx");
    EXPECT_EQ(item->render_side, "a");
    ASSERT_TRUE(item->default_binding.has_value());
    EXPECT_NE(item->default_binding->display_path.find("source_attack_overlay"), std::string::npos);
}

TEST(RenderCommand, TeamAttackReceivesTeamAttackDefault) {
    // D-side source (Nightmare) attacks A-side heroes → team overlay targets A side.
    // Entity is on D, but effect_lookup_side returns "a" (opposite) for TeamAttackOverlayFx.
    const auto      state = make_state(R"({
        "battle_effect_default_profiles": {
            "team_attack": {
                "a": {"x": -15.0, "y": 35.0, "scale_x": 1.0, "scale_y": 1.0, "level": 3},
                "d": {"x": 0.0, "y": 0.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0}
            }
        }
    })");
    FakeTexProvider tex;
    RenderBatch     cmds;
    const auto      snap =
        make_effect_snapshot(BattleSide::Defender, "UU0103", BindingRole::TargetTeam,
                             BattleEffectVisualRole::TeamAttackOverlayFx, "G000UU0103HEFFA1D00");
    const auto* item = render_and_get_item(snap, make_options(state), tex, cmds);
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->visual_category, "TeamAttackOverlayFx");
    EXPECT_EQ(item->render_side, "d"); // entity IS on D
    ASSERT_TRUE(item->default_binding.has_value());
    // default lookup flips to opposite: team_attack.a (not .d)
    EXPECT_NE(item->default_binding->display_path.find("team_attack.a"), std::string::npos);
}

TEST(RenderCommand, TargetDamageFxReceivesTargetDamageDefault) {
    const auto      state = make_state(R"({
        "battle_effect_default_profiles": {
            "target_damage": {
                "a": {"x": -15.0, "y": 35.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0},
                "d": {"x": -15.0, "y": 35.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0}
            }
        }
    })");
    FakeTexProvider tex;
    RenderBatch     cmds;
    const auto      snap =
        make_effect_snapshot(BattleSide::Attacker, "UU0001", BindingRole::Target,
                             BattleEffectVisualRole::TargetDamageFx, "G000UU0103TUCHA1B00");
    const auto* item = render_and_get_item(snap, make_options(state), tex, cmds);
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->visual_category, "TargetDamageFx");
    EXPECT_EQ(item->render_side, "a");
    ASSERT_TRUE(item->default_binding.has_value());
    EXPECT_NE(item->default_binding->display_path.find("target_damage"), std::string::npos);
}

TEST(RenderCommand, BDirectionAssetResolvesByRenderSideNotB) {
    // TUCHA1B00 has 'B' in sequence name; must resolve default by render side (a/d), not "b".
    const auto      state = make_state(R"({
        "battle_effect_default_profiles": {
            "target_damage": {
                "a": {"x": -15.0, "y": 35.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0},
                "d": {"x": -15.0, "y": 35.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0}
            }
        }
    })");
    FakeTexProvider tex;
    RenderBatch     cmds;
    const auto      snap =
        make_effect_snapshot(BattleSide::Defender, "UU0103", BindingRole::Target,
                             BattleEffectVisualRole::TargetDamageFx, "G000UU0103TUCHA1B00");
    const auto* item = render_and_get_item(snap, make_options(state), tex, cmds);
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->asset_direction, "B"); // asset letter is B
    EXPECT_EQ(item->render_side, "d");     // but placement resolved by battlefield side
    ASSERT_TRUE(item->default_binding.has_value());
    EXPECT_NE(item->default_binding->display_path.find(".d"), std::string::npos); // .d not .b
}

TEST(RenderCommand, EffectDefaultLevelParticiatesInDrawLayer) {
    // D-side entity with TeamAttackOverlayFx → effect_lookup_side returns "a" (opposite).
    // team_attack.a with level=1: Effect base (4) + 1 = 5 = Overlay.
    const auto      state = make_state(R"({
        "battle_effect_default_profiles": {
            "team_attack": {
                "a": {"x": 0.0, "y": 0.0, "scale_x": 1.0, "scale_y": 1.0, "level": 1},
                "d": {"x": 0.0, "y": 0.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0}
            }
        }
    })");
    FakeTexProvider tex;
    RenderBatch     cmds;
    const auto      snap =
        make_effect_snapshot(BattleSide::Defender, "UU0103", BindingRole::TargetTeam,
                             BattleEffectVisualRole::TeamAttackOverlayFx, "G000UU0103HEFFA1D00");
    const auto* item = render_and_get_item(snap, make_options(state), tex, cmds);
    ASSERT_NE(item, nullptr);
    // Effect base = TrackRenderLayer::Effect = 4; +1 default level = 5 = Overlay
    EXPECT_EQ(item->layer, static_cast<int>(TrackRenderLayer::Overlay));
}

// ---- team-effect opposite-side resolution tests --------------------------------

TEST(RenderCommand, TeamAttackASourceLooksUpDDefault) {
    // A-side attacker targeting D-side defenders → opposite team = D → team_attack.d
    const auto      state = make_state(R"({
        "battle_effect_default_profiles": {
            "team_attack": {
                "a": {"x": 0.0, "y": 0.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0},
                "d": {"x": 77.0, "y": 55.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0}
            }
        }
    })");
    FakeTexProvider tex;
    RenderBatch     cmds;
    const auto      snap =
        make_effect_snapshot(BattleSide::Attacker, "UU0001", BindingRole::TargetTeam,
                             BattleEffectVisualRole::TeamAttackOverlayFx, "G000UU0001HEFFA1D00");
    const auto* item = render_and_get_item(snap, make_options(state), tex, cmds);
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->render_side, "a"); // entity IS on A
    ASSERT_TRUE(item->default_binding.has_value());
    EXPECT_NE(item->default_binding->display_path.find("team_attack.d"), std::string::npos);
}

TEST(RenderCommand, TeamAttackDSourceLooksUpADefault) {
    // D-side attacker targeting A-side heroes → opposite team = A → team_attack.a
    const auto      state = make_state(R"({
        "battle_effect_default_profiles": {
            "team_attack": {
                "a": {"x": -15.0, "y": 35.0, "scale_x": 1.0, "scale_y": 1.0, "level": 3},
                "d": {"x": 0.0, "y": 0.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0}
            }
        }
    })");
    FakeTexProvider tex;
    RenderBatch     cmds;
    const auto      snap =
        make_effect_snapshot(BattleSide::Defender, "UU0103", BindingRole::TargetTeam,
                             BattleEffectVisualRole::TeamAttackOverlayFx, "G000UU0103HEFFA1D00");
    const auto* item = render_and_get_item(snap, make_options(state), tex, cmds);
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->render_side, "d"); // entity IS on D
    ASSERT_TRUE(item->default_binding.has_value());
    EXPECT_NE(item->default_binding->display_path.find("team_attack.a"), std::string::npos);
}

// ---- idle A2 default binding tests --------------------------------------------

TEST(RenderCommand, DIdleA2AngelReceivesIdleA2DDefault) {
    const auto      state = make_state(R"({
        "unit_visual_layer_default_profiles": {
            "idle": {
                "a2": {
                    "a": {"x": -10.0, "y": -154.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0},
                    "d": {"x":   7.0, "y": -146.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
                }
            }
        }
    })");
    FakeTexProvider tex;
    RenderBatch     cmds;
    const auto      snap = make_unit_snapshot(BattleSide::Defender, "UU0014", BindingRole::UnitIdle,
                                              LayerSlot::A2, "G000UU0014IDLEA2D00");
    const auto*     item = render_and_get_item(snap, make_options(state), tex, cmds);
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->render_side, "d");
    EXPECT_EQ(item->visual_category, "UnitIdleA2");
    ASSERT_TRUE(item->default_binding.has_value());
    EXPECT_NE(item->default_binding->display_path.find("idle.a2.d"), std::string::npos);
}

TEST(RenderCommand, AIdleA2DoesNotReceiveIdleA2DDefault) {
    const auto      state = make_state(R"({
        "unit_visual_layer_default_profiles": {
            "idle": {
                "a2": {
                    "a": {"x": -10.0, "y": -154.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0},
                    "d": {"x":   7.0, "y": -146.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
                }
            }
        }
    })");
    FakeTexProvider tex;
    RenderBatch     cmds;
    const auto      snap = make_unit_snapshot(BattleSide::Attacker, "UU0014", BindingRole::UnitIdle,
                                              LayerSlot::A2, "G000UU0014IDLEA2A00");
    const auto*     item = render_and_get_item(snap, make_options(state), tex, cmds);
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->render_side, "a");
    ASSERT_TRUE(item->default_binding.has_value());
    EXPECT_NE(item->default_binding->display_path.find("idle.a2.a"), std::string::npos);
    EXPECT_EQ(item->default_binding->display_path.find("idle.a2.d"), std::string::npos);
}

TEST(RenderCommand, DIdleA2OtherUnitReceivesSameIdleA2DDefault) {
    const auto      state = make_state(R"({
        "unit_visual_layer_default_profiles": {
            "idle": {
                "a2": {
                    "a": {"x": -10.0, "y": -154.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0},
                    "d": {"x":   7.0, "y": -146.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
                }
            }
        }
    })");
    FakeTexProvider tex;
    RenderBatch     cmds;
    const auto      snap = make_unit_snapshot(BattleSide::Defender, "UU0103", BindingRole::UnitIdle,
                                              LayerSlot::A2, "G000UU0103IDLEA2D00");
    const auto*     item = render_and_get_item(snap, make_options(state), tex, cmds);
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->render_side, "d");
    ASSERT_TRUE(item->default_binding.has_value());
    EXPECT_NE(item->default_binding->display_path.find("idle.a2.d"), std::string::npos);
}

// ---- default attack.a2.d round-trip --------------------------------------------

TEST(LayerDefaultProfiles, AttackA2DDefaultSurvivesRoundTrip) {
    const auto tmp = std::filesystem::path{"test_a2d_default_rt.json"};
    auto       doc = make_document(R"({
        "unit_visual_layer_default_profiles": {
            "attack": {
                "a2": {
                    "d": {"x": 5.0, "y": -218.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
                }
            }
        },
        "unit_visual_layer_profiles": {}
    })");

    BattleTuningState state;
    load_battle_tuning_config(state, doc, tmp);
    BattleTuningState reloaded = save_and_reload(state, doc, tmp);

    const std::string def_key = "UnitVisualLayerDefaultProfile:a2:UnitAttack:d";
    ASSERT_TRUE(reloaded.placements.contains(def_key));
    EXPECT_FLOAT_EQ(reloaded.placements.at(def_key).x, 5.0f);
    EXPECT_FLOAT_EQ(reloaded.placements.at(def_key).y, -218.0f);
}

// ---- production-default regression tests -------------------------------------

TEST(LayerDefaultProfiles, AttackA2DBothSidesPresentInDefault) {
    const auto state = make_state(R"({
        "unit_visual_layer_default_profiles": {
            "attack": {
                "a2": {
                    "a": {"x": -10.0, "y": -155.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0},
                    "d": {"x":    5.0, "y": -218.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
                }
            }
        }
    })");

    ASSERT_TRUE(state.placements.contains("UnitVisualLayerDefaultProfile:a2:UnitAttack:a"));
    ASSERT_TRUE(state.placements.contains("UnitVisualLayerDefaultProfile:a2:UnitAttack:d"));
}

TEST(LayerDefaultProfiles, AttackA2DDefaultIsNotCorruptedValue) {
    const auto state = make_state(R"({
        "unit_visual_layer_default_profiles": {
            "attack": {
                "a2": {
                    "d": {"x": 5.0, "y": -218.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0, "frame_delay": 0}
                }
            }
        }
    })");

    auto d = state.placements.at("UnitVisualLayerDefaultProfile:a2:UnitAttack:d");
    EXPECT_NE(d.x, -270.0f);
    EXPECT_NE(d.y, -73.0f);
    EXPECT_NE(d.x, -13.0f);
    EXPECT_NE(d.y, -153.0f);
    EXPECT_FLOAT_EQ(d.x, 5.0f);
    EXPECT_FLOAT_EQ(d.y, -218.0f);
    EXPECT_FLOAT_EQ(d.scale_x, 1.0f);
    EXPECT_EQ(d.level, 0);
}

TEST(LayerDefaultProfiles, NightmareTeamAttackAOverlayResolvesTeamAttackD) {
    const auto state = make_state(R"({
        "battle_effect_default_profiles": {
            "team_attack": {
                "a": {"x": 0.0, "y": 0.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0},
                "d": {"x": -15.0, "y": 35.0, "scale_x": 1.0, "scale_y": 1.0, "level": 3}
            }
        }
    })");

    EXPECT_TRUE(state.placements.contains("EffectDefaultProfile:team_attack:TargetTeam:a"));
    EXPECT_TRUE(state.placements.contains("EffectDefaultProfile:team_attack:TargetTeam:d"));
}

TEST(LayerDefaultProfiles, SourceAttackOverlayASideIsCalibratedSide) {
    const auto state = make_state(R"({
        "battle_effect_default_profiles": {
            "source_attack_overlay": {
                "a": {"x": 93.0, "y": 3.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0},
                "d": {"x": 0.0, "y": 0.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0}
            }
        }
    })");

    ASSERT_TRUE(state.placements.contains("EffectDefaultProfile:source_attack_overlay:Source:a"));
    ASSERT_TRUE(state.placements.contains("EffectDefaultProfile:source_attack_overlay:Source:d"));
    EXPECT_FLOAT_EQ(state.placements.at("EffectDefaultProfile:source_attack_overlay:Source:a").x,
                    93.0f);
    EXPECT_FLOAT_EQ(state.placements.at("EffectDefaultProfile:source_attack_overlay:Source:d").x,
                    0.0f);
}

TEST(LayerDefaultProfiles, NoBCommonSideInAnyDefault) {
    const auto state = make_state(R"({
        "battle_effect_default_profiles": {
            "source_attack_overlay": {
                "a": {"x": 93.0, "y": 3.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0},
                "d": {"x": 0.0, "y": 0.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0}
            },
            "team_attack": {
                "a": {"x": 0.0, "y": 0.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0},
                "d": {"x": -15.0, "y": 35.0, "scale_x": 1.0, "scale_y": 1.0, "level": 3}
            },
            "target_damage": {
                "a": {"x": -15.0, "y": 35.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0},
                "d": {"x": -15.0, "y": 35.0, "scale_x": 1.0, "scale_y": 1.0, "level": 0}
            }
        }
    })");

    EXPECT_FALSE(state.placements.contains("EffectDefaultProfile:source_attack_overlay:Source:b"));
    EXPECT_FALSE(state.placements.contains("EffectDefaultProfile:team_attack:TargetTeam:b"));
    EXPECT_FALSE(state.placements.contains("EffectDefaultProfile:target_damage:Target:b"));
}

} // namespace
} // namespace d2engine
