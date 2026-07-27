#include <gtest/gtest.h>
#include "d2res/extractor/png_decode.hpp"
#include <lodepng.h>
#include <vector>

using namespace d2res;

static std::vector<uint8_t> make_1x1_png(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    std::vector<uint8_t> const rgba_in = {r, g, b, a};
    std::vector<uint8_t>       png_out;
    const unsigned             err = lodepng::encode(png_out, rgba_in, 1, 1);
    EXPECT_EQ(err, 0u);
    return png_out;
}

TEST(PngDecode, Decodes1x1Png) {
    auto png = make_1x1_png(255, 0, 255, 255);
    auto result = decode_png_data(png);
    EXPECT_EQ(result.width, 1u);
    EXPECT_EQ(result.height, 1u);
    ASSERT_EQ(result.rgba.size(), 4u);
    EXPECT_EQ(result.rgba[0], 255u);
    EXPECT_EQ(result.rgba[1], 0u);
    EXPECT_EQ(result.rgba[2], 255u);
    EXPECT_EQ(result.rgba[3], 255u);
}

TEST(PngDecode, Decodes2x2Png) {
    std::vector<uint8_t> const rgba_in = {
        255, 0,   0,   255, // (0,0) red
        0,   255, 0,   255, // (1,0) green
        0,   0,   255, 255, // (0,1) blue
        255, 255, 0,   255  // (1,1) yellow
    };
    std::vector<uint8_t> png_out;
    const unsigned       err = lodepng::encode(png_out, rgba_in, 2, 2);
    EXPECT_EQ(err, 0u);

    auto result = decode_png_data(png_out);
    EXPECT_EQ(result.width, 2u);
    EXPECT_EQ(result.height, 2u);
    ASSERT_EQ(result.rgba.size(), 16u);
}

TEST(PngDecode, ThrowsOnInvalidBytes) {
    const std::vector<uint8_t> garbage = {0x00, 0x11, 0x22, 0x33, 0x44};
    auto thrower = [&]() -> void { static_cast<void>(decode_png_data(garbage)); };
    EXPECT_THROW(thrower(), std::runtime_error);
}

TEST(PngDecode, ThrowsOnEmptyData) {
    const std::vector<uint8_t> empty;
    auto thrower = [&]() -> void { static_cast<void>(decode_png_data(empty)); };
    EXPECT_THROW(thrower(), std::runtime_error);
}

TEST(PngDecodeFile, ThrowsOnMissingFile) {
    auto thrower = [&]() -> void {
        static_cast<void>(decode_png_file("/nonexistent/path/to/file.png"));
    };
    EXPECT_THROW(thrower(), std::runtime_error);
}
