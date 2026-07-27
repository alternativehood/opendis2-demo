#include <gtest/gtest.h>

#include <fstream>
#include <nlohmann/json.hpp>

#include "d2engine/animation/animation_player.hpp"
#include "d2engine/animation/animation_sequence.hpp"
#include "d2engine/app/battle_tuning_state.hpp"
#include "d2engine/app/screen_config_store.hpp"

#include "d2engine/battle_view/battle_debug_scene_controller.hpp"
#include "d2engine/battle_view/battle_effect_clip.hpp"
#include "d2engine/app/battle_screen.hpp"
#include "d2engine/battle_view/battle_render_tree_contract.hpp"
#include "d2engine/render/render_tree.hpp"
#include "d2engine/render/vec2.hpp"
#include "d2engine/battle_view/battle_presenter.hpp"
#include "d2engine/battle_view/battle_scene.hpp"
#include "d2engine/battle_view/battle_scene_presentation_state.hpp"
#include "d2engine/battle_view/battle_selection_controller.hpp"
#include "d2engine/battle_view/battle_slot.hpp"
#include "d2engine/battle_view/debug_battle_outcome_resolver.hpp"
#include "d2engine/battle_view/unit_animation_role_set.hpp"
#include "d2engine/battle_view/unit_state_clip.hpp"
#include "d2engine/battle_view/unit_lifecycle_visual_profile.hpp"

