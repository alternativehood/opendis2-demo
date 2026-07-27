#include "raw_inventory.hpp"

#include "png_metadata.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <utility>

namespace d2res {
namespace {

std::string hex_prefix(std::span<const uint8_t> data, std::size_t count) {
    std::ostringstream out;
    out << std::hex << std::uppercase << std::setfill('0');
    const std::size_t limit = std::min(count, data.size());
    for (std::size_t i = 0; i < limit; ++i) {
        if (i != 0u) {
            out << ' ';
        }
        out << std::setw(2) << static_cast<int>(data[i]);
    }
    return out.str();
}

std::string ascii_prefix(std::span<const uint8_t> data, std::size_t count) {
    std::string       out;
    const std::size_t limit = std::min(count, data.size());
    out.reserve(limit);
    for (std::size_t i = 0; i < limit; ++i) {
        out.push_back(std::isprint(data[i]) != 0 ? static_cast<char>(data[i]) : '.');
    }
    return out;
}

std::string record_magic_ascii(int32_t magic) {
    std::array<char, 4> bytes{};
    bytes[0] = static_cast<char>(magic & 0xFF);
    bytes[1] = static_cast<char>((magic >> 8) & 0xFF);
    bytes[2] = static_cast<char>((magic >> 16) & 0xFF);
    bytes[3] = static_cast<char>((magic >> 24) & 0xFF);
    return {bytes.begin(), bytes.end()};
}

bool starts_with(std::span<const uint8_t> payload, std::string_view magic) {
    if (payload.size() < magic.size()) {
        return false;
    }
    return std::equal(magic.begin(), magic.end(), payload.begin());
}

bool starts_with_png_signature(std::span<const uint8_t> payload) {
    constexpr std::array<uint8_t, 8> signature = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    return payload.size() >= signature.size() &&
           std::equal(signature.begin(), signature.end(), payload.begin());
}

} // namespace

const char* payload_classification_name(PayloadClassification classification) {
    switch (classification) {
    case PayloadClassification::Png:
        return "PNG";
    case PayloadClassification::ApngCandidate:
        return "APNG_CANDIDATE";
    case PayloadClassification::Mff:
        return "MFF";
    case PayloadClassification::UnkTxt1:
        return "UNK_TXT_1";
    case PayloadClassification::UnkTxt2:
        return "UNK_TXT_2";
    case PayloadClassification::UnkData:
        return "UNK_DATA";
    case PayloadClassification::Unknown:
        return "UNKNOWN";
    }
    return "UNKNOWN";
}

PayloadClassification classify_payload(std::span<const uint8_t> payload) {
    if (starts_with_png_signature(payload)) {
        const auto metadata = scan_png_metadata(payload);
        return metadata.has_actl || !metadata.fctl.empty() ? PayloadClassification::ApngCandidate
                                                           : PayloadClassification::Png;
    }
    if (starts_with(payload, "MFF")) {
        return PayloadClassification::Mff;
    }
    if (starts_with(payload, "TXT1")) {
        return PayloadClassification::UnkTxt1;
    }
    if (starts_with(payload, "TXT2")) {
        return PayloadClassification::UnkTxt2;
    }
    if (starts_with(payload, "DATA")) {
        return PayloadClassification::UnkData;
    }
    return PayloadClassification::Unknown;
}

RawRecordInspection inspect_raw_record(const Entry& entry) {
    RawRecordInspection result;
    result.index = entry.record.index;
    result.id = entry.record.id;
    result.name = entry.record.name;
    result.size = entry.record.size;
    result.real_file_size = entry.record.realFileSize;
    result.is_not_deleted = entry.record.isNotDeleted;
    result.record_magic = entry.record.recordMagic;
    result.payload_offset = entry.record.payloadOffset;
    result.record_magic_ascii = record_magic_ascii(entry.record.recordMagic);
    result.payload_magic_hex = hex_prefix(entry.payload, 8);
    result.payload_magic_ascii = ascii_prefix(entry.payload, 8);
    result.classification = classify_payload(entry.payload);
    if (entry.payload.size() < 8u) {
        result.warnings.emplace_back("payload shorter than 8-byte magic sample");
    }
    if (std::cmp_not_equal(entry.record.realFileSize, entry.payload.size())) {
        result.warnings.emplace_back("payload size differs from realFileSize");
    }
    return result;
}

std::vector<RawRecordInspection> inspect_raw_records(const MqdbContainer& container) {
    std::vector<RawRecordInspection> result;
    result.reserve(container.records().size());
    for (const auto& record : container.records()) {
        result.push_back(inspect_raw_record(container.read_record(record.index)));
    }
    return result;
}

nlohmann::json to_json(const RawRecordInspection& inspection) {
    return {{"index", inspection.index},
            {"id", inspection.id},
            {"name",
             inspection.name.empty() ? nlohmann::json(nullptr) : nlohmann::json(inspection.name)},
            {"size", inspection.size},
            {"real_file_size", inspection.real_file_size},
            {"is_not_deleted", inspection.is_not_deleted},
            {"deleted", inspection.is_not_deleted == 0},
            {"record_magic", inspection.record_magic},
            {"record_magic_ascii", inspection.record_magic_ascii},
            {"payload_offset", inspection.payload_offset},
            {"payload_magic_hex", inspection.payload_magic_hex},
            {"payload_magic_ascii", inspection.payload_magic_ascii},
            {"classification", payload_classification_name(inspection.classification)},
            {"warnings", inspection.warnings}};
}

nlohmann::json to_json(const std::vector<RawRecordInspection>& inspections,
                       const MqdbContainer&                    container) {
    nlohmann::json records = nlohmann::json::array();
    for (const auto& inspection : inspections) {
        records.push_back(to_json(inspection));
    }
    return {{"container", container.path().string()},
            {"format", "MQDB"},
            {"records", records},
            {"warnings", container.warnings()}};
}

} // namespace d2res
