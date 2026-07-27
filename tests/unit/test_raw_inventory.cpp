#include <gtest/gtest.h>

#include "d2res/raw_inventory.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace {

std::vector<uint8_t> png_payload() {
    return {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n', 0, 0,
            0,    0,   'I', 'E', 'N',  'D',  0,    0,    0, 0};
}

void push_u32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>((value >> 24U) & 0xFFU));
    out.push_back(static_cast<uint8_t>((value >> 16U) & 0xFFU));
    out.push_back(static_cast<uint8_t>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<uint8_t>(value & 0xFFU));
}

void chunk(std::vector<uint8_t>& out, const std::string& type, const std::vector<uint8_t>& data) {
    push_u32(out, static_cast<uint32_t>(data.size()));
    out.insert(out.end(), type.begin(), type.end());
    out.insert(out.end(), data.begin(), data.end());
    push_u32(out, 0);
}

std::vector<uint8_t> apng_payload() {
    auto payload = png_payload();
    payload.resize(8);
    chunk(payload, "IHDR", {0, 0, 0, 1, 0, 0, 0, 1, 8, 6, 0, 0, 0});
    chunk(payload, "acTL", {0, 0, 0, 2, 0, 0, 0, 0});
    chunk(payload, "IEND", {});
    return payload;
}

d2res::Entry entry_with_payload(std::vector<uint8_t> payload) {
    d2res::Entry entry;
    entry.record.index = 7;
    entry.record.id = 42;
    entry.record.size = 100;
    entry.record.realFileSize = static_cast<int32_t>(payload.size());
    entry.record.isNotDeleted = 1;
    entry.record.payloadOffset = 32;
    entry.record.name = "record";
    entry.payload = std::move(payload);
    return entry;
}

} // namespace

TEST(RawInventory, ClassifiesKnownPayloads) {
    EXPECT_EQ(d2res::classify_payload(png_payload()), d2res::PayloadClassification::Png);
    EXPECT_EQ(d2res::classify_payload(apng_payload()), d2res::PayloadClassification::ApngCandidate);
    EXPECT_EQ(d2res::classify_payload(std::vector<uint8_t>{'M', 'F', 'F', 0}),
              d2res::PayloadClassification::Mff);
    EXPECT_EQ(d2res::classify_payload(std::vector<uint8_t>{'T', 'X', 'T', '1'}),
              d2res::PayloadClassification::UnkTxt1);
    EXPECT_EQ(d2res::classify_payload(std::vector<uint8_t>{'T', 'X', 'T', '2'}),
              d2res::PayloadClassification::UnkTxt2);
    EXPECT_EQ(d2res::classify_payload(std::vector<uint8_t>{'D', 'A', 'T', 'A'}),
              d2res::PayloadClassification::UnkData);
    EXPECT_EQ(d2res::classify_payload(std::vector<uint8_t>{0, 1, 2, 3}),
              d2res::PayloadClassification::Unknown);
}

TEST(RawInventory, InspectRecordReportsStableFields) {
    const auto inspection = d2res::inspect_raw_record(entry_with_payload(png_payload()));

    EXPECT_EQ(inspection.index, 7u);
    EXPECT_EQ(inspection.id, 42);
    EXPECT_EQ(inspection.name, "record");
    EXPECT_EQ(inspection.payload_offset, 32u);
    EXPECT_EQ(inspection.classification, d2res::PayloadClassification::Png);
    EXPECT_EQ(inspection.payload_magic_ascii.substr(1, 3), "PNG");
    EXPECT_TRUE(inspection.warnings.empty());
}

TEST(RawInventory, TruncatedPayloadWarns) {
    const auto inspection = d2res::inspect_raw_record(entry_with_payload({1, 2, 3}));
    EXPECT_EQ(inspection.classification, d2res::PayloadClassification::Unknown);
    ASSERT_FALSE(inspection.warnings.empty());
}
