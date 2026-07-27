#include <gtest/gtest.h>

#include <cstddef>

#include "d2engine/battle_view/layered_animation_player.hpp"

namespace d2engine {
namespace {

AnimationSequence make_seq(std::size_t n, std::uint16_t dur_ms = 40) {
    AnimationSequence seq;
    seq.container_path = "Test.ff";
    seq.name = "test";
    for (std::size_t i = 0; i < n; ++i) {
        seq.frames.push_back(
            {.image_name = "f" + std::to_string(i), .index = 0, .duration_ms = dur_ms});
    }
    return seq;
}

// Build a sequence with per-frame durations from an initializer list.
AnimationSequence make_nonuniform_seq(std::initializer_list<std::uint16_t> durations) {
    AnimationSequence seq;
    seq.container_path = "Test.ff";
    seq.name = "nonuniform";
    std::size_t i = 0;
    for (auto d : durations) {
        seq.frames.push_back(
            {.image_name = "f" + std::to_string(i++), .index = 0, .duration_ms = d});
    }
    return seq;
}

} // namespace

TEST(SampleFrame, A1DrivesEqualLengths) {
    LayeredAnimationClip clip;
    clip.a1 = make_seq(16, 40);
    clip.s1 = make_seq(16, 40);
    clip.a2 = make_seq(16, 40);
    clip.loop_policy = true;
    LayeredAnimationPlayer const player{.clip = &clip, .elapsed_ms = 8u * 40u};
    EXPECT_EQ(sample_frame(player, LayerSlot::A1), 8u);
    EXPECT_EQ(sample_frame(player, LayerSlot::S1), 8u);
    EXPECT_EQ(sample_frame(player, LayerSlot::A2), 8u);
}

TEST(SampleFrame, S1ShorterThanA1ProportionalMapping) {
    LayeredAnimationClip clip;
    clip.a1 = make_seq(16, 40);
    clip.s1 = make_seq(8, 40);
    clip.loop_policy = true;
    LayeredAnimationPlayer const player{.clip = &clip, .elapsed_ms = 12u * 40u};
    // phase=12/16=0.75; S1 own_phase=0.75*320=240ms → S1 frame 6
    EXPECT_EQ(sample_frame(player, LayerSlot::S1), 6u);
}

TEST(SampleFrame, S1LongerThanA1Watchman) {
    // Watchman HMOV: A1=43, S1=86 uniform 40ms each.
    // elapsed=841ms (1ms into frame 21 of A1) avoids the exact frame-boundary.
    // p = 841/1720; S1 phase ≈ 1682ms → S1 frame 42.
    LayeredAnimationClip clip;
    clip.a1 = make_seq(43, 40);
    clip.s1 = make_seq(86, 40);
    LayeredAnimationPlayer const player{.clip = &clip, .elapsed_ms = 841u};
    EXPECT_EQ(sample_frame(player, LayerSlot::S1), 42u);
}

TEST(SampleFrame, NonLoopingS1LongerThanA1ClampedPhase) {
    // Non-looping: elapsed far past driver exhaustion → phase clamps just below 1.0 → last S1
    // frame.
    LayeredAnimationClip clip;
    clip.a1 = make_seq(43, 40);
    clip.s1 = make_seq(86, 40);
    LayeredAnimationPlayer const player{.clip = &clip, .elapsed_ms = 100u * 40u, .looping = false};
    EXPECT_EQ(sample_frame(player, LayerSlot::S1), 85u);
}

// Regression: A1=43, S1=86, looping=true: S1 can reach frame 85 within one driver cycle.
TEST(SampleFrame, S1LongerThanA1CanReachLastFrameLooping) {
    LayeredAnimationClip clip;
    clip.a1 = make_seq(43, 40);
    clip.s1 = make_seq(86, 40);
    // 1ms before the driver cycle wraps → phase near 1.0 → S1 near frame 85.
    const uint32_t               driver_total = 43u * 40u; // 1720ms
    LayeredAnimationPlayer const player{
        .clip = &clip, .elapsed_ms = driver_total - 1u, .looping = true};
    EXPECT_EQ(sample_frame(player, LayerSlot::S1), 85u);
}

// Regression: non-uniform frame durations must sample by cumulative time, not first-frame duration.
// A1 = [100ms, 200ms], S1 = [50ms, 250ms]. At elapsed=99ms:
//   - old (first-frame=100ms): elapsed_frames=0, p=0, S1 frame 0. WRONG.
//   - new (time-based): p=99/300=0.33, S1 phase=99ms > 50ms → S1 frame 1. CORRECT.
TEST(SampleFrame, NonUniformFrameDurationsUsesTimeNotFirstFrameDuration) {
    LayeredAnimationClip clip;
    clip.a1 = make_nonuniform_seq({100u, 200u}); // total 300ms
    clip.s1 = make_nonuniform_seq({50u, 250u});  // total 300ms
    LayeredAnimationPlayer player{.clip = &clip, .elapsed_ms = 99u};
    // 99ms is past S1 frame 0 (50ms) → should be frame 1
    EXPECT_EQ(sample_frame(player, LayerSlot::S1), 1u);
    // 49ms is inside S1 frame 0 → should be frame 0
    player.elapsed_ms = 49u;
    EXPECT_EQ(sample_frame(player, LayerSlot::S1), 0u);
}

TEST(SampleFrame, NullLayerReturns0) {
    LayeredAnimationClip clip;
    clip.a1 = make_seq(16, 40);
    clip.s1 = make_seq(16, 40);
    // no a2
    clip.loop_policy = true;
    LayeredAnimationPlayer const player{.clip = &clip, .elapsed_ms = 8u * 40u};
    EXPECT_EQ(sample_frame(player, LayerSlot::A2), 0u);
}

TEST(SampleFrame, A1AbsentS1IsDriver) {
    LayeredAnimationClip clip;
    clip.s1 = make_seq(10, 40);
    clip.loop_policy = true;
    LayeredAnimationPlayer const player{.clip = &clip, .elapsed_ms = 5u * 40u};
    EXPECT_EQ(sample_frame(player, LayerSlot::S1), 5u);
}

TEST(SampleFrame, FirstFrameAtElapsedZero) {
    LayeredAnimationClip clip;
    clip.a1 = make_seq(16, 40);
    clip.s1 = make_seq(8, 40);
    clip.loop_policy = true;
    LayeredAnimationPlayer const player{.clip = &clip, .elapsed_ms = 0u};
    EXPECT_EQ(sample_frame(player, LayerSlot::A1), 0u);
    EXPECT_EQ(sample_frame(player, LayerSlot::S1), 0u);
}

TEST(SampleFrame, NullClipReturns0) {
    LayeredAnimationPlayer const player;
    EXPECT_EQ(sample_frame(player, LayerSlot::A1), 0u);
}

// Runtime looping=true overrides clip.loop_policy=false
TEST(SampleFrame, RuntimeLoopingOverridesClipPolicy) {
    LayeredAnimationClip clip;
    clip.a1 = make_seq(10, 40);
    clip.s1 = make_seq(10, 40);
    clip.loop_policy = false;
    // elapsed = 12 frames → looping: phase = fmod(480, 400)=80ms → frame 2
    LayeredAnimationPlayer const player{.clip = &clip, .elapsed_ms = 12u * 40u, .looping = true};
    EXPECT_EQ(sample_frame(player, LayerSlot::A1), 2u);
    EXPECT_EQ(sample_frame(player, LayerSlot::S1), 2u);
}

TEST(SampleFrame, NonLoopingClampedWhenRuntimeLoopingFalse) {
    LayeredAnimationClip clip;
    clip.a1 = make_seq(10, 40);
    clip.loop_policy = false;
    LayeredAnimationPlayer const player{.clip = &clip, .elapsed_ms = 15u * 40u, .looping = false};
    // phase clamps just below driver_total → last frame (9)
    EXPECT_EQ(sample_frame(player, LayerSlot::A1), 9u);
}

TEST(SampleFrame, ReversePlaybackMirrorsPhase) {
    LayeredAnimationClip clip;
    clip.a1 = make_seq(10, 40); // total 400ms
    // At elapsed=80ms forward: p=0.2 → frame 2
    // At elapsed=80ms reverse: p=1-0.2=0.8 → own_phase=320ms → frame 8
    LayeredAnimationPlayer const fwd{.clip = &clip, .elapsed_ms = 80u, .looping = true};
    LayeredAnimationPlayer const rev{
        .clip = &clip, .elapsed_ms = 80u, .looping = true, .reverse_playback = true};
    EXPECT_EQ(sample_frame(fwd, LayerSlot::A1), 2u);
    EXPECT_EQ(sample_frame(rev, LayerSlot::A1), 8u);
}

// Necromancer: A1=21, A2=15 — no crash, within range
TEST(SampleFrame, NecromancerHmovA1_21_A2_15) {
    LayeredAnimationClip clip;
    clip.a1 = make_seq(21, 40);
    clip.a2 = make_seq(15, 40);
    clip.loop_policy = true;
    for (std::size_t t = 0; t < static_cast<std::size_t>(21u * 40u); t += 40u) {
        LayeredAnimationPlayer const player{
            .clip = &clip, .elapsed_ms = static_cast<uint32_t>(t), .looping = true};
        const std::size_t f = sample_frame(player, LayerSlot::A2);
        EXPECT_LT(f, 15u) << "frame out of range at elapsed=" << t;
    }
}

// Spiritess STIL: A1=9, S1=1 — S1 always at frame 0 regardless of A1 progress
TEST(SampleFrame, SpiritessStilA1_9_S1_1) {
    LayeredAnimationClip clip;
    clip.a1 = make_seq(9, 40);
    clip.s1 = make_seq(1, 40);
    clip.loop_policy = true;
    for (uint32_t t = 0; t < 9u * 40u; t += 40u) {
        LayeredAnimationPlayer const player{.clip = &clip, .elapsed_ms = t, .looping = true};
        EXPECT_EQ(sample_frame(player, LayerSlot::S1), 0u);
    }
}

} // namespace d2engine
