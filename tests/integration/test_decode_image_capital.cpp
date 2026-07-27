#include <gtest/gtest.h>
#include "d2res/mqdb.hpp"
#include "d2res/opt_maps.hpp"
#include "d2res/image_decoder.hpp"
#include <filesystem>

#ifndef DISCIPLES2_GAME_ROOT
#define DISCIPLES2_GAME_ROOT ""
#endif

static const std::filesystem::path GAME_ROOT{DISCIPLES2_GAME_ROOT};

TEST(DecodeImageCapital, DecodeAndValidateCapitalImage) {
    const auto ff = GAME_ROOT / "Imgs/Capital.ff";
    if (!std::filesystem::exists(ff))
        GTEST_SKIP() << "Game file not found: " << ff;
    auto container = std::make_unique<d2res::MqdbContainer>(d2res::MqdbContainer::open(ff));
    auto maps = std::make_unique<d2res::OptMaps>(d2res::parse_opt_maps(*container));
    d2res::ImageResourceDecoder decoder(*container, *maps);

    auto img = decoder.decode_image("EMP_IMPGUILD");
    EXPECT_GT(img.rgba.size(), 0u);
    EXPECT_GT(img.width, 0u);
    EXPECT_GT(img.height, 0u);

    ASSERT_TRUE(img.metadata.contains("output_size"));
    const auto& os = img.metadata["output_size"];
    EXPECT_EQ(os["w"].get<uint32_t>(), img.width);
    EXPECT_EQ(os["h"].get<uint32_t>(), img.height);
}
