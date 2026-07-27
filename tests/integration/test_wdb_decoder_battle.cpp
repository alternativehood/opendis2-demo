#include <gtest/gtest.h>
#include "d2res/wdb_decoder.hpp"
#include "d2res/mqdb.hpp"
#include <filesystem>

#ifndef DISCIPLES2_GAME_ROOT
#define DISCIPLES2_GAME_ROOT ""
#endif

static const std::filesystem::path GAME_ROOT{DISCIPLES2_GAME_ROOT};
namespace fs = std::filesystem;

TEST(WdbDecoderBattle, DecodeAndValidateBattleSounds) {
    const fs::path path = GAME_ROOT / "Sounds/Battle.wdb";
    if (!fs::exists(path))
        GTEST_SKIP() << "Game file not found: " << path;
    auto              container = d2res::MqdbContainer::open(path);
    d2res::WdbDecoder decoder(container);

    const auto names = decoder.list_sounds();
    EXPECT_GT(names.size(), 0u);

    ASSERT_FALSE(names.empty());
    const auto sound = decoder.decode_sound(names[0]);
    EXPECT_FALSE(sound.payload.empty());
    EXPECT_EQ(sound.detected_format, "WAV");

    ASSERT_EQ(sound.detected_format, "WAV");
    EXPECT_TRUE(sound.metadata.contains("logical_name"));
    EXPECT_TRUE(sound.metadata.contains("source_container"));
    EXPECT_TRUE(sound.metadata.contains("payload_size"));
    EXPECT_TRUE(sound.metadata.contains("detected_format"));
    EXPECT_TRUE(sound.metadata.contains("format_tag"));
    EXPECT_TRUE(sound.metadata.contains("channels"));
    EXPECT_TRUE(sound.metadata.contains("sample_rate"));
    EXPECT_GT(sound.metadata["sample_rate"].get<int>(), 0);

    bool found_mpeg_wave = false;
    for (const auto& name : names) {
        const auto candidate = decoder.decode_sound(name);
        if (!candidate.metadata.contains("format_tag") ||
            candidate.metadata["format_tag"].get<int>() != 85) {
            continue;
        }
        found_mpeg_wave = true;
        ASSERT_GE(candidate.payload.size(), 12U);
        EXPECT_EQ(candidate.detected_format, "WAV");
        EXPECT_EQ(candidate.payload[0], static_cast<std::uint8_t>('R'));
        EXPECT_EQ(candidate.payload[8], static_cast<std::uint8_t>('W'));
        EXPECT_EQ(candidate.metadata["format_tag"].get<int>(), 85);
        break;
    }
    if (!found_mpeg_wave)
        GTEST_SKIP() << "format-tag 85 WAVE is absent in this localized build";
}
