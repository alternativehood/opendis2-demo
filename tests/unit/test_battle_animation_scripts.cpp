#include <gtest/gtest.h>

#include "d2engine/app/battle_tuning_state.hpp"
#include "d2engine/app/battle_tuning_controller.hpp"
#include "d2engine/battle_view/battle_animation_scripts.hpp"
#include "d2engine/battle_view/battle_effect_clip.hpp"
#include "d2engine/battle_view/battle_scene.hpp"
#include "d2engine/battle_view/command_timeline.hpp"
#include "d2engine/battle_view/unit_state_clip.hpp"

#include <string>
#include <utility>

namespace d2engine {
namespace {

AnimationSequence sequence(std::string name, bool looping = false) {
    AnimationSequence result;
    result.name = std::move(name);
    result.container_path = "Imgs/Test.ff";
    result.is_looping = looping;
    result.frames.push_back(
        {.image_name = "frame", .index = 0, .duration_ms = static_cast<std::uint16_t>(100)});
    return result;
}

AnimationSequence sequence_n(std::string name, int n, std::uint16_t dur_ms = 100u) {
    AnimationSequence result;
    result.name = std::move(name);
    result.container_path = "Imgs/Test.ff";
    for (int i = 0; i < n; ++i) {
        result.frames.push_back(
            {.image_name = "f" + std::to_string(i), .index = 0, .duration_ms = dur_ms});
    }
    return result;
}

BattleScene scene_with_two_units() {
    BattleScene scene;
    for (std::uint32_t id = 1; id <= 2; ++id) {
        BattleUnit unit;
        unit.unit_instance_id = UnitInstanceId{id};
        unit.visual_profile_id = UnitVisualProfileId{id};
        unit.lifecycle_profile_id = UnitLifecycleVisualProfileId{id};
        VisualTrack base;
        base.kind = TrackKind::Base;
        base.player.load(sequence("idle", true));
        base.player.play();
        unit.tracks.push_back(std::move(base));
        scene.add_unit(std::move(unit));
    }
    return scene;
}

std::vector<UnitAnimationRoleSet> roles() {
    std::vector<UnitAnimationRoleSet> result(2);
    for (std::size_t i = 0; i < result.size(); ++i) {
        result[i].idle = UnitStateClip::from_a1(sequence("idle_" + std::to_string(i), true));
        result[i].attack = UnitStateClip::from_a1(sequence("attack_" + std::to_string(i)));
        result[i].hit = UnitStateClip::from_a1(sequence("hit_" + std::to_string(i)));
        result[i].heff = BattleEffectClip{.clip = {.a1 = sequence("heff_" + std::to_string(i))}};
        result[i].tuch = BattleEffectClip{.clip = {.a1 = sequence("tuch_" + std::to_string(i))}};
    }
    return result;
}

UnitVisualProfileRegistry role_registry() {
    UnitVisualProfileRegistry registry;
    for (auto role : roles()) {
        static_cast<void>(registry.add(std::move(role)));
    }
    return registry;
}

UnitLifecycleVisualProfileRegistry lifecycle_registry(std::size_t count,
                                                      bool        with_death_fx = true) {
    UnitLifecycleVisualProfileRegistry registry;
    for (std::size_t i = 0; i < count; ++i) {
        UnitLifecycleVisualProfile lp;
        lp.death.corpse_sprite_base = "DEAD_TEST";
        if (with_death_fx) {
            lp.death.death_fx_name = "DEATH_TEST";
            lp.death.death_fx = sequence("death_test");
        }
        lp.revive.small_fx_name = "REVIVEANIMS";
        lp.revive.small_fx = sequence("revive_small");
        lp.revive.large_fx_name = "REVIVEANIML";
        lp.revive.large_fx = sequence("revive_large");
        static_cast<void>(registry.add(std::move(lp)));
    }
    return registry;
}

UnitLifecycleVisualProfileRegistry lifecycle_registry_with_stil(std::size_t count,
                                                                bool        with_death_fx = true) {
    UnitLifecycleVisualProfileRegistry registry;
    for (std::size_t i = 0; i < count; ++i) {
        UnitLifecycleVisualProfile lp;
        lp.death.stil_clip = UnitStateClip::from_a1(sequence("stil_test", false));
        if (with_death_fx) {
            lp.death.death_fx_name = "DEATH_TEST";
            lp.death.death_fx = sequence("death_test");
        }
        lp.revive.small_fx_name = "REVIVEANIMS";
        lp.revive.small_fx = sequence("revive_small");
        lp.revive.large_fx_name = "REVIVEANIML";
        lp.revive.large_fx = sequence("revive_large");
        static_cast<void>(registry.add(std::move(lp)));
    }
    return registry;
}

const Sequence& as_sequence(const CommandNode& node) {
    return std::get<Sequence>(node.value);
}

const VisualCommand& as_command(const CommandNode& node) {
    return std::get<VisualCommand>(node.value);
}

// Returns the A1 sequence from a layered-clip PlayClip.
const AnimationSequence& transitional_sequence(const PlayClip& clip) {
    const auto* layered = std::get_if<const LayeredAnimationClip*>(&clip.clip);
    if (layered != nullptr && *layered != nullptr && (*layered)->a1.has_value()) {
        return *(*layered)->a1;
    }
    // Fallback to legacy AnimationSequence path (used in a few tests that still build directly)
    return std::get<AnimationSequence>(clip.clip);
}

const SpawnEffect& first_spawn(const CommandNode& command) {
    return std::get<SpawnEffect>(as_command(as_sequence(command).children.front()));
}

} // namespace

TEST(BattleAnimationScripts, TimedTrackSpecDefaultsAreNeutralEffectTrack) {
    const TimedTrackSpec spec;

    EXPECT_EQ(spec.track, TrackKind::Effect);
    EXPECT_EQ(spec.layer, TrackRenderLayer::Effect);
    EXPECT_EQ(spec.anchor, AnchorPolicy::UnitFoot);
    EXPECT_EQ(spec.visibility, TrackVisibility::Visible);
    EXPECT_FALSE(spec.playback.reverse_playback);
    EXPECT_FALSE(spec.playback.flip_x);
    EXPECT_FALSE(spec.playback.flip_y);
    EXPECT_EQ(spec.lifetime, EffectLifetime::TrackCompletion);
    EXPECT_FLOAT_EQ(spec.start_delay_ms, 0.0f);
    EXPECT_FALSE(spec.looping);
    EXPECT_TRUE(spec.remove_on_complete);
}

TEST(BattleAnimationScripts, TimedTrackLoweringSupportsDelayDurationAndPlayback) {
    TimedTrackSpec spec;
    spec.entity_id = VisualEntityId{7u};
    spec.effect_id = EffectInstanceId{9u};
    spec.sequence = sequence("spark");
    spec.track = TrackKind::Effect;
    spec.layer = TrackRenderLayer::Overlay;
    spec.anchor = AnchorPolicy::OppositeLaneMidpoint;
    spec.visibility = TrackVisibility::HiddenButPlaying;
    spec.playback.reverse_playback = true;
    spec.lifetime = EffectLifetime::Duration;
    spec.duration_ms = 75.0f;
    spec.start_delay_ms = 50.0f;
    spec.looping = true;

    const CommandNode command = BattleAnimationScripts::build_timed_track(spec);
    const auto&       children = as_sequence(command).children;
    ASSERT_EQ(children.size(), 4u);
    EXPECT_FLOAT_EQ(std::get<WaitTime>(as_command(children[0])).duration_ms, 50.0f);
    const auto& spawn = std::get<SpawnEffect>(as_command(children[1]));
    EXPECT_EQ(spawn.entity_id, VisualEntityId{7u});
    EXPECT_EQ(spawn.track.id, TrackId{9u});
    EXPECT_EQ(spawn.track.kind, TrackKind::Effect);
    EXPECT_EQ(spawn.track.layer, TrackRenderLayer::Overlay);
    EXPECT_EQ(spawn.track.anchor, AnchorPolicy::OppositeLaneMidpoint);
    EXPECT_EQ(spawn.track.visibility, TrackVisibility::HiddenButPlaying);
    EXPECT_TRUE(spawn.track.playback.reverse_playback);
    EXPECT_TRUE(spawn.track.player.sequence().is_looping);
    EXPECT_FLOAT_EQ(std::get<WaitTime>(as_command(children[2])).duration_ms, 75.0f);
    EXPECT_EQ(std::get<DespawnEffect>(as_command(children[3])).effect_id, EffectInstanceId{9u});
}

TEST(BattleAnimationScripts, TimedTrackLoweringSupportsCompletionWaitAndNoDespawn) {
    TimedTrackSpec spec;
    spec.entity_id = VisualEntityId{7u};
    spec.effect_id = EffectInstanceId{9u};
    spec.sequence = sequence("spark");
    spec.remove_on_complete = false;

    const CommandNode command = BattleAnimationScripts::build_timed_track(spec);
    const auto&       children = as_sequence(command).children;
    ASSERT_EQ(children.size(), 2u);
    EXPECT_TRUE(std::holds_alternative<SpawnEffect>(as_command(children[0])));
    const auto& wait = std::get<WaitTrackComplete>(as_command(children[1]));
    EXPECT_EQ(wait.entity_id, VisualEntityId{7u});
    EXPECT_EQ(wait.track.id, TrackId{9u});
}

TEST(BattleAnimationScripts, BuildsAttackSequenceAndRejectsMissingTargets) {
    const BattleScene               scene = scene_with_two_units();
    UnitVisualProfileRegistry const unit_profiles = role_registry();
    const BattleScriptContext       context{
        .scene = scene,
        .assets = {.unit_profiles = &unit_profiles},
    };

    const auto command = BattleAnimationScripts::build(
        AttackStarted{.attack_id = AttackInstanceId{3u},
                      .source = UnitInstanceId{1u},
                      .targets = TargetSet::single(UnitInstanceId{2u})},
        context);
    ASSERT_TRUE(command.has_value());
    ASSERT_TRUE(std::holds_alternative<Sequence>(command->value));
    const auto& attack_sequence = as_sequence(*command);
    ASSERT_EQ(attack_sequence.children.size(), 5u);
    EXPECT_TRUE(std::holds_alternative<PlayClip>(as_command(attack_sequence.children[0])));
    EXPECT_TRUE(std::holds_alternative<WaitTime>(as_command(attack_sequence.children[1])));
    const auto& emitted = std::get<EmitBattleEvent>(as_command(attack_sequence.children[2]));
    ASSERT_TRUE(std::holds_alternative<AttackImpactCue>(emitted.event));
    const auto& cue = std::get<AttackImpactCue>(emitted.event);
    EXPECT_EQ(cue.attack_id, AttackInstanceId{3u});
    EXPECT_EQ(cue.source, UnitInstanceId{1u});
    ASSERT_TRUE(cue.targets.single_target().has_value());
    EXPECT_EQ(*cue.targets.single_target(), UnitInstanceId{2u});

    EXPECT_FALSE(
        BattleAnimationScripts::build(UnitHitReceived{.target = UnitInstanceId{99u}}, context)
            .has_value());
}

TEST(BattleAnimationScripts, TargetDamagedDoesNotBuildHitReaction) {
    const BattleScene               scene = scene_with_two_units();
    UnitVisualProfileRegistry const unit_profiles = role_registry();
    const BattleScriptContext       context{
        .scene = scene,
        .assets = {.unit_profiles = &unit_profiles},
    };

    const auto command =
        BattleAnimationScripts::build(TargetDamaged{.attack_id = AttackInstanceId{3u},
                                                    .source = UnitInstanceId{1u},
                                                    .target = UnitInstanceId{2u}},
                                      context);
    EXPECT_FALSE(
        command.has_value()); // direct build returns nullopt; FX spawned via BattleEffectStarted
}

TEST(BattleAnimationScripts, UnitHitReceivedBuildsHitReaction) {
    const BattleScene               scene = scene_with_two_units();
    UnitVisualProfileRegistry const unit_profiles = role_registry();
    const BattleScriptContext       context{
        .scene = scene,
        .assets = {.unit_profiles = &unit_profiles},
    };

    const auto command =
        BattleAnimationScripts::build(UnitHitReceived{.target = UnitInstanceId{2u}}, context);
    ASSERT_TRUE(command.has_value());
    const auto& hit_sequence = as_sequence(*command);
    ASSERT_EQ(hit_sequence.children.size(), 3u);
    const auto& hit = std::get<PlayClip>(as_command(hit_sequence.children[0]));
    EXPECT_EQ(transitional_sequence(hit).name, "hit_1");
}

TEST(BattleAnimationScripts, ReorderedUnitsUseStoredProfileIds) {
    BattleScene scene;
    {
        BattleUnit unit;
        unit.unit_instance_id = UnitInstanceId{7u};
        unit.visual_profile_id = UnitVisualProfileId{2u};
        unit.lifecycle_profile_id = UnitLifecycleVisualProfileId{2u};
        VisualTrack base;
        base.kind = TrackKind::Base;
        base.player.load(sequence("idle", true));
        base.player.play();
        unit.tracks.push_back(std::move(base));
        scene.add_unit(std::move(unit));
    }
    {
        BattleUnit unit;
        unit.unit_instance_id = UnitInstanceId{3u};
        unit.visual_profile_id = UnitVisualProfileId{1u};
        unit.lifecycle_profile_id = UnitLifecycleVisualProfileId{1u};
        VisualTrack base;
        base.kind = TrackKind::Base;
        base.player.load(sequence("idle", true));
        base.player.play();
        unit.tracks.push_back(std::move(base));
        scene.add_unit(std::move(unit));
    }

    UnitVisualProfileRegistry const unit_profiles = role_registry();
    const BattleScriptContext       context{
        .scene = scene,
        .assets = {.unit_profiles = &unit_profiles},
    };

    const auto command =
        BattleAnimationScripts::build(UnitHitReceived{.target = UnitInstanceId{3u}}, context);

    ASSERT_TRUE(command.has_value());
    const auto& hit_sequence = as_sequence(*command);
    const auto& hit = std::get<PlayClip>(as_command(hit_sequence.children[0]));
    EXPECT_EQ(transitional_sequence(hit).name, "hit_0");
}

TEST(BattleAnimationScripts, MissAndResistDoNotBuildHitReaction) {
    const BattleScene               scene = scene_with_two_units();
    UnitVisualProfileRegistry const unit_profiles = role_registry();
    const BattleScriptContext       context{
        .scene = scene,
        .assets = {.unit_profiles = &unit_profiles},
    };

    EXPECT_FALSE(
        BattleAnimationScripts::build(
            TargetMissed{.attack_id = AttackInstanceId{3u}, .target = UnitInstanceId{2u}}, context)
            .has_value());
    EXPECT_FALSE(BattleAnimationScripts::build(TargetResisted{.attack_id = AttackInstanceId{3u},
                                                              .target = UnitInstanceId{2u}},
                                               context)
                     .has_value());
}

TEST(BattleAnimationScripts, UsesConfiguredDeathFadeAndParallelStructure) {
    const BattleScene                        scene = scene_with_two_units();
    UnitVisualProfileRegistry const          unit_profiles = role_registry();
    UnitLifecycleVisualProfileRegistry const lifecycle_profiles = lifecycle_registry(2);
    BattleScriptParameters                   parameters;
    parameters.death_fade_ms = 250.0f;
    const BattleScriptContext context{
        .scene = scene,
        .assets = {.unit_profiles = &unit_profiles, .lifecycle_profiles = &lifecycle_profiles},
        .parameters = parameters,
    };

    const auto command =
        BattleAnimationScripts::build(UnitKilled{.target = UnitInstanceId{2u}}, context);
    ASSERT_TRUE(command.has_value());
    const auto& parallel = std::get<Parallel>(command->value);
    ASSERT_EQ(parallel.children.size(), 2u);
    const auto& death_sequence = as_sequence(parallel.children[1]);
    const auto& tween = std::get<TweenAlpha>(as_command(death_sequence.children[1]));
    EXPECT_FLOAT_EQ(tween.duration_ms, 250.0f);
    EXPECT_TRUE(std::holds_alternative<SpawnEffect>(as_command(death_sequence.children.back())));
}

TEST(BattleAnimationScripts, KillUsesLifecycleDeathFxNotEffectsField) {
    const BattleScene                        scene = scene_with_two_units();
    UnitVisualProfileRegistry const          unit_profiles = role_registry();
    UnitLifecycleVisualProfileRegistry const lifecycle_profiles = lifecycle_registry(2);
    const BattleScriptContext                context{
        .scene = scene,
        .assets = {.unit_profiles = &unit_profiles, .lifecycle_profiles = &lifecycle_profiles},
    };

    const auto command =
        BattleAnimationScripts::build(UnitKilled{.target = UnitInstanceId{2u}}, context);
    ASSERT_TRUE(command.has_value());
    const auto& parallel = std::get<Parallel>(command->value);
    const auto& spawn =
        std::get<SpawnEffect>(as_command(as_sequence(parallel.children[0]).children[0]));
    EXPECT_EQ(spawn.track.player.sequence().name, "death_test");
}

TEST(BattleAnimationScripts, KillWithoutDeathFxBuildsDeathSequenceOnly) {
    const BattleScene                        scene = scene_with_two_units();
    UnitVisualProfileRegistry const          unit_profiles = role_registry();
    UnitLifecycleVisualProfileRegistry const lifecycle_profiles = lifecycle_registry(2, false);
    const BattleScriptContext                context{
        .scene = scene,
        .assets = {.unit_profiles = &unit_profiles, .lifecycle_profiles = &lifecycle_profiles},
    };

    const auto command =
        BattleAnimationScripts::build(UnitKilled{.target = UnitInstanceId{2u}}, context);
    // death_fx is optional — kill must succeed without it
    ASSERT_TRUE(command.has_value());
    // No death_fx → command is a Sequence, not a Parallel
    EXPECT_TRUE(std::holds_alternative<Sequence>(command->value));
}

TEST(BattleAnimationScripts, KillEmitsSetLifeVisualStateDead) {
    const BattleScene                        scene = scene_with_two_units();
    UnitVisualProfileRegistry const          unit_profiles = role_registry();
    UnitLifecycleVisualProfileRegistry const lifecycle_profiles = lifecycle_registry(2);
    const BattleScriptContext                context{
        .scene = scene,
        .assets = {.unit_profiles = &unit_profiles, .lifecycle_profiles = &lifecycle_profiles},
    };

    const auto command =
        BattleAnimationScripts::build(UnitKilled{.target = UnitInstanceId{2u}}, context);
    ASSERT_TRUE(command.has_value());
    const auto& parallel = std::get<Parallel>(command->value);
    const auto& death_seq = as_sequence(parallel.children[1]);
    const auto& state_cmd = std::get<SetLifeVisualState>(as_command(death_seq.children[2]));
    EXPECT_EQ(state_cmd.state, LifeVisualState::Dead);
}

TEST(BattleAnimationScripts, KillUsesCorpseSpriteBaseForDeathBodyTrack) {
    const BattleScene                        scene = scene_with_two_units();
    UnitVisualProfileRegistry const          unit_profiles = role_registry();
    UnitLifecycleVisualProfileRegistry const lifecycle_profiles = lifecycle_registry(2);
    const BattleScriptContext                context{
        .scene = scene,
        .assets = {.unit_profiles = &unit_profiles, .lifecycle_profiles = &lifecycle_profiles},
    };

    const auto command =
        BattleAnimationScripts::build(UnitKilled{.target = UnitInstanceId{2u}}, context);
    ASSERT_TRUE(command.has_value());
    const auto& parallel = std::get<Parallel>(command->value);
    const auto& death_seq = as_sequence(parallel.children[1]);
    const auto& body_spawn = std::get<SpawnEffect>(as_command(death_seq.children.back()));
    EXPECT_EQ(body_spawn.track.kind, TrackKind::DeathBody);
    EXPECT_EQ(body_spawn.track.player.sequence().frames[0].image_name, "DEAD_TEST00");
}

TEST(BattleAnimationScripts, KillWithStilClipPlaysStilOnBase) {
    const BattleScene                        scene = scene_with_two_units();
    UnitVisualProfileRegistry const          unit_profiles = role_registry();
    UnitLifecycleVisualProfileRegistry const lifecycle_profiles = lifecycle_registry_with_stil(2);
    const BattleScriptContext                context{
        .scene = scene,
        .assets = {.unit_profiles = &unit_profiles, .lifecycle_profiles = &lifecycle_profiles},
    };

    const auto command =
        BattleAnimationScripts::build(UnitKilled{.target = UnitInstanceId{2u}}, context);
    ASSERT_TRUE(command.has_value());
    // STIL + death_fx → Parallel; branch 1 is the STIL sequence
    const auto& parallel = std::get<Parallel>(command->value);
    ASSERT_GT(parallel.children.size(), 1U);
    const auto& death_seq = as_sequence(parallel.children[1]);
    const auto& play = std::get<PlayClip>(as_command(death_seq.children[0]));
    EXPECT_EQ(play.track.kind, TrackKind::Base);
    const auto* layered = std::get_if<const LayeredAnimationClip*>(&play.clip);
    ASSERT_NE(layered, nullptr);
    const LayeredAnimationClip* clip = *layered;
    ASSERT_NE(clip, nullptr);
    ASSERT_TRUE(clip->a1.has_value());
    EXPECT_EQ(clip->a1->name, "stil_test");
}

TEST(BattleAnimationScripts, KillWithStilClipSetsLifeStateDead) {
    const BattleScene                        scene = scene_with_two_units();
    UnitVisualProfileRegistry const          unit_profiles = role_registry();
    UnitLifecycleVisualProfileRegistry const lifecycle_profiles = lifecycle_registry_with_stil(2);
    const BattleScriptContext                context{
        .scene = scene,
        .assets = {.unit_profiles = &unit_profiles, .lifecycle_profiles = &lifecycle_profiles},
    };

    const auto command =
        BattleAnimationScripts::build(UnitKilled{.target = UnitInstanceId{2u}}, context);
    ASSERT_TRUE(command.has_value());
    const auto& parallel = std::get<Parallel>(command->value);
    const auto& death_seq = as_sequence(parallel.children[1]);
    // SetLifeVisualState is the last step (after wait + hide + optional corpse).
    const auto& last = death_seq.children.back();
    const auto& state = std::get<SetLifeVisualState>(as_command(last));
    EXPECT_EQ(state.state, LifeVisualState::Dead);
}

TEST(BattleAnimationScripts, KillWithStilClipDoesNotSpawnDeathBody) {
    const BattleScene                        scene = scene_with_two_units();
    UnitVisualProfileRegistry const          unit_profiles = role_registry();
    UnitLifecycleVisualProfileRegistry const lifecycle_profiles = lifecycle_registry_with_stil(2);
    const BattleScriptContext                context{
        .scene = scene,
        .assets = {.unit_profiles = &unit_profiles, .lifecycle_profiles = &lifecycle_profiles},
    };

    const auto command =
        BattleAnimationScripts::build(UnitKilled{.target = UnitInstanceId{2u}}, context);
    ASSERT_TRUE(command.has_value());
    const auto& parallel = std::get<Parallel>(command->value);
    for (const auto& branch : parallel.children) {
        for (const auto& step : as_sequence(branch).children) {
            if (const auto* vc = std::get_if<VisualCommand>(&step.value)) {
                if (const auto* spawn = std::get_if<SpawnEffect>(vc)) {
                    EXPECT_NE(spawn->track.kind, TrackKind::DeathBody);
                }
            }
        }
    }
}

TEST(BattleAnimationScripts, KillWithStilClipNoDeathFxReturnsSequence) {
    const BattleScene                        scene = scene_with_two_units();
    UnitVisualProfileRegistry const          unit_profiles = role_registry();
    UnitLifecycleVisualProfileRegistry const lifecycle_profiles =
        lifecycle_registry_with_stil(2, false /*no death_fx*/);
    const BattleScriptContext context{
        .scene = scene,
        .assets = {.unit_profiles = &unit_profiles, .lifecycle_profiles = &lifecycle_profiles},
    };

    const auto command =
        BattleAnimationScripts::build(UnitKilled{.target = UnitInstanceId{2u}}, context);
    ASSERT_TRUE(command.has_value());
    // No death_fx → plain Sequence, not Parallel
    EXPECT_TRUE(std::holds_alternative<Sequence>(command->value));
    const auto& seq = as_sequence(*command);
    ASSERT_GE(seq.children.size(), 2u);
    EXPECT_TRUE(std::holds_alternative<VisualCommand>(seq.children[0].value));
    EXPECT_TRUE(std::holds_alternative<VisualCommand>(seq.children[1].value));
}

TEST(BattleAnimationScripts, KillWithStilClipUsesUnitBaseRole) {
    const BattleScene                        scene = scene_with_two_units();
    UnitVisualProfileRegistry const          unit_profiles = role_registry();
    UnitLifecycleVisualProfileRegistry const lifecycle_profiles = lifecycle_registry_with_stil(2);
    const BattleScriptContext                context{
        .scene = scene,
        .assets = {.unit_profiles = &unit_profiles, .lifecycle_profiles = &lifecycle_profiles},
    };

    const auto command =
        BattleAnimationScripts::build(UnitKilled{.target = UnitInstanceId{2u}}, context);
    ASSERT_TRUE(command.has_value());
    const auto& parallel = std::get<Parallel>(command->value);
    const auto& death_seq = as_sequence(parallel.children[1]);
    const auto& play = std::get<PlayClip>(as_command(death_seq.children[0]));
    EXPECT_EQ(play.unit_anim_role, BindingRole::UnitBase);
    EXPECT_NE(play.unit_anim_role, BindingRole::UnitIdle);
}

// Lifecycle registry with STIL, a corpse sprite, and optionally multi-frame STIL.
UnitLifecycleVisualProfileRegistry
lifecycle_registry_with_stil_and_corpse(std::size_t count, int stil_frames = 1,
                                        bool with_death_fx = false) {
    UnitLifecycleVisualProfileRegistry registry;
    for (std::size_t i = 0; i < count; ++i) {
        UnitLifecycleVisualProfile lp;
        lp.death.stil_clip = UnitStateClip::from_a1(sequence_n("stil_test", stil_frames, 100u));
        lp.death.corpse_sprite_base = "DEAD_TEST";
        if (with_death_fx) {
            lp.death.death_fx_name = "DEATH_TEST";
            lp.death.death_fx = sequence("death_test");
        }
        lp.revive.small_fx = sequence("revive_small");
        lp.revive.large_fx = sequence("revive_large");
        static_cast<void>(registry.add(std::move(lp)));
    }
    return registry;
}

// Regression: STIL plays then Base is hidden and DeathBody is spawned.
TEST(BattleAnimationScripts, KillWithStilAndCorpseHidesBaseAndSpawnsDeathBody) {
    const BattleScene                        scene = scene_with_two_units();
    UnitVisualProfileRegistry const          unit_profiles = role_registry();
    UnitLifecycleVisualProfileRegistry const lifecycle_profiles =
        lifecycle_registry_with_stil_and_corpse(2, /*stil_frames=*/1, /*with_death_fx=*/false);
    const BattleScriptContext context{
        .scene = scene,
        .assets = {.unit_profiles = &unit_profiles, .lifecycle_profiles = &lifecycle_profiles},
    };

    const auto cmd =
        BattleAnimationScripts::build(UnitKilled{.target = UnitInstanceId{2u}}, context);
    ASSERT_TRUE(cmd.has_value());
    const auto& seq = as_sequence(*cmd);

    // Must contain SetTrackVisibility Disabled on Base.
    bool has_disable = false;
    bool has_corpse = false;
    for (const auto& step : seq.children) {
        if (const auto* vc = std::get_if<VisualCommand>(&step.value)) {
            if (const auto* vis = std::get_if<SetTrackVisibility>(vc)) {
                if (vis->visibility == TrackVisibility::Disabled)
                    has_disable = true;
            }
            if (const auto* spawn = std::get_if<SpawnEffect>(vc)) {
                if (spawn->track.kind == TrackKind::DeathBody)
                    has_corpse = true;
            }
        }
    }
    EXPECT_TRUE(has_disable) << "Base must be disabled after STIL hold";
    EXPECT_TRUE(has_corpse) << "DeathBody must be spawned after STIL";
}

// Regression: 1-frame STIL uses WaitTime, not WaitTrackComplete.
TEST(BattleAnimationScripts, KillWith1FrameStilUsesWaitTime) {
    const BattleScene                        scene = scene_with_two_units();
    UnitVisualProfileRegistry const          unit_profiles = role_registry();
    UnitLifecycleVisualProfileRegistry const lifecycle_profiles =
        lifecycle_registry_with_stil_and_corpse(2, /*stil_frames=*/1);
    const BattleScriptContext context{
        .scene = scene,
        .assets = {.unit_profiles = &unit_profiles, .lifecycle_profiles = &lifecycle_profiles},
    };

    const auto cmd =
        BattleAnimationScripts::build(UnitKilled{.target = UnitInstanceId{2u}}, context);
    ASSERT_TRUE(cmd.has_value());
    const auto& seq = as_sequence(*cmd);
    // Second step must be WaitTime, not WaitTrackComplete.
    ASSERT_GE(seq.children.size(), 2u);
    const auto* wt = std::get_if<WaitTime>(std::get_if<VisualCommand>(&seq.children[1].value));
    EXPECT_NE(wt, nullptr) << "1-frame STIL must use WaitTime for hold, not WaitTrackComplete";
}

// Regression: multi-frame (animated) STIL uses WaitTrackComplete.
TEST(BattleAnimationScripts, KillWithMultiFrameStilUsesWaitTrackComplete) {
    const BattleScene                        scene = scene_with_two_units();
    UnitVisualProfileRegistry const          unit_profiles = role_registry();
    UnitLifecycleVisualProfileRegistry const lifecycle_profiles =
        lifecycle_registry_with_stil_and_corpse(2, /*stil_frames=*/9);
    const BattleScriptContext context{
        .scene = scene,
        .assets = {.unit_profiles = &unit_profiles, .lifecycle_profiles = &lifecycle_profiles},
    };

    const auto cmd =
        BattleAnimationScripts::build(UnitKilled{.target = UnitInstanceId{2u}}, context);
    ASSERT_TRUE(cmd.has_value());
    const auto& seq = as_sequence(*cmd);
    ASSERT_GE(seq.children.size(), 2u);
    const auto* wtc =
        std::get_if<WaitTrackComplete>(std::get_if<VisualCommand>(&seq.children[1].value));
    EXPECT_NE(wtc, nullptr) << "multi-frame STIL must wait for track completion";
}

// Regression: death_fx runs in parallel with STIL transition.
TEST(BattleAnimationScripts, KillWithStilAndCorpseAndDeathFxRunsInParallel) {
    const BattleScene                        scene = scene_with_two_units();
    UnitVisualProfileRegistry const          unit_profiles = role_registry();
    UnitLifecycleVisualProfileRegistry const lifecycle_profiles =
        lifecycle_registry_with_stil_and_corpse(2, /*stil_frames=*/1, /*with_death_fx=*/true);
    const BattleScriptContext context{
        .scene = scene,
        .assets = {.unit_profiles = &unit_profiles, .lifecycle_profiles = &lifecycle_profiles},
    };

    const auto cmd =
        BattleAnimationScripts::build(UnitKilled{.target = UnitInstanceId{2u}}, context);
    ASSERT_TRUE(cmd.has_value());
    // death_fx + STIL → Parallel node.
    EXPECT_TRUE(std::holds_alternative<Parallel>(cmd->value))
        << "death_fx must run in parallel with STIL sequence";
    const auto& parallel = std::get<Parallel>(cmd->value);
    // The STIL branch (branch 1) must still contain DeathBody.
    const auto& stil_branch = as_sequence(parallel.children[1]);
    bool        has_corpse = false;
    for (const auto& step : stil_branch.children) {
        if (const auto* vc = std::get_if<VisualCommand>(&step.value)) {
            if (const auto* spawn = std::get_if<SpawnEffect>(vc)) {
                if (spawn->track.kind == TrackKind::DeathBody)
                    has_corpse = true;
            }
        }
    }
    EXPECT_TRUE(has_corpse) << "STIL branch must still spawn DeathBody when death_fx is parallel";
}

TEST(BattleAnimationScripts, TargetKilledReusesDeathFlow) {
    const BattleScene                        scene = scene_with_two_units();
    UnitVisualProfileRegistry const          unit_profiles = role_registry();
    UnitLifecycleVisualProfileRegistry const lifecycle_profiles = lifecycle_registry(2);
    const BattleScriptContext                context{
        .scene = scene,
        .assets = {.unit_profiles = &unit_profiles, .lifecycle_profiles = &lifecycle_profiles},
    };

    const auto command = BattleAnimationScripts::build(
        TargetKilled{.attack_id = AttackInstanceId{3u}, .target = UnitInstanceId{2u}}, context);
    ASSERT_TRUE(command.has_value());
    EXPECT_TRUE(std::holds_alternative<Parallel>(command->value));
}

TEST(BattleAnimationScripts, DrainAndHealUseOnlyOutcomeEvents) {
    const BattleScene               scene = scene_with_two_units();
    UnitVisualProfileRegistry const unit_profiles = role_registry();
    const AnimationSequence         drain = sequence("drain");
    const AnimationSequence         heal = sequence("heal");
    const BattleScriptContext       context{
        .scene = scene,
        .assets = {.unit_profiles = &unit_profiles, .drain = &drain, .heal = &heal},
    };

    const auto drained =
        BattleAnimationScripts::build(LifeDrained{.attack_id = AttackInstanceId{3u},
                                                  .source = UnitInstanceId{1u},
                                                  .target = UnitInstanceId{2u}},
                                      context);
    ASSERT_TRUE(drained.has_value());
    const auto& drain_spawn = std::get<SpawnEffect>(as_command(as_sequence(*drained).children[0]));
    EXPECT_EQ(drain_spawn.track.player.sequence().name, "drain");

    const auto healed = BattleAnimationScripts::build(
        SourceHealed{.attack_id = AttackInstanceId{3u}, .source = UnitInstanceId{1u}}, context);
    ASSERT_TRUE(healed.has_value());
    const auto& heal_spawn = std::get<SpawnEffect>(as_command(as_sequence(*healed).children[0]));
    EXPECT_EQ(heal_spawn.track.player.sequence().name, "heal");
}

TEST(BattleAnimationScripts, EffectPlaybackTransformComesFromParameters) {
    const BattleScene               scene = scene_with_two_units();
    UnitVisualProfileRegistry const unit_profiles = role_registry();
    const AnimationSequence         drain = sequence("drain");
    const AnimationSequence         heal = sequence("heal");
    BattleScriptParameters          parameters;
    parameters.drain.playback.reverse_playback = true;
    parameters.heal.playback.flip_y = true;
    const BattleScriptContext context{
        .scene = scene,
        .assets = {.unit_profiles = &unit_profiles, .drain = &drain, .heal = &heal},
        .parameters = parameters,
    };

    const auto drained =
        BattleAnimationScripts::build(LifeDrained{.attack_id = AttackInstanceId{3u},
                                                  .source = UnitInstanceId{1u},
                                                  .target = UnitInstanceId{2u}},
                                      context);
    ASSERT_TRUE(drained.has_value());
    const auto& drain_spawn = std::get<SpawnEffect>(as_command(as_sequence(*drained).children[0]));
    EXPECT_TRUE(drain_spawn.track.playback.reverse_playback);

    const auto healed = BattleAnimationScripts::build(
        SourceHealed{.attack_id = AttackInstanceId{3u}, .source = UnitInstanceId{1u}}, context);
    ASSERT_TRUE(healed.has_value());
    const auto& heal_spawn = std::get<SpawnEffect>(as_command(as_sequence(*healed).children[0]));
    EXPECT_TRUE(heal_spawn.track.playback.flip_y);
}

TEST(BattleAnimationScripts, UsesConfiguredCastAnchor) {
    const BattleScene               scene = scene_with_two_units();
    UnitVisualProfileRegistry const unit_profiles = role_registry();
    BattleScriptParameters          parameters;
    parameters.heff.anchor = AnchorPolicy::OppositeLaneMidpoint;
    const BattleScriptContext context{
        .scene = scene,
        .assets = {.unit_profiles = &unit_profiles},
        .parameters = parameters,
    };

    const auto cast = BattleAnimationScripts::build(
        CastEffectStarted{.caster = UnitInstanceId{1u}, .role = BattleEffectRole::Heff}, context);
    ASSERT_TRUE(cast.has_value());
    const auto& cast_spawn = std::get<SpawnEffect>(as_command(as_sequence(*cast).children.front()));
    EXPECT_EQ(cast_spawn.track.anchor, AnchorPolicy::OppositeLaneMidpoint);
}

TEST(BattleAnimationScripts, BattleEffectSourceUnitUsesSourceRoleClipAndAnchor) {
    const BattleScene               scene = scene_with_two_units();
    UnitVisualProfileRegistry const unit_profiles = role_registry();
    const BattleScriptContext       context{
        .scene = scene,
        .assets = {.unit_profiles = &unit_profiles},
    };

    const auto command = BattleAnimationScripts::build(
        BattleEffectStarted{.source = UnitInstanceId{1u},
                            .role = BattleEffectRole::Heff,
                            .visual_role = BattleEffectVisualRole::SourceCastFx},
        context);

    ASSERT_TRUE(command.has_value());
    const auto& spawn = first_spawn(*command);
    EXPECT_EQ(spawn.entity_id, scene.units()[0].id);
    EXPECT_EQ(spawn.track.player.sequence().name, "heff_0");
    EXPECT_EQ(spawn.track.anchor, AnchorPolicy::UnitFoot);
}

TEST(BattleAnimationScripts, BattleEffectOverlayUsesSourceClipOneOverlayTrack) {
    const BattleScene               scene = scene_with_two_units();
    UnitVisualProfileRegistry const unit_profiles = role_registry();
    const BattleScriptContext       context{
        .scene = scene,
        .assets = {.unit_profiles = &unit_profiles},
    };

    const auto command = BattleAnimationScripts::build(
        BattleEffectStarted{.source = UnitInstanceId{1u},
                            .role = BattleEffectRole::Tuch,
                            .visual_role = BattleEffectVisualRole::TeamOverlayFx,
                            .targets = TargetSet::unit_list({UnitInstanceId{2u}, UnitInstanceId{3u},
                                                             UnitInstanceId{4u}, UnitInstanceId{5u},
                                                             UnitInstanceId{6u}})},
        context);

    ASSERT_TRUE(command.has_value());
    const auto& children = as_sequence(*command).children;
    ASSERT_EQ(children.size(), 3u);
    const auto& spawn = first_spawn(*command);
    EXPECT_EQ(spawn.track.player.sequence().name, "tuch_0");
    EXPECT_EQ(spawn.track.anchor, AnchorPolicy::OppositeTeamCentroid);
    EXPECT_EQ(spawn.track.layer, TrackRenderLayer::Overlay);
}

TEST(BattleAnimationScripts, BattleEffectDoesNotBreakCastEffectStarted) {
    const BattleScene               scene = scene_with_two_units();
    UnitVisualProfileRegistry const unit_profiles = role_registry();
    BattleScriptParameters          parameters;
    parameters.heff.anchor = AnchorPolicy::OppositeLaneMidpoint;
    const BattleScriptContext context{
        .scene = scene,
        .assets = {.unit_profiles = &unit_profiles},
        .parameters = parameters,
    };

    const auto command = BattleAnimationScripts::build(
        CastEffectStarted{.caster = UnitInstanceId{1u}, .role = BattleEffectRole::Heff}, context);

    ASSERT_TRUE(command.has_value());
    const auto& spawn = first_spawn(*command);
    EXPECT_EQ(spawn.track.player.sequence().name, "heff_0");
    EXPECT_EQ(spawn.track.anchor, AnchorPolicy::OppositeLaneMidpoint);
}

TEST(BattleAnimationScripts, BuildsSelectionAndReviveVisibilityCommands) {
    const BattleScene               scene = scene_with_two_units();
    UnitVisualProfileRegistry const unit_profiles = role_registry();
    const AnimationSequence         marker = sequence("marker", true);
    const BattleScriptContext       context{
        .scene = scene,
        .assets = {.unit_profiles = &unit_profiles, .marker = &marker},
    };

    const auto selected = BattleAnimationScripts::build(
        ActorSelected{.previous = UnitInstanceId{1u}, .selected = UnitInstanceId{2u}}, context);
    ASSERT_TRUE(selected.has_value());
    EXPECT_EQ(as_sequence(*selected).children.size(), 2u);

    const auto target = BattleAnimationScripts::build(
        TargetSelected{.previous = UnitInstanceId{1u}, .selected = UnitInstanceId{2u}}, context);
    ASSERT_TRUE(target.has_value());

    const auto revived =
        BattleAnimationScripts::build(UnitRevived{.target = UnitInstanceId{2u}}, context);
    ASSERT_TRUE(revived.has_value());
    const auto& revive_sequence = as_sequence(*revived);
    EXPECT_TRUE(
        std::holds_alternative<SetTrackVisibility>(as_command(revive_sequence.children[3])));
    EXPECT_TRUE(std::holds_alternative<TweenAlpha>(as_command(revive_sequence.children[6])));
}

TEST(BattleAnimationScripts, SmallUnitUsesSmallMarkerFromAssets) {
    BattleScene scene;
    {
        BattleUnit unit;
        unit.unit_instance_id = UnitInstanceId{1u};
        unit.visual_profile_id = UnitVisualProfileId{1u};
        unit.is_large = false;
        VisualTrack base;
        base.kind = TrackKind::Base;
        base.player.load(sequence("idle", true));
        base.player.play();
        unit.tracks.push_back(std::move(base));
        scene.add_unit(std::move(unit));
    }
    {
        BattleUnit unit;
        unit.unit_instance_id = UnitInstanceId{2u};
        unit.visual_profile_id = UnitVisualProfileId{2u};
        unit.is_large = false;
        VisualTrack base;
        base.kind = TrackKind::Base;
        base.player.load(sequence("idle", true));
        base.player.play();
        unit.tracks.push_back(std::move(base));
        scene.add_unit(std::move(unit));
    }
    UnitVisualProfileRegistry const unit_profiles = role_registry();
    const AnimationSequence         marker_small = sequence("MRKCURSMALLA", true);
    const AnimationSequence         marker_large = sequence("MRKCURLARGEA", true);
    const BattleScriptContext       context{
        .scene = scene,
        .assets = {.unit_profiles = &unit_profiles,
                   .marker_small = &marker_small,
                   .marker_large = &marker_large},
    };

    const auto selected = BattleAnimationScripts::build(
        ActorSelected{.previous = UnitInstanceId{1u}, .selected = UnitInstanceId{2u}}, context);
    ASSERT_TRUE(selected.has_value());
    ASSERT_EQ(as_sequence(*selected).children.size(), 2u);
    const auto& spawn = std::get<SpawnEffect>(as_command(as_sequence(*selected).children[1]));
    EXPECT_EQ(spawn.track.player.sequence().name, "MRKCURSMALLA");
}

TEST(BattleAnimationScripts, LargeUnitUsesLargeMarkerFromAssets) {
    BattleScene scene;
    {
        BattleUnit unit;
        unit.unit_instance_id = UnitInstanceId{1u};
        unit.visual_profile_id = UnitVisualProfileId{1u};
        unit.is_large = false;
        VisualTrack base;
        base.kind = TrackKind::Base;
        base.player.load(sequence("idle", true));
        base.player.play();
        unit.tracks.push_back(std::move(base));
        scene.add_unit(std::move(unit));
    }
    {
        BattleUnit unit;
        unit.unit_instance_id = UnitInstanceId{2u};
        unit.visual_profile_id = UnitVisualProfileId{2u};
        unit.is_large = true;
        VisualTrack base;
        base.kind = TrackKind::Base;
        base.player.load(sequence("idle", true));
        base.player.play();
        unit.tracks.push_back(std::move(base));
        scene.add_unit(std::move(unit));
    }
    UnitVisualProfileRegistry const unit_profiles = role_registry();
    const AnimationSequence         marker_small = sequence("MRKCURSMALLA", true);
    const AnimationSequence         marker_large = sequence("MRKCURLARGEA", true);
    const BattleScriptContext       context{
        .scene = scene,
        .assets = {.unit_profiles = &unit_profiles,
                   .marker_small = &marker_small,
                   .marker_large = &marker_large},
    };

    const auto selected = BattleAnimationScripts::build(
        ActorSelected{.previous = UnitInstanceId{1u}, .selected = UnitInstanceId{2u}}, context);
    ASSERT_TRUE(selected.has_value());
    ASSERT_EQ(as_sequence(*selected).children.size(), 2u);
    const auto& spawn = std::get<SpawnEffect>(as_command(as_sequence(*selected).children[1]));
    EXPECT_EQ(spawn.track.player.sequence().name, "MRKCURLARGEA");
}

TEST(BattleAnimationScripts, MarkerFallsBackToLegacyWhenSizeSpecificNull) {
    BattleScene scene;
    {
        BattleUnit unit;
        unit.unit_instance_id = UnitInstanceId{1u};
        unit.visual_profile_id = UnitVisualProfileId{1u};
        unit.is_large = false;
        VisualTrack base;
        base.kind = TrackKind::Base;
        base.player.load(sequence("idle", true));
        base.player.play();
        unit.tracks.push_back(std::move(base));
        scene.add_unit(std::move(unit));
    }
    {
        BattleUnit unit;
        unit.unit_instance_id = UnitInstanceId{2u};
        unit.visual_profile_id = UnitVisualProfileId{2u};
        unit.is_large = true;
        VisualTrack base;
        base.kind = TrackKind::Base;
        base.player.load(sequence("idle", true));
        base.player.play();
        unit.tracks.push_back(std::move(base));
        scene.add_unit(std::move(unit));
    }
    UnitVisualProfileRegistry const unit_profiles = role_registry();
    const AnimationSequence         marker = sequence("legacy_marker", true);
    const BattleScriptContext       context{
        .scene = scene,
        .assets = {.unit_profiles = &unit_profiles, .marker = &marker},
    };

    const auto selected = BattleAnimationScripts::build(
        ActorSelected{.previous = UnitInstanceId{1u}, .selected = UnitInstanceId{2u}}, context);
    ASSERT_TRUE(selected.has_value());
    ASSERT_EQ(as_sequence(*selected).children.size(), 2u);
    const auto& spawn = std::get<SpawnEffect>(as_command(as_sequence(*selected).children[1]));
    EXPECT_EQ(spawn.track.player.sequence().name, "legacy_marker");
}

TEST(BattleAnimationScripts, ReviveStartedUsesSmallFxForSmallUnit) {
    const BattleScene                        scene = scene_with_two_units();
    UnitVisualProfileRegistry const          unit_profiles = role_registry();
    UnitLifecycleVisualProfileRegistry const lifecycle_profiles = lifecycle_registry(2);
    const BattleScriptContext                context{
        .scene = scene,
        .assets = {.unit_profiles = &unit_profiles, .lifecycle_profiles = &lifecycle_profiles},
    };

    const auto command =
        BattleAnimationScripts::build(UnitReviveStarted{.target = UnitInstanceId{2u}}, context);
    ASSERT_TRUE(command.has_value());
    const auto& seq = as_sequence(*command);
    ASSERT_GE(seq.children.size(), 2u);
    const auto& spawn = std::get<SpawnEffect>(as_command(seq.children[1]));
    EXPECT_EQ(spawn.track.player.sequence().name, "revive_small");
}

TEST(BattleAnimationScripts, ReviveStartedUsesLargeFxForLargeUnit) {
    BattleScene scene;
    {
        BattleUnit unit;
        unit.unit_instance_id = UnitInstanceId{1u};
        unit.visual_profile_id = UnitVisualProfileId{1u};
        unit.lifecycle_profile_id = UnitLifecycleVisualProfileId{1u};
        unit.is_large = true;
        VisualTrack base;
        base.kind = TrackKind::Base;
        base.player.load(sequence("idle", true));
        base.player.play();
        unit.tracks.push_back(std::move(base));
        scene.add_unit(std::move(unit));
    }
    UnitVisualProfileRegistry const          unit_profiles = role_registry();
    UnitLifecycleVisualProfileRegistry const lifecycle_profiles = lifecycle_registry(2);
    const BattleScriptContext                context{
        .scene = scene,
        .assets = {.unit_profiles = &unit_profiles, .lifecycle_profiles = &lifecycle_profiles},
    };

    const auto command =
        BattleAnimationScripts::build(UnitReviveStarted{.target = UnitInstanceId{1u}}, context);
    ASSERT_TRUE(command.has_value());
    const auto& seq = as_sequence(*command);
    ASSERT_GE(seq.children.size(), 2u);
    const auto& spawn = std::get<SpawnEffect>(as_command(seq.children[1]));
    EXPECT_EQ(spawn.track.player.sequence().name, "revive_large");
}

TEST(BattleAnimationScripts, RevivedEmitsDespawnDeathBodyAndSetAlive) {
    const BattleScene                        scene = scene_with_two_units();
    UnitVisualProfileRegistry const          unit_profiles = role_registry();
    UnitLifecycleVisualProfileRegistry const lifecycle_profiles = lifecycle_registry(2);
    const BattleScriptContext                context{
        .scene = scene,
        .assets = {.unit_profiles = &unit_profiles, .lifecycle_profiles = &lifecycle_profiles},
    };

    const auto revived =
        BattleAnimationScripts::build(UnitRevived{.target = UnitInstanceId{2u}}, context);
    ASSERT_TRUE(revived.has_value());
    const auto& seq = as_sequence(*revived);
    EXPECT_TRUE(std::holds_alternative<SetLifeVisualState>(as_command(seq.children.back())));
    const auto& final_state = std::get<SetLifeVisualState>(as_command(seq.children.back()));
    EXPECT_EQ(final_state.state, LifeVisualState::Alive);
}

TEST(BattleAnimationScripts, KillTreeContainsNoReviveCommands) {
    const BattleScene                        scene = scene_with_two_units();
    UnitVisualProfileRegistry const          unit_profiles = role_registry();
    UnitLifecycleVisualProfileRegistry const lifecycle_profiles = lifecycle_registry(2);
    const BattleScriptContext                context{
        .scene = scene,
        .assets = {.unit_profiles = &unit_profiles, .lifecycle_profiles = &lifecycle_profiles},
    };

    const auto command =
        BattleAnimationScripts::build(UnitKilled{.target = UnitInstanceId{2u}}, context);
    ASSERT_TRUE(command.has_value());
    const auto& parallel = std::get<Parallel>(command->value);
    for (const auto& child : parallel.children) {
        const auto& seq = as_sequence(child);
        for (const auto& step : seq.children) {
            if (const auto* vc = std::get_if<VisualCommand>(&step.value); vc != nullptr) {
                if (const auto* spawn = std::get_if<SpawnEffect>(vc); spawn != nullptr) {
                    EXPECT_NE(spawn->track.kind, TrackKind::ReviveFx);
                }
            }
        }
    }
}

TEST(BattleAnimationScripts, ReviveStartedTreeContainsNoDeathFxCommands) {
    const BattleScene                        scene = scene_with_two_units();
    UnitVisualProfileRegistry const          unit_profiles = role_registry();
    UnitLifecycleVisualProfileRegistry const lifecycle_profiles = lifecycle_registry(2);
    const BattleScriptContext                context{
        .scene = scene,
        .assets = {.unit_profiles = &unit_profiles, .lifecycle_profiles = &lifecycle_profiles},
    };

    const auto command =
        BattleAnimationScripts::build(UnitReviveStarted{.target = UnitInstanceId{2u}}, context);
    ASSERT_TRUE(command.has_value());
    const auto& seq = as_sequence(*command);
    for (const auto& step : seq.children) {
        if (const auto* vc = std::get_if<VisualCommand>(&step.value); vc != nullptr) {
            if (const auto* spawn = std::get_if<SpawnEffect>(vc); spawn != nullptr) {
                EXPECT_NE(spawn->track.kind, TrackKind::DeathFx);
            }
        }
    }
}

// --- pre-impact Tuch scheduling ---

namespace {
// Attacker scene: 1 unit with a custom role set, targets unit 2 (need not exist in scene).
struct AttackFixture {
    BattleScene               scene;
    UnitVisualProfileRegistry roles_reg;

