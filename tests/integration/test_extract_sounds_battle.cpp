#include <gtest/gtest.h>
#include "cli/commands_extract_sounds.hpp"
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

TEST(ExtractSoundsBattle, ExtractAndValidateSoundArtifacts) {
    const fs::path container = GAME_ROOT / "Sounds/Battle.wdb";
    if (!fs::exists(container))
        GTEST_SKIP() << "Game file not found: " << container;

    TempDir out("snd_battle");
    ASSERT_EQ(cmd_extract_sounds(container.string(), out.str(), "", true), 0);

    bool found_wav = false;
    for (const auto& entry : fs::directory_iterator(out.path())) {
        if (entry.path().extension() == ".wav") {
            found_wav = true;
            break;
        }
    }
    EXPECT_TRUE(found_wav) << "No .wav files found in " << out.str();

    std::ifstream f(out.path() / "manifest.json");
    ASSERT_TRUE(f.is_open());
    json j;
    f >> j;
    EXPECT_TRUE(j.contains("source_container"));
    EXPECT_TRUE(j.contains("total_sounds"));
    EXPECT_TRUE(j.contains("written_sounds"));
    EXPECT_TRUE(j.contains("failed_sounds"));
    EXPECT_GT(j["total_sounds"].get<int>(), 0);
}
