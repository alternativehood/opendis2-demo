#include <gtest/gtest.h>
#include "cli/commands_extract_anim.hpp"
#include "tests/test_helpers.hpp"
#include <filesystem>
#include <fstream>

#ifndef DISCIPLES2_GAME_ROOT
#define DISCIPLES2_GAME_ROOT ""
#endif

static const std::filesystem::path GAME_ROOT{DISCIPLES2_GAME_ROOT};
namespace fs = std::filesystem;

TEST(ExtractAnimGif, GifModeProducesExpectedOutput) {
    const fs::path container = GAME_ROOT / "Imgs/BatUnits.ff";
    if (!fs::exists(container))
        GTEST_SKIP() << "Game file not found: " << container;

    TempDir out("anim_gif");
    ASSERT_EQ(cmd_extract_anim(container.string(), out.str(), "G000UU0001IDLEA1A00", "", false,
                               true, 100),
              0);

    EXPECT_TRUE(fs::exists(out.path() / "preview.gif"));
    EXPECT_GT(fs::file_size(out.path() / "preview.gif"), 100u);

    std::ifstream f(out.path() / "preview.gif", std::ios::binary);
    ASSERT_TRUE(f.is_open());
    char hdr[3];
    f.read(hdr, 3);
    EXPECT_EQ(hdr[0], 'G');
    EXPECT_EQ(hdr[1], 'I');
    EXPECT_EQ(hdr[2], 'F');
}
