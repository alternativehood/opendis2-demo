#include <gtest/gtest.h>
#include "d2res/mqdb.hpp"
#include "d2res/special_files.hpp"
#include <filesystem>

#ifndef DISCIPLES2_GAME_ROOT
#define DISCIPLES2_GAME_ROOT ""
#endif

static const std::filesystem::path GAME_ROOT{DISCIPLES2_GAME_ROOT};

class ListSpecialBatUnits : public ::testing::Test {
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

TEST_F(ListSpecialBatUnits, ListsThreeSpecialRecordsWithCorrectNames) {
    const auto& records = container_.records();
    std::size_t dash_count = 0;
    bool        found_index = false;
    bool        found_images = false;
    bool        found_anims = false;
    for (const auto& rec : records) {
        if (rec.name.empty() || rec.name[0] != '-')
            continue;
        ++dash_count;
        if (rec.name == "-INDEX.OPT")
            found_index = true;
        if (rec.name == "-IMAGES.OPT")
            found_images = true;
        if (rec.name == "-ANIMS.OPT")
            found_anims = true;
        EXPECT_NE(d2res::classify_record(rec), d2res::SpecialFileType::NOT_SPECIAL)
            << "Record " << rec.name << " should not be NOT_SPECIAL";
    }
    EXPECT_EQ(dash_count, 3u);
    EXPECT_TRUE(found_index);
    EXPECT_TRUE(found_images);
    EXPECT_TRUE(found_anims);
}
