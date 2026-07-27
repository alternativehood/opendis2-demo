#include <gtest/gtest.h>

#include "d2engine/animation/animation_player.hpp"

namespace d2engine {

// Helper to create a simple 3-frame non-looping sequence
AnimationSequence make_test_sequence(bool looping = false, std::size_t frame_count = 3) {
    AnimationSequence seq;
    seq.name = "test_anim";
    seq.container_path = "Imgs/Test.ff";
    seq.is_looping = looping;

    for (std::size_t i = 0; i < frame_count; ++i) {
        AnimationFrame frame;
        frame.image_name = "frame_" + std::to_string(i);
        frame.index = i;
        frame.duration_ms = 100;
        seq.frames.push_back(frame);
    }

    return seq;
}

TEST(AnimationPlayer, DefaultConstruction) {
    const AnimationPlayer player;
    EXPECT_EQ(player.state(), AnimationPlayerState::Stopped);
    EXPECT_TRUE(player.sequence().frames.empty());
    EXPECT_THROW(static_cast<void>(player.current_frame()), std::runtime_error);
}

TEST(AnimationPlayer, LoadSequence) {
    const AnimationPlayer player(make_test_sequence());
    EXPECT_EQ(player.state(), AnimationPlayerState::Stopped);
    EXPECT_EQ(player.sequence().frames.size(), 3u);
    EXPECT_EQ(player.current_frame_index(), 0u);
}

TEST(AnimationPlayer, PlayStartsAtFirstFrame) {
    AnimationPlayer player(make_test_sequence());
    player.play();
    EXPECT_EQ(player.state(), AnimationPlayerState::Playing);
    EXPECT_EQ(player.current_frame_index(), 0u);
    EXPECT_EQ(player.current_frame().image_name, "frame_0");
}

TEST(AnimationPlayer, PauseStopsAdvancing) {
    AnimationPlayer player(make_test_sequence());
    player.play();
    player.update(50.0f); // Half a frame
    player.pause();
    EXPECT_EQ(player.state(), AnimationPlayerState::Paused);
    EXPECT_EQ(player.current_frame_index(), 0u);

    player.update(200.0f); // Should not advance while paused
    EXPECT_EQ(player.current_frame_index(), 0u);
}

TEST(AnimationPlayer, StopResetsToFirstFrame) {
    AnimationPlayer player(make_test_sequence());
    player.play();
    player.update(250.0f); // 2.5 frames
    EXPECT_EQ(player.current_frame_index(), 2u);

    player.stop();
    EXPECT_EQ(player.state(), AnimationPlayerState::Stopped);
    EXPECT_EQ(player.current_frame_index(), 0u);
}

TEST(AnimationPlayer, ReversePlaybackStartsFromFinalFrame) {
    AnimationPlayer player(make_test_sequence(false, 3));
    player.set_reverse_playback(true);
    player.restart();

    EXPECT_EQ(player.current_frame_index(), 2u);
    EXPECT_EQ(player.current_frame().image_name, "frame_2");
}

TEST(AnimationPlayer, ReverseNonLoopingCompletesAtFirstFrame) {
    AnimationPlayer player(make_test_sequence(false, 3));
    player.set_reverse_playback(true);
    player.play();

    player.update(100.0f);
    EXPECT_EQ(player.current_frame_index(), 1u);
    player.update(100.0f);
    EXPECT_EQ(player.current_frame_index(), 0u);
    player.update(100.0f);
    EXPECT_EQ(player.current_frame_index(), 0u);
    EXPECT_EQ(player.state(), AnimationPlayerState::Completed);
}

TEST(AnimationPlayer, ReverseLoopingWrapsToFinalFrame) {
    AnimationPlayer player(make_test_sequence(true, 3));
    player.set_reverse_playback(true);
    player.play();

    player.update(300.0f);
    EXPECT_EQ(player.state(), AnimationPlayerState::Playing);
    EXPECT_EQ(player.current_frame_index(), 2u);
}

TEST(AnimationPlayer, ReverseSkipsZeroDurationFrames) {
    AnimationSequence seq;
    seq.name = "reverse_zero";
    seq.is_looping = false;
    seq.frames = {
        {.image_name = "frame_0", .index = 0, .duration_ms = 100},
        {.image_name = "frame_1", .index = 1, .duration_ms = 0},
        {.image_name = "frame_2", .index = 2, .duration_ms = 100},
    };
    AnimationPlayer player(seq);
    player.set_reverse_playback(true);
    player.play();

    player.update(100.0f);
    EXPECT_EQ(player.current_frame_index(), 0u);
    EXPECT_EQ(player.current_frame().image_name, "frame_0");
}

TEST(AnimationPlayer, RestartFromCompleted) {
    AnimationPlayer player(make_test_sequence(false, 2));
    player.play();
    player.update(250.0f); // Exceeds 2 frames
    EXPECT_EQ(player.state(), AnimationPlayerState::Completed);
    EXPECT_TRUE(player.is_done());

    player.play(); // Should restart
    EXPECT_EQ(player.state(), AnimationPlayerState::Playing);
    EXPECT_EQ(player.current_frame_index(), 0u);
    EXPECT_FALSE(player.is_done());
}

TEST(AnimationPlayer, VariableFrameDurationsAdvanceCorrectly) {
    AnimationSequence seq;
    seq.name = "variable_test";
    seq.is_looping = false;
    seq.frames = {
        {.image_name = "frame_0", .index = 0, .duration_ms = 50},
        {.image_name = "frame_1", .index = 1, .duration_ms = 150},
        {.image_name = "frame_2", .index = 2, .duration_ms = 100},
    };
    AnimationPlayer player(seq);
    player.play();
    player.update(200.0f); // 50ms (frame_0) + 150ms (frame_1) → should be at frame_2
    EXPECT_EQ(player.current_frame_index(), 2u);
    EXPECT_EQ(player.current_frame().image_name, "frame_2");
}

TEST(AnimationPlayer, LargeDeltaMsSpansMultipleFrames) {
    AnimationSequence seq;
    seq.name = "large_delta";
    seq.is_looping = true;
    for (std::size_t i = 0; i < 5; ++i) {
        seq.frames.push_back(
            {.image_name = "frame_" + std::to_string(i), .index = i, .duration_ms = 100});
    }
    AnimationPlayer player(seq);
    player.play();
    player.update(600.0f); // 5 frames = 500ms for one loop, remaining 100ms advances to frame_1
    EXPECT_EQ(player.current_frame_index(), 1u);
    EXPECT_EQ(player.state(), AnimationPlayerState::Playing);
}

TEST(AnimationPlayer, ZeroDurationFrameSkipped) {
    AnimationSequence seq;
    seq.name = "zero_duration";
    seq.is_looping = false;
    seq.frames = {
        {.image_name = "frame_0", .index = 0, .duration_ms = 0},
        {.image_name = "frame_1", .index = 1, .duration_ms = 100},
    };
    AnimationPlayer player(seq);
    player.play();
    player.update(100.0f); // Should skip frame_0 (0ms) and advance into frame_1
    EXPECT_EQ(player.current_frame_index(), 1u);
    EXPECT_EQ(player.current_frame().image_name, "frame_1");
}

TEST(AnimationPlayer, NonLoopingAnimationCompletes) {
    AnimationPlayer player(make_test_sequence(false, 3));
    player.play();
    player.update(350.0f); // 3.5 frames (exceeds 3-frame sequence)
    EXPECT_EQ(player.state(), AnimationPlayerState::Completed);
    EXPECT_EQ(player.current_frame_index(), 2u); // Last frame
    EXPECT_TRUE(player.is_done());
}

TEST(AnimationPlayer, LoopingAnimationWraps) {
    AnimationPlayer player(make_test_sequence(true, 3));
    player.play();
    player.update(350.0f); // 3.5 frames
    EXPECT_EQ(player.state(), AnimationPlayerState::Playing);
    EXPECT_EQ(player.current_frame_index(), 0u); // Wrapped back to first
    EXPECT_FALSE(player.is_done());
}

TEST(AnimationPlayer, FrameStepForward) {
    AnimationPlayer player(make_test_sequence());
    player.step_forward();
    EXPECT_EQ(player.current_frame_index(), 1u);
    EXPECT_EQ(player.current_frame().image_name, "frame_1");

    player.step_forward();
    EXPECT_EQ(player.current_frame_index(), 2u);

    // Non-looping: stays on last frame
    player.step_forward();
    EXPECT_EQ(player.current_frame_index(), 2u);
}

TEST(AnimationPlayer, FrameStepBackward) {
    AnimationPlayer player(make_test_sequence());
    player.step_forward();
    player.step_forward();
    EXPECT_EQ(player.current_frame_index(), 2u);

    player.step_backward();
    EXPECT_EQ(player.current_frame_index(), 1u);

    player.step_backward();
    EXPECT_EQ(player.current_frame_index(), 0u);

    // Wraps to last frame
    player.step_backward();
    EXPECT_EQ(player.current_frame_index(), 2u);
}

TEST(AnimationPlayer, DeltaTimeAdvancesFrames) {
    AnimationPlayer player(make_test_sequence());
    player.play();
    EXPECT_EQ(player.current_frame_index(), 0u);

    player.update(50.0f); // Half frame
    EXPECT_EQ(player.current_frame_index(), 0u);

    player.update(60.0f); // 110ms total, exceeds first frame
    EXPECT_EQ(player.current_frame_index(), 1u);

    player.update(100.0f); // Exceeds second frame
    EXPECT_EQ(player.current_frame_index(), 2u);
}

TEST(AnimationPlayer, TimingFallback) {
    const AnimationSequence seq = make_test_sequence();
    // Verify default duration is 100ms
    for (const auto& frame : seq.frames) {
        EXPECT_EQ(frame.duration_ms, 100u);
    }

    const AnimationPlayer player(seq);
    EXPECT_EQ(player.current_frame().duration_ms, 100u);
}

TEST(AnimationPlayer, EmptySequence) {
    AnimationSequence seq;
    seq.name = "empty";
    AnimationPlayer player(seq);

    player.play();
    EXPECT_EQ(player.state(), AnimationPlayerState::Stopped); // No frames to play
    EXPECT_NO_THROW(player.update(100.0f));
    EXPECT_NO_THROW(player.step_forward());
}

TEST(AnimationPlayer, CurrentFrameIndex) {
    AnimationPlayer player(make_test_sequence());
    EXPECT_EQ(player.current_frame_index(), 0u);
    player.step_forward();
    EXPECT_EQ(player.current_frame_index(), 1u);
}

TEST(AnimationPlayer, MixedDurationsLargeDelta) {
    AnimationSequence seq;
    seq.name = "mixed_delta";
    seq.is_looping = false;
    seq.frames = {
        {.image_name = "frame_0", .index = 0, .duration_ms = 100},
        {.image_name = "frame_1", .index = 1, .duration_ms = 0},
        {.image_name = "frame_2", .index = 2, .duration_ms = 50},
        {.image_name = "frame_3", .index = 3, .duration_ms = 200},
    };
    AnimationPlayer player(seq);
    player.play();
    player.update(400.0f); // 100ms(frame_0) + 0ms(frame_1) + 50ms(frame_2) + 200ms(frame_3) =
                           // 350ms, remaining 50ms into end
    EXPECT_EQ(player.current_frame_index(), 3u);
    EXPECT_EQ(player.state(), AnimationPlayerState::Completed);
}

TEST(AnimationPlayer, MultipleZeroDurationFrames) {
    AnimationSequence seq;
    seq.name = "multiple_zero";
    seq.is_looping = false;
    seq.frames = {
        {.image_name = "frame_0", .index = 0, .duration_ms = 0},
        {.image_name = "frame_1", .index = 1, .duration_ms = 0},
        {.image_name = "frame_2", .index = 2, .duration_ms = 100},
        {.image_name = "frame_3", .index = 3, .duration_ms = 0},
        {.image_name = "frame_4", .index = 4, .duration_ms = 200},
    };
    AnimationPlayer player(seq);
    player.play();
    player.update(150.0f); // skip frame_0(0), skip frame_1(0), 100ms into frame_2, skip frame_3(0),
                           // 50ms into frame_4
    EXPECT_EQ(player.current_frame_index(), 4u);
    EXPECT_EQ(player.current_frame().image_name, "frame_4");
}

TEST(AnimationPlayer, VariableDurations200ms) {
    AnimationSequence seq;
    seq.name = "variable_200";
    seq.is_looping = false;
    seq.frames = {
        {.image_name = "frame_0", .index = 0, .duration_ms = 50},
        {.image_name = "frame_1", .index = 1, .duration_ms = 150},
        {.image_name = "frame_2", .index = 2, .duration_ms = 100},
    };
    AnimationPlayer player(seq);
    player.play();
    player.update(200.0f); // 50ms(frame_0) + 150ms(frame_1) = 200ms, now at frame_2
    EXPECT_EQ(player.current_frame_index(), 2u);
    EXPECT_EQ(player.current_frame().image_name, "frame_2");
}

} // namespace d2engine
