#include <gtest/gtest.h>
#include "cli/commands_extract_images.hpp"
#include "tests/test_helpers.hpp"
#include <filesystem>
#include <string>
#include <fnmatch.h>

#ifndef DISCIPLES2_GAME_ROOT
#define DISCIPLES2_GAME_ROOT ""
#endif

static const std::filesystem::path GAME_ROOT{DISCIPLES2_GAME_ROOT};
namespace fs = std::filesystem;

TEST(ExtractImagesInterf, ExtractAndValidateInterfImages) {
    const fs::path container = GAME_ROOT / "Interf/Interf.ff";
    if (!fs::exists(container))
        GTEST_SKIP() << "Game file not found: " << container;

    TempDir           out("extr_interf");
    const std::string pattern = "DLG_H_ARCHER*";
    ASSERT_EQ(cmd_extract_images(container.string(), out.str(), pattern), 0);

    bool found_archer = false;
    for (const auto& entry : fs::directory_iterator(out.path())) {
        if (entry.path().extension() != ".png")
            continue;
        const std::string stem = entry.path().stem().string();
        if (fnmatch("DLG_H_ARCHER*", stem.c_str(), FNM_CASEFOLD) == 0) {
            found_archer = true;
        } else {
            ADD_FAILURE() << "Unexpected non-matching PNG: " << stem;
        }
    }
    EXPECT_TRUE(found_archer) << "No DLG_H_ARCHER* .png file found in output";
}
