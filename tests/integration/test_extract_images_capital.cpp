#include <gtest/gtest.h>
#include "cli/commands_extract_images.hpp"
#include "tests/test_helpers.hpp"
#include "d2res/mqdb.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>

#ifndef DISCIPLES2_GAME_ROOT
#define DISCIPLES2_GAME_ROOT ""
#endif

static const std::filesystem::path GAME_ROOT{DISCIPLES2_GAME_ROOT};
namespace fs = std::filesystem;
using json = nlohmann::json;

TEST(ExtractImagesCapital, ExtractAndValidateExpectedArtifacts) {
    const fs::path container = GAME_ROOT / "Imgs/Capital.ff";
    if (!fs::exists(container))
        GTEST_SKIP() << "Game file not found: " << container;

    TempDir out("extr_capital");
    ASSERT_EQ(cmd_extract_images(container.string(), out.str(), "EMP_IMPGUILD*"), 0);

    EXPECT_TRUE(fs::exists(out.path() / "EMP_IMPGUILD.png"));
    EXPECT_GT(fs::file_size(out.path() / "EMP_IMPGUILD.png"), 0u);

    {
        std::ifstream f(out.path() / "EMP_IMPGUILD.json");
        ASSERT_TRUE(f.is_open());
        json j;
        f >> j;
        EXPECT_TRUE(j.contains("logical_name"));
        EXPECT_TRUE(j.contains("source_container"));
        EXPECT_TRUE(j.contains("base_image"));
        EXPECT_TRUE(j.contains("palette_id"));
        EXPECT_TRUE(j.contains("transparency_mode"));
        EXPECT_TRUE(j.contains("transparent_colors_bgr"));
        EXPECT_TRUE(j.contains("parts"));
        EXPECT_TRUE(j.contains("output_size"));
    }

    {
        std::ifstream f(out.path() / "images_manifest.json");
        ASSERT_TRUE(f.is_open());
        json j;
        f >> j;
        EXPECT_GT(j["written_images"].get<std::size_t>(), 0u);
    }
}

TEST(ExtractImagesCity, ImageOnlyContainerDoesNotRequireAnimationMap) {
    const fs::path container = GAME_ROOT / "Imgs/City.ff";
    if (!fs::is_regular_file(container))
        GTEST_SKIP() << "Game file not found: " << container;

    d2res::MqdbContainer mqdb = d2res::MqdbContainer::open(container);
    auto                 names = mqdb.names();
    std::string          pattern;
    for (const auto& n : names) {
        if (n.find(".PNG") != std::string::npos || n.find(".png") != std::string::npos) {
            auto stem = n;
            auto dot = stem.rfind('.');
            if (dot != std::string::npos)
                stem = stem.substr(0, dot);
            pattern = stem + "*";
            break;
        }
    }
    if (pattern.empty())
        GTEST_SKIP() << "No PNG records found in City.ff";

    TempDir out("extr_city");
    ASSERT_EQ(cmd_extract_images(container.string(), out.str(), pattern), 0);

    std::ifstream input(out.path() / "images_manifest.json");
    ASSERT_TRUE(input.is_open());
    json manifest;
    input >> manifest;
    EXPECT_GT(manifest["written_images"].get<std::size_t>(), 0U)
        << "must extract at least one image using pattern=" << pattern;
}
