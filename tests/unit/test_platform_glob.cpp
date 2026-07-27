#include <gtest/gtest.h>
#include "d2res/platform/glob.hpp"

using d2res::platform::glob_match;

TEST(PlatformGlob, AsteriskMatchesSubstring) {
    EXPECT_TRUE(glob_match("DLG_H_*", "DLG_H_ARCHER"));
    EXPECT_TRUE(glob_match("*", "anything"));
    EXPECT_TRUE(glob_match("*", ""));
}

TEST(PlatformGlob, CaseInsensitive) {
    EXPECT_TRUE(glob_match("dlg_h_*", "DLG_H_ARCHER"));
    EXPECT_TRUE(glob_match("DLG_H_*", "dlg_h_archer"));
    EXPECT_TRUE(glob_match("ITEM_*", "item_sword"));
}

TEST(PlatformGlob, NonMatch) {
    EXPECT_FALSE(glob_match("UNIT_*", "DLG_H_ARCHER"));
    EXPECT_FALSE(glob_match("ABC", "ABCD"));
    EXPECT_FALSE(glob_match("ABC", "AB"));
}

TEST(PlatformGlob, EmptyPattern) {
    EXPECT_TRUE(glob_match("", ""));
    EXPECT_FALSE(glob_match("", "X"));
}

TEST(PlatformGlob, QuestionMark) {
    EXPECT_TRUE(glob_match("A?C", "ABC"));
    EXPECT_TRUE(glob_match("A?C", "AXC"));
    EXPECT_FALSE(glob_match("A?C", "AC"));
}

TEST(PlatformGlob, ExactMatch) {
    EXPECT_TRUE(glob_match("ARCHER", "ARCHER"));
    EXPECT_TRUE(glob_match("archer", "ARCHER"));
}
