#include <gtest/gtest.h>

#include "d2res/dat_parser.hpp"

using namespace d2res;

TEST(DatParser, BasicKeyValue) {
    DatParser p;
    p.parse("FOO=bar\nBAZ=123");
    const auto& e = p.entries();
    ASSERT_EQ(e.size(), 2u);
    EXPECT_EQ(e.at("FOO"), "bar");
    EXPECT_EQ(e.at("BAZ"), "123");
}

TEST(DatParser, CommentSkipped) {
    DatParser p;
    p.parse("; this is a comment\nK=V");
    const auto& e = p.entries();
    ASSERT_EQ(e.size(), 1u);
    EXPECT_EQ(e.at("K"), "V");
}

TEST(DatParser, NoEqualsSkipped) {
    DatParser p;
    p.parse("NOEQUALSSIGN\nK=V");
    const auto& e = p.entries();
    ASSERT_EQ(e.size(), 1u);
    EXPECT_EQ(e.at("K"), "V");
}

TEST(DatParser, WhitespaceTrimmed) {
    DatParser p;
    p.parse("  K  =  V  ");
    const auto& e = p.entries();
    ASSERT_EQ(e.size(), 1u);
    EXPECT_EQ(e.at("K"), "V");
}

TEST(DatParser, ToJsonIsObject) {
    DatParser p;
    p.parse("KEY=val");
    auto j = p.to_json();
    EXPECT_TRUE(j.is_object());
    EXPECT_EQ(j["KEY"].get<std::string>(), "val");
}
