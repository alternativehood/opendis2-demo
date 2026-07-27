#include <gtest/gtest.h>
#include "d2res/mqdb.hpp"
#include <filesystem>

#ifndef DISCIPLES2_GAME_ROOT
#define DISCIPLES2_GAME_ROOT ""
#endif

static const std::filesystem::path GAME_ROOT{DISCIPLES2_GAME_ROOT};

class MqdbBattleWdb : public ::testing::Test {
protected:
    void SetUp() override {
        path_ = GAME_ROOT / "Sounds/Battle.wdb";
        if (!std::filesystem::exists(path_))
            GTEST_SKIP() << "Game file not found: " << path_;
        container_ = d2res::MqdbContainer::open(path_);
    }
    std::filesystem::path path_;
    d2res::MqdbContainer  container_;
};

TEST_F(MqdbBattleWdb, OpensAndMetadataIsConsistent) {
    EXPECT_GE(container_.names().size(), 1309u);
    EXPECT_FALSE(container_.names().empty());
    EXPECT_GT(container_.records().size(), 0u);

    auto names = container_.names();
    ASSERT_FALSE(names.empty());
    auto rec = container_.find_by_name(names[0]);
    EXPECT_TRUE(rec.has_value());
}
