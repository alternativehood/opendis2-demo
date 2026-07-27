#include <gtest/gtest.h>

#include "d2res/png_metadata.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace {

void push_u32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>((value >> 24U) & 0xFFU));
    out.push_back(static_cast<uint8_t>((value >> 16U) & 0xFFU));
    out.push_back(static_cast<uint8_t>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<uint8_t>(value & 0xFFU));
}

void push_u16(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(static_cast<uint8_t>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<uint8_t>(value & 0xFFU));
}

void chunk(std::vector<uint8_t>& out, const std::string& type, const std::vector<uint8_t>& data) {
    push_u32(out, static_cast<uint32_t>(data.size()));
    out.insert(out.end(), type.begin(), type.end());
    out.insert(out.end(), data.begin(), data.end());
    push_u32(out, 0);
}

std::vector<uint8_t> png_start() {
    return {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
}

std::vector<uint8_t> ihdr(uint32_t width, uint32_t height) {
    std::vector<uint8_t> data;
    push_u32(data, width);
    push_u32(data, height);
    data.insert(data.end(), {8, 6, 0, 0, 0});
    return data;
}

} // namespace

TEST(PngMetadata, ReadsPlainPngIhdr) {
    auto payload = png_start();
    chunk(payload, "IHDR", ihdr(12, 34));
    chunk(payload, "IEND", {});

    const auto metadata = d2res::scan_png_metadata(payload);

    ASSERT_TRUE(metadata.is_png);
    ASSERT_TRUE(metadata.has_ihdr);
    EXPECT_EQ(metadata.width, 12u);
    EXPECT_EQ(metadata.height, 34u);
    EXPECT_FALSE(metadata.has_actl);
    EXPECT_TRUE(metadata.fctl.empty());
}

TEST(PngMetadata, ReadsApngCandidateChunks) {
    auto payload = png_start();
    chunk(payload, "IHDR", ihdr(12, 34));
    std::vector<uint8_t> actl;
    push_u32(actl, 2);
    push_u32(actl, 0);
    chunk(payload, "acTL", actl);
    std::vector<uint8_t> fctl;
    push_u32(fctl, 1);
    push_u32(fctl, 10);
    push_u32(fctl, 20);
    push_u32(fctl, 3);
    push_u32(fctl, 4);
    push_u16(fctl, 1);
    push_u16(fctl, 30);
    fctl.insert(fctl.end(), {0, 1});
    chunk(payload, "fcTL", fctl);
    chunk(payload, "IEND", {});

    const auto metadata = d2res::scan_png_metadata(payload);

    EXPECT_TRUE(metadata.has_actl);
    ASSERT_EQ(metadata.fctl.size(), 1u);
    EXPECT_EQ(metadata.fctl[0].width, 10u);
    EXPECT_EQ(metadata.fctl[0].height, 20u);
    EXPECT_EQ(metadata.fctl[0].x_offset, 3u);
}

TEST(PngMetadata, PreservesUnknownChunksAndTextKeys) {
    auto payload = png_start();
    chunk(payload, "IHDR", ihdr(1, 1));
    chunk(payload, "zzZZ", {1, 2, 3});
    chunk(payload, "tEXt", {'A', 'u', 't', 'h', 'o', 'r', 0, 'x'});
    chunk(payload, "IEND", {});

    const auto metadata = d2res::scan_png_metadata(payload);

    EXPECT_EQ(metadata.unknown_chunks, (std::vector<std::string>{"zzZZ"}));
    ASSERT_EQ(metadata.text_keys.size(), 1u);
    EXPECT_EQ(metadata.text_keys[0].key, "Author");
}

TEST(PngMetadata, InvalidAndTruncatedPayloadsWarn) {
    const auto invalid = d2res::scan_png_metadata(std::vector<uint8_t>{1, 2, 3});
    EXPECT_FALSE(invalid.is_png);
    ASSERT_FALSE(invalid.warnings.empty());

    auto truncated = png_start();
    truncated.insert(truncated.end(), {0, 0, 0});
    const auto metadata = d2res::scan_png_metadata(truncated);
    EXPECT_TRUE(metadata.is_png);
    ASSERT_FALSE(metadata.warnings.empty());
}
