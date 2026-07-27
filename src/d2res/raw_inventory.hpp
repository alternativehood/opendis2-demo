#pragma once

#include "mqdb.hpp"

#include <span>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace d2res {

enum class PayloadClassification {
    Png,
    ApngCandidate,
    Mff,
    UnkTxt1,
    UnkTxt2,
    UnkData,
    Unknown,
};

struct RawRecordInspection {
    std::size_t              index = 0;
    int32_t                  id = 0;
    std::string              name;
    int32_t                  size = 0;
    int32_t                  real_file_size = 0;
    int32_t                  is_not_deleted = 0;
    int32_t                  record_magic = 0;
    std::size_t              payload_offset = 0;
    std::string              record_magic_ascii;
    std::string              payload_magic_hex;
    std::string              payload_magic_ascii;
    PayloadClassification    classification = PayloadClassification::Unknown;
    std::vector<std::string> warnings;
};

[[nodiscard]] const char* payload_classification_name(PayloadClassification classification);
[[nodiscard]] PayloadClassification            classify_payload(std::span<const uint8_t> payload);
[[nodiscard]] RawRecordInspection              inspect_raw_record(const Entry& entry);
[[nodiscard]] std::vector<RawRecordInspection> inspect_raw_records(const MqdbContainer& container);
[[nodiscard]] nlohmann::json                   to_json(const RawRecordInspection& inspection);
[[nodiscard]] nlohmann::json to_json(const std::vector<RawRecordInspection>& inspections,
                                     const MqdbContainer&                    container);

} // namespace d2res
