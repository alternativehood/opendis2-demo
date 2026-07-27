#include <gtest/gtest.h>
#include "d2res/rgba_buffer.hpp"
#include "d2res/byte_reader.hpp"
#include "d2res/opt_images.hpp"
#include <lodepng.h>
#include <array>
#include <vector>

using namespace d2res;

static std::vector<uint8_t> make_1x1_png(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    std::vector<uint8_t> const rgba_in = {r, g, b, a};
    std::vector<uint8_t>       png_out;
    const unsigned             err = lodepng::encode(png_out, rgba_in, 1, 1);
    EXPECT_EQ(err, 0u);
    return png_out;
}

// Build a 1×1 RgbaBuffer directly (no PNG encode/decode round-trip).
static RgbaBuffer make_1x1_buf(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    RgbaBuffer buf;
    buf.width = 1;
    buf.height = 1;
    buf.rgba = {r, g, b, a};
    return buf;
}

// Build a single-piece ImageFrame covering a 1×1 base at (0,0).
static ImageFrame make_1x1_frame() {
    ImagePiece p;
    p.base_x = 0;
    p.base_y = 0;
    p.output_x = 0;
    p.output_y = 0;
    p.width = 1;
    p.height = 1;
    ImageFrame f;
    f.output_width = 1;
    f.output_height = 1;
    f.pieces = {p};
    return f;
}

static const std::array<std::array<uint8_t, 4>, 256> kDefaultPalette{};

TEST(DecodeBasePng, CorrectDimensionsFor1x1) {
    auto png = make_1x1_png(255, 0, 255, 255);
    auto buf = decode_base_png(std::span<const uint8_t>(png));
    EXPECT_EQ(buf.width, 1u);
    EXPECT_EQ(buf.height, 1u);
    EXPECT_EQ(buf.rgba.size(), 4u);
}

TEST(DecodeBasePng, CorrectPixelForMagenta) {
    auto png = make_1x1_png(255, 0, 255, 255);
    auto buf = decode_base_png(std::span<const uint8_t>(png));
    ASSERT_EQ(buf.rgba.size(), 4u);
    EXPECT_EQ(buf.rgba[0], 255u); // R
    EXPECT_EQ(buf.rgba[1], 0u);   // G
    EXPECT_EQ(buf.rgba[2], 255u); // B
    EXPECT_EQ(buf.rgba[3], 255u); // A
}

TEST(DecodeBasePng, ThrowsOnCorruptData) {
    const std::vector<uint8_t> garbage = {0x00, 0x11, 0x22, 0x33, 0x44};
    EXPECT_THROW(decode_base_png(std::span<const uint8_t>(garbage)), ParseError);
}

TEST(DecodeBasePng, ThrowsOnEmptySpan) {
    EXPECT_THROW(decode_base_png(std::span<const uint8_t>{}), ParseError);
}

// ---------------------------------------------------------------------------
// compose_image — additive-blend transparency (opacity_algorithm 300 / 400)
// ---------------------------------------------------------------------------

TEST(AdditiveBlend, BlackPixelBecomesTransparent) {
    // Black pixel: R=0, G=0, B=0 → max(0,0,0)=0 → alpha=0
    RgbaBuffer const base = make_1x1_buf(0, 0, 0, 255);
    ImageFrame const frame = make_1x1_frame();
    RgbaBuffer       out = compose_image(frame, base, 0, kDefaultPalette, 300);
    ASSERT_EQ(out.rgba.size(), 4u);
    EXPECT_EQ(out.rgba[3], 0u);
    EXPECT_EQ(out.rgba[0], 0u);
    EXPECT_EQ(out.rgba[1], 0u);
    EXPECT_EQ(out.rgba[2], 0u);
}

TEST(AdditiveBlend, WhitePixelStaysOpaque) {
    // White pixel: R=255, G=255, B=255 → max=255 → alpha=255
    RgbaBuffer const base = make_1x1_buf(255, 255, 255, 255);
    ImageFrame const frame = make_1x1_frame();
    RgbaBuffer       out = compose_image(frame, base, 0, kDefaultPalette, 300);
    ASSERT_EQ(out.rgba.size(), 4u);
    EXPECT_EQ(out.rgba[3], 255u);
    EXPECT_EQ(out.rgba[0], 255u);
    EXPECT_EQ(out.rgba[1], 255u);
    EXPECT_EQ(out.rgba[2], 255u);
}

TEST(AdditiveBlend, MidBrightPixelGetsPartialAlpha) {
    // Pixel (128, 64, 0): max=128 → alpha=128, RGB preserved
    RgbaBuffer const base = make_1x1_buf(128, 64, 0, 255);
    ImageFrame const frame = make_1x1_frame();
    RgbaBuffer       out = compose_image(frame, base, 0, kDefaultPalette, 300);
    ASSERT_EQ(out.rgba.size(), 4u);
    EXPECT_EQ(out.rgba[3], 128u);
    EXPECT_EQ(out.rgba[0], 128u);
    EXPECT_EQ(out.rgba[1], 64u);
    EXPECT_EQ(out.rgba[2], 0u);
}

TEST(AdditiveBlend, Opacity400AlsoUsesAdditivePath) {
    // opacity_algorithm=400 must also trigger the additive-blend pass
    RgbaBuffer const base = make_1x1_buf(0, 0, 0, 255);
    ImageFrame const frame = make_1x1_frame();
    RgbaBuffer       out = compose_image(frame, base, 0, kDefaultPalette, 400);
    ASSERT_EQ(out.rgba.size(), 4u);
    EXPECT_EQ(out.rgba[3], 0u);
}
