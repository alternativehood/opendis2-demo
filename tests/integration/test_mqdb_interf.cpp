#include <gtest/gtest.h>
#include "d2res/mqdb.hpp"
#include <filesystem>

#ifndef DISCIPLES2_GAME_ROOT
#define DISCIPLES2_GAME_ROOT ""
#endif

static const std::filesystem::path GAME_ROOT{DISCIPLES2_GAME_ROOT};

TEST(MqdbInterf, OpensAndHasIndex) {
    auto path = GAME_ROOT / "Interf/Interf.ff";
    if (!std::filesystem::exists(path))
        GTEST_SKIP() << "Game file not found: " << path;

    auto c = d2res::MqdbContainer::open(path);
    EXPECT_GT(c.records().size(), 0u);
    EXPECT_TRUE(c.find_by_name("-INDEX.OPT").has_value());
    EXPECT_FALSE(c.names().empty());
}
