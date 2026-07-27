#include <gtest/gtest.h>

#include "d2engine/battle_view/battle_animation_scripts.hpp"
#include "d2engine/battle_view/animation_clip_store.hpp"
#include "d2engine/battle_view/battle_scene.hpp"
#include "d2engine/battle_view/command_timeline.hpp"

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

BattleScene scene_with_unit() {
    BattleScene scene;
    BattleUnit  unit;
    VisualTrack base;
    base.kind = TrackKind::Base;
    base.player.load(sequence("idle", true));
    base.player.play();
    unit.tracks.push_back(std::move(base));
    scene.add_unit(std::move(unit));
    return scene;
}

CommandNode command(VisualCommand value) {
    return CommandNode::command(std::move(value));
}

} // namespace

TEST(CommandTimeline, SequenceUsesUnusedDelta) {
    BattleScene     scene = scene_with_unit();
    const auto      id = scene.units().at(0).id;
    CommandTimeline timeline;
    timeline.enqueue(CommandNode::sequence(
        {command(WaitTime{.duration_ms = 100.0f}),
         command(SetAlpha{.entity_id = id, .track = TrackKind::Base, .alpha = 0.0f})}));

    timeline.update(scene, 50.0f);
    EXPECT_TRUE(timeline.busy());
    EXPECT_FLOAT_EQ(scene.find_track(id, TrackKind::Base)->alpha, 1.0f);

    timeline.update(scene, 100.0f);
    EXPECT_FALSE(timeline.busy());
    EXPECT_FLOAT_EQ(scene.find_track(id, TrackKind::Base)->alpha, 0.0f);
}

TEST(AnimationClipStore, ResolvesValidDefaultAndUnknownHandles) {
    AnimationClipStore store;

    EXPECT_EQ(AnimationClipHandle{}.value, 0u);
    EXPECT_FALSE(AnimationClipHandle{}.valid());
    EXPECT_EQ(store.get(AnimationClipHandle{}), nullptr);
    EXPECT_EQ(store.get(AnimationClipHandle{99u}), nullptr);

    const AnimationClipHandle handle = store.add(sequence("stored"));

    ASSERT_TRUE(handle.valid());
    ASSERT_NE(store.get(handle), nullptr);
    EXPECT_EQ(store.get(handle)->name, "stored");
}

TEST(CommandTimeline, PlayClipCanResolveHandlePayload) {
    AnimationClipStore        store;
    const AnimationClipHandle handle = store.add(sequence("stored_hit"));
    BattleScene               scene = scene_with_unit();
    const auto                id = scene.units().at(0).id;
    CommandTimeline           timeline(&store);

    timeline.enqueue(command(PlayClip{.entity_id = id, .track = TrackKind::Base, .clip = handle}));
    timeline.update(scene, 0.0f);

    const VisualTrack* track = scene.find_track(id, TrackKind::Base);
    ASSERT_NE(track, nullptr);
    EXPECT_EQ(track->player.sequence().name, "stored_hit");
    EXPECT_FALSE(timeline.busy());
}

TEST(CommandTimeline, MissingClipHandleIsNoOp) {
    BattleScene     scene = scene_with_unit();
    const auto      id = scene.units().at(0).id;
    CommandTimeline timeline;

    timeline.enqueue(command(
        PlayClip{.entity_id = id, .track = TrackKind::Base, .clip = AnimationClipHandle{7u}}));
    timeline.update(scene, 0.0f);

    const VisualTrack* track = scene.find_track(id, TrackKind::Base);
    ASSERT_NE(track, nullptr);
    EXPECT_EQ(track->player.sequence().name, "idle");
    EXPECT_FALSE(timeline.busy());
}

TEST(CommandTimeline, ParallelCompletesAfterLongestChild) {
    BattleScene     scene = scene_with_unit();
    CommandTimeline timeline;
    timeline.enqueue(CommandNode::parallel(
        {command(WaitTime{.duration_ms = 100.0f}), command(WaitTime{.duration_ms = 200.0f})}));

    timeline.update(scene, 100.0f);
    EXPECT_TRUE(timeline.busy());
    timeline.update(scene, 100.0f);
    EXPECT_FALSE(timeline.busy());
}

