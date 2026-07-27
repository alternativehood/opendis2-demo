#include <gtest/gtest.h>
#include "d2res/mqdb.hpp"
#include "d2res/special_files.hpp"
#include <filesystem>
#include <vector>
#include <string>

#ifndef DISCIPLES2_GAME_ROOT
#define DISCIPLES2_GAME_ROOT ""
#endif

static const std::filesystem::path GAME_ROOT{DISCIPLES2_GAME_ROOT};

class SpecialBatUnits : public ::testing::Test {
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

TEST_F(SpecialBatUnits, HasExactlyThreeSpecialFilesWithCorrectTypesAndSizes) {
    const auto&              records = container_.records();
    std::vector<std::string> special_names;
    for (const auto& rec : records) {
        auto t = d2res::classify_record(rec);
        if (t != d2res::SpecialFileType::NOT_SPECIAL)
            special_names.push_back(rec.name);
    }
    ASSERT_EQ(special_names.size(), 3u);

    auto index_rec = container_.find_by_name("-INDEX.OPT");
    auto images_rec = container_.find_by_name("-IMAGES.OPT");
    auto anims_rec = container_.find_by_name("-ANIMS.OPT");
    ASSERT_TRUE(index_rec.has_value());
    ASSERT_TRUE(images_rec.has_value());
    ASSERT_TRUE(anims_rec.has_value());
    EXPECT_EQ(d2res::classify_record(index_rec.value()), d2res::SpecialFileType::INDEX_OPT);
    EXPECT_EQ(d2res::classify_record(images_rec.value()), d2res::SpecialFileType::IMAGES_OPT);
    EXPECT_EQ(d2res::classify_record(anims_rec.value()), d2res::SpecialFileType::ANIMS_OPT);

    for (const auto& rec : records) {
        if (d2res::classify_record(rec) == d2res::SpecialFileType::NOT_SPECIAL)
            continue;
        auto entry = container_.read_record(rec.index);
        EXPECT_EQ(static_cast<int32_t>(entry.payload.size()), rec.realFileSize)
            << "Payload size mismatch for " << rec.name;
    }
}
