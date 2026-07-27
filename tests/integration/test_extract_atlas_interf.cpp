#include <gtest/gtest.h>
#include "cli/commands_extract_atlas.hpp"
#include "tests/test_helpers.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <string>

#ifndef DISCIPLES2_GAME_ROOT
#define DISCIPLES2_GAME_ROOT ""
#endif

static const std::filesystem::path GAME_ROOT{DISCIPLES2_GAME_ROOT};
namespace fs = std::filesystem;
using json = nlohmann::json;

TEST(ExtractAtlasInterf, ExtractsAtLeastOneSheet) {
    const fs::path container = GAME_ROOT / "Interf/Interf.ff";
    if (!fs::exists(container))
        GTEST_SKIP() << "Game file not found: " << container;

    TempDir out("atlas_interf");
    ASSERT_EQ(cmd_extract_atlas(container.string(), out.str(), "_PG0500IX*", false, 4096), 0);

    std::ifstream f(out.path() / "atlas.json");
    ASSERT_TRUE(f.is_open());
    json j;
    f >> j;
    EXPECT_GE(j["sheet_count"].get<int>(), 1);
    EXPECT_GT(j["total_sprites"].get<int>(), 0);
}
