#include <gtest/gtest.h>
#include "d2res/hexdump.hpp"
#include <sstream>
#include <string>
#include <vector>

static std::string hexdump_str(const std::vector<uint8_t>& data, d2res::HexdumpOptions opts = {}) {
    std::ostringstream ss;
    d2res::write_hexdump(ss, std::span<const uint8_t>(data), opts);
    return ss.str();
}

TEST(Hexdump, HeaderRowPresent) {
    std::vector<uint8_t> const data(16, 0x41);
    auto                       out = hexdump_str(data);
    EXPECT_NE(out.find("Offset"), std::string::npos);
    EXPECT_NE(out.find("ASCII"), std::string::npos);
    EXPECT_NE(out.find("u32le"), std::string::npos);
}

TEST(Hexdump, EmptySpanOnlyHeader) {
    auto out = hexdump_str({});
    // Header must be present
    EXPECT_NE(out.find("Offset"), std::string::npos);
    // No data rows: no "0x000000"
    EXPECT_EQ(out.find("0x000000"), std::string::npos);
    // No truncation notice
    EXPECT_EQ(out.find("truncated"), std::string::npos);
}

TEST(Hexdump, CorrectOffsetsFor32Bytes) {
    std::vector<uint8_t> const data(32, 0x00);
    auto                       out = hexdump_str(data);
    EXPECT_NE(out.find("0x000000"), std::string::npos);
    EXPECT_NE(out.find("0x000010"), std::string::npos);
}

TEST(Hexdump, NonPrintableShowsDot) {
    // All non-printable bytes
    std::vector<uint8_t> const data(16, 0x01);
    auto                       out = hexdump_str(data);
    // ASCII column should contain dots
    EXPECT_NE(out.find("................"), std::string::npos);
}

TEST(Hexdump, PrintableShowsChar) {
    // 'A' = 0x41, printable
    std::vector<uint8_t> const data(16, 0x41);
    auto                       out = hexdump_str(data);
    EXPECT_NE(out.find("AAAAAAAAAAAAAAAA"), std::string::npos);
}

TEST(Hexdump, U32leDecodeCorrect) {
    // Bytes: 0x44 0x42 0x51 0x4D → little-endian u32 = 0x4D514244
    std::vector<uint8_t> const data = {0x44, 0x42, 0x51, 0x4D, 0x00, 0x00, 0x00, 0x00,
                                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    auto                       out = hexdump_str(data);
    EXPECT_NE(out.find("0x4D514244"), std::string::npos);
}

TEST(Hexdump, TruncationNotice) {
    std::vector<uint8_t> const data(100, 0xAB);
    d2res::HexdumpOptions      opts;
    opts.limit_bytes = 32;
    auto out = hexdump_str(data, opts);
    EXPECT_NE(out.find("truncated at 32 bytes"), std::string::npos);
    EXPECT_NE(out.find("total 100 bytes"), std::string::npos);
}

TEST(Hexdump, NoTruncationWhenDataFits) {
    std::vector<uint8_t> const data(16, 0x00);
    auto                       out = hexdump_str(data);
    EXPECT_EQ(out.find("truncated"), std::string::npos);
}
