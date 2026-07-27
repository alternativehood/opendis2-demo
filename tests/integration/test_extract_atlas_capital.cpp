#include <gtest/gtest.h>
#include "cli/commands_extract_atlas.hpp"
#include "tests/test_helpers.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>

#ifndef DISCIPLES2_GAME_ROOT
#define DISCIPLES2_GAME_ROOT ""
#endif

static const std::filesystem::path GAME_ROOT{DISCIPLES2_GAME_ROOT};
namespace fs = std::filesystem;
using json = nlohmann::json;

TEST(ExtractAtlasCapital, ExtractAndValidateAtlasArtifacts) {
    const fs::path container = GAME_ROOT / "Imgs/Capital.ff";
    if (!fs::exists(container))
        GTEST_SKIP() << "Game file not found: " << container;

    TempDir out("atlas_capital");
    ASSERT_EQ(cmd_extract_atlas(container.string(), out.str(), "EMP_IMPGUILD*", false, 4096), 0);

    EXPECT_TRUE(fs::exists(out.path() / "atlas_000.png"));

    std::ifstream f(out.path() / "atlas.json");
    ASSERT_TRUE(f.is_open());
    json j;
    f >> j;
    EXPECT_TRUE(j.contains("source_container"));
    EXPECT_TRUE(j.contains("max_sheet_size"));
    EXPECT_TRUE(j.contains("sheet_count"));
    EXPECT_TRUE(j.contains("total_sprites"));
    EXPECT_TRUE(j.contains("skipped_sprites"));
    EXPECT_TRUE(j.contains("entries"));
    EXPECT_GT(j["total_sprites"].get<int>(), 0);

    const int max_sz = j["max_sheet_size"].get<int>();
    for (const auto& e : j["entries"]) {
        int const x = e["x"].get<int>();
        int const y = e["y"].get<int>();
        int const w = e["w"].get<int>();
        int const h = e["h"].get<int>();
        EXPECT_LE(x + w, max_sz) << "Entry " << e["name"] << " exceeds max_size";
        EXPECT_LE(y + h, max_sz) << "Entry " << e["name"] << " exceeds max_size";
    }
}
