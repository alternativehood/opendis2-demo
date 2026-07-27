#include <gtest/gtest.h>
#include "cli/commands_scan.hpp"
#include "tests/test_helpers.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <cctype>

#ifndef DISCIPLES2_GAME_ROOT
#define DISCIPLES2_GAME_ROOT ""
#endif

static const std::filesystem::path GAME_ROOT{DISCIPLES2_GAME_ROOT};
namespace fs = std::filesystem;
using json = nlohmann::json;

TEST(ScanGame, ScanProduceStructureValid) {
    if (!fs::is_directory(GAME_ROOT))
        GTEST_SKIP() << "Game root not found: " << GAME_ROOT;

    TempDir        synth("scan_synth");
    const fs::path synth_root = synth.path() / "synth_root";
    fs::create_directories(synth_root / "Interf");

    const fs::path src = GAME_ROOT / "Interf/Interf.ff";
    if (!fs::exists(src))
        GTEST_SKIP() << "Interf.ff not found at " << src;

    fs::copy_file(src, synth_root / "Interf/Interf.ff");

    TempDir out("scan_game");
    ASSERT_EQ(cmd_scan(synth_root.string(), out.str()), 0);

    EXPECT_TRUE(fs::exists(out.path() / "game_manifest.json"));

    std::ifstream f(out.path() / "game_manifest.json");
    ASSERT_TRUE(f.is_open());
    json j;
    f >> j;

    EXPECT_TRUE(j.contains("game_root"));
    EXPECT_TRUE(j.contains("scan_timestamp"));
    EXPECT_TRUE(j.contains("total_files"));
    EXPECT_TRUE(j.contains("mqdb_containers"));
    EXPECT_FALSE(j["mqdb_containers"].empty());
    EXPECT_GT(j["total_files"].get<int>(), 0);

    for (const auto& c : j["mqdb_containers"]) {
        EXPECT_EQ(c["sha256"].get<std::string>().size(), 64u)
            << "sha256 wrong length for " << c["path"];
    }
}
