#include <gtest/gtest.h>
#include "d2res/mqdb.hpp"
#include "d2res/opt_index.hpp"
#include "d2res/opt_anims.hpp"
#include <filesystem>
#include <string>
#include <vector>

#ifndef DISCIPLES2_GAME_ROOT
#define DISCIPLES2_GAME_ROOT ""
#endif

static const std::filesystem::path GAME_ROOT{DISCIPLES2_GAME_ROOT};

TEST(OptAnimsBatUnits, ParsedAnimsMatchesKnownBatUnitsData) {
    const auto ff = GAME_ROOT / "Imgs/BatUnits.ff";
    if (!std::filesystem::exists(ff))
        GTEST_SKIP() << "Game file not found: " << ff;
    auto container = d2res::MqdbContainer::open(ff);

    auto idx_rec = container.find_by_name("-INDEX.OPT");
    ASSERT_TRUE(idx_rec.has_value());
    auto idx_entry = container.read_record(idx_rec.value().index);
    auto index_map = d2res::IndexParser::parse(std::span<const uint8_t>(idx_entry.payload));

    std::vector<std::string> anim_names;
    for (const auto& e : index_map.entries) {
        if (e.is_animation())
            anim_names.push_back(e.name);
    }

    auto anm_rec = container.find_by_name("-ANIMS.OPT");
    ASSERT_TRUE(anm_rec.has_value());
    auto anm_entry = container.read_record(anm_rec.value().index);
    auto map = d2res::AnimsParser::parse(std::span<const uint8_t>(anm_entry.payload), anim_names);

    EXPECT_EQ(map.blocks.size(), 4912u);

    auto it = map.name_to_block.find("G000UU0001IDLEA1A00");
    ASSERT_NE(it, map.name_to_block.end());
    const auto& frames = map.blocks[it->second].frames;
    EXPECT_EQ(frames.size(), 16u);

    const std::vector<std::string> expected = {"XE", "YE", "ZE", "0E", "1E", "2E", "3E", "4E",
                                               "5E", "4E", "3E", "2E", "1E", "0E", "ZE", "YE"};
    EXPECT_EQ(frames, expected);
}