TEST(CommandTimeline, TweenAlphaClampsAndCompletes) {
    BattleScene     scene = scene_with_unit();
    const auto      id = scene.units().at(0).id;
    CommandTimeline timeline;
    timeline.enqueue(command(TweenAlpha{.entity_id = id,
                                        .track = TrackKind::Base,
                                        .from = 1.0f,
                                        .to = -1.0f,
                                        .duration_ms = 100.0f}));

    timeline.update(scene, 50.0f);
    EXPECT_FLOAT_EQ(scene.find_track(id, TrackKind::Base)->alpha, 0.0f);
    timeline.update(scene, 50.0f);
    EXPECT_FALSE(timeline.busy());
    EXPECT_FLOAT_EQ(scene.find_track(id, TrackKind::Base)->alpha, 0.0f);
}

TEST(CommandTimeline, WaitTrackCompleteAndMissingTarget) {
    BattleScene     scene = scene_with_unit();
    const auto      id = scene.units().at(0).id;
    CommandTimeline timeline;
    timeline.enqueue(CommandNode::sequence(
        {command(PlayClip{
             .entity_id = id, .track = TrackKind::Base, .clip = sequence("hit"), .looping = false}),
         command(WaitTrackComplete{.entity_id = id, .track = TrackKind::Base}),
         command(SetLifeVisualState{.entity_id = id, .state = LifeVisualState::Dead})}));

    timeline.update(scene, 0.0f);
    EXPECT_TRUE(timeline.busy());
    scene.update(100.0f);
    timeline.update(scene, 0.0f);
    EXPECT_FALSE(timeline.busy());
    EXPECT_EQ(scene.try_unit_by_id(id)->life_state, LifeVisualState::Dead);

    timeline.enqueue(command(
        SetAlpha{.entity_id = VisualEntityId{999u}, .track = TrackKind::Base, .alpha = 0.5f}));
    EXPECT_NO_THROW(timeline.update(scene, 0.0f));
    EXPECT_FALSE(timeline.busy());
}

TEST(CommandTimeline, VisibilityTransformAndEffects) {
    BattleScene scene = scene_with_unit();
    const auto  id = scene.units().at(0).id;
    VisualTrack effect;
    effect.kind = TrackKind::Effect;
    effect.effect_id = EffectInstanceId{4u};
    effect.player.load(sequence("effect"));

    CommandTimeline timeline;
    timeline.enqueue(CommandNode::sequence(
        {command(SetTrackVisibility{.entity_id = id,
                                    .track = TrackKind::Base,
                                    .visibility = TrackVisibility::HiddenButPlaying}),
         command(SetTransform{.entity_id = id,
                              .track = TrackKind::Base,
                              .transform = {.position_offset = {.x = 2.0f, .y = 3.0f},
                                            .scale_x = 1.5f,
                                            .scale_y = 0.5f}}),
         command(SpawnEffect{.entity_id = id, .track = std::move(effect)}),
         command(DespawnEffect{.effect_id = EffectInstanceId{4u}})}));
    timeline.update(scene, 0.0f);

    EXPECT_FALSE(timeline.busy());
    const auto* base = scene.find_track(id, TrackKind::Base);
    ASSERT_NE(base, nullptr);
    EXPECT_EQ(base->visibility, TrackVisibility::HiddenButPlaying);
    EXPECT_FLOAT_EQ(base->transform.scale_x, 1.5f);
    EXPECT_EQ(scene.find_effect(EffectInstanceId{4u}), nullptr);
    ASSERT_EQ(scene.units()[0].tracks.size(), 2u);
    EXPECT_EQ(scene.units()[0].tracks[1].lifecycle, TrackLifecycle::PendingRemoval);
}

