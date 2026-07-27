#include <gtest/gtest.h>
#include "cli/commands_extract_anim.hpp"
#include "d2res/mqdb.hpp"
#include "d2res/opt_maps.hpp"
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

TEST(ExtractAnimPattern, PatternExtractCreatesSubdirsAndManifest) {
    const fs::path container_path = GAME_ROOT / "Imgs/BatUnits.ff";
    if (!fs::exists(container_path))
        GTEST_SKIP() << "Game file not found: " << container_path;

    auto container = d2res::MqdbContainer::open(container_path);
    auto maps = d2res::parse_opt_maps(container);

    // Select the animation with the smallest non-zero frame count
    std::string target_name;
    std::size_t target_frames = SIZE_MAX;
    for (const auto& block : maps.anim_map.blocks) {
        if (!block.frames.empty() && block.frames.size() < target_frames) {
            target_frames = block.frames.size();
            target_name = block.name;
        } else if (!block.frames.empty() && block.frames.size() == target_frames) {
            if (block.name < target_name)
                target_name = block.name;
        }
    }
    ASSERT_FALSE(target_name.empty()) << "no animation with >0 frames found";

    // Derive shortest unique prefix
    std::string unique_pattern;
    for (std::size_t len = 1; len <= target_name.size(); ++len) {
        std::string prefix = target_name.substr(0, len);
        std::size_t match_count = 0;
        for (const auto& block : maps.anim_map.blocks) {
            if (block.name.size() >= prefix.size() &&
                block.name.compare(0, prefix.size(), prefix) == 0)
                ++match_count;
        }
        if (match_count == 1) {
            unique_pattern = prefix + "*";
            break;
        }
    }
    ASSERT_FALSE(unique_pattern.empty()) << "no unique prefix found for " << target_name;

    TempDir out("anim_pattern");
    ASSERT_EQ(cmd_extract_anim_from_container(container, maps, out.str(), "", unique_pattern, false,
                                              false, 100),
              0);

    std::size_t dir_count = 0;
    for (const auto& entry : fs::directory_iterator(out.path())) {
        if (entry.is_directory())
            ++dir_count;
    }
    EXPECT_EQ(dir_count, 1u) << "exactly one animation directory expected";

    std::ifstream f(out.path() / "anim_manifest.json");
    ASSERT_TRUE(f.is_open());
    json j;
    f >> j;
    EXPECT_EQ(j["written_animations"].get<std::size_t>(), 1u);
    EXPECT_EQ(j["failed_animations"].get<std::size_t>(), 0u);
}
