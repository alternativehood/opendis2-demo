#include <gtest/gtest.h>
#include "d2res/rgba_buffer.hpp"

using namespace d2res;

TEST(SanitizeFilename, PathSeparatorsReplaced) {
    EXPECT_EQ(sanitize_filename("A/B\\C"), "A_B_C");
}

TEST(SanitizeFilename, ColonAndWildcardReplaced) {
    EXPECT_EQ(sanitize_filename("A:Name*.PNG"), "A_Name_.png");
}

TEST(SanitizeFilename, NormalNameUnchangedExceptPngSuffix) {
    EXPECT_EQ(sanitize_filename("EMP_IMPGUILD.PNG"), "EMP_IMPGUILD.png");
}

TEST(SanitizeFilename, AllReplacedReturnsUnderscore) {
    EXPECT_EQ(sanitize_filename("@#$"), "___");
}

TEST(SanitizeFilename, EmptyInputReturnsUnderscore) {
    EXPECT_EQ(sanitize_filename(""), "_");
}

TEST(SanitizeFilename, LowercasePngSuffixUnchanged) {
    EXPECT_EQ(sanitize_filename("foo.png"), "foo.png");
}

TEST(SanitizeFilename, DashAndDotAllowed) {
    EXPECT_EQ(sanitize_filename("my-file.name"), "my-file.name");
}

TEST(SanitizeFilename, DotComponentReplaced) {
    EXPECT_EQ(sanitize_filename("."), "_");
}

TEST(SanitizeFilename, DotDotComponentReplaced) {
    EXPECT_EQ(sanitize_filename(".."), "_");
}