    // attack_frames * 100ms = total attack duration; impact = total/2
    // tuch: 1 frame, 100ms avg_frame_ms
    explicit AttackFixture(int attack_frames = 10) {
        BattleUnit unit;
        unit.unit_instance_id = UnitInstanceId{1u};
        unit.visual_profile_id = UnitVisualProfileId{1u};
        VisualTrack base;
        base.kind = TrackKind::Base;
        base.player.load(sequence("idle", true));
        base.player.play();
        unit.tracks.push_back(std::move(base));
        scene.add_unit(std::move(unit));

        UnitAnimationRoleSet r;
        r.idle = UnitStateClip::from_a1(sequence("idle", true));
        r.attack = UnitStateClip::from_a1(sequence_n("attack", attack_frames));
        r.hit = UnitStateClip::from_a1(sequence("hit"));
        r.heff = BattleEffectClip{.clip = LayeredAnimationClip{.a1 = sequence("heff")},
                                  .family = "heff"};
        r.tuch = BattleEffectClip{.clip = LayeredAnimationClip{.a1 = sequence("tuch")},
                                  .family = "tuch"}; // 1 frame, 100ms
        static_cast<void>(roles_reg.add(std::move(r)));
    }

    [[nodiscard]] BattleScriptContext context(int tuch_frame_delay) const {
        BattleScriptParameters params;
        if (tuch_frame_delay != 0) {
            params.sequence_frame_delays["tuch"] = tuch_frame_delay;
        }
        return BattleScriptContext{
            .scene = scene, .assets = {.unit_profiles = &roles_reg}, .parameters = params};
    }
};
} // namespace

// build_attack always returns a plain Sequence; SourceCastFx is injected by the resolver, not
// in-tree.
TEST(BattleAnimationScripts, AttackBuildAlwaysProducesSequence) {
    AttackFixture const fx;
    for (const int frame_delay : {-2, 0, 3}) {
        const auto ctx = fx.context(frame_delay);
        const auto cmd = BattleAnimationScripts::build(
            AttackStarted{.attack_id = AttackInstanceId{1u},
                          .source = UnitInstanceId{1u},
                          .targets = TargetSet::single(UnitInstanceId{2u})},
            ctx);
        ASSERT_TRUE(cmd.has_value());
        EXPECT_TRUE(std::holds_alternative<Sequence>(cmd->value)) << "frame_delay=" << frame_delay;
    }
}

// --- frame_delay regression tests ---

// Negative delay clamps to 0 and never pre-advances the player.
// 4-frame sequence at 100ms/frame, start_delay=250ms, frame_delay=-2
// → offset = -200ms → total = max(250-200, 0) = 50ms → WaitTime{50ms} + SpawnEffect at frame 0
TEST(BattleAnimationScripts, NegativeFrameDelayReducesWaitAndKeepsFrame0) {
    AnimationSequence seq;
    seq.name = "effect";
    for (int i = 0; i < 4; ++i) {
        seq.frames.push_back(
            {.image_name = "f" + std::to_string(i), .index = 0, .duration_ms = 100u});
    }
    TimedTrackSpec spec;
    spec.entity_id = VisualEntityId{1u};
    spec.effect_id = EffectInstanceId{1u};
    spec.sequence = seq;
    spec.start_delay_ms = 250.0f;
    spec.frame_delay = -2;

    const CommandNode command = BattleAnimationScripts::build_timed_track(std::move(spec));
    const auto&       children = as_sequence(command).children;
    // children: WaitTime{50}, SpawnEffect, WaitTrackComplete, DespawnEffect
    ASSERT_GE(children.size(), 2u);
    const auto& wait = std::get<WaitTime>(as_command(children[0]));
    EXPECT_FLOAT_EQ(wait.duration_ms, 50.0f);
    const auto& spawn = std::get<SpawnEffect>(as_command(children[1]));
    EXPECT_EQ(spawn.track.player.current_frame_index(), 0u);
}

// start_delay=0, frame_delay=-2 → total = max(0-200, 0) = 0 → no WaitTime, SpawnEffect at frame 0
TEST(BattleAnimationScripts, NegativeDelayWithZeroStartDelaySpawnsImmediatelyAtFrame0) {
    AnimationSequence seq;
    seq.name = "effect";
    for (int i = 0; i < 4; ++i) {
        seq.frames.push_back(
            {.image_name = "f" + std::to_string(i), .index = 0, .duration_ms = 100u});
    }
    TimedTrackSpec spec;
    spec.entity_id = VisualEntityId{1u};
    spec.effect_id = EffectInstanceId{2u};
    spec.sequence = seq;
    spec.start_delay_ms = 0.0f;
    spec.frame_delay = -2;

    const CommandNode command = BattleAnimationScripts::build_timed_track(std::move(spec));
    const auto&       children = as_sequence(command).children;
    // No WaitTime: first child is SpawnEffect
    const auto& spawn = std::get<SpawnEffect>(as_command(children[0]));
    EXPECT_EQ(spawn.track.player.current_frame_index(), 0u);
}

// start_delay=50ms, frame_delay=+2, 100ms/frame → total = 50+200 = 250ms → WaitTime{250} + frame 0
TEST(BattleAnimationScripts, PositiveFrameDelayIncreasesWaitAndKeepsFrame0) {
    TimedTrackSpec spec;
    spec.entity_id = VisualEntityId{1u};
    spec.effect_id = EffectInstanceId{3u};
    spec.sequence = sequence("effect");
    spec.start_delay_ms = 50.0f;
    spec.frame_delay = 2;

    const CommandNode command = BattleAnimationScripts::build_timed_track(std::move(spec));
    const auto&       children = as_sequence(command).children;
    ASSERT_GE(children.size(), 2u);
    const auto& wait = std::get<WaitTime>(as_command(children[0]));
    EXPECT_FLOAT_EQ(wait.duration_ms, 250.0f);
    const auto& spawn = std::get<SpawnEffect>(as_command(children[1]));
    EXPECT_EQ(spawn.track.player.current_frame_index(), 0u);
}

// SpawnEffect in the command timeline must not pre-advance the player.
TEST(BattleAnimationScripts, SpawnEffectNeverPreAdvancesPlayer) {
    BattleScene scene;
    {
        BattleUnit  unit;
        VisualTrack base;
        base.kind = TrackKind::Base;
        base.player.load(sequence("idle", true));
        base.player.play();
        unit.tracks.push_back(std::move(base));
        scene.add_unit(std::move(unit));
    }
    const auto id = scene.units()[0].id;

    VisualTrack effect_track;
    effect_track.id = TrackId{7u};
    effect_track.kind = TrackKind::Effect;
    effect_track.effect_id = EffectInstanceId{7u};
    AnimationSequence eff_seq;
    eff_seq.name = "eff";
    for (int i = 0; i < 4; ++i) {
        eff_seq.frames.push_back(
            {.image_name = "f" + std::to_string(i), .index = 0, .duration_ms = 100u});
    }
    effect_track.player.load(std::move(eff_seq));

    CommandTimeline timeline;
    timeline.enqueue(CommandNode::command(SpawnEffect{.entity_id = id, .track = effect_track}));
    timeline.update(scene, 0.0f);

    const VisualTrack* spawned = scene.find_effect(EffectInstanceId{7u});
    ASSERT_NE(spawned, nullptr);
    EXPECT_EQ(spawned->player.current_frame_index(), 0u);
}

// Debug tuning: pressing -= or = must only change frame_delay, not step any track frame.
TEST(BattleAnimationScripts, DebugTuningDelayKeysChangeOnlyFrameDelay) {
    ScreenConfigStore      test_store(std::filesystem::path{"/tmp"});
    BattleTuningController controller(test_store);
    controller.set_enabled(true);

    DebugRenderableItem item;
    item.stable_id = "test_effect";
    item.binding = ConfigBinding{.config_file = "dummy.json",
                                 .owner_kind = BindingOwnerKind::EffectProfile,
                                 .tree_path = "test_effect",
                                 .role = BindingRole::Global,
                                 .display_path = "test"};
    controller.state().current_items.push_back(item);
    controller.state().selected_debug_id = "test_effect";

    EXPECT_TRUE(
        controller.apply_edit(DebugTuningEditAction{DebugTuningEditKind::ParameterIncrease, 1.0f}));
    EXPECT_EQ(controller.state().placement(*item.binding).frame_delay, 1);

    EXPECT_TRUE(
        controller.apply_edit(DebugTuningEditAction{DebugTuningEditKind::ParameterDecrease, 1.0f}));
    EXPECT_EQ(controller.state().placement(*item.binding).frame_delay, 0);

    EXPECT_TRUE(
        controller.apply_edit(DebugTuningEditAction{DebugTuningEditKind::ParameterDecrease, 1.0f}));
    EXPECT_EQ(controller.state().placement(*item.binding).frame_delay, -1);
}

TEST(BattleAnimationScripts, STILALoadedIntoRolesDeathField) {
    UnitVisualProfileRegistry reg;
    UnitAnimationRoleSet      roles;
    roles.death = UnitStateClip::from_a1(sequence("stila_test"));
    static_cast<void>(reg.add(std::move(roles)));

    BattleScene scene;
    {
        BattleUnit unit;
        unit.unit_instance_id = UnitInstanceId{1u};
        unit.visual_profile_id = UnitVisualProfileId{1u};
        unit.lifecycle_profile_id = UnitLifecycleVisualProfileId{1u};
        VisualTrack base;
        base.kind = TrackKind::Base;
        base.player.load(sequence("idle", true));
        base.player.play();
        unit.tracks.push_back(std::move(base));
        scene.add_unit(std::move(unit));
    }
    const UnitAnimationRoleSet* loaded = reg.roles(UnitVisualProfileId{1u});
    ASSERT_NE(loaded, nullptr);
    EXPECT_FALSE(loaded->death.is_empty());
    ASSERT_TRUE(loaded->death.clip.a1.has_value());
    EXPECT_EQ(loaded->death.clip.a1->name, "stila_test");
}

TEST(BattleAnimationScripts, TeamAttackOverlayFxSpawnUsesTargetTeamRole) {
    BattleScene               scene;
    UnitVisualProfileRegistry unit_profiles;
    {
        UnitAnimationRoleSet roles;
        roles.idle = UnitStateClip::from_a1(sequence("idle_nm", true));
        roles.attack = UnitStateClip::from_a1(sequence("attack_nm"));
        roles.attack_fx_override = UnitAttackFxOverride{
            .source_attack_overlay =
                BattleEffectClip{.clip = LayeredAnimationClip{.a1 = sequence("source_fx")},
                                 .family = "source_fx"},
            .team_attack_overlay =
                BattleEffectClip{.clip = LayeredAnimationClip{.a1 = sequence("team_fx")},
                                 .family = "team_fx"},
            .target_damage_fx =
                BattleEffectClip{.clip = LayeredAnimationClip{.a1 = sequence("target_fx")},
                                 .family = "target_fx"},
        };
        const auto profile_id = unit_profiles.add(std::move(roles));
        BattleUnit unit;
        unit.unit_instance_id = UnitInstanceId{1u};
        unit.visual_profile_id = profile_id;
        VisualTrack base;
        base.kind = TrackKind::Base;
        base.player.load(sequence("idle_nm", true));
        base.player.play();
        unit.tracks.push_back(std::move(base));
        scene.add_unit(std::move(unit));
    }

    const BattleScriptContext context{
        .scene = scene,
        .assets = {.unit_profiles = &unit_profiles},
    };

    const auto command = BattleAnimationScripts::build(
        BattleEffectStarted{.source = UnitInstanceId{1u},
                            .visual_role = BattleEffectVisualRole::TeamAttackOverlayFx},
        context);

    ASSERT_TRUE(command.has_value());
    const auto& spawn = first_spawn(*command);
    EXPECT_EQ(spawn.track.effect_role, BindingRole::TargetTeam)
        << "TeamAttackOverlayFx must use TargetTeam role, not Source";
}

} // namespace d2engine
