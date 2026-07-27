#include <gtest/gtest.h>

#include "d2engine/animation/animation_sequence.hpp"
#include "d2engine/battle_view/battle_ids.hpp"
#include "d2engine/battle_view/battle_scene.hpp"
#include "d2engine/battle_view/layered_animation_clip.hpp"

namespace d2engine {

static_assert(VisualEntityId{7u}.value == 7u);
static_assert(VisualEntityId{} != VisualEntityId{1u});

namespace {

AnimationSequence make_looping_sequence(int frame_count = 3) {
    AnimationSequence seq;
    seq.name = "idle";
    seq.container_path = "Imgs/Test.ff";
    seq.is_looping = true;

    for (int i = 0; i < frame_count; ++i) {
        AnimationFrame frame;
        frame.image_name = "frame_" + std::to_string(i);
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

AnimationSequence make_oneshot_sequence(const std::string& name = "hit") {
    AnimationSequence seq;
    seq.name = name;
    seq.container_path = "Imgs/Test.ff";
    seq.is_looping = false;
    AnimationFrame frame;
    frame.image_name = "frame_0";
    frame.index = 0;
    frame.duration_ms = 100;
    seq.frames.push_back(frame);
    return seq;
}

BattleUnit make_unit(BattleSide side, int lane, BattleDepth depth) {
    BattleUnit unit;
    unit.coord = BattleSlotCoord{.side = side, .lane = lane, .depth = depth};
    unit.direction = (side == BattleSide::Defender) ? 'D' : 'A';
    VisualTrack base;
    base.kind = TrackKind::Base;
    base.layer = TrackRenderLayer::Base;
    base.anchor = AnchorPolicy::UnitCanvasFoot;
    base.player.load(make_looping_sequence());
    base.player.play();
    unit.tracks.push_back(std::move(base));
    return unit;
}

} // namespace

TEST(BattleScene, EmptyByDefault) {
    const BattleScene scene;
    EXPECT_TRUE(scene.units().empty());
}

TEST(BattleScene, AddUnit) {
    BattleScene scene;
    scene.add_unit(make_unit(BattleSide::Attacker, 0, BattleDepth::Front));
    EXPECT_EQ(scene.units().size(), 1u);
}

TEST(BattleScene, UpdateEmptySceneDoesNotCrash) {
    BattleScene scene;
    EXPECT_NO_THROW(scene.update(16.0f));
}

TEST(BattleScene, UpdateAdvancesAllPlayers) {
    BattleScene scene;
    scene.add_unit(make_unit(BattleSide::Attacker, 0, BattleDepth::Front));
    scene.add_unit(make_unit(BattleSide::Attacker, 1, BattleDepth::Front));
    scene.add_unit(make_unit(BattleSide::Defender, 0, BattleDepth::Front));

    // All players start at frame 0
    for (const auto& unit : scene.units()) {
        EXPECT_EQ(base_player(unit).current_frame_index(), 0u);
    }

    // After 150ms each player should be on frame 1 (100ms per frame, looping)
    scene.update(150.0f);

    for (const auto& unit : scene.units()) {
        EXPECT_EQ(base_player(unit).current_frame_index(), 1u);
    }
}

TEST(BattleScene, AttackerUnitsHaveDirectionA) {
    BattleScene scene;
    scene.add_unit(make_unit(BattleSide::Attacker, 0, BattleDepth::Front));
    scene.add_unit(make_unit(BattleSide::Attacker, 1, BattleDepth::Back));
    for (const auto& unit : scene.units()) {
        EXPECT_EQ(unit.direction, 'A');
    }
}

TEST(BattleScene, DefenderUnitsHaveDirectionD) {
    BattleScene scene;
    scene.add_unit(make_unit(BattleSide::Defender, 0, BattleDepth::Front));
    scene.add_unit(make_unit(BattleSide::Defender, 2, BattleDepth::Back));
    for (const auto& unit : scene.units()) {
        EXPECT_EQ(unit.direction, 'D');
    }
}

TEST(BattleScene, ReplaceAnimationStartsPlayback) {
    BattleScene scene;
    scene.add_unit(make_unit(BattleSide::Attacker, 0, BattleDepth::Front));
    ASSERT_TRUE(scene.replace_track_clip(scene.units()[0].id, TrackKind::Base,
                                         make_oneshot_sequence("hit"), false));
    EXPECT_EQ(base_player(scene.units()[0]).state(), AnimationPlayerState::Playing);
    EXPECT_EQ(base_player(scene.units()[0]).sequence().name, "hit");
}

TEST(BattleScene, ReplaceAnimationNonLoopingCompletesCorrectly) {
    BattleScene scene;
    scene.add_unit(make_unit(BattleSide::Attacker, 0, BattleDepth::Front));
    ASSERT_TRUE(scene.replace_track_clip(scene.units()[0].id, TrackKind::Base,
                                         make_oneshot_sequence("hit"), false));
    // Advance past the single 100ms frame → state becomes Completed
    scene.update(100.0f);
    EXPECT_EQ(base_player(scene.units()[0]).state(), AnimationPlayerState::Completed);
}

TEST(BattleScene, SnapshotCopiesRenderPlacement) {
    BattleScene scene;
    scene.add_unit(make_unit(BattleSide::Attacker, 0, BattleDepth::Front));

    const BattleRenderSnapshot snapshot = scene.snapshot();

    ASSERT_EQ(snapshot.entities.size(), 1u);
    ASSERT_EQ(snapshot.entities[0].tracks.size(), 1u);
    const SnapshotTrack& track = snapshot.entities[0].tracks[0];
    EXPECT_EQ(track.placement.pass, TrackRenderLayer::Base);
    EXPECT_EQ(track.placement.depth_mode, DepthMode::AnchorY);
    EXPECT_EQ(track.placement.position_anchor, AnchorPolicy::UnitCanvasFoot);
    EXPECT_EQ(track.placement.depth_anchor, AnchorPolicy::UnitCanvasFoot);
    EXPECT_EQ(track.placement.depth_bias, 0);
}

TEST(BattleScene, SetTrackAlphaClampsToRange) {
    BattleScene scene;
    scene.add_unit(make_unit(BattleSide::Attacker, 0, BattleDepth::Front));
    const auto id = scene.units()[0].id;

    ASSERT_TRUE(scene.set_track_alpha(id, TrackKind::Base, 0.5f));
    EXPECT_FLOAT_EQ(scene.units()[0].tracks[0].alpha, 0.5f);

    ASSERT_TRUE(scene.set_track_alpha(id, TrackKind::Base, -0.5f));
    EXPECT_FLOAT_EQ(scene.units()[0].tracks[0].alpha, 0.0f);

    ASSERT_TRUE(scene.set_track_alpha(id, TrackKind::Base, 1.5f));
    EXPECT_FLOAT_EQ(scene.units()[0].tracks[0].alpha, 1.0f);
}

TEST(BattleScene, DefaultAlphaIsOne) {
    BattleScene scene;
    scene.add_unit(make_unit(BattleSide::Attacker, 0, BattleDepth::Front));
    EXPECT_FLOAT_EQ(scene.units()[0].alpha, 1.0f);
}

TEST(BattleScene, AddUnit_AssignsUniqueId) {
    BattleScene scene;
    scene.add_unit(make_unit(BattleSide::Attacker, 0, BattleDepth::Front));
    scene.add_unit(make_unit(BattleSide::Attacker, 1, BattleDepth::Front));
    scene.add_unit(make_unit(BattleSide::Defender, 0, BattleDepth::Front));

    EXPECT_EQ(scene.units()[0].id.value, 1u);
    EXPECT_EQ(scene.units()[1].id.value, 2u);
    EXPECT_EQ(scene.units()[2].id.value, 3u);
}

TEST(BattleScene, FindUnit_ReturnsCorrectIndex) {
    BattleScene scene;
    scene.add_unit(make_unit(BattleSide::Attacker, 0, BattleDepth::Front));
    scene.add_unit(make_unit(BattleSide::Defender, 0, BattleDepth::Front));

    const auto idx0 = scene.find_unit(scene.units()[0].id);
    ASSERT_TRUE(idx0.has_value());
    EXPECT_EQ(*idx0, 0u);

    const auto idx1 = scene.find_unit(scene.units()[1].id);
    ASSERT_TRUE(idx1.has_value());
    EXPECT_EQ(*idx1, 1u);
}

TEST(BattleScene, FindUnit_ReturnsNulloptForUnknown) {
    BattleScene scene;
    scene.add_unit(make_unit(BattleSide::Attacker, 0, BattleDepth::Front));

    const auto idx = scene.find_unit(VisualEntityId{999u});
    EXPECT_FALSE(idx.has_value());
}

TEST(BattleScene, UnitById_ReturnsReference) {
    BattleScene scene;
    scene.add_unit(make_unit(BattleSide::Attacker, 0, BattleDepth::Front));

    const auto id = scene.units()[0].id;
    auto*      unit = scene.try_unit_by_id(id);
    ASSERT_NE(unit, nullptr);
    EXPECT_EQ(unit->coord.lane, 0);
}

TEST(BattleScene, TryUnitById_ReturnsNullptrForUnknown) {
    BattleScene scene;
    scene.add_unit(make_unit(BattleSide::Attacker, 0, BattleDepth::Front));

    auto* unit = scene.try_unit_by_id(VisualEntityId{999u});
    EXPECT_EQ(unit, nullptr);
}

TEST(BattleScene, Clear_ResetsIds) {
    BattleScene scene;
    scene.add_unit(make_unit(BattleSide::Attacker, 0, BattleDepth::Front));
    scene.clear();
    scene.add_unit(make_unit(BattleSide::Attacker, 0, BattleDepth::Front));

    EXPECT_EQ(scene.units()[0].id.value, 1u);
}

TEST(BattleScene, AddTrack) {
    BattleScene scene;
    scene.add_unit(make_unit(BattleSide::Attacker, 0, BattleDepth::Front));

    ASSERT_TRUE(scene.add_track(scene.units()[0].id, VisualTrack{.kind = TrackKind::DeathFx}));
    EXPECT_EQ(scene.units()[0].tracks.size(), 2u);
    EXPECT_EQ(scene.units()[0].tracks[1].kind, TrackKind::DeathFx);
}

TEST(BattleScene, RemoveTrackByKind) {
    BattleScene scene;
    scene.add_unit(make_unit(BattleSide::Attacker, 0, BattleDepth::Front));
    const auto id = scene.units()[0].id;
    ASSERT_TRUE(scene.add_track(id, VisualTrack{.kind = TrackKind::DeathFx}));

    ASSERT_TRUE(scene.remove_track(id, TrackKind::DeathFx));
    scene.update(0.0f); // triggers removal
    EXPECT_EQ(scene.units()[0].tracks.size(), 1u);
    EXPECT_EQ(scene.units()[0].tracks[0].kind, TrackKind::Base);
}

TEST(BattleScene, SetTrackVisibility) {
    BattleScene scene;
    scene.add_unit(make_unit(BattleSide::Attacker, 0, BattleDepth::Front));

    ASSERT_TRUE(scene.set_track_visibility(scene.units()[0].id, TrackKind::Base,
                                           TrackVisibility::HiddenButPlaying));
    EXPECT_EQ(scene.units()[0].tracks[0].visibility, TrackVisibility::HiddenButPlaying);
}

TEST(BattleScene, SetTrackAlpha) {
    BattleScene scene;
    scene.add_unit(make_unit(BattleSide::Attacker, 0, BattleDepth::Front));

    ASSERT_TRUE(scene.set_track_alpha(scene.units()[0].id, TrackKind::Base, 0.5f));
    EXPECT_FLOAT_EQ(scene.units()[0].tracks[0].alpha, 0.5f);
}

TEST(BattleScene, SetTrackAlphaClamps) {
    BattleScene scene;
    scene.add_unit(make_unit(BattleSide::Attacker, 0, BattleDepth::Front));
    const auto id = scene.units()[0].id;

    ASSERT_TRUE(scene.set_track_alpha(id, TrackKind::Base, -0.5f));
    EXPECT_FLOAT_EQ(scene.units()[0].tracks[0].alpha, 0.0f);

    ASSERT_TRUE(scene.set_track_alpha(id, TrackKind::Base, 2.0f));
    EXPECT_FLOAT_EQ(scene.units()[0].tracks[0].alpha, 1.0f);
}

TEST(BattleScene, ReplaceTrackClip) {
    BattleScene scene;
    scene.add_unit(make_unit(BattleSide::Attacker, 0, BattleDepth::Front));

    AnimationSequence seq;
    seq.name = "test";
    seq.is_looping = false;
    AnimationFrame frame;
    frame.duration_ms = 100;
    seq.frames.push_back(frame);

    ASSERT_TRUE(scene.replace_track_clip(scene.units()[0].id, TrackKind::Base, seq, false));
    EXPECT_EQ(scene.units()[0].tracks.size(), 1u);
    EXPECT_EQ(scene.units()[0].tracks[0].kind, TrackKind::Base);
    EXPECT_EQ(scene.units()[0].tracks[0].player.sequence().name, "test");
}

TEST(BattleScene, UpdateAdvancesTracks) {
    BattleScene scene;
    scene.add_unit(make_unit(BattleSide::Attacker, 0, BattleDepth::Front));

    AnimationSequence seq;
    seq.is_looping = true;
    AnimationFrame frame;
    frame.duration_ms = 100;
    seq.frames.push_back(frame);
    seq.frames.push_back(frame);

    VisualTrack track;
    track.kind = TrackKind::Base;
    track.player.load(seq);
    track.player.play();
    ASSERT_TRUE(scene.add_track(scene.units()[0].id, track));

    scene.update(150.0f);
    EXPECT_EQ(scene.units()[0].tracks[0].player.current_frame_index(), 1u);
}

TEST(BattleScene, UpdateSkipsDisabledTracks) {
    BattleScene scene;
    scene.add_unit(make_unit(BattleSide::Attacker, 0, BattleDepth::Front));

    AnimationSequence seq;
    seq.is_looping = true;
    AnimationFrame frame;
    frame.duration_ms = 100;
    seq.frames.push_back(frame);
    seq.frames.push_back(frame);

    VisualTrack track;
    track.kind = TrackKind::DeathFx;
    track.visibility = TrackVisibility::Disabled;
    track.player.load(seq);
    track.player.play();
    ASSERT_TRUE(scene.add_track(scene.units()[0].id, track));

    scene.update(150.0f);
    EXPECT_EQ(scene.units()[0].tracks[1].player.current_frame_index(), 0u);
}

TEST(BattleScene, LifeVisualStateDefault) {
    BattleScene scene;
    scene.add_unit(make_unit(BattleSide::Attacker, 0, BattleDepth::Front));
    EXPECT_EQ(scene.units()[0].life_state, LifeVisualState::Alive);
}

TEST(BattleScene, SnapshotCopiesCurrentFrameWithoutSharingState) {
    BattleScene scene;
    scene.add_unit(make_unit(BattleSide::Attacker, 0, BattleDepth::Front));

    const BattleRenderSnapshot snapshot = scene.snapshot();
    ASSERT_EQ(snapshot.entities.size(), 1u);
    ASSERT_EQ(snapshot.entities[0].tracks.size(), 1u);
    EXPECT_EQ(snapshot.entities[0].tracks[0].sequence_name, "idle");
    EXPECT_EQ(snapshot.entities[0].tracks[0].current_frame_name, "frame_0");

    scene.update(150.0f);
    EXPECT_EQ(snapshot.entities[0].tracks[0].current_frame_index, 0u);
    EXPECT_EQ(scene.snapshot().entities[0].tracks[0].current_frame_index, 1u);
}

TEST(BattleScene, SnapshotSkipsPendingRemovalTracks) {
    BattleScene scene;
    scene.add_unit(make_unit(BattleSide::Attacker, 0, BattleDepth::Front));
    VisualTrack pending;
    pending.kind = TrackKind::DeathFx;
    pending.lifecycle = TrackLifecycle::PendingRemoval;
    pending.player.load(make_oneshot_sequence());
    ASSERT_TRUE(scene.add_track(scene.units()[0].id, std::move(pending)));

    const BattleRenderSnapshot snapshot = scene.snapshot();
    ASSERT_EQ(snapshot.entities.size(), 1u);
    ASSERT_EQ(snapshot.entities[0].tracks.size(), 1u);
    EXPECT_EQ(snapshot.entities[0].tracks[0].kind, TrackKind::Base);
}

TEST(BattleScene, IdBasedMutationAndEffectIdentity) {
    BattleScene scene;
    scene.add_unit(make_unit(BattleSide::Attacker, 0, BattleDepth::Front));
    const auto id = scene.units()[0].id;

    EXPECT_TRUE(scene.set_track_alpha(id, TrackKind::Base, 0.25f));
    EXPECT_TRUE(scene.set_track_transform(
        id, TrackKind::Base,
        VisualTransform{.position_offset = {.x = 3.0f, .y = 4.0f}, .scale_x = 2.0f}));
    EXPECT_TRUE(scene.set_life_state(id, LifeVisualState::FadingOut));
    EXPECT_FALSE(scene.set_track_alpha(VisualEntityId{999u}, TrackKind::Base, 0.5f));

    VisualTrack effect;
    effect.kind = TrackKind::Effect;
    effect.effect_id = EffectInstanceId{7u};
    effect.player.load(make_oneshot_sequence());
    EXPECT_TRUE(scene.add_track(id, effect));
    EXPECT_FALSE(scene.add_track(id, std::move(effect)));
    EXPECT_NE(scene.find_effect(EffectInstanceId{7u}), nullptr);
    EXPECT_TRUE(scene.remove_effect(EffectInstanceId{7u}));

    const auto snapshot = scene.snapshot();
    ASSERT_EQ(snapshot.entities.size(), 1u);
    ASSERT_EQ(snapshot.entities[0].tracks.size(), 1u);
    EXPECT_FLOAT_EQ(snapshot.entities[0].tracks[0].alpha, 0.25f);
    EXPECT_FLOAT_EQ(snapshot.entities[0].tracks[0].transform.position_offset.x, 3.0f);
    EXPECT_FLOAT_EQ(snapshot.entities[0].tracks[0].transform.scale_x, 2.0f);
    EXPECT_EQ(snapshot.entities[0].life_state, LifeVisualState::FadingOut);
}

TEST(BattleScene, LogicalUnitLookupAndSnapshotIdentity) {
    BattleScene scene;
    BattleUnit  unit = make_unit(BattleSide::Attacker, 0, BattleDepth::Front);
    unit.unit_instance_id = UnitInstanceId{7u};
    scene.add_unit(std::move(unit));

    EXPECT_EQ(scene.visual_entity_for(UnitInstanceId{7u}), scene.units()[0].id);
    EXPECT_FALSE(scene.visual_entity_for(UnitInstanceId{99u}).has_value());
    EXPECT_EQ(scene.snapshot().entities[0].unit_instance_id, UnitInstanceId{7u});
    EXPECT_NE(scene.snapshot().entities[0].tracks[0].id.value, 0u);
}

TEST(BattleScene, RejectsDuplicateLogicalUnitId) {
    BattleScene scene;
    BattleUnit  first = make_unit(BattleSide::Attacker, 0, BattleDepth::Front);
    first.unit_instance_id = UnitInstanceId{3u};
    scene.add_unit(std::move(first));

    BattleUnit duplicate = make_unit(BattleSide::Defender, 0, BattleDepth::Front);
    duplicate.unit_instance_id = UnitInstanceId{3u};
    EXPECT_THROW(scene.add_unit(std::move(duplicate)), std::runtime_error);
}

TEST(BattleScene, AssignsDeterministicNonzeroLogicalIds) {
    BattleScene first;
    BattleScene second;
    for (int lane = 0; lane < 3; ++lane) {
        first.add_unit(make_unit(BattleSide::Attacker, lane, BattleDepth::Front));
        second.add_unit(make_unit(BattleSide::Attacker, lane, BattleDepth::Front));
    }
    for (std::size_t i = 0; i < first.units().size(); ++i) {
        EXPECT_NE(first.units()[i].unit_instance_id.value, 0u);
        EXPECT_EQ(first.units()[i].unit_instance_id, second.units()[i].unit_instance_id);
    }
}

TEST(BattleScene, AssignsDeterministicNonzeroTrackIds) {
    BattleScene first;
    BattleScene second;
    first.add_unit(make_unit(BattleSide::Attacker, 0, BattleDepth::Front));
    first.add_unit(make_unit(BattleSide::Defender, 0, BattleDepth::Front));
    second.add_unit(make_unit(BattleSide::Attacker, 0, BattleDepth::Front));
    second.add_unit(make_unit(BattleSide::Defender, 0, BattleDepth::Front));

    ASSERT_EQ(first.units().size(), second.units().size());
    for (std::size_t i = 0; i < first.units().size(); ++i) {
        ASSERT_EQ(first.units()[i].tracks.size(), second.units()[i].tracks.size());
        for (std::size_t track = 0; track < first.units()[i].tracks.size(); ++track) {
            EXPECT_NE(first.units()[i].tracks[track].id.value, 0u);
            EXPECT_EQ(first.units()[i].tracks[track].id, second.units()[i].tracks[track].id);
        }
    }
}

TEST(BattleScene, FindsTracksByIdSelector) {
    BattleScene scene;
    scene.add_unit(make_unit(BattleSide::Attacker, 0, BattleDepth::Front));
    const auto id = scene.units()[0].id;
    const auto track_id = scene.units()[0].tracks[0].id;

    EXPECT_EQ(scene.find_track(id, TrackSelector::by_id(track_id)), scene.units()[0].tracks.data());
    EXPECT_EQ(scene.find_track(id, TrackSelector::by_id(TrackId{999u})), nullptr);
}

TEST(BattleScene, AllowsDuplicateEffectKindsWithDistinctEffectIdsAndTrackIds) {
    BattleScene scene;
    scene.add_unit(make_unit(BattleSide::Attacker, 0, BattleDepth::Front));
    const auto id = scene.units()[0].id;

    VisualTrack first;
    first.kind = TrackKind::Effect;
    first.effect_id = EffectInstanceId{11u};
    first.player.load(make_oneshot_sequence("fx1"));
    VisualTrack second;
    second.kind = TrackKind::Effect;
    second.effect_id = EffectInstanceId{12u};
    second.player.load(make_oneshot_sequence("fx2"));

    ASSERT_TRUE(scene.add_track(id, std::move(first)));
    ASSERT_TRUE(scene.add_track(id, std::move(second)));
    ASSERT_EQ(scene.units()[0].tracks.size(), 3u);
    EXPECT_EQ(scene.units()[0].tracks[1].kind, TrackKind::Effect);
    EXPECT_EQ(scene.units()[0].tracks[2].kind, TrackKind::Effect);
    EXPECT_NE(scene.units()[0].tracks[1].id, scene.units()[0].tracks[2].id);
    EXPECT_NE(scene.find_effect(EffectInstanceId{11u}), nullptr);
    EXPECT_NE(scene.find_effect(EffectInstanceId{12u}), nullptr);
}

TEST(BattleScene, RejectsDuplicateActiveEffectId) {
    BattleScene scene;
    scene.add_unit(make_unit(BattleSide::Attacker, 0, BattleDepth::Front));
    const auto id = scene.units()[0].id;

    VisualTrack first;
    first.kind = TrackKind::Effect;
    first.effect_id = EffectInstanceId{11u};
    VisualTrack duplicate;
    duplicate.kind = TrackKind::Effect;
    duplicate.effect_id = EffectInstanceId{11u};

    EXPECT_TRUE(scene.add_track(id, std::move(first)));
    EXPECT_FALSE(scene.add_track(id, std::move(duplicate)));
}

TEST(BattleScene, ReplaceLayeredWithSingleClearsLayeredState) {
    BattleScene scene;
    scene.add_unit(make_unit(BattleSide::Attacker, 0, BattleDepth::Front));
    const auto id = scene.units()[0].id;

    // Put a layered clip on Base
    LayeredAnimationClip layered;
    layered.a1 = make_looping_sequence(8);
    layered.a1->name = "layered_a1";
    layered.s1 = make_looping_sequence(4);
    layered.s1->name = "layered_s1";
    ASSERT_TRUE(scene.replace_track_layered_clip(id, TrackSelector::singleton(TrackKind::Base),
                                                 layered, true));

    // Snapshot should show layered sequences
    {
        const auto  snap = scene.snapshot();
        const auto& ent = snap.entities[0];
        bool        found_layered = false;
        for (const auto& t : ent.tracks) {
            if (t.sequence_name == "layered_a1" || t.sequence_name == "layered_s1") {
                found_layered = true;
            }
        }
        EXPECT_TRUE(found_layered);
    }

    // Replace Base with a plain single sequence
    AnimationSequence single;
    single.name = "single_seq";
    single.container_path = "Test.ff";
    single.is_looping = true;
    AnimationFrame f;
    f.image_name = "single_0";
    f.duration_ms = 100;
    single.frames.push_back(f);
    ASSERT_TRUE(scene.replace_track_clip(id, TrackKind::Base, single, true));

    // Snapshot must show single sequence only; no layered sequences
    {
        const auto  snap = scene.snapshot();
        const auto& ent = snap.entities[0];
        ASSERT_EQ(ent.tracks.size(), 1u);
        EXPECT_EQ(ent.tracks[0].sequence_name, "single_seq");
    }
}

} // namespace d2engine
