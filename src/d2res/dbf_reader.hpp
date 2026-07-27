#pragma once
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace d2res {

struct DbfField {
    std::string name;
    char        type = '\0';
    int         length = 0;
    int         decimal_count = 0;
};

class DbfReader {
public:
    explicit DbfReader(std::span<const uint8_t> data);

    [[nodiscard]] int                          record_count() const { return record_count_; }
    [[nodiscard]] const std::vector<DbfField>& list_fields() const { return fields_; }
    [[nodiscard]] std::vector<std::map<std::string, std::string>> read_records() const;

    [[nodiscard]] nlohmann::json to_schema_json(std::string_view source) const;
    [[nodiscard]] nlohmann::json to_records_json() const;

private:
    std::span<const uint8_t> data_;
    int                      record_count_{};
    int                      header_size_{};
    int                      record_size_{};
    std::vector<DbfField>    fields_;
};

} // namespace d2res
