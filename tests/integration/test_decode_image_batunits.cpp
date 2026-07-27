#include <gtest/gtest.h>
#include "d2res/mqdb.hpp"
#include "d2res/opt_maps.hpp"
#include "d2res/image_decoder.hpp"
#include <filesystem>

#ifndef DISCIPLES2_GAME_ROOT
#define DISCIPLES2_GAME_ROOT ""
#endif

static const std::filesystem::path GAME_ROOT{DISCIPLES2_GAME_ROOT};

TEST(DecodeImageBatUnits, DecodeAndValidateKnownImages) {
    const auto ff = GAME_ROOT / "Imgs/BatUnits.ff";
    if (!std::filesystem::exists(ff))
        GTEST_SKIP() << "Game file not found: " << ff;
    auto container = std::make_unique<d2res::MqdbContainer>(d2res::MqdbContainer::open(ff));
    auto maps = std::make_unique<d2res::OptMaps>(d2res::parse_opt_maps(*container));
    d2res::ImageResourceDecoder decoder(*container, *maps);

    const auto names = decoder.list_images();
    EXPECT_EQ(names.size(), 65009u);

    auto img = decoder.decode_image("XE");
    EXPECT_EQ(img.width, 800u);
    EXPECT_EQ(img.height, 600u);
    EXPECT_EQ(img.rgba.size(), 800u * 600u * 4u);

    EXPECT_TRUE(img.metadata.contains("logical_name"));
    EXPECT_TRUE(img.metadata.contains("source_container"));
    EXPECT_TRUE(img.metadata.contains("base_image"));
    EXPECT_TRUE(img.metadata.contains("palette_id"));
    EXPECT_TRUE(img.metadata.contains("transparency_mode"));
    EXPECT_TRUE(img.metadata.contains("transparent_colors_bgr"));
    EXPECT_TRUE(img.metadata.contains("parts"));
    EXPECT_TRUE(img.metadata.contains("output_size"));

    auto img2 = decoder.decode_image("XE");
    EXPECT_EQ(img.width, img2.width);
    EXPECT_EQ(img.height, img2.height);
}
