#include <gtest/gtest.h>
#include "d2res/opt_anims.hpp"
#include "d2res/byte_reader.hpp"
#include <vector>
#include <string>

// ---- helpers ---------------------------------------------------------------

static void write_i32(std::vector<uint8_t>& buf, int32_t v) {
    buf.push_back(static_cast<uint8_t>(v));
    buf.push_back(static_cast<uint8_t>(v >> 8));
    buf.push_back(static_cast<uint8_t>(v >> 16));
    buf.push_back(static_cast<uint8_t>(v >> 24));
}

static void write_cstr(std::vector<uint8_t>& buf, const std::string& s) {
    for (char c : s)
        buf.push_back(static_cast<uint8_t>(c));
    buf.push_back(0);
}

// ---- tests -----------------------------------------------------------------

TEST(OptAnims, EmptyPayload) {
    std::vector<uint8_t>           buf;
    std::vector<std::string> const names;
    auto map = d2res::AnimsParser::parse(std::span<const uint8_t>(buf), names);
    EXPECT_TRUE(map.blocks.empty());
}

TEST(OptAnims, SingleBlockTwoFrames) {
    std::vector<uint8_t> buf;
    write_i32(buf, 2); // framesCount
    write_cstr(buf, "FRAMEA");
    write_cstr(buf, "FRAMEB");

    std::vector<std::string> const names = {"ANIM1"};
    auto map = d2res::AnimsParser::parse(std::span<const uint8_t>(buf), names);
    ASSERT_EQ(map.blocks.size(), 1u);
    EXPECT_EQ(map.blocks[0].name, "ANIM1");
    ASSERT_EQ(map.blocks[0].frames.size(), 2u);
    EXPECT_EQ(map.blocks[0].frames[0], "FRAMEA");
    EXPECT_EQ(map.blocks[0].frames[1], "FRAMEB");
}

TEST(OptAnims, NameLookup) {
    std::vector<uint8_t> buf;
    write_i32(buf, 1);
    write_cstr(buf, "F1");

    std::vector<std::string> const names = {"MYANIM"};
    auto map = d2res::AnimsParser::parse(std::span<const uint8_t>(buf), names);
    EXPECT_NE(map.name_to_block.find("MYANIM"), map.name_to_block.end());
}

TEST(OptAnims, MismatchedNameCountThrows) {
    // Two blocks in data but only one name supplied
    std::vector<uint8_t> buf;
    write_i32(buf, 1);
    write_cstr(buf, "F1"); // block 0
    write_i32(buf, 1);
    write_cstr(buf, "F2"); // block 1

    std::vector<std::string> const names = {"ANIM0"}; // only 1 name, but 2 blocks
    EXPECT_THROW(d2res::AnimsParser::parse(std::span<const uint8_t>(buf), names),
                 d2res::ParseError);
}
