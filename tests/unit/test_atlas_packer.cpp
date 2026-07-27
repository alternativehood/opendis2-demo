#include <gtest/gtest.h>
#include "d2res/atlas_packer.hpp"

using namespace d2res;

static RgbaBuffer make_sprite(uint32_t w, uint32_t h, uint8_t r, uint8_t g, uint8_t b,
                              uint8_t a = 255) {
    RgbaBuffer buf;
    buf.width = w;
    buf.height = h;
    buf.rgba.resize(static_cast<std::size_t>(w) * h * 4U);
    for (std::size_t i = 0; i < static_cast<std::size_t>(w) * h; ++i) {
        buf.rgba[(i * 4) + 0] = r;
        buf.rgba[(i * 4) + 1] = g;
        buf.rgba[(i * 4) + 2] = b;
        buf.rgba[(i * 4) + 3] = a;
    }
    return buf;
}

TEST(AtlasPacker, SingleSpriteOnOneSheet) {
    auto result = d2res::AtlasPacker::pack({{"spr", make_sprite(64, 64, 255, 0, 0)}}, 512);
    ASSERT_EQ(result.sheets.size(), 1u);
    ASSERT_EQ(result.entries.size(), 1u);
    EXPECT_EQ(result.entries[0].sheet, 0);
    EXPECT_EQ(result.entries[0].x, 0);
    EXPECT_EQ(result.entries[0].y, 0);
    EXPECT_EQ(result.entries[0].w, 64);
    EXPECT_EQ(result.entries[0].h, 64);
    EXPECT_TRUE(result.skipped.empty());
}

TEST(AtlasPacker, PixelBlit) {
    // 2×2 sprite with distinct pixels
    RgbaBuffer buf;
    buf.width = 2;
    buf.height = 2;
    buf.rgba = {
        0xFF, 0x00, 0x00, 0xFF, // (0,0) red
        0x00, 0xFF, 0x00, 0xFF, // (1,0) green
        0x00, 0x00, 0xFF, 0xFF, // (0,1) blue
        0xFF, 0xFF, 0x00, 0xFF, // (1,1) yellow
    };
    auto result = d2res::AtlasPacker::pack({{"px", buf}}, 512);
    ASSERT_EQ(result.entries.size(), 1u);
    const auto&       e = result.entries[0];
    const auto&       canvas = result.sheets[0].canvas.rgba;
    const std::size_t W = result.sheets[0].canvas.width;
    auto              at = [&](int row, int col, int ch) {
        std::size_t const idx =
            (((static_cast<std::size_t>(e.y + row) * W) + static_cast<std::size_t>(e.x + col)) *
             4U) +
            static_cast<std::size_t>(ch);
        return canvas[idx];
    };
    // row 0
    EXPECT_EQ(at(0, 0, 0), 0xFF); // red R
    EXPECT_EQ(at(0, 1, 1), 0xFF); // green G
    // row 1
    EXPECT_EQ(at(1, 0, 2), 0xFF); // blue B
    EXPECT_EQ(at(1, 1, 0), 0xFF); // yellow R
    EXPECT_EQ(at(1, 1, 1), 0xFF); // yellow G
}

TEST(AtlasPacker, NoOverlap) {
    std::vector<std::pair<std::string, RgbaBuffer>> sprites;
    sprites.reserve(20U);
    for (uint32_t i = 0; i < 20U; ++i) {
        sprites.emplace_back(std::to_string(i),
                             make_sprite(30U + (i * 2U), 20U + i, 128, 128, 128));
    }
    auto result = d2res::AtlasPacker::pack(sprites, 512);

    // Check no two entries on the same sheet overlap
    for (std::size_t i = 0; i < result.entries.size(); ++i) {
        for (std::size_t j = i + 1; j < result.entries.size(); ++j) {
            const auto& a = result.entries[i];
            const auto& b = result.entries[j];
            if (a.sheet != b.sheet)
                continue;
            bool const overlap =
                a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
            EXPECT_FALSE(overlap) << "Overlap: " << a.name << " and " << b.name;
        }
    }
}

