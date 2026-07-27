#include <gtest/gtest.h>

#include "d2engine/battle_view/battle_animation_engine.hpp"
#include "d2engine/battle_view/battle_effect_clip.hpp"
#include "d2engine/battle_view/unit_state_clip.hpp"

#include <string>
#include <variant>

namespace d2engine {
namespace {

AnimationSequence sequence(std::string name, bool looping = false) {
    AnimationSequence result;
    result.name = std::move(name);
    result.is_looping = looping;
    result.frames.push_back(
        {.image_name = "frame", .index = 0, .duration_ms = static_cast<std::uint16_t>(100)});
    return result;
}

struct EngineFixture {
    BattleScene                        scene;
    UnitVisualProfileRegistry          roles;
    UnitLifecycleVisualProfileRegistry lifecycle_assets;

    EngineFixture() {
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

            UnitAnimationRoleSet role;
            role.idle = UnitStateClip::from_a1(sequence("idle_" + std::to_string(id), true));
            role.attack = UnitStateClip::from_a1(sequence("attack_" + std::to_string(id)));
            role.hit = UnitStateClip::from_a1(sequence("hit_" + std::to_string(id)));
            role.heff = BattleEffectClip{.clip = {.a1 = sequence("heff_" + std::to_string(id))}};
            role.tuch = BattleEffectClip{.clip = {.a1 = sequence("tuch_" + std::to_string(id))}};
            static_cast<void>(roles.add(std::move(role)));

            UnitLifecycleVisualProfile lp;
            lp.death.death_fx = sequence("death_fx");
            lp.death.corpse_sprite_base = "DEAD_TEST";
            lp.revive.small_fx = sequence("revive_small");
            lp.revive.large_fx = sequence("revive_large");
            static_cast<void>(lifecycle_assets.add(std::move(lp)));
        }
    }

    [[nodiscard]] BattleScriptAssets assets() const {
        return {.unit_profiles = &roles, .lifecycle_profiles = &lifecycle_assets};
    }
};

} // namespace

TEST(BattleAnimationEngine, ExecutesFifoAndReportsInspection) {
    EngineFixture          fixture;
    BattleScriptParameters parameters;
    parameters.death_fade_ms = 100.0f;
    parameters.revive_delay_ms = 50.0f;
    parameters.revive_fade_ms = 100.0f;
    BattleAnimationEngine engine{fixture.scene, fixture.assets(), parameters};

    engine.submit(UnitKilled{.target = UnitInstanceId{2u}});
    engine.submit(UnitReviveStarted{.target = UnitInstanceId{2u}});
    engine.submit(UnitRevived{.target = UnitInstanceId{2u}});
    engine.update(0.0f);

    auto inspection = engine.inspection();
    ASSERT_TRUE(inspection.active_event.has_value());
    EXPECT_EQ(*inspection.active_event, "UnitKilled");
    EXPECT_EQ(inspection.queued_event_count, 2u);
    EXPECT_GT(inspection.active_command_count, 0u);
    ASSERT_EQ(inspection.snapshot.entities.size(), 2u);
    EXPECT_EQ(inspection.snapshot.entities[1].unit_instance_id, UnitInstanceId{2u});

    fixture.scene.update(100.0f); // advance death_fx animation player → Completed
    engine.update(100.0f);        // WaitTrackComplete resolves; kill completes; revive/revived run
    inspection = engine.inspection();
    ASSERT_TRUE(inspection.active_event.has_value());
    EXPECT_EQ(*inspection.active_event, "UnitRevived");
    EXPECT_EQ(fixture.scene.units()[1].life_state, LifeVisualState::Reviving);

    engine.update(150.0f);
    inspection = engine.inspection();
    EXPECT_FALSE(inspection.active_event.has_value());
    EXPECT_EQ(inspection.active_command_count, 0u);
    EXPECT_EQ(fixture.scene.units()[1].life_state, LifeVisualState::Alive);
}

TEST(BattleAnimationEngine, SkipsMissingTargetAndRunsNextEvent) {
    EngineFixture         fixture;
    BattleAnimationEngine engine{fixture.scene, fixture.assets()};
    engine.submit(UnitKilled{.target = UnitInstanceId{99u}});
    engine.submit(UnitHitReceived{.target = UnitInstanceId{2u}});
    engine.update(0.0f);

    const auto inspection = engine.inspection();
    ASSERT_TRUE(inspection.active_event.has_value());
    EXPECT_EQ(*inspection.active_event, "UnitHitReceived");
    EXPECT_EQ(inspection.queued_event_count, 0u);
}

