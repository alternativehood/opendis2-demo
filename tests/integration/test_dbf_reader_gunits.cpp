#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdint>

#include "d2res/dbf_reader.hpp"

namespace fs = std::filesystem;

static const char* GAME_ROOT = DISCIPLES2_GAME_ROOT;

class DbfReaderGunitsTest : public ::testing::Test {
protected:
    void SetUp() override {
        if ((GAME_ROOT == nullptr) || std::string(GAME_ROOT).empty()) {
            GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";
        }
        dbf_path_ = fs::path(GAME_ROOT) / "Globals" / "Gunits.dbf";
        if (!fs::exists(dbf_path_)) {
            GTEST_SKIP() << "Gunits.dbf not found: " << dbf_path_;
        }
        std::ifstream f(dbf_path_, std::ios::binary);
        data_.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    }

    fs::path             dbf_path_;
    std::vector<uint8_t> data_;
};

TEST_F(DbfReaderGunitsTest, RecordCount) {
    d2res::DbfReader const reader(
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(data_.data()), data_.size()));
    EXPECT_EQ(reader.record_count(), 359);
}

TEST_F(DbfReaderGunitsTest, RecordsArraySize) {
    d2res::DbfReader const reader(
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(data_.data()), data_.size()));
    auto records = reader.read_records();
    // header record_count is 359; some may be marked deleted — expect <=359 and >0
    EXPECT_LE(static_cast<int>(records.size()), 359);
    EXPECT_GT(static_cast<int>(records.size()), 0);
}

TEST_F(DbfReaderGunitsTest, FirstRecordHasUnitId) {
    d2res::DbfReader const reader(
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(data_.data()), data_.size()));
    auto records = reader.read_records();
    ASSERT_FALSE(records.empty());
    EXPECT_TRUE(records[0].contains("UNIT_ID"));
}

TEST_F(DbfReaderGunitsTest, SchemaHasCharField) {
    d2res::DbfReader const reader(
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(data_.data()), data_.size()));
    auto j = reader.to_schema_json("Gunits.dbf");
    ASSERT_TRUE(j.contains("fields"));
    bool found_c = false;
    for (const auto& f : j["fields"]) {
        if (f["type"].get<std::string>() == "C") {
            found_c = true;
            break;
        }
    }
    EXPECT_TRUE(found_c);
}
