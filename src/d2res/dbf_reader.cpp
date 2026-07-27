#include "d2res/dbf_reader.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace d2res {

namespace {

uint32_t read_u32le(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

uint16_t read_u16le(const uint8_t* p) {
    return static_cast<uint16_t>(static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8U));
}

std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(' ');
    if (first == std::string::npos)
        return {};
    const auto last = s.find_last_not_of(' ');
    return s.substr(first, last - first + 1);
}

} // namespace

DbfReader::DbfReader(std::span<const uint8_t> data) : data_(data) {
    if (data.size() < 32)
        throw std::runtime_error("DBF data too short for header");

    record_count_ = static_cast<int>(read_u32le(data.data() + 4));
    header_size_ = static_cast<int>(read_u16le(data.data() + 8));
    record_size_ = static_cast<int>(read_u16le(data.data() + 10));

    // Field descriptors start at offset 32
    size_t offset = 32;
    while (offset < data.size()) {
        if (data[offset] == 0x0D)
            break; // terminator
        if (offset + 32 > data.size())
            break;

        DbfField field;
        // Name: 11 bytes, null-padded
        char name_buf[12]{};
        std::memcpy(name_buf, data.data() + offset, 11);
        name_buf[11] = '\0';
        field.name = name_buf;
        // Trim trailing nulls
        const auto nul_pos = field.name.find('\0');
        if (nul_pos != std::string::npos)
            field.name.erase(nul_pos);

        field.type = static_cast<char>(data[offset + 11]);
        field.length = static_cast<int>(data[offset + 16]);
        field.decimal_count = static_cast<int>(data[offset + 17]);

        fields_.push_back(std::move(field));
        offset += 32;
    }
}

std::vector<std::map<std::string, std::string>> DbfReader::read_records() const {
    std::vector<std::map<std::string, std::string>> result;
    if (record_size_ == 0 || fields_.empty())
        return result;

    auto pos = static_cast<size_t>(header_size_);
    for (int i = 0; i < record_count_; ++i) {
        if (pos + static_cast<size_t>(record_size_) > data_.size())
            break;

        const uint8_t deletion_flag = data_[pos];
        if (deletion_flag == 0x2A) { // deleted
            pos += static_cast<size_t>(record_size_);
            continue;
        }

        std::map<std::string, std::string> record;
        size_t                             field_pos = pos + 1; // skip deletion flag
        for (const auto& field : fields_) {
            if (field_pos + static_cast<size_t>(field.length) > data_.size())
                break;
            std::string const value(reinterpret_cast<const char*>(data_.data() + field_pos),
                                    static_cast<size_t>(field.length));
            record[field.name] = trim(value);
            field_pos += static_cast<size_t>(field.length);
        }
        result.push_back(std::move(record));
        pos += static_cast<size_t>(record_size_);
    }
    return result;
}

nlohmann::json DbfReader::to_schema_json(std::string_view source) const {
    nlohmann::json j;
    j["source"] = std::string(source);
    j["record_count"] = record_count_;
    auto& fields_arr = j["fields"] = nlohmann::json::array();
    for (const auto& f : fields_) {
        nlohmann::json fj;
        fj["name"] = f.name;
        fj["type"] = std::string(1, f.type);
        fj["length"] = f.length;
        fj["decimal_count"] = f.decimal_count;
        fields_arr.push_back(std::move(fj));
    }
    return j;
}

nlohmann::json DbfReader::to_records_json() const {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& rec : read_records()) {
        nlohmann::json obj;
        for (const auto& [k, v] : rec)
            obj[k] = v;
        arr.push_back(std::move(obj));
    }
    return arr;
}

} // namespace d2res
