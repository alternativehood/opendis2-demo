#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>

#include "cli/commands_extract_all.hpp"
#include "tests/test_helpers.hpp"
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

TEST(ExtractAll, ProducesManifestsForEmptyAndNonexistentRoot) {
    TempDir game("extract_all_game");
    TempDir out("extract_all_out");

    ASSERT_EQ(cmd_extract_all(game.str(), out.str()), 0);
    EXPECT_TRUE(fs::exists(out.path() / "extraction_manifest.json"));
    EXPECT_TRUE(fs::exists(out.path() / "game_manifest.json"));

    std::ifstream f(out.path() / "extraction_manifest.json");
    ASSERT_TRUE(f);
    nlohmann::json j;
    f >> j;
    EXPECT_TRUE(j.contains("game_root"));
    EXPECT_TRUE(j.contains("out_dir"));
    EXPECT_TRUE(j.contains("total_containers"));
    EXPECT_TRUE(j.contains("extracted"));
    EXPECT_TRUE(j.contains("skipped"));
    EXPECT_TRUE(j.contains("failed"));
    EXPECT_EQ(j["total_containers"].get<int>(), 0);
    EXPECT_EQ(j["extracted"].get<int>(), 0);

    std::ifstream g(out.path() / "game_manifest.json");
    ASSERT_TRUE(g);
    nlohmann::json gm;
    g >> gm;
    EXPECT_TRUE(gm.contains("asset_schema_version"));
    EXPECT_EQ(gm["asset_schema_version"].get<int>(), 1);
    EXPECT_TRUE(gm.contains("containers"));
    EXPECT_TRUE(gm.contains("assets"));
    EXPECT_TRUE(gm.contains("warnings"));

    EXPECT_NE(cmd_extract_all("/nonexistent/game/dir", out.str()), 0);
}
