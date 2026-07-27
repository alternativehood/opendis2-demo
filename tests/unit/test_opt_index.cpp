#include <gtest/gtest.h>
#include "d2res/opt_index.hpp"
#include "d2res/byte_reader.hpp"
#include <vector>

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

struct IndexPayloadBuilder {
    std::vector<std::tuple<int32_t, std::string, int32_t, int32_t>> entries;

    [[nodiscard]] std::vector<uint8_t> build() const {
        std::vector<uint8_t> buf;
        write_i32(buf, static_cast<int32_t>(entries.size()));
        for (const auto& [id, name, offset, sz] : entries) {
            write_i32(buf, id);
            write_cstr(buf, name);
            write_i32(buf, offset);
            write_i32(buf, sz);
        }
        return buf;
    }
};

// ---- tests -----------------------------------------------------------------

TEST(OptIndex, EmptyPayload) {
    std::vector<uint8_t> buf;
    write_i32(buf, 0); // framesCount = 0
    auto map = d2res::IndexParser::parse(std::span<const uint8_t>(buf));
    EXPECT_TRUE(map.entries.empty());
    EXPECT_TRUE(map.name_to_index.empty());
    EXPECT_TRUE(map.warnings.empty());
}

TEST(OptIndex, ImageEntry) {
    IndexPayloadBuilder b;
    b.entries.emplace_back(1234, "MYFRAME", 100, 50);
    auto buf = b.build();
    auto map = d2res::IndexParser::parse(std::span<const uint8_t>(buf));
    ASSERT_EQ(map.entries.size(), 1u);
    EXPECT_EQ(map.entries[0].id, 1234);
    EXPECT_EQ(map.entries[0].name, "MYFRAME");
    EXPECT_EQ(map.entries[0].related_offset, 100);
    EXPECT_EQ(map.entries[0].size, 50);
    EXPECT_FALSE(map.entries[0].is_animation());
}

TEST(OptIndex, AnimationEntry) {
    IndexPayloadBuilder b;
    b.entries.emplace_back(-1, "MYANIM", 200, 60);
    auto buf = b.build();
    auto map = d2res::IndexParser::parse(std::span<const uint8_t>(buf));
    ASSERT_EQ(map.entries.size(), 1u);
    EXPECT_EQ(map.entries[0].id, -1);
    EXPECT_TRUE(map.entries[0].is_animation());
}

TEST(OptIndex, NameLookupCaseInsensitive) {
    IndexPayloadBuilder b;
    b.entries.emplace_back(10, "Hello", 0, 1);
    auto buf = b.build();
    auto map = d2res::IndexParser::parse(std::span<const uint8_t>(buf));
    EXPECT_NE(map.name_to_index.find("HELLO"), map.name_to_index.end());
    EXPECT_EQ(map.name_to_index.count("hello"), 0u); // stored as uppercase only
}

TEST(OptIndex, DuplicateNameFirstWins) {
    IndexPayloadBuilder b;
    b.entries.emplace_back(10, "DUP", 0, 1);
    b.entries.emplace_back(20, "DUP", 5, 2);
    auto buf = b.build();
    auto map = d2res::IndexParser::parse(std::span<const uint8_t>(buf));
    ASSERT_EQ(map.entries.size(), 2u);
    // First entry wins in name_to_index
    auto it = map.name_to_index.find("DUP");
    ASSERT_NE(it, map.name_to_index.end());
    EXPECT_EQ(map.entries[it->second].id, 10);
    EXPECT_FALSE(map.warnings.empty());
}

TEST(OptIndex, TruncatedPayloadThrows) {
    // framesCount=2 but only 1 entry's worth of data
    IndexPayloadBuilder b;
    b.entries.emplace_back(1, "A", 0, 1);
    auto buf = b.build();
    // Overwrite framesCount to 2 (but only 1 entry in buf)
    buf[0] = 2;
    buf[1] = 0;
    buf[2] = 0;
    buf[3] = 0;
    EXPECT_THROW(d2res::IndexParser::parse(std::span<const uint8_t>(buf)), d2res::ParseError);
}
