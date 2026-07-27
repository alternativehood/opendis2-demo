#include <gtest/gtest.h>
#include "d2res/mqdb.hpp"
#include "d2res/opt_maps.hpp"
#include <filesystem>

#ifndef DISCIPLES2_GAME_ROOT
#define DISCIPLES2_GAME_ROOT ""
#endif

static const std::filesystem::path GAME_ROOT{DISCIPLES2_GAME_ROOT};

TEST(OptMapsBatUnits, ParsedMapsMatchesKnownBatUnitsData) {
    const auto ff = GAME_ROOT / "Imgs/BatUnits.ff";
    if (!std::filesystem::exists(ff))
        GTEST_SKIP() << "Game file not found: " << ff;
    auto container = d2res::MqdbContainer::open(ff);
    auto maps = d2res::parse_opt_maps(container);

    EXPECT_EQ(maps.index_map.entries.size(), 69921u);
    EXPECT_EQ(maps.anim_map.blocks.size(), 4912u);

    for (const auto& w : maps.warnings) {
        EXPECT_EQ(w.find("container has no"), std::string::npos) << "Fatal parse warning: " << w;
    }

    auto it = maps.anim_map.name_to_block.find("G000UU0001IDLEA1A00");
    ASSERT_NE(it, maps.anim_map.name_to_block.end());
    EXPECT_EQ(maps.anim_map.blocks[it->second].frames.size(), 16u);
}
