#include <d2engine/app/adventure_visual_resources.hpp>

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

namespace {

d2engine::AnimationSequence sequence(std::size_t count, int width, int height) {
    d2engine::AnimationSequence result;
    result.frames.reserve(count);
    result.native_canvas_w = width;
    result.native_canvas_h = height;
    for (std::size_t i = 0; i < count; ++i)
        result.frames.push_back({"frame_" + std::to_string(i), i, 7});
    return result;
}

d2engine::AdventureRoutePreviewVisualContract contract(std::string_view name, std::size_t count,
                                                       int width, int height, int anchor_x,
                                                       int anchor_y) {
    return {name, count, width, height, anchor_x, anchor_y};
}

} // namespace

TEST(AdventureRoutePreviewVisualResources, ValidContractsPreserveFramesAndPolicy) {
    const auto cases = {
        contract("MOVENORMAL", 11, 72, 72, 35, 40),
        contract("MOVEACTION", 11, 72, 72, 35, 40),
        contract("TILE_HIGHLIGHT", 31, 480, 480, 240, 240),
    };
    for (const auto& expected : cases) {
        const auto actual = d2engine::build_adventure_route_preview_visual(
            sequence(expected.expected_frame_count, expected.expected_width,
                     expected.expected_height),
            "Imgs/IsoCmon.ff", expected);
        EXPECT_EQ(actual.container_path, "Imgs/IsoCmon.ff");
        EXPECT_EQ(actual.semantic_anchor_x, expected.semantic_anchor_x);
        EXPECT_EQ(actual.semantic_anchor_y, expected.semantic_anchor_y);
        EXPECT_EQ(actual.src_width, expected.expected_width);
        EXPECT_EQ(actual.src_height, expected.expected_height);
        EXPECT_EQ(actual.animation.animation_name, expected.animation_name);
        EXPECT_TRUE(actual.animation.is_looping);
        EXPECT_EQ(actual.animation.timing_source,
                  d2engine::AdventureRoutePreviewPlaybackPolicy::timing_source);
        ASSERT_EQ(actual.animation.frames.size(), expected.expected_frame_count);
        for (std::size_t i = 0; i < actual.animation.frames.size(); ++i) {
            EXPECT_EQ(actual.animation.frames[i].record_name, "frame_" + std::to_string(i));
            EXPECT_EQ(actual.animation.frames[i].duration_ms,
                      d2engine::AdventureRoutePreviewPlaybackPolicy::frame_duration_ms);
        }
    }
}

TEST(AdventureRoutePreviewVisualResources, ContractMismatchesAreActionable) {
    const auto expected = contract("MOVENORMAL", 11, 72, 72, 35, 40);
    for (const auto& bad : {sequence(10, 72, 72), sequence(11, 71, 72), sequence(11, 72, 71)}) {
        try {
            (void)d2engine::build_adventure_route_preview_visual(bad, "Imgs/IsoCmon.ff", expected);
            FAIL();
        } catch (const std::runtime_error& error) {
            const std::string message = error.what();
            EXPECT_NE(message.find("Imgs/IsoCmon.ff"), std::string::npos);
            EXPECT_NE(message.find("MOVENORMAL"), std::string::npos);
            EXPECT_NE(message.find("actual_frames"), std::string::npos);
            EXPECT_NE(message.find("expected_frames"), std::string::npos);
            EXPECT_NE(message.find("actual_dimensions"), std::string::npos);
            EXPECT_NE(message.find("expected_dimensions"), std::string::npos);
        }
    }
}

TEST(AdventureRoutePreviewVisualResources, EmptyFrameNameIsRejected) {
    const auto expected = contract("MOVEACTION", 1, 72, 72, 35, 40);
    auto       input = sequence(1, 72, 72);
    input.frames.front().image_name.clear();
    EXPECT_THROW(
        {
            (void)d2engine::build_adventure_route_preview_visual(input, "Imgs/IsoCmon.ff",
                                                                 expected);
        },
        std::runtime_error);
}