TEST(AtlasPacker, SheetOverflow) {
    // 10 sprites of 200×200, max_size=512: 4 per sheet (2×2 per shelf arrangement), needs 3 sheets
    std::vector<std::pair<std::string, RgbaBuffer>> sprites;
    sprites.reserve(10);
    for (int i = 0; i < 10; ++i)
        sprites.emplace_back(std::to_string(i), make_sprite(200, 200, 0, 0, 0));
    auto result = d2res::AtlasPacker::pack(sprites, 512);
    EXPECT_GT(result.sheets.size(), 1u);
    EXPECT_EQ(result.entries.size(), 10u);
}

TEST(AtlasPacker, OversizedSpriteSkipped) {
    auto result = d2res::AtlasPacker::pack({{"big", make_sprite(600, 600, 0, 0, 0)}}, 512);
    EXPECT_EQ(result.entries.size(), 0u);
    ASSERT_EQ(result.skipped.size(), 1u);
    EXPECT_NE(result.skipped[0].find("big"), std::string::npos);
}

TEST(AtlasPacker, TransparentPadding) {
    auto result = d2res::AtlasPacker::pack(
        {{"a", make_sprite(10, 10, 255, 0, 0)}, {"b", make_sprite(10, 10, 0, 255, 0)}}, 128);
    ASSERT_EQ(result.sheets.size(), 1u);
    ASSERT_EQ(result.entries.size(), 2u);
    const auto& canvas = result.sheets[0].canvas;
    // Padding between packed sprites should be transparent
    const int x1 = result.entries[0].x + result.entries[0].w;
    const int x2 = result.entries[1].x;
    if (x1 < x2 && x2 < static_cast<int>(canvas.width)) {
        std::size_t const idx =
            (static_cast<std::size_t>(0) * static_cast<std::size_t>(canvas.width) +
             static_cast<std::size_t>(x1)) *
            4;
        EXPECT_EQ(canvas.rgba[idx + 3], 0u) << "Padding pixel should be alpha=0";
    }
}

TEST(AtlasPacker, ManifestJsonKeys) {
    auto result = d2res::AtlasPacker::pack({{"spr", make_sprite(8, 8, 0, 0, 0)}}, 64);
    auto j = result.to_json("test.ff", 64);
    EXPECT_TRUE(j.contains("source_container"));
    EXPECT_TRUE(j.contains("max_sheet_size"));
    EXPECT_TRUE(j.contains("sheet_count"));
    EXPECT_TRUE(j.contains("total_sprites"));
    EXPECT_TRUE(j.contains("skipped_sprites"));
    EXPECT_TRUE(j.contains("entries"));
    EXPECT_EQ(j["total_sprites"].get<int>(), 1);
    EXPECT_EQ(j["entries"].size(), 1u);
}

TEST(AtlasPacker, SingleSmallSpriteHasTightCanvas) {
    auto result = d2res::AtlasPacker::pack({{"s", make_sprite(64, 64, 255, 0, 0)}}, 4096);
    ASSERT_EQ(result.sheets.size(), 1u);
    EXPECT_EQ(result.sheets[0].canvas.width, 64u);
    EXPECT_EQ(result.sheets[0].canvas.height, 64u);
    EXPECT_EQ(result.sheets[0].canvas.rgba.size(), 64u * 64 * 4);
}

TEST(AtlasPacker, MultipleSpritesCanvasMatchesPackedBounds) {
    auto result = d2res::AtlasPacker::pack(
        {{"a", make_sprite(50, 30, 255, 0, 0)}, {"b", make_sprite(30, 50, 0, 255, 0)}}, 4096);
    ASSERT_EQ(result.sheets.size(), 1u);
    ASSERT_EQ(result.entries.size(), 2u);
    int max_r = 0, max_b = 0;
    for (const auto& e : result.entries) {
        if (e.x + e.w > max_r)
            max_r = e.x + e.w;
        if (e.y + e.h > max_b)
            max_b = e.y + e.h;
    }
    EXPECT_EQ(result.sheets[0].canvas.width, static_cast<uint32_t>(max_r));
    EXPECT_EQ(result.sheets[0].canvas.height, static_cast<uint32_t>(max_b));
    EXPECT_LE(result.sheets[0].canvas.width, 4096u);
    EXPECT_LE(result.sheets[0].canvas.height, 4096u);
}
