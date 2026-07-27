#include <gtest/gtest.h>
#include "d2res/gif_encoder.hpp"
#include "d2res/image_decoder.hpp"
#include <filesystem>
#include <fstream>
#include <vector>

using namespace d2res;
namespace fs = std::filesystem;

static DecodedImage make_solid_frame(uint32_t w, uint32_t h, uint8_t r, uint8_t g, uint8_t b) {
    DecodedImage img;
    img.logical_name = "test";
    img.width = w;
    img.height = h;
    img.rgba.assign(static_cast<std::size_t>(w) * h * 4, 0);
    for (std::size_t i = 0; i + 3 < img.rgba.size(); i += 4) {
        img.rgba[i + 0] = r;
        img.rgba[i + 1] = g;
        img.rgba[i + 2] = b;
        img.rgba[i + 3] = 255;
    }
    return img;
}

TEST(GifEncoder, WritesNonEmptyFile) {
    const auto                      out = fs::temp_directory_path() / "test_gifenc.gif";
    std::vector<DecodedImage> const frames = {
        make_solid_frame(4, 4, 255, 0, 0),
        make_solid_frame(4, 4, 0, 255, 0),
    };
    ASSERT_NO_THROW(encode_gif(frames, out, 100));
    EXPECT_TRUE(fs::exists(out));
    EXPECT_GT(fs::file_size(out), 0u);
}

TEST(GifEncoder, FileStartsWithGifSignature) {
    const auto                      out = fs::temp_directory_path() / "test_gifsig.gif";
    std::vector<DecodedImage> const frames = {make_solid_frame(2, 2, 100, 100, 100)};
    encode_gif(frames, out, 100);
    std::ifstream f(out, std::ios::binary);
    ASSERT_TRUE(f.is_open());
    char hdr[3];
    f.read(hdr, 3);
    EXPECT_EQ(hdr[0], 'G');
    EXPECT_EQ(hdr[1], 'I');
    EXPECT_EQ(hdr[2], 'F');
}

TEST(GifEncoder, ThrowsOnEmptyFrames) {
    const auto out = fs::temp_directory_path() / "test_gifempty.gif";
    EXPECT_THROW(encode_gif({}, out, 100), std::runtime_error);
}
