#include <gtest/gtest.h>
#include "d2res/mqdb.hpp"
#include <filesystem>

#ifndef DISCIPLES2_GAME_ROOT
#define DISCIPLES2_GAME_ROOT ""
#endif

static const std::filesystem::path GAME_ROOT{DISCIPLES2_GAME_ROOT};

class MqdbBatUnits : public ::testing::Test {
protected:
    void SetUp() override {
        path_ = GAME_ROOT / "Imgs/BatUnits.ff";
        if (!std::filesystem::exists(path_))
            GTEST_SKIP() << "Game file not found (set DISCIPLES2_GAME_ROOT): " << path_;
        container_ = d2res::MqdbContainer::open(path_);
    }
    std::filesystem::path path_;
    d2res::MqdbContainer  container_;
};

TEST_F(MqdbBatUnits, ContainerOpensAndAllMetadataValidates) {
    EXPECT_EQ(container_.records().size(), 4975u);
    EXPECT_EQ(container_.names().size(), 4972u);

    auto index = container_.find_by_name("-INDEX.OPT");
    ASSERT_TRUE(index.has_value());
    EXPECT_GT(index.value().realFileSize, 0);

    EXPECT_TRUE(container_.find_by_name("-IMAGES.OPT").has_value());
    EXPECT_TRUE(container_.find_by_name("-ANIMS.OPT").has_value());

    auto upper = container_.find_by_name("-INDEX.OPT");
    auto lower = container_.find_by_name("-index.opt");
    auto mixed = container_.find_by_name("-Index.Opt");
    ASSERT_TRUE(upper.has_value());
    ASSERT_TRUE(lower.has_value());
    ASSERT_TRUE(mixed.has_value());
    EXPECT_EQ(upper.value().index, lower.value().index);
    EXPECT_EQ(upper.value().index, mixed.value().index);

    EXPECT_FALSE(container_.find_by_name("DOES_NOT_EXIST_XYZ").has_value());
    EXPECT_TRUE(container_.warnings().empty());

    auto entry = container_.read_record(index.value().index);
    EXPECT_EQ(entry.payload.size(), static_cast<std::size_t>(index.value().realFileSize));
}
