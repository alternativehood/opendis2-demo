#include <gtest/gtest.h>
#include "d2res/mqdb.hpp"
#include "d2res/opt_maps.hpp"
#include "d2res/anim_decoder.hpp"
#include "d2res/byte_reader.hpp"
#include <filesystem>

#ifndef DISCIPLES2_GAME_ROOT
#define DISCIPLES2_GAME_ROOT ""
#endif

static const std::filesystem::path GAME_ROOT{DISCIPLES2_GAME_ROOT};

TEST(AnimDecoderBatUnits, DecodeAndValidateKnownAnimations) {
    const auto ff = GAME_ROOT / "Imgs/BatUnits.ff";
    if (!std::filesystem::exists(ff))
        GTEST_SKIP() << "Game file not found: " << ff;
    auto container = std::make_unique<d2res::MqdbContainer>(d2res::MqdbContainer::open(ff));
    auto maps = std::make_unique<d2res::OptMaps>(d2res::parse_opt_maps(*container));
    d2res::AnimationDecoder decoder(*container, *maps);

    const auto names = decoder.list_animations();
    EXPECT_EQ(names.size(), 4912u);

    const auto anim = decoder.decode_animation("G000UU0001IDLEA1A00");
    EXPECT_EQ(anim.frames.size(), 16u);
    ASSERT_GE(anim.frames.size(), 16u);
    EXPECT_EQ(anim.frames[0].logical_name, "XE");
    EXPECT_EQ(anim.frames[1].logical_name, "YE");
    EXPECT_EQ(anim.frames[15].logical_name, "YE");

    EXPECT_TRUE(anim.metadata.contains("name"));
    EXPECT_TRUE(anim.metadata.contains("frame_count"));
    EXPECT_TRUE(anim.metadata.contains("source_container"));
    EXPECT_TRUE(anim.metadata.contains("frame_delay_ms"));
    EXPECT_TRUE(anim.metadata.contains("frames"));
    EXPECT_EQ(anim.metadata["frame_count"].get<int>(), 16);

    const auto frames = decoder.list_animation_frames("G000UU0001IDLEA1A00");
    ASSERT_EQ(frames.size(), anim.frames.size());
    for (std::size_t i = 0; i < frames.size(); ++i) {
        EXPECT_EQ(frames[i], anim.frames[i].logical_name);
    }

    EXPECT_THROW(decoder.list_animation_frames("NONEXISTENT_ANIM"), d2res::ParseError);
}
