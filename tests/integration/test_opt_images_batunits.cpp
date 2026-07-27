#include <gtest/gtest.h>
#include "d2res/mqdb.hpp"
#include "d2res/opt_images.hpp"
#include <filesystem>

#ifndef DISCIPLES2_GAME_ROOT
#define DISCIPLES2_GAME_ROOT ""
#endif

static const std::filesystem::path GAME_ROOT{DISCIPLES2_GAME_ROOT};

TEST(OptImagesBatUnits, ParsedImagesMatchesKnownBatUnitsData) {
    const auto ff = GAME_ROOT / "Imgs/BatUnits.ff";
    if (!std::filesystem::exists(ff))
        GTEST_SKIP() << "Game file not found: " << ff;
    auto container = d2res::MqdbContainer::open(ff);
    auto rec = container.find_by_name("-IMAGES.OPT");
    ASSERT_TRUE(rec.has_value()) << "-IMAGES.OPT not in container";
    auto map = d2res::ImagesParser::parse(container.payload_view(rec.value().index));

    EXPECT_NE(map.frame_name_to_block.find("XE"), map.frame_name_to_block.end());

    auto it = map.frame_name_to_block.find("XE");
    ASSERT_NE(it, map.frame_name_to_block.end());
    const auto& block = map.blocks[it->second];
    EXPECT_EQ(block.opacity_algorithm, 255);
    EXPECT_EQ(block.size_x, 221);
    EXPECT_EQ(block.size_y, 218);

    const d2res::ImageFrame* xe = nullptr;
    for (const auto& f : block.frames) {
        if (f.name == "XE") {
            xe = &f;
            break;
        }
    }
    ASSERT_NE(xe, nullptr);
    EXPECT_EQ(xe->pieces.size(), 4u);
    EXPECT_EQ(xe->output_width, 800);
    EXPECT_EQ(xe->output_height, 600);

    ASSERT_GE(xe->pieces.size(), 1u);
    const auto& p0 = xe->pieces[0];
    EXPECT_EQ(p0.output_x, 388);
    EXPECT_EQ(p0.output_y, 448);
    EXPECT_EQ(p0.base_x, 0);
    EXPECT_EQ(p0.base_y, 0);
    EXPECT_EQ(p0.width, 80);
    EXPECT_EQ(p0.height, 32);

    // Palette: entry 0 is magenta (BGRA)
    EXPECT_EQ(block.palette[0][0], 255u);
    EXPECT_EQ(block.palette[0][1], 0u);
    EXPECT_EQ(block.palette[0][2], 255u);
}