namespace d2engine {

namespace {

AnimationSequence make_seq(const std::string& name, int frame_count = 3, bool looping = true) {
    AnimationSequence seq;
    seq.name = name;
    seq.container_path = "Imgs/Test.ff";
    seq.is_looping = looping;
    for (int i = 0; i < frame_count; ++i) {
        AnimationFrame frame;
        frame.image_name = "fr_" + std::to_string(i);
        frame.index = static_cast<std::size_t>(i);
        frame.duration_ms = 100;
        seq.frames.push_back(frame);
    }
    return seq;
}

[[nodiscard]] const AnimationPlayer& base_player(const BattleUnit& unit) {
    const auto idx = unit.find_track(TrackKind::Base);
    EXPECT_TRUE(idx.has_value()) << "Unit has no Base track";
    return unit.tracks[*idx].player;
}

UnitAnimationRoleSet make_unit_roles(const std::string& tag) {
    UnitAnimationRoleSet a;
    a.idle = UnitStateClip::from_a1(make_seq(tag + "_idle", 3, true));
    a.hit = UnitStateClip::from_a1(make_seq(tag + "_hit", 1, false));
    a.attack = UnitStateClip::from_a1(make_seq(tag + "_attack", 2, false));
    a.heff = BattleEffectClip{.clip = {.a1 = make_seq(tag + "_heff", 1, false)}};
    a.tuch = BattleEffectClip{.clip = {.a1 = make_seq(tag + "_tuch", 1, false)}};
    return a;
}

UnitVisualProfileRegistry make_unit_profiles(std::vector<UnitAnimationRoleSet> roles) {
    UnitVisualProfileRegistry registry;
    for (auto& role : roles) {
        static_cast<void>(registry.add(std::move(role)));
    }
    return registry;
}

UnitLifecycleVisualProfileRegistry
make_lifecycle_profiles(std::size_t count, const AnimationSequence& death_seq = {},
                        const AnimationSequence& revive_seq = {}) {
    UnitLifecycleVisualProfileRegistry registry;
    for (std::size_t i = 0; i < count; ++i) {
        UnitLifecycleVisualProfile lp;
        lp.death.corpse_sprite_base = "DEAD_TEST";
        lp.death.death_fx = death_seq;
        lp.revive.small_fx = revive_seq;
        lp.revive.large_fx = revive_seq;
        static_cast<void>(registry.add(std::move(lp)));
    }
    return registry;
}

BattlePresenter make_presenter(BattleScene& scene, std::vector<UnitAnimationRoleSet> roles,
                               std::size_t /*lp_count*/, std::size_t                 attacker_count,
                               UnitLifecycleVisualProfileRegistry lifecycle_profiles = {}) {
    return BattlePresenter{scene, make_unit_profiles(std::move(roles)),
                           std::move(lifecycle_profiles), attacker_count};
}

// Build a BattleScene with n_attackers + n_defenders pre-loaded with looping IDLE sequences.
// Attacker sequences named "ATT_IDLE", defender sequences named "DEF_IDLE_<i>".
BattleScene make_scene(int n_attackers, int n_defenders) {
    BattleScene scene;
    for (int i = 0; i < n_attackers; ++i) {
        BattleUnit unit;
        const auto id = static_cast<std::uint32_t>(i + 1);
        unit.unit_instance_id = UnitInstanceId{id};
        unit.visual_profile_id = UnitVisualProfileId{id};
        unit.lifecycle_profile_id = UnitLifecycleVisualProfileId{id};
        unit.coord =
            BattleSlotCoord{.side = BattleSide::Attacker, .lane = i, .depth = BattleDepth::Front};
        unit.direction = 'A';
        VisualTrack base;
        base.kind = TrackKind::Base;
        base.layer = TrackRenderLayer::Base;
        base.anchor = AnchorPolicy::UnitCanvasFoot;
        base.player.load(make_seq("ATT_IDLE"));
        base.player.play();
        unit.tracks.push_back(std::move(base));
        scene.add_unit(std::move(unit));
    }
    for (int i = 0; i < n_defenders; ++i) {
        BattleUnit unit;
        const auto id = static_cast<std::uint32_t>(n_attackers + i + 1);
        unit.unit_instance_id = UnitInstanceId{id};
        unit.visual_profile_id = UnitVisualProfileId{id};
        unit.lifecycle_profile_id = UnitLifecycleVisualProfileId{id};
        unit.coord =
            BattleSlotCoord{.side = BattleSide::Defender, .lane = i, .depth = BattleDepth::Front};
        unit.direction = 'D';
        VisualTrack base;
        base.kind = TrackKind::Base;
        base.layer = TrackRenderLayer::Base;
        base.anchor = AnchorPolicy::UnitCanvasFoot;
        base.player.load(make_seq("DEF_IDLE_" + std::to_string(i)));
        base.player.play();
        unit.tracks.push_back(std::move(base));
        scene.add_unit(std::move(unit));
    }
    return scene;
}

} // namespace

TEST(BattleSelectionController, CyclesSemanticActorAndPreservesValidTarget) {
    BattleSelectionModel model{.actor = UnitInstanceId{1u}, .target = UnitInstanceId{3u}};
    const std::vector<BattleSelectableUnit> units{
        {.id = UnitInstanceId{1u}, .selectable = true},
        {.id = UnitInstanceId{2u}, .selectable = true},
        {.id = UnitInstanceId{3u}, .selectable = true},
    };

    const auto change = d2engine::BattleSelectionController::select_next_actor(model, units);

    ASSERT_TRUE(change.actor.has_value());
    EXPECT_EQ(change.actor->previous, UnitInstanceId{1u});
    EXPECT_EQ(change.actor->selected, UnitInstanceId{2u});
    EXPECT_EQ(model.actor, UnitInstanceId{2u});
    EXPECT_EQ(model.target, UnitInstanceId{3u});
    EXPECT_FALSE(change.target.has_value());
}

TEST(BattleSelectionController, ReplacesInvalidTargetWithDefault) {
    BattleSelectionModel model{.actor = UnitInstanceId{1u}, .target = UnitInstanceId{2u}};
    const std::vector<BattleSelectableUnit> units{
        {.id = UnitInstanceId{1u}, .selectable = true},
        {.id = UnitInstanceId{2u}, .selectable = false},
        {.id = UnitInstanceId{3u}, .selectable = true},
    };

    const auto change = d2engine::BattleSelectionController::select_next_actor(model, units);

    ASSERT_TRUE(change.actor.has_value());
    EXPECT_EQ(change.actor->selected, UnitInstanceId{3u});
    ASSERT_TRUE(change.target.has_value());
    EXPECT_EQ(change.target->previous, UnitInstanceId{2u});
    EXPECT_EQ(change.target->selected, UnitInstanceId{1u});
    EXPECT_EQ(model.target, UnitInstanceId{1u});
}

TEST(DebugBattleOutcomeResolver, ImpactCueProducesTargetDamagedWithoutPresenter) {
    const BattleSelectionModel model{.actor = UnitInstanceId{1u}, .target = UnitInstanceId{2u}};
    const BattleVisualEvent    emitted =
        AttackImpactCue{.attack_id = AttackInstanceId{7u},
                        .source = UnitInstanceId{1u},
                        .targets = TargetSet::single(UnitInstanceId{2u})};

    const auto resolved = d2engine::DebugBattleOutcomeResolver::resolve(emitted, model);

    ASSERT_EQ(resolved.size(), 1u);
    const auto* damaged = std::get_if<TargetDamaged>(&resolved.front());
    ASSERT_NE(damaged, nullptr);
    EXPECT_EQ(damaged->attack_id, AttackInstanceId{7u});
    EXPECT_EQ(damaged->target, UnitInstanceId{2u});
}

TEST(BattlePresenter, DeadUnitStaysFrozen) {
    BattleScene scene = make_scene(1, 1);

    std::vector<UnitAnimationRoleSet> anims;
    anims.push_back(make_unit_roles("att0"));
    anims.push_back(make_unit_roles("def0"));

    const std::size_t n_anims = anims.size();
    BattlePresenter   presenter = make_presenter(scene, std::move(anims), n_anims, 1u);

    // Drive through 3 hits → kill → fade → Dead
    for (int hit = 0; hit < 3; ++hit) {
        presenter.update(2001.0f);
        scene.update(100.0f);
        presenter.update(0.0f);
    }
    presenter.update(2001.0f); // 4th trigger → FadingOut + overlay starts
    presenter.update(500.0f);  // fade completes → Dead

    const std::string frozen_name = base_player(scene.units()[1]).sequence().name;
    const auto        frozen_state = base_player(scene.units()[1]).state();

    // Many more updates must not change the frozen unit
    for (int i = 0; i < 10; ++i) {
        presenter.update(2001.0f);
    }
    EXPECT_EQ(base_player(scene.units()[1]).sequence().name, frozen_name);
    EXPECT_EQ(base_player(scene.units()[1]).state(), frozen_state);
}

TEST(BattlePresenter, AttackersNeverTargeted) {
    BattleScene scene = make_scene(6, 6);

    std::vector<UnitAnimationRoleSet> anims;
    anims.reserve(12);
    for (int i = 0; i < 6; ++i) {
        anims.push_back(make_unit_roles("att" + std::to_string(i)));
    }
    for (int i = 0; i < 6; ++i) {
        anims.push_back(make_unit_roles("def" + std::to_string(i)));
    }

    const std::size_t n_anims = anims.size();
    BattlePresenter   presenter = make_presenter(scene, std::move(anims), n_anims, 6u);

    // Fire 30 triggers — enough to hit all defenders multiple times
    for (int i = 0; i < 30; ++i) {
        presenter.update(2001.0f);
    }

    // All attacker units (indices 0-5) must still have their original IDLE sequence
    for (std::size_t i = 0; i < 6; ++i) {
        EXPECT_EQ(base_player(scene.units()[i]).sequence().name, "ATT_IDLE")
            << "Attacker " << i << " was unexpectedly changed";
    }
}

TEST(BattlePresenter, KillUnitEntersFadingOut) {
    BattleScene                       scene = make_scene(1, 1);
    std::vector<UnitAnimationRoleSet> anims;
    anims.push_back(make_unit_roles("att0"));
    anims.push_back(make_unit_roles("def0"));
    const std::size_t n_anims = anims.size();
    BattlePresenter   presenter =
        make_presenter(scene, std::move(anims), n_anims, 1u,
                       make_lifecycle_profiles(n_anims, make_seq("death_fx", 1, false)));

    EXPECT_FLOAT_EQ(scene.find_track(scene.units()[1].id, TrackKind::Base)->alpha, 1.0f);
    presenter.submit_visual_event(UnitKilled{.target = UnitInstanceId{2u}});

    // After kill, unit is FadingOut — alpha not yet changed
    EXPECT_FLOAT_EQ(scene.find_track(scene.units()[1].id, TrackKind::Base)->alpha, 1.0f);
    EXPECT_NE(base_player(scene.units()[1]).sequence().name, "def0_death");

    // First update begins the fade
    presenter.update(100.0f);
    EXPECT_LT(scene.find_track(scene.units()[1].id, TrackKind::Base)->alpha, 1.0f);
}

TEST(BattlePresenter, FadeOutDecreasesAlpha) {
    BattleScene                       scene = make_scene(1, 1);
    std::vector<UnitAnimationRoleSet> anims;
    anims.push_back(make_unit_roles("att0"));
    anims.push_back(make_unit_roles("def0"));
    const std::size_t n_anims = anims.size();
    BattlePresenter   presenter =
        make_presenter(scene, std::move(anims), n_anims, 1u,
                       make_lifecycle_profiles(n_anims, make_seq("death_fx", 1, false)));

    presenter.submit_visual_event(UnitKilled{.target = UnitInstanceId{2u}});
    presenter.update(250.0f); // half of FADE_DURATION_MS=500
    EXPECT_FLOAT_EQ(scene.find_track(scene.units()[1].id, TrackKind::Base)->alpha, 0.5f);
}

TEST(BattlePresenter, FadeCompleteTriggersDeathAnim) {
    BattleScene                       scene = make_scene(1, 1);
    std::vector<UnitAnimationRoleSet> anims;
    anims.push_back(make_unit_roles("att0"));
    anims.push_back(make_unit_roles("def0"));
    const std::size_t n_anims = anims.size();
    BattlePresenter   presenter =
        make_presenter(scene, std::move(anims), n_anims, 1u,
                       make_lifecycle_profiles(n_anims, make_seq("def0_death", 1, false)));

    presenter.submit_visual_event(UnitKilled{.target = UnitInstanceId{2u}});
    // DeathFx track starts immediately on kill_unit
    const auto& unit1 = scene.units()[1];
    const auto  death_fx_idx = unit1.find_track(TrackKind::DeathFx);
    EXPECT_TRUE(death_fx_idx.has_value());
    EXPECT_EQ(unit1.tracks[*death_fx_idx].player.sequence().name, "def0_death");

    presenter.update(500.0f);                      // full fade → Dead
    EXPECT_FLOAT_EQ(scene.units()[1].alpha, 1.0f); // alpha reset
    EXPECT_EQ(scene.units()[1].life_state, LifeVisualState::Dead);
    // DeathFx track still present (not completed yet)
    const auto death_fx_idx2 = scene.units()[1].find_track(TrackKind::DeathFx);
    EXPECT_TRUE(death_fx_idx2.has_value());
    EXPECT_EQ(scene.units()[1].tracks[*death_fx_idx2].player.sequence().name, "def0_death");
}

TEST(BattlePresenter, FadeCompleteNoDeathAnimGoesDead) {
    BattleScene scene = make_scene(1, 1);

    UnitAnimationRoleSet no_death_anim;
    no_death_anim.idle = UnitStateClip::from_a1(make_seq("att0_idle", 3, true));
    no_death_anim.hit = UnitStateClip::from_a1(make_seq("att0_hit", 1, false));
    // death is default empty

    UnitAnimationRoleSet def_no_death;
    def_no_death.idle = UnitStateClip::from_a1(make_seq("def0_idle", 3, true));
    def_no_death.hit = UnitStateClip::from_a1(make_seq("def0_hit", 1, false));
    // death is default empty

    std::vector<UnitAnimationRoleSet> anims;
    anims.push_back(std::move(no_death_anim));
    anims.push_back(std::move(def_no_death));
    const std::size_t n_anims = anims.size();
    BattlePresenter   presenter = make_presenter(scene, std::move(anims), n_anims, 1u);

    // Load idle so player has a sequence name to check against
    ASSERT_TRUE(scene.replace_track_clip(scene.units()[1].id, TrackKind::Base,
                                         make_seq("def0_idle", 3, true), true));

    presenter.submit_visual_event(UnitKilled{.target = UnitInstanceId{2u}});
    presenter.update(500.0f); // fade completes
    EXPECT_FLOAT_EQ(scene.units()[1].alpha, 1.0f);
    // With no death frames, sequence stays whatever was playing
    EXPECT_NE(base_player(scene.units()[1]).sequence().name, "def0_death");
}

TEST(BattlePresenter, AttackSequenceDoesNotConvertDamageIntoHit) {
    BattleScene                       scene = make_scene(1, 1);
    std::vector<UnitAnimationRoleSet> anims{make_unit_roles("att0"), make_unit_roles("def0")};
    BattlePresenter                   presenter = make_presenter(scene, std::move(anims), 2, 1u);

    presenter.trigger_attack();
    EXPECT_EQ(base_player(scene.units()[0]).sequence().name, "att0_attack");
    scene.update(100.0f);
    presenter.update(100.0f);
    EXPECT_EQ(base_player(scene.units()[1]).sequence().name, "DEF_IDLE_0");
    scene.update(100.0f);
    presenter.update(100.0f);
    EXPECT_EQ(base_player(scene.units()[1]).sequence().name, "DEF_IDLE_0");
    scene.update(100.0f);
    presenter.update(100.0f);
    EXPECT_EQ(base_player(scene.units()[0]).sequence().name, "att0_idle");
    EXPECT_EQ(base_player(scene.units()[1]).sequence().name, "DEF_IDLE_0");
}

TEST(BattlePresenter, SubmittedActorSelectedUpdatesSelection) {
    BattleScene                       scene = make_scene(1, 1);
    std::vector<UnitAnimationRoleSet> anims{make_unit_roles("att0"), make_unit_roles("def0")};
    BattlePresenter                   presenter = make_presenter(scene, std::move(anims), 2, 1u);
    presenter.set_marker_large_animation(make_seq("marker"));

    presenter.submit_visual_event(
        ActorSelected{.previous = UnitInstanceId{1u}, .selected = UnitInstanceId{2u}});

    EXPECT_EQ(presenter.selected_unit_id(), UnitInstanceId{2u});
    EXPECT_TRUE(scene.units()[1].find_track(TrackKind::ActorMarker).has_value());
}

TEST(BattlePresenter, SmallUnitSelectionUsesSmallMarker) {
    BattleScene                       scene = make_scene(1, 1); // both units small by default
    std::vector<UnitAnimationRoleSet> anims{make_unit_roles("att0"), make_unit_roles("def0")};
    BattlePresenter                   presenter = make_presenter(scene, std::move(anims), 2, 1u);
    presenter.set_marker_small_animation(make_seq("MRKCURSMALLA", 1, true));
    presenter.set_marker_large_animation(make_seq("MRKCURLARGEA", 1, true));

    presenter.submit_visual_event(
        ActorSelected{.previous = UnitInstanceId{1u}, .selected = UnitInstanceId{2u}});

    const auto marker_idx = scene.units()[1].find_track(TrackKind::ActorMarker);
    ASSERT_TRUE(marker_idx.has_value()) << "ActorMarker track must be spawned on selected unit";
    EXPECT_EQ(scene.units()[1].tracks[*marker_idx].player.sequence().name, "MRKCURSMALLA")
        << "Small unit must use MRKCURSMALLA, not large marker";
}

TEST(BattlePresenter, LargeUnitSelectionUsesLargeMarker) {
    BattleScene scene;
    {
        BattleUnit unit;
        unit.unit_instance_id = UnitInstanceId{1u};
        unit.visual_profile_id = UnitVisualProfileId{1u};
        unit.lifecycle_profile_id = UnitLifecycleVisualProfileId{1u};
        unit.is_large = false;
        unit.coord =
            BattleSlotCoord{.side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Front};
        unit.direction = 'A';
        VisualTrack base;
        base.kind = TrackKind::Base;
        base.layer = TrackRenderLayer::Base;
        base.anchor = AnchorPolicy::UnitCanvasFoot;
        base.player.load(make_seq("ATT_IDLE"));
        base.player.play();
        unit.tracks.push_back(std::move(base));
        scene.add_unit(std::move(unit));
    }
    {
        BattleUnit unit;
        unit.unit_instance_id = UnitInstanceId{2u};
        unit.visual_profile_id = UnitVisualProfileId{2u};
        unit.lifecycle_profile_id = UnitLifecycleVisualProfileId{2u};
        unit.is_large = true; // large!
        unit.coord =
            BattleSlotCoord{.side = BattleSide::Defender, .lane = 0, .depth = BattleDepth::Front};
        unit.direction = 'D';
        VisualTrack base;
        base.kind = TrackKind::Base;
        base.layer = TrackRenderLayer::Base;
        base.anchor = AnchorPolicy::UnitCanvasFoot;
        base.player.load(make_seq("DEF_IDLE_0"));
        base.player.play();
        unit.tracks.push_back(std::move(base));
        scene.add_unit(std::move(unit));
    }
    std::vector<UnitAnimationRoleSet> anims{make_unit_roles("att0"), make_unit_roles("def0")};
    BattlePresenter                   presenter = make_presenter(scene, std::move(anims), 2, 1u);
    presenter.set_marker_small_animation(make_seq("MRKCURSMALLA", 1, true));
    presenter.set_marker_large_animation(make_seq("MRKCURLARGEA", 1, true));

    presenter.submit_visual_event(
        ActorSelected{.previous = UnitInstanceId{1u}, .selected = UnitInstanceId{2u}});

    const auto marker_idx = scene.units()[1].find_track(TrackKind::ActorMarker);
    ASSERT_TRUE(marker_idx.has_value()) << "ActorMarker track must be spawned on selected unit";
    EXPECT_EQ(scene.units()[1].tracks[*marker_idx].player.sequence().name, "MRKCURLARGEA")
        << "Large unit must use MRKCURLARGEA, not small marker";
}

TEST(BattlePresenter, SubmittedAttackReachesAnimationInspection) {
    BattleScene                       scene = make_scene(1, 1);
    std::vector<UnitAnimationRoleSet> anims{make_unit_roles("att0"), make_unit_roles("def0")};
    BattlePresenter                   presenter = make_presenter(scene, std::move(anims), 2, 1u);

    presenter.submit_visual_event(AttackStarted{.attack_id = AttackInstanceId{99u},
                                                .source = UnitInstanceId{1u},
                                                .targets = TargetSet::single(UnitInstanceId{2u})});

    const auto inspection = presenter.inspection();
    ASSERT_TRUE(inspection.active_event.has_value());
    EXPECT_EQ(*inspection.active_event, "AttackStarted");
    EXPECT_GT(inspection.active_command_count, 0u);
}

TEST(BattlePresenter, TriggerAttackReachesAnimationInspection) {
    BattleScene                       scene = make_scene(1, 1);
    std::vector<UnitAnimationRoleSet> anims{make_unit_roles("att0"), make_unit_roles("def0")};
    BattlePresenter                   presenter = make_presenter(scene, std::move(anims), 2, 1u);

    presenter.trigger_attack();

    const auto inspection = presenter.inspection();
    ASSERT_TRUE(inspection.active_event.has_value());
    EXPECT_EQ(*inspection.active_event, "AttackStarted");
    EXPECT_GT(inspection.active_command_count, 0u);
}

TEST(BattlePresenter, ReviveUsesTimelineFadeAndRemovesDeathBody) {
    BattleScene                       scene = make_scene(1, 1);
    std::vector<UnitAnimationRoleSet> anims{make_unit_roles("att0"), make_unit_roles("def0")};
    BattlePresenter                   presenter = make_presenter(
        scene, std::move(anims), 2, 1u,
        make_lifecycle_profiles(2, make_seq("death", 1, false), make_seq("revive", 1, false)));

    presenter.submit_visual_event(UnitKilled{.target = UnitInstanceId{2u}});
    scene.update(100.0f); // advance death_fx animation to completed so WaitTrackComplete resolves
    presenter.update(500.0f);
    ASSERT_EQ(scene.units()[1].life_state, LifeVisualState::Dead);
    ASSERT_TRUE(scene.units()[1].find_track(TrackKind::DeathBody).has_value());

    presenter.submit_visual_event(UnitReviveStarted{.target = UnitInstanceId{2u}});
    presenter.submit_visual_event(UnitRevived{.target = UnitInstanceId{2u}});
    EXPECT_EQ(scene.units()[1].life_state, LifeVisualState::Reviving);
    presenter.update(2000.0f);
    EXPECT_EQ(scene.units()[1].life_state, LifeVisualState::Alive);
    EXPECT_FLOAT_EQ(scene.find_track(scene.units()[1].id, TrackKind::Base)->alpha, 1.0f);
    scene.update(0.0f);
    EXPECT_FALSE(scene.units()[1].find_track(TrackKind::DeathBody).has_value());
}

TEST(BattlePresenter, HeffAndTuchUseTimelineEffects) {
    BattleScene                       scene = make_scene(1, 1);
    std::vector<UnitAnimationRoleSet> anims{make_unit_roles("att0"), make_unit_roles("def0")};
    BattlePresenter                   presenter = make_presenter(scene, std::move(anims), 2, 1u);

    presenter.trigger_heff();
    auto effect = scene.units()[0].find_track(TrackKind::Effect);
    ASSERT_TRUE(effect.has_value());
    EXPECT_EQ(scene.units()[0].tracks[*effect].anchor, AnchorPolicy::OppositeTeamCentroid);
    scene.update(100.0f);
    presenter.update(0.0f);
    scene.update(0.0f);

    presenter.trigger_tuch();
    effect = scene.units()[0].find_track(TrackKind::Effect);
    ASSERT_TRUE(effect.has_value());
    EXPECT_EQ(scene.units()[0].tracks[*effect].anchor, AnchorPolicy::OppositeLaneMidpoint);
}

TEST(BattlePresenter, ExposesSemanticEventInspection) {
    BattleScene                       scene = make_scene(1, 1);
    std::vector<UnitAnimationRoleSet> anims{make_unit_roles("att0"), make_unit_roles("def0")};
    BattlePresenter                   presenter = make_presenter(
        scene, std::move(anims), 2, 1u, make_lifecycle_profiles(2, make_seq("death_fx", 1, false)));

    presenter.submit_visual_event(UnitKilled{.target = scene.units().at(1).unit_instance_id});
    const auto inspection = presenter.inspection();
    ASSERT_TRUE(inspection.active_event.has_value());
    EXPECT_EQ(*inspection.active_event, "UnitKilled");
    EXPECT_GT(inspection.active_command_count, 0u);
    EXPECT_EQ(inspection.snapshot.entities[1].unit_instance_id, UnitInstanceId{2u});
}

TEST(BattlePresenter, SelectionUsesEventWhilePauseRemainsDirect) {
    BattleScene                       scene = make_scene(1, 1);
    std::vector<UnitAnimationRoleSet> anims{make_unit_roles("att0"), make_unit_roles("def0")};
    BattlePresenter                   presenter = make_presenter(scene, std::move(anims), 2, 1u);
    presenter.set_marker_large_animation(make_seq("marker"));

    presenter.cycle_actor();
    EXPECT_EQ(presenter.selected_unit_id(), UnitInstanceId{2u});
    EXPECT_TRUE(scene.units()[1].find_track(TrackKind::ActorMarker).has_value());

    presenter.cycle_target();
    EXPECT_EQ(presenter.selected_target_id(), UnitInstanceId{1u});
    EXPECT_TRUE(scene.units()[0].find_track(TrackKind::TargetMarker).has_value());
    EXPECT_TRUE(scene.units()[1].find_track(TrackKind::ActorMarker).has_value());

    // P remains Battle-local visual pause; it is dispatched as ToggleVisualPause.
    BattleUnit* unit2 = scene.try_unit_by_id(presenter.selected_entity_id());
    ASSERT_NE(unit2, nullptr);
    unit2->paused = true;
    EXPECT_TRUE(unit2->paused);
    EXPECT_FALSE(presenter.inspection().active_event.has_value());
}

TEST(BattlePresenter, CycleActorSkipsDead) {
    // 3 units: attacker(0), defender(1)=dead, defender(2)=alive
    BattleScene scene;
    for (int i = 0; i < 3; ++i) {
        BattleUnit unit;
        const auto uid = static_cast<std::uint32_t>(i + 1);
        unit.unit_instance_id = UnitInstanceId{uid};
        unit.visual_profile_id = UnitVisualProfileId{uid};
        unit.lifecycle_profile_id = UnitLifecycleVisualProfileId{uid};
        unit.coord = BattleSlotCoord{.side = (i == 0 ? BattleSide::Attacker : BattleSide::Defender),
                                     .lane = i,
                                     .depth = BattleDepth::Front};
        VisualTrack base;
        base.kind = TrackKind::Base;
        base.layer = TrackRenderLayer::Base;
        base.anchor = AnchorPolicy::UnitCanvasFoot;
        base.player.load(make_seq("seq"));
        base.player.play();
        unit.tracks.push_back(std::move(base));
        scene.add_unit(std::move(unit));
    }

    std::vector<UnitAnimationRoleSet> anims;
    anims.push_back(make_unit_roles("att0"));
    anims.push_back(make_unit_roles("def0"));
    anims.push_back(make_unit_roles("def1"));
    const std::size_t n_anims = anims.size();
    BattlePresenter   presenter =
        make_presenter(scene, std::move(anims), n_anims, 1u,
                       make_lifecycle_profiles(n_anims, make_seq("death_fx", 1, false)));

    // Kill unit 1 (make it Dead)
    presenter.submit_visual_event(UnitKilled{.target = scene.units().at(1).unit_instance_id});
    presenter.update(500.0f); // fade → Dead

    // selected_unit_idx_ starts at 0; Tab should skip 1 (Dead) and go to 2
    presenter.cycle_actor();
    EXPECT_EQ(presenter.selected_unit_id(), UnitInstanceId{3u});
}

TEST(BattlePresenter, PreviewRoleIdleReplacesAnimation) {
    BattleScene                       scene = make_scene(1, 1);
    std::vector<UnitAnimationRoleSet> anims;
    anims.push_back(make_unit_roles("att0"));
    anims.push_back(make_unit_roles("def0"));
    const std::size_t n_anims = anims.size();
    BattlePresenter   presenter = make_presenter(scene, std::move(anims), n_anims, 1u);

    // Trigger hit so the sequence changes from idle
    presenter.update(2001.0f);
    EXPECT_EQ(base_player(scene.units()[0]).sequence().name, "ATT_IDLE");

    // Select unit 1 and set role to idle
    presenter.cycle_actor(); // select unit 1
    presenter.preview_role_idle();
    // Base track should have the idle sequence
    const auto base_idx = scene.units()[1].find_track(TrackKind::Base);
    EXPECT_TRUE(base_idx.has_value());
    EXPECT_EQ(scene.units()[1].tracks[*base_idx].player.sequence().name, "def0_idle");
}

TEST(BattleScenePresentationState, ToggleLayersCyclesVisibility) {
    BattleScenePresentationState state;

    EXPECT_TRUE(state.background_visible);
    EXPECT_TRUE(state.frame_visible);
    EXPECT_EQ(state.layer_cycle, 0);

    state.toggle_layers();
    EXPECT_TRUE(state.background_visible);
    EXPECT_FALSE(state.frame_visible);
    EXPECT_EQ(state.layer_cycle, 1);

    state.toggle_layers();
    EXPECT_FALSE(state.background_visible);
    EXPECT_FALSE(state.frame_visible);
    EXPECT_EQ(state.layer_cycle, 2);

    state.toggle_layers();
    EXPECT_TRUE(state.background_visible);
    EXPECT_TRUE(state.frame_visible);
    EXPECT_EQ(state.layer_cycle, 0);
}

TEST(BattlePresenter, RuntimeHpStateMutatesFromVisualEventsAndSnapshots) {
    BattleScene scene = make_scene(1, 1);
    auto*       att = scene.try_unit_by_id(scene.units().at(0).id);
    ASSERT_NE(att, nullptr);
    att->display_name = "Attacker";
    att->current_hp = 40;
    att->max_hp = 40;
    auto* def = scene.try_unit_by_id(scene.units().at(1).id);
    ASSERT_NE(def, nullptr);
    def->display_name = "Defender";
    def->current_hp = 250;
    def->max_hp = 250;

    auto presenter = make_presenter(scene, {make_unit_roles("att0"), make_unit_roles("def0")}, 2, 1,
                                    make_lifecycle_profiles(2));

    presenter.submit_visual_event(TargetDamaged{.attack_id = AttackInstanceId{1},
                                                .source = UnitInstanceId{1},
                                                .target = UnitInstanceId{2},
                                                .current_hp = 177});

    const auto snapshot = scene.snapshot();
    ASSERT_EQ(snapshot.entities.size(), 2u);
    EXPECT_EQ(snapshot.entities[1].display_name, "Defender");
    EXPECT_EQ(snapshot.entities[1].current_hp, 177);
    EXPECT_EQ(snapshot.entities[1].max_hp, 250);
}

TEST(BattleDebugSceneController, PausedUnitDoesNotAdvance) {
    BattleScene                      scene = make_scene(0, 1);
    const BattleDebugSceneController debug_controller;
    const std::size_t frame_before = base_player(scene.units()[0]).current_frame_index();

    ASSERT_TRUE(debug_controller.toggle_pause(scene, scene.units().at(0).id));
    scene.update(500.0f);

    EXPECT_EQ(base_player(scene.units()[0]).current_frame_index(), frame_before);
}

TEST(BattleDebugSceneController, StepFrameAdvancesWhenPaused) {
    BattleScene                      scene = make_scene(0, 1);
    const BattleDebugSceneController debug_controller;
    ASSERT_TRUE(debug_controller.toggle_pause(scene, scene.units().at(0).id));
    const std::size_t frame_before = base_player(scene.units()[0]).current_frame_index();

    ASSERT_TRUE(debug_controller.step_frame(scene, scene.units().at(0).id, 1));
    EXPECT_EQ(base_player(scene.units()[0]).current_frame_index(), frame_before + 1);
}

TEST(BattleDebugSceneController, MoveUnitAppliesOffset) {
    BattleScene                      scene = make_scene(0, 1);
    const BattleDebugSceneController debug_controller;
    const VisualEntityId             id = scene.units().at(0).id;

    ASSERT_TRUE(debug_controller.move_unit(scene, id, Vec2{.x = 3.0f, .y = -2.0f}));
    const auto offset = d2engine::BattleDebugSceneController::unit_position_offset(scene, id);

    ASSERT_TRUE(offset.has_value());
    EXPECT_FLOAT_EQ(offset->x, 3.0f);
    EXPECT_FLOAT_EQ(offset->y, -2.0f);
}

TEST(AnimationPlayer, Step_ClampsAtEnd) {
    AnimationPlayer         player;
    AnimationSequence const seq = make_seq("test", 3, false);
    player.load(seq);
    player.play();

    // Step past end — non-looping should clamp/complete at last frame
    player.step(10);
    EXPECT_LT(player.current_frame_index(), seq.frames.size());
}

TEST(BattleScene, BackRowYLessThanFrontRowY) {
    // Back row is farther from the center. For Attacker (left side), back is higher on screen
    // (lower y); for Defender (right side), back is lower on screen (higher y).
    ScreenConfigStore store(std::filesystem::path(OPENDIS2_SOURCE_DIR) / "configs");
    auto config = store.load_validated("battle_screen", BattleScreen::required_layout_nodes());
    BattleTuningState state;
    load_battle_tuning_config(state, config.document, config.config_path);
    TreeLayout tree = std::move(config.tree_layout);
    for (const BattleSide side : {BattleSide::Attacker, BattleSide::Defender}) {
        for (int lane = 0; lane < 3; ++lane) {
            const BattleSlotCoord back{.side = side, .lane = lane, .depth = BattleDepth::Back};
            const BattleSlotCoord front{.side = side, .lane = lane, .depth = BattleDepth::Front};
            const float           back_y = tree.compose(battlefield_unit_tree_path(back)).y;
            const float           front_y = tree.compose(battlefield_unit_tree_path(front)).y;
            if (side == BattleSide::Attacker) {
                EXPECT_LT(back_y, front_y);
            } else {
                EXPECT_GT(back_y, front_y);
            }
        }
    }
}

TEST(BattlePresenter, UpdateDoesNotCrashWhenSceneSizeMismatch) {
    // Simulate a partial failure: presenter expects 2 units but scene only has 1
    BattleScene                       scene = make_scene(1, 0);
    std::vector<UnitAnimationRoleSet> anims;
    anims.push_back(make_unit_roles("att0"));
    anims.push_back(make_unit_roles("def0")); // Extra animation for missing unit

    const std::size_t n_anims = anims.size();
    BattlePresenter   presenter = make_presenter(scene, std::move(anims), n_anims, 1u);

    // update() must not crash when states_.size() > scene_.units().size()
    EXPECT_NO_THROW(presenter.update(100.0f));
    EXPECT_NO_THROW(presenter.update(500.0f));
}

// --- Regression: idle restored on reset_to_idle_state ---

UnitLifecycleVisualProfileRegistry make_lifecycle_profiles_with_stil(std::size_t count) {
    UnitLifecycleVisualProfileRegistry registry;
    for (std::size_t i = 0; i < count; ++i) {
        UnitLifecycleVisualProfile lp;
        lp.death.stil_clip = UnitStateClip::from_a1(make_seq("stil", 2, false));
        static_cast<void>(registry.add(std::move(lp)));
    }
    return registry;
}

TEST(BattlePresenter, ResetToIdleRestoresIdleClipAfterStilDeath) {
    BattleScene                       scene = make_scene(0, 1);
    std::vector<UnitAnimationRoleSet> anims;
    anims.push_back(make_unit_roles("def0"));
    const std::size_t n_anims = anims.size();
    BattlePresenter   presenter = make_presenter(scene, std::move(anims), n_anims, 0u,
                                                 make_lifecycle_profiles_with_stil(n_anims));

    // Kill the unit so Base becomes STIL.
    const UnitInstanceId def_id = scene.units().back().unit_instance_id;
    presenter.submit_visual_event(UnitKilled{.target = def_id});
    presenter.update(500.0f);

    // Verify Base now shows STIL (not idle).
    {
        const auto& unit = scene.units().back();
        const auto  idx = unit.find_track(TrackKind::Base);
        ASSERT_TRUE(idx.has_value());
        const auto& lp = unit.tracks[*idx].layered_player;
        ASSERT_NE(lp.clip, nullptr);
        ASSERT_TRUE(lp.clip->a1.has_value());
        EXPECT_EQ(lp.clip->a1->name, "stil");
    }

    // Reset: Base must return to idle clip.
}

// --- Regression: step_frame advances layered_player for layered Base ---

TEST(BattlePresenter, StepFrameAdvancesLayeredPlayerOnLayeredBase) {
    BattleScene scene = make_scene(1, 0);

    std::vector<UnitAnimationRoleSet> anims;
    anims.push_back(make_unit_roles("att0"));
    const std::size_t     n_anims = anims.size();
    BattlePresenter const presenter = make_presenter(scene, std::move(anims), n_anims, 1u);

    const VisualEntityId             eid = scene.units().at(0).id;
    BattleDebugSceneController const controller;

    // Pause the unit.
    ASSERT_TRUE(controller.set_all_paused(scene, true));

    // Set up a layered clip (8 frames @40ms each).
    LayeredAnimationClip layered_clip;
    layered_clip.a1 = make_seq("a1_seq", 8, true);
    for (auto& f : layered_clip.a1->frames) {
        f.duration_ms = 40;
    }
    static_cast<void>(scene.replace_track_layered_clip(eid, TrackKind::Base, layered_clip, true));

    // At start: elapsed_ms=0 → frame 0.
    EXPECT_EQ(scene.snapshot().entities[0].tracks[0].current_frame_index, 0u);

    // Step forward once → should show frame 1.
    ASSERT_TRUE(controller.step_frame(scene, eid, 1));
    EXPECT_EQ(scene.snapshot().entities[0].tracks[0].current_frame_index, 1u)
        << "StepForward must advance layered snapshot frame index";

    // Step backward once → back to frame 0.
    ASSERT_TRUE(controller.step_frame(scene, eid, -1));
    EXPECT_EQ(scene.snapshot().entities[0].tracks[0].current_frame_index, 0u)
        << "StepBackward must return layered snapshot to frame 0";
}

TEST(ActiveStepOwnershipTest, StepRemainsValidAfterSubmitReturns) {
    BattleScene     scene;
    BattlePresenter presenter(scene, UnitVisualProfileRegistry{},
                              UnitLifecycleVisualProfileRegistry{}, 0);
    {
        BattleVisualStep step;
        step.id = "test_step";
        step.complete = BattleVisualStepCompletion::Immediate;
        BattleVisualEventEnvelope env;
        env.id = "env1";
        env.event = ActorSelected{.previous = UnitInstanceId{}, .selected = UnitInstanceId{}};
        step.envelopes.push_back(std::move(env));
        presenter.submit_visual_step(step);
    } // step destroyed here — active_step_ must survive

    // Active step and its content must remain valid after the original object
    // has been destroyed (no dangling pointer, no ASAN failure)
    EXPECT_TRUE(presenter.has_active_visual_step());
    // After immediate step completes, the step was not cancelled — active_step_
    // still points to internal storage, not the destroyed local
    EXPECT_TRUE(presenter.visual_step_complete());
    EXPECT_FALSE(presenter.visual_step_failed());

    // Clear active step — still no crash
    presenter.finish_visual_step();
    EXPECT_FALSE(presenter.has_active_visual_step());
    EXPECT_TRUE(presenter.visual_step_complete());
}

} // namespace d2engine
