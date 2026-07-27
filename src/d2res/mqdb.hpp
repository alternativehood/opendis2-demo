#pragma once
#include "mapped_file.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace d2res {

struct Record {
    std::size_t index = 0;
    int32_t     id = 0;
    int32_t     size = 0;
    int32_t     realFileSize = 0;
    int32_t     isNotDeleted = 0;
    int32_t     recordMagic = 0;
    std::size_t payloadOffset = 0;
    std::string name;
};

struct Entry {
    Record               record;
    std::vector<uint8_t> payload;
};

class MqdbContainer {
public:
    MqdbContainer() = default;

    static MqdbContainer open(const std::filesystem::path& path);

    [[nodiscard]] const std::filesystem::path&    path() const noexcept { return path_; }
    [[nodiscard]] const std::vector<Record>&      records() const noexcept { return records_; }
    [[nodiscard]] const std::vector<std::string>& warnings() const noexcept { return warnings_; }

    [[nodiscard]] std::optional<Record>    find_by_name(std::string_view name) const;
    [[nodiscard]] std::optional<Record>    find_by_id(int32_t id) const;
    [[nodiscard]] std::vector<std::string> names() const;

    [[nodiscard]] Entry read_record(std::size_t index) const;
    [[nodiscard]] Entry read_name(std::string_view name) const;

    [[nodiscard]] std::span<const uint8_t> payload_view(std::size_t index) const;

private:
    void parse(MappedFile&& mapped);
    void parse_ff_name_table(const Record& table_rec);
    void parse_wdb_name_table(const Record& table_rec);

    static std::string normalize(std::string_view s);

    std::filesystem::path                                     path_;
    MappedFile                                                file_;
    std::vector<Record>                                       records_;
    std::unordered_map<std::string, std::vector<std::size_t>> name_to_records_;
    std::unordered_map<int32_t, std::size_t>                  id_to_record_index_;
    std::vector<std::string>                                  warnings_;
};

} // namespace d2res
