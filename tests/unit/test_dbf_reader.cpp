#include <gtest/gtest.h>
#include <cstdint>
#include <cstring>
#include <vector>

#include "d2res/dbf_reader.hpp"

using namespace d2res;

// Build a minimal dBASE III byte payload.
// One field descriptor named NAME (type C, length 10).
// header_size = 32 (header) + 32 (one field) + 1 (terminator 0x0D) = 65, padded to 66.
// record_size = 1 (deletion flag) + 10 (field) = 11.
static std::vector<uint8_t> make_dbf(int num_records, bool include_deleted = false) {
    const uint16_t header_size = 66; // 32 + 32 + 1 + 1 (pad)
    const uint16_t record_size = 11;
    int const      total_records = num_records + (include_deleted ? 1 : 0);

    std::vector<uint8_t> buf(header_size + (static_cast<size_t>(total_records) * record_size),
                             0x20);

    // Header
    buf[0] = 0x03; // version dBASE III
    // record_count at offset 4 (u32le)
    auto rc = static_cast<uint32_t>(total_records);
    std::memcpy(buf.data() + 4, &rc, 4);
    // header_size at offset 8 (u16le)
    uint16_t hs = header_size;
    std::memcpy(buf.data() + 8, &hs, 2);
    // record_size at offset 10 (u16le)
    uint16_t rs = record_size;
    std::memcpy(buf.data() + 10, &rs, 2);

    // Field descriptor at offset 32
    std::memcpy(buf.data() + 32, "NAME\0\0\0\0\0\0\0", 11); // name, 11 bytes
    buf[32 + 11] = 'C';                                     // type
    buf[32 + 16] = 10;                                      // length
    buf[32 + 17] = 0;                                       // decimal
    // Terminator
    buf[64] = 0x0D;

    // Records
    size_t pos = header_size;
    for (int i = 0; i < num_records; ++i) {
        buf[pos] = 0x20; // active
        std::memcpy(buf.data() + pos + 1, "RECORD    ", 10);
        pos += record_size;
    }
    if (include_deleted) {
        buf[pos] = 0x2A; // deleted
        std::memcpy(buf.data() + pos + 1, "DELETED   ", 10);
    }
    return buf;
}

TEST(DbfReader, ParseHeader) {
    auto            buf = make_dbf(3);
    DbfReader const reader(std::span<const uint8_t>(buf.data(), buf.size()));
    EXPECT_EQ(reader.record_count(), 3);
}

TEST(DbfReader, FieldNameTrimmed) {
    auto            buf = make_dbf(1);
    DbfReader const reader(std::span<const uint8_t>(buf.data(), buf.size()));
    ASSERT_EQ(reader.list_fields().size(), 1u);
    EXPECT_EQ(reader.list_fields()[0].name, "NAME");
}

TEST(DbfReader, DeletedRecordSkipped) {
    auto            buf = make_dbf(1, true); // 1 active + 1 deleted
    DbfReader const reader(std::span<const uint8_t>(buf.data(), buf.size()));
    auto            records = reader.read_records();
    EXPECT_EQ(records.size(), 1u);
}

TEST(DbfReader, SchemaJsonKeys) {
    auto            buf = make_dbf(2);
    DbfReader const reader(std::span<const uint8_t>(buf.data(), buf.size()));
    auto            j = reader.to_schema_json("test.dbf");
    EXPECT_TRUE(j.contains("source"));
    EXPECT_TRUE(j.contains("record_count"));
    EXPECT_TRUE(j.contains("fields"));
}

TEST(DbfReader, RecordsJsonIsArray) {
    auto            buf = make_dbf(2);
    DbfReader const reader(std::span<const uint8_t>(buf.data(), buf.size()));
    auto            j = reader.to_records_json();
    EXPECT_TRUE(j.is_array());
    EXPECT_EQ(j.size(), 2u);
}
