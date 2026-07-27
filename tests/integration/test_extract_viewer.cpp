#include <gtest/gtest.h>
#include "cli/commands_extract_images.hpp"
#include "cli/commands_extract_viewer.hpp"
#include "tests/test_helpers.hpp"
#include <filesystem>
#include <fstream>
#include <string>

#ifndef DISCIPLES2_GAME_ROOT
#define DISCIPLES2_GAME_ROOT ""
#endif

static const std::filesystem::path GAME_ROOT{DISCIPLES2_GAME_ROOT};
namespace fs = std::filesystem;

TEST(ExtractViewer, GeneratesValidHtmlWithEmbeddedPng) {
    const auto capital = GAME_ROOT / "Imgs/Capital.ff";
    if (!fs::exists(capital))
        GTEST_SKIP() << "Game file not found: " << capital;

    TempDir        assets_dir("viewer_assets");
    TempDir        out_dir("viewer_out");
    const fs::path out_html = out_dir.path() / "viewer.html";

    ASSERT_EQ(cmd_extract_images(capital.string(), assets_dir.str(), "EMP_IMPGUILD*"), 0);
    ASSERT_EQ(cmd_extract_viewer(assets_dir.str(), out_html.string()), 0);

    EXPECT_TRUE(fs::exists(out_html));
    EXPECT_GT(fs::file_size(out_html), static_cast<uintmax_t>(1000));

    std::ifstream f(out_html);
    ASSERT_TRUE(f.is_open());
    std::string line;
    std::getline(f, line);
    EXPECT_EQ(line, "<!DOCTYPE html>");

    std::string content;
    content.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    EXPECT_NE(content.find("data:image/png;base64,"), std::string::npos);
}