TEST(CommandTimeline, IdenticalInputIsDeterministic) {
    BattleScene     first = scene_with_unit();
    BattleScene     second = scene_with_unit();
    const auto      first_id = first.units().at(0).id;
    const auto      second_id = second.units().at(0).id;
    CommandTimeline first_timeline;
    CommandTimeline second_timeline;
    first_timeline.enqueue(command(TweenAlpha{.entity_id = first_id,
                                              .track = TrackKind::Base,
                                              .from = 1.0f,
                                              .to = 0.0f,
                                              .duration_ms = 100.0f}));
    second_timeline.enqueue(command(TweenAlpha{.entity_id = second_id,
                                               .track = TrackKind::Base,
                                               .from = 1.0f,
                                               .to = 0.0f,
                                               .duration_ms = 100.0f}));

    first_timeline.update(first, 37.0f);
    second_timeline.update(second, 37.0f);
    EXPECT_FLOAT_EQ(first.find_track(first_id, TrackKind::Base)->alpha,
                    second.find_track(second_id, TrackKind::Base)->alpha);
    EXPECT_EQ(first_timeline.busy(), second_timeline.busy());
}

TEST(CommandTimeline, WaitTrackCompleteTargetsTrackIdNotKind) {
    BattleScene scene = scene_with_unit();
    const auto  id = scene.units().at(0).id;

    VisualTrack first;
    first.id = TrackId{101u};
    first.kind = TrackKind::Effect;
    first.effect_id = EffectInstanceId{101u};
    first.player.load(sequence("first"));
    first.player.play();
    VisualTrack second;
    second.id = TrackId{102u};
    second.kind = TrackKind::Effect;
    second.effect_id = EffectInstanceId{102u};
    second.player.load(sequence("second", true));
    second.player.play();
    ASSERT_TRUE(scene.add_track(id, std::move(first)));
    ASSERT_TRUE(scene.add_track(id, std::move(second)));

    CommandTimeline timeline;
    timeline.enqueue(CommandNode::sequence(
        {command(WaitTrackComplete{.entity_id = id, .track = TrackSelector::by_id(TrackId{102u})}),
         command(SetLifeVisualState{.entity_id = id, .state = LifeVisualState::Dead})}));

    scene.update(100.0f);
    timeline.update(scene, 0.0f);
    EXPECT_TRUE(timeline.busy());
    EXPECT_EQ(scene.try_unit_by_id(id)->life_state, LifeVisualState::Alive);
}

TEST(CommandTimeline, LoweredTimedTrackUsesExistingCommands) {
    BattleScene scene = scene_with_unit();
    const auto  id = scene.units().at(0).id;

    TimedTrackSpec spec;
    spec.entity_id = id;
    spec.effect_id = EffectInstanceId{55u};
    spec.sequence = sequence("effect");
    spec.lifetime = EffectLifetime::Duration;
    spec.duration_ms = 50.0f;

    CommandTimeline timeline;
    timeline.enqueue(BattleAnimationScripts::build_timed_track(std::move(spec)));

    timeline.update(scene, 0.0f);
    ASSERT_NE(scene.find_effect(EffectInstanceId{55u}), nullptr);
    EXPECT_TRUE(timeline.busy());

    timeline.update(scene, 50.0f);
    EXPECT_FALSE(timeline.busy());
    ASSERT_EQ(scene.units()[0].tracks.size(), 2u);
    EXPECT_EQ(scene.units()[0].tracks[1].lifecycle, TrackLifecycle::PendingRemoval);
}

TEST(CommandTimeline, EmitsBattleEvents) {
    BattleScene     scene = scene_with_unit();
    CommandTimeline timeline;
    timeline.enqueue(command(EmitBattleEvent{
        .event = AttackImpactCue{.attack_id = AttackInstanceId{9u},
                                 .source = UnitInstanceId{1u},
                                 .targets = TargetSet::single(UnitInstanceId{2u})}}));

    timeline.update(scene, 0.0F);

    auto emitted = timeline.drain_emitted_events();
    ASSERT_EQ(emitted.size(), 1u);
    ASSERT_TRUE(std::holds_alternative<AttackImpactCue>(emitted.front()));
    EXPECT_TRUE(timeline.drain_emitted_events().empty());
}

} // namespace d2engine
