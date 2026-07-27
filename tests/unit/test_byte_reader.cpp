#include <gtest/gtest.h>
#include "d2res/byte_reader.hpp"
#include <limits>
#include <vector>

using namespace d2res;

// Helper: owns the buffer so the span stays valid for the test lifetime.
struct Buf {
    std::vector<uint8_t> data;
    ByteReader           reader;

    explicit Buf(std::initializer_list<uint8_t> bytes)
        : data(bytes), reader(std::span<const uint8_t>(data)) {}
};

// ---- u8 --------------------------------------------------------------------

TEST(ByteReader, ReadU8) {
    Buf b{0xAB, 0xCD};
    EXPECT_EQ(b.reader.u8(), 0xABu);
    EXPECT_EQ(b.reader.pos(), 1u);
    EXPECT_EQ(b.reader.u8(), 0xCDu);
    EXPECT_EQ(b.reader.remaining(), 0u);
}

TEST(ByteReader, ReadU8OutOfBoundsThrows) {
    Buf b{};
    EXPECT_THROW(b.reader.u8(), ParseError);
}

// ---- u16le -----------------------------------------------------------------

TEST(ByteReader, ReadU16LE) {
    Buf b{0x01, 0x02};
    EXPECT_EQ(b.reader.u16le(), 0x0201u);
}

TEST(ByteReader, ReadU16LEOutOfBoundsThrows) {
    Buf b{0x01};
    EXPECT_THROW(b.reader.u16le(), ParseError);
}

// ---- u32le -----------------------------------------------------------------

TEST(ByteReader, ReadU32LE) {
    Buf b{0x01, 0x02, 0x03, 0x04};
    EXPECT_EQ(b.reader.u32le(), 0x04030201u);
    EXPECT_EQ(b.reader.pos(), 4u);
}

TEST(ByteReader, ReadU32LEOutOfBoundsThrows) {
    Buf b{0x01, 0x02, 0x03};
    EXPECT_THROW(b.reader.u32le(), ParseError);
}

// ---- i32le -----------------------------------------------------------------

TEST(ByteReader, ReadI32LEPositive) {
    Buf b{0x01, 0x00, 0x00, 0x00};
    EXPECT_EQ(b.reader.i32le(), 1);
}

TEST(ByteReader, ReadI32LENegative) {
    Buf b{0xFF, 0xFF, 0xFF, 0xFF};
    EXPECT_EQ(b.reader.i32le(), -1);
}

// ---- fixed_string ----------------------------------------------------------

TEST(ByteReader, FixedStringStripsTrailingNulls) {
    Buf b{'H', 'i', '\0', '\0', '\0'};
    EXPECT_EQ(b.reader.fixed_string(5), "Hi");
    EXPECT_EQ(b.reader.pos(), 5u);
}

TEST(ByteReader, FixedStringIgnoresBytesAfterFirstNull) {
    Buf b{'H', 'i', '\0', 0xC0, '\t'};
    EXPECT_EQ(b.reader.fixed_string(5), "Hi");
    EXPECT_EQ(b.reader.pos(), 5u);
}

TEST(ByteReader, FixedStringAllNulls) {
    Buf b{'\0', '\0'};
    EXPECT_EQ(b.reader.fixed_string(2), "");
}

TEST(ByteReader, FixedStringOutOfBoundsThrows) {
    Buf b{'A', 'B'};
    EXPECT_THROW(b.reader.fixed_string(5), ParseError);
}

// ---- null_terminated_string ------------------------------------------------

TEST(ByteReader, NullTerminatedString) {
    Buf b{'H', 'e', 'l', 'l', 'o', '\0', 'X'};
    EXPECT_EQ(b.reader.null_terminated_string(), "Hello");
    EXPECT_EQ(b.reader.pos(), 6u);
}

TEST(ByteReader, NullTerminatedStringEmpty) {
    Buf b{'\0'};
    EXPECT_EQ(b.reader.null_terminated_string(), "");
    EXPECT_EQ(b.reader.pos(), 1u);
}

TEST(ByteReader, NullTerminatedStringNoNullThrows) {
    Buf b{'A', 'B', 'C'};
    EXPECT_THROW(b.reader.null_terminated_string(), ParseError);
}

// ---- bytes -----------------------------------------------------------------

TEST(ByteReader, Bytes) {
    Buf  b{0x01, 0x02, 0x03};
    auto v = b.reader.bytes(2);
    ASSERT_EQ(v.size(), 2u);
    EXPECT_EQ(v[0], 0x01u);
    EXPECT_EQ(v[1], 0x02u);
    EXPECT_EQ(b.reader.pos(), 2u);
}

TEST(ByteReader, BytesOutOfBoundsThrows) {
    Buf b{0x01};
    EXPECT_THROW(b.reader.bytes(5), ParseError);
}

// ---- seek / skip -----------------------------------------------------------

TEST(ByteReader, Seek) {
    Buf b{0x00, 0x00, 0xAB, 0xCD};
    b.reader.seek(2);
    EXPECT_EQ(b.reader.pos(), 2u);
    EXPECT_EQ(b.reader.u8(), 0xABu);
}

TEST(ByteReader, SeekBeyondEndThrows) {
    Buf b{0x00, 0x01};
    EXPECT_THROW(b.reader.seek(10), ParseError);
}

TEST(ByteReader, SeekToEndIsOk) {
    Buf b{0x01, 0x02};
    EXPECT_NO_THROW(b.reader.seek(2));
    EXPECT_EQ(b.reader.remaining(), 0u);
}

TEST(ByteReader, Skip) {
    Buf b{0xAA, 0xBB, 0xCC};
    b.reader.skip(2);
    EXPECT_EQ(b.reader.pos(), 2u);
    EXPECT_EQ(b.reader.u8(), 0xCCu);
}

TEST(ByteReader, SkipBeyondEndThrows) {
    Buf b{0x01, 0x02};
    EXPECT_THROW(b.reader.skip(10), ParseError);
}

// ---- remaining / size ------------------------------------------------------

TEST(ByteReader, RemainingAndSize) {
    Buf b{1, 2, 3, 4, 5};
    EXPECT_EQ(b.reader.size(), 5u);
    EXPECT_EQ(b.reader.remaining(), 5u);
    b.reader.skip(3);
    EXPECT_EQ(b.reader.remaining(), 2u);
}

// ---- ParseError carries offset ---------------------------------------------

TEST(ByteReader, RequireDoesNotOverflowOnHugeN) {
    // With pos_>0, old code `pos_ + n > size()` wraps to 0 and does NOT throw;
    // the fixed code `n > size() - pos_` correctly throws.
    Buf b{0x01, 0x02, 0x03, 0x04};
    b.reader.u8(); // advance pos_ to 1
    EXPECT_THROW(b.reader.bytes(std::numeric_limits<std::size_t>::max()), ParseError);
}

TEST(ByteReader, ParseErrorCarriesOffset) {
    Buf b{0x01, 0x02, 0x03};
    b.reader.skip(2);
    try {
        b.reader.u32le();
        FAIL() << "expected ParseError";
    } catch (const ParseError& e) {
        EXPECT_EQ(e.offset(), 2u);
    }
}
