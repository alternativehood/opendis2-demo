#include <gtest/gtest.h>
#include "d2res/rgba_buffer.hpp"
#include "d2res/opt_images.hpp"
#include <array>
#include <cstring>

using namespace d2res;

static std::array<std::array<uint8_t, 4>, 256> empty_palette() {
    std::array<std::array<uint8_t, 4>, 256> p{};
    return p;
}

static RgbaBuffer make_base(uint32_t w, uint32_t h, uint8_t fill_r, uint8_t fill_g,
                            uint8_t fill_b) {
    RgbaBuffer b;
    b.width = w;
    b.height = h;
    b.rgba.resize(static_cast<std::size_t>(w) * h * 4);
    for (std::size_t i = 0; i < b.rgba.size(); i += 4) {
        b.rgba[i + 0] = fill_r;
        b.rgba[i + 1] = fill_g;
        b.rgba[i + 2] = fill_b;
        b.rgba[i + 3] = 255;
    }
    return b;
}

static ImageFrame make_frame(int ow, int oh, std::vector<ImagePiece> pieces) {
    ImageFrame f;
    f.name = "test";
    f.output_width = ow;
    f.output_height = oh;
    f.pieces = std::move(pieces);
    return f;
}

TEST(ComposeImage, SingleFullFramePieceCorrectSize) {
    auto base = make_base(10, 20, 100, 150, 200);
    auto frame = make_frame(
        10, 20,
        {{.output_x = 0, .output_y = 0, .base_x = 0, .base_y = 0, .width = 10, .height = 20}});
    auto out = compose_image(frame, base, 0, empty_palette(), 0);
    EXPECT_EQ(out.width, 10u);
    EXPECT_EQ(out.height, 20u);
    EXPECT_EQ(out.rgba.size(), 10u * 20u * 4u);
}

TEST(ComposeImage, PieceBlitsCopiesCorrectPixel) {
    // Base 5×5 with a distinctive pixel at (2,3) = red
    RgbaBuffer base;
    base.width = 5;
    base.height = 5;
    base.rgba.assign(std::size_t{5} * 5U * 4U, 0);
    uint8_t* px = &base.rgba[static_cast<std::size_t>((3U * 5U) + 2U) * 4U];
    px[0] = 200;
    px[1] = 10;
    px[2] = 30;
    px[3] = 255;

    // Piece: copy (2,3,1,1) → dest (0,0)
    auto frame = make_frame(
        5, 5, {{.output_x = 0, .output_y = 0, .base_x = 2, .base_y = 3, .width = 1, .height = 1}});
    auto out = compose_image(frame, base, 0, empty_palette(), 0);

    EXPECT_EQ(out.rgba[0], 200u);
    EXPECT_EQ(out.rgba[1], 10u);
    EXPECT_EQ(out.rgba[2], 30u);
    EXPECT_EQ(out.rgba[3], 255u);
}

TEST(ComposeImage, OutOfBoundsPieceSkippedNoCrash) {
    auto base = make_base(4, 4, 50, 60, 70);
    // Piece src rect (3, 3, 3, 3) would exceed base width/height of 4
    auto frame = make_frame(
        4, 4, {{.output_x = 0, .output_y = 0, .base_x = 3, .base_y = 3, .width = 3, .height = 3}});
    EXPECT_NO_THROW({
        auto out = compose_image(frame, base, 0, empty_palette(), 0);
        EXPECT_EQ(out.width, 4u);
        EXPECT_EQ(out.height, 4u);
    });
}

TEST(ComposeImage, MagentaPixelBecomesTransparent) {
    // Base with a magenta pixel at (0,0): R=255, G=0, B=255
    RgbaBuffer base;
    base.width = 1;
    base.height = 1;
    base.rgba = {255, 0, 255, 255};

    auto frame = make_frame(
        1, 1, {{.output_x = 0, .output_y = 0, .base_x = 0, .base_y = 0, .width = 1, .height = 1}});
    auto out = compose_image(frame, base, 0, empty_palette(), 1);
    EXPECT_EQ(out.rgba[0], 0u) << "Magenta pixel R zeroed";
    EXPECT_EQ(out.rgba[1], 0u) << "Magenta pixel G zeroed";
    EXPECT_EQ(out.rgba[2], 0u) << "Magenta pixel B zeroed";
    EXPECT_EQ(out.rgba[3], 0u) << "Magenta pixel alpha=0";
}

TEST(ComposeImage, NearMagentaAlsoBecomesTransparent) {
    // R=255, G=0, B=254 — within ±1 range
    RgbaBuffer base;
    base.width = 1;
    base.height = 1;
    base.rgba = {255, 0, 254, 255};

    auto frame = make_frame(
        1, 1, {{.output_x = 0, .output_y = 0, .base_x = 0, .base_y = 0, .width = 1, .height = 1}});
    auto out = compose_image(frame, base, 0, empty_palette(), 1);
    EXPECT_EQ(out.rgba[0], 0u) << "Near-magenta R zeroed";
    EXPECT_EQ(out.rgba[1], 0u) << "Near-magenta G zeroed";
    EXPECT_EQ(out.rgba[2], 0u) << "Near-magenta B zeroed";
    EXPECT_EQ(out.rgba[3], 0u) << "Near-magenta alpha=0";
}

TEST(ComposeImage, NonMagentaPixelRetainsAlpha) {
    // R=100, G=150, B=200 — clearly not magenta
    RgbaBuffer base;
    base.width = 1;
    base.height = 1;
    base.rgba = {100, 150, 200, 255};

    auto frame = make_frame(
        1, 1, {{.output_x = 0, .output_y = 0, .base_x = 0, .base_y = 0, .width = 1, .height = 1}});
    auto out = compose_image(frame, base, 0, empty_palette(), 1);
    EXPECT_EQ(out.rgba[3], 255u) << "Non-magenta pixel should retain alpha=255";
}

TEST(ComposeImage, OpacityAlgorithmZeroDisablesTransparency) {
    // Magenta pixel but opacity_algorithm==0
    RgbaBuffer base;
    base.width = 1;
    base.height = 1;
    base.rgba = {255, 0, 255, 255};

    auto frame = make_frame(
        1, 1, {{.output_x = 0, .output_y = 0, .base_x = 0, .base_y = 0, .width = 1, .height = 1}});
    auto out = compose_image(frame, base, 0, empty_palette(), 0);
    EXPECT_EQ(out.rgba[3], 255u) << "With opacity_algorithm==0 alpha must not change";
}
