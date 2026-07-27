#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace d2scenario {

struct FieldProvenance {
    std::string field_name;
    std::size_t offset = 0;
    std::size_t value_offset = 0;
    std::size_t value_length = 0;
    bool        is_string = false;
    bool        is_int = false;
    std::string raw_value;
};

class SgRecordReader {
public:
    explicit SgRecordReader(const std::vector<uint8_t>& record);

    static bool is_word_boundary(uint8_t c);

    std::string          read_string_exact(const std::string& key) const;
    int                  read_int32_exact(const std::string& key) const;
    std::uint8_t         read_uint8_exact(const std::string& key) const;
    bool                 read_bool_exact(const std::string& key) const;
    std::vector<uint8_t> read_bytes_exact(const std::string& key) const;

    std::vector<std::string> read_all_string_fields(const std::string& key) const;
    std::vector<int>         read_all_int_fields(const std::string& key) const;

    bool has_field_exact(const std::string& key) const;

    // Specialized common patterns
    std::string read_object_id() const;

private:
    const std::vector<uint8_t>& rec_;
    std::size_t                 start_pos_;

    mutable std::vector<FieldProvenance> provenance_;

    bool before_word_boundary(std::size_t pos) const;

    std::size_t find_key_exact(const std::string& key) const;
    std::size_t find_key_numeric(const std::string& key) const;
    std::size_t find_key_exact_at(const std::string& key, std::size_t search_from) const;
};

} // namespace d2scenario
