#include <gtest/gtest.h>
#include "d2res/mqdb.hpp"
#include <filesystem>

#ifndef DISCIPLES2_GAME_ROOT
#define DISCIPLES2_GAME_ROOT ""
#endif

static const std::filesystem::path GAME_ROOT{DISCIPLES2_GAME_ROOT};

TEST(MqdbPayloadSizes, AllBatUnitsPayloadSizesMatch) {
    auto path = GAME_ROOT / "Imgs/BatUnits.ff";
    if (!std::filesystem::exists(path))
        GTEST_SKIP() << "Game file not found: " << path;

    auto c = d2res::MqdbContainer::open(path);
    for (const auto& rec : c.records()) {
        auto view = c.payload_view(rec.index);
        EXPECT_EQ(view.size(), static_cast<std::size_t>(rec.realFileSize))
            << "Record " << rec.index << " (name='" << rec.name << "') size mismatch";
    }
}

TEST(MqdbPayloadSizes, AllBattleWdbPayloadSizesMatch) {
    auto path = GAME_ROOT / "Sounds/Battle.wdb";
    if (!std::filesystem::exists(path))
        GTEST_SKIP() << "Game file not found: " << path;

    auto c = d2res::MqdbContainer::open(path);
    for (const auto& rec : c.records()) {
        auto view = c.payload_view(rec.index);
        EXPECT_EQ(view.size(), static_cast<std::size_t>(rec.realFileSize))
            << "Record " << rec.index << " size mismatch";
    }
}