TEST(BattleAnimationEngine, IdenticalUpdatesAreDeterministic) {
    EngineFixture          first;
    EngineFixture          second;
    BattleScriptParameters parameters;
    parameters.death_fade_ms = 100.0f;
    BattleAnimationEngine first_engine{first.scene, first.assets(), parameters};
    BattleAnimationEngine second_engine{second.scene, second.assets(), parameters};
    first_engine.submit(UnitKilled{.target = UnitInstanceId{2u}});
    second_engine.submit(UnitKilled{.target = UnitInstanceId{2u}});

    first_engine.update(37.0f);
    second_engine.update(37.0f);
    const auto first_id = first.scene.units().at(1).id;
    const auto second_id = second.scene.units().at(1).id;
    EXPECT_FLOAT_EQ(first.scene.find_track(first_id, TrackKind::Base)->alpha,
                    second.scene.find_track(second_id, TrackKind::Base)->alpha);
    EXPECT_EQ(first_engine.inspection().active_event, second_engine.inspection().active_event);
}

TEST(BattleAnimationEngine, DrainsImpactCueFromAttackPresentation) {
    EngineFixture         fixture;
    BattleAnimationEngine engine{fixture.scene, fixture.assets()};

    engine.submit(AttackStarted{.attack_id = AttackInstanceId{5u},
                                .source = UnitInstanceId{1u},
                                .targets = TargetSet::single(UnitInstanceId{2u})});
    engine.update(50.0F);

    auto emitted = engine.drain_emitted_events();
    ASSERT_EQ(emitted.size(), 1u);
    ASSERT_TRUE(std::holds_alternative<AttackImpactCue>(emitted.front()));
    const auto& cue = std::get<AttackImpactCue>(emitted.front());
    EXPECT_EQ(cue.attack_id, AttackInstanceId{5u});
    ASSERT_TRUE(cue.targets.single_target().has_value());
    EXPECT_EQ(*cue.targets.single_target(), UnitInstanceId{2u});
}

TEST(BattleAnimationEngine, BatchStartsAttackAndSourceFxTogether) {
    EngineFixture         fixture;
    BattleAnimationEngine engine{fixture.scene, fixture.assets()};

    const auto result = engine.submit_batch(
        {AttackStarted{.attack_id = AttackInstanceId{5u},
                       .source = UnitInstanceId{1u},
                       .targets = TargetSet::single(UnitInstanceId{2u})},
         CastEffectStarted{.caster = UnitInstanceId{1u}, .role = BattleEffectRole::Heff}});

    // Resolver injects BattleEffectStarted{Tuch/SourceCastFx} from AttackStarted, so 3 commands run
    // concurrently.
    EXPECT_EQ(result.started_event_names.size(), 3u);
    EXPECT_EQ(result.missing_event_count, 0u);
    EXPECT_EQ(engine.inspection().active_command_count, 3u);
}

TEST(BattleAnimationEngine, ConcurrentSourceCastFxAndTeamOverlayGetDistinctEffectTracks) {
    // Regression: SourceCastFx and TeamOverlayFx from the same source must occupy distinct
    // EffectSlots (9 and 10) so both succeed without a duplicate-track rejection.
    EngineFixture         fixture;
    BattleAnimationEngine engine{fixture.scene, fixture.assets()};

    const auto source = UnitInstanceId{1u};
    const auto result = engine.submit_batch(
        {BattleEffectStarted{.source = source,
                             .role = BattleEffectRole::Tuch,
                             .visual_role = BattleEffectVisualRole::SourceCastFx},
         BattleEffectStarted{.source = source,
                             .role = BattleEffectRole::Tuch,
                             .visual_role = BattleEffectVisualRole::TeamOverlayFx,
                             .targets = TargetSet::single(UnitInstanceId{2u})}});

    // Both events must succeed — neither is rejected as a duplicate track.
    EXPECT_EQ(result.missing_event_count, 0u);
    // Each BattleEffectStarted that builds a track adds one active command.
    EXPECT_GE(engine.inspection().active_command_count, 1u);
}

TEST(BattleAnimationEngine, TargetKilledReachesDeadState) {
    EngineFixture          fixture;
    BattleScriptParameters parameters;
    parameters.death_fade_ms = 100.0F;
    BattleAnimationEngine engine{fixture.scene, fixture.assets(), parameters};

    engine.submit(TargetKilled{.attack_id = AttackInstanceId{6u}, .target = UnitInstanceId{2u}});
    engine.update(0.0F);
    EXPECT_EQ(fixture.scene.units()[1].life_state, LifeVisualState::FadingOut);

    engine.update(100.0F);
    EXPECT_EQ(fixture.scene.units()[1].life_state, LifeVisualState::Dead);
}

} // namespace d2engine
