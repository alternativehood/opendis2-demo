#include <gtest/gtest.h>
#include "cli/commands_extract_anim.hpp"
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

TEST(ExtractAnimAtlas, AtlasModeProducesExpectedOutput) {
    const fs::path container = GAME_ROOT / "Imgs/BatUnits.ff";
    if (!fs::exists(container))
        GTEST_SKIP() << "Game file not found: " << container;

    TempDir out("anim_atlas");
    ASSERT_EQ(cmd_extract_anim(container.string(), out.str(), "G000UU0001IDLEA1A00", "", false,
                               false, 100, true, 4096),
              0);

    EXPECT_TRUE(fs::exists(out.path() / "atlas_000.png"));

    std::ifstream f(out.path() / "atlas.json");
    ASSERT_TRUE(f.is_open());
    json j;
    f >> j;
    EXPECT_EQ(j["total_sprites"].get<int>(), 16);

    EXPECT_FALSE(fs::exists(out.path() / "frame_000.png"))
        << "atlas mode should not write individual frame PNGs";
}
