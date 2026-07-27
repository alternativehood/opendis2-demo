#include <gtest/gtest.h>
#include "d2res/platform/hash.hpp"
#include <cstdint>
#include <string>
#include <vector>

using d2res::platform::sha256_hex;

TEST(PlatformHash, EmptyInput) {
    std::vector<uint8_t> const empty;
    EXPECT_EQ(sha256_hex(empty),
              "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(PlatformHash, AbcVector) {
    std::vector<uint8_t> const abc = {'a', 'b', 'c'};
    EXPECT_EQ(sha256_hex(abc), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(PlatformHash, NonEmptyReturns64LowercaseHex) {
    std::vector<uint8_t> const data = {0x00, 0x01, 0x02, 0x03};
    std::string const          hex = sha256_hex(data);
    ASSERT_EQ(hex.size(), 64u);
    for (char const c : hex) {
        EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')) << "unexpected char: " << c;
    }
}
