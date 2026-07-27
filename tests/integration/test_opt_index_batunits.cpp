#include <gtest/gtest.h>
#include "d2res/mqdb.hpp"
#include "d2res/opt_index.hpp"
#include <filesystem>

#ifndef DISCIPLES2_GAME_ROOT
#define DISCIPLES2_GAME_ROOT ""
#endif

static const std::filesystem::path GAME_ROOT{DISCIPLES2_GAME_ROOT};

TEST(OptIndexBatUnits, ParsedIndexMatchesKnownBatUnitsData) {
    const auto ff = GAME_ROOT / "Imgs/BatUnits.ff";
    if (!std::filesystem::exists(ff))
        GTEST_SKIP() << "Game file not found: " << ff;
    auto container = d2res::MqdbContainer::open(ff);
    auto rec = container.find_by_name("-INDEX.OPT");
    ASSERT_TRUE(rec.has_value()) << "-INDEX.OPT not in container";
    auto map = d2res::IndexParser::parse(container.payload_view(rec.value().index));

    EXPECT_EQ(map.entries.size(), 69921u);

    std::size_t anim_count = 0;
    for (const auto& e : map.entries) {
        if (e.is_animation())
            ++anim_count;
    }
    EXPECT_EQ(anim_count, 4912u);
    EXPECT_EQ(map.entries.size() - anim_count, 65009u);

    auto it = map.name_to_index.find("G000UU0001IDLEA1A00");
    ASSERT_NE(it, map.name_to_index.end());
    EXPECT_TRUE(map.entries[it->second].is_animation());

    EXPECT_NE(map.name_to_index.find("G000UU0001IDLEA1A00"), map.name_to_index.end());
}
