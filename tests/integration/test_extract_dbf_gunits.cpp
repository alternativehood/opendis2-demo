#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>

#include "cli/commands_extract_dbf.hpp"
#include "tests/test_helpers.hpp"
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

static const char* GAME_ROOT = DISCIPLES2_GAME_ROOT;

TEST(ExtractDbfGunits, ProducesTwoJsonFilesWithContent) {
    if ((GAME_ROOT == nullptr) || std::string(GAME_ROOT).empty())
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";
    const fs::path src = fs::path(GAME_ROOT) / "Globals" / "Gunits.dbf";
    if (!fs::exists(src))
        GTEST_SKIP() << "Gunits.dbf not found: " << src;

    TempDir out("dbf_gunits");
    ASSERT_EQ(cmd_extract_dbf(src.string(), out.str()), 0);
    EXPECT_TRUE(fs::exists(out.path() / "Gunits.schema.json"));
    EXPECT_TRUE(fs::exists(out.path() / "Gunits.records.json"));

    std::ifstream f(out.path() / "Gunits.records.json");
    ASSERT_TRUE(f);
    nlohmann::json j;
    f >> j;
    EXPECT_TRUE(j.is_array());
    EXPECT_GT(j.size(), 0u);
    EXPECT_LE(j.size(), 359u);
}
