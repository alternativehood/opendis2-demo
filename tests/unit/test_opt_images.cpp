#include <gtest/gtest.h>
#include "d2res/opt_images.hpp"
#include "d2res/byte_reader.hpp"
#include <vector>

// ---- helpers ---------------------------------------------------------------

static void write_i32(std::vector<uint8_t>& buf, int32_t v) {
    buf.push_back(static_cast<uint8_t>(v));
    buf.push_back(static_cast<uint8_t>(v >> 8));
    buf.push_back(static_cast<uint8_t>(v >> 16));
    buf.push_back(static_cast<uint8_t>(v >> 24));
}

static void write_i16(std::vector<uint8_t>& buf, int16_t v) {
    buf.push_back(static_cast<uint8_t>(v));
    buf.push_back(static_cast<uint8_t>(v >> 8));
}

static void write_cstr(std::vector<uint8_t>& buf, const std::string& s) {
    for (char c : s)
        buf.push_back(static_cast<uint8_t>(c));
    buf.push_back(0);
}

// Build a minimal -IMAGES.OPT payload with one block, one frame, one piece
static std::vector<uint8_t> build_single_block(uint8_t transparent_idx, int16_t opacity, int32_t sx,
                                               int32_t sy, const std::string& frame_name,
                                               int32_t out_w, int32_t out_h,
                                               // piece fields
                                               int32_t ox, int32_t oy, int32_t bx, int32_t by,
                                               int32_t pw, int32_t ph) {
    std::vector<uint8_t> buf;
    // Block header
    buf.push_back(transparent_idx);
    write_i16(buf, opacity);
    write_i32(buf, sx);
    write_i32(buf, sy);
    // Palette: 256 × 4 bytes, all zero
    buf.insert(buf.end(), 1024, 0);
    // framesCount = 1
    write_i32(buf, 1);
    // frame
    write_cstr(buf, frame_name);
    write_i32(buf, 1); // piecesCount
    write_i32(buf, out_w);
    write_i32(buf, out_h);
    // piece
    write_i32(buf, ox);
    write_i32(buf, oy);
    write_i32(buf, bx);
    write_i32(buf, by);
    write_i32(buf, pw);
    write_i32(buf, ph);
    return buf;
}

// ---- tests -----------------------------------------------------------------

TEST(OptImages, EmptyPayload) {
    std::vector<uint8_t> buf; // zero bytes
    auto                 map = d2res::ImagesParser::parse(std::span<const uint8_t>(buf));
    EXPECT_TRUE(map.blocks.empty());
    EXPECT_TRUE(map.frame_name_to_block.empty());
}

TEST(OptImages, SingleBlockRoundTrip) {
    auto buf = build_single_block(3, 255, 100, 200, "FRAME1", 800, 600, 10, 20, 30, 40, 50, 60);
    auto map = d2res::ImagesParser::parse(std::span<const uint8_t>(buf));
    ASSERT_EQ(map.blocks.size(), 1u);
    const auto& block = map.blocks[0];
    EXPECT_EQ(block.transparent_color_index, 3u);
    EXPECT_EQ(block.opacity_algorithm, 255);
    EXPECT_EQ(block.size_x, 100);
    EXPECT_EQ(block.size_y, 200);
    ASSERT_EQ(block.frames.size(), 1u);
    const auto& frame = block.frames[0];
    EXPECT_EQ(frame.name, "FRAME1");
    EXPECT_EQ(frame.output_width, 800);
    EXPECT_EQ(frame.output_height, 600);
    ASSERT_EQ(frame.pieces.size(), 1u);
    const auto& piece = frame.pieces[0];
    EXPECT_EQ(piece.output_x, 10);
    EXPECT_EQ(piece.output_y, 20);
    EXPECT_EQ(piece.base_x, 30);
    EXPECT_EQ(piece.base_y, 40);
    EXPECT_EQ(piece.width, 50);
    EXPECT_EQ(piece.height, 60);
}

TEST(OptImages, FrameNameIndexed) {
    auto buf = build_single_block(0, 0, 1, 1, "XE", 10, 10, 0, 0, 0, 0, 1, 1);
    auto map = d2res::ImagesParser::parse(std::span<const uint8_t>(buf));
    EXPECT_NE(map.frame_name_to_block.find("XE"), map.frame_name_to_block.end());
    EXPECT_EQ(map.frame_name_to_block.at("XE"), 0u);
}

TEST(OptImages, TruncatedHeaderThrows) {
    // Only 3 bytes — can't read block header
    std::vector<uint8_t> buf(3, 0xAB);
    EXPECT_THROW(d2res::ImagesParser::parse(std::span<const uint8_t>(buf)), d2res::ParseError);
}
