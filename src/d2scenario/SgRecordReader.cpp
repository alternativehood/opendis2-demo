#include "SgRecordReader.hpp"

#include <algorithm>
#include <cstring>

namespace d2scenario {

static const std::vector<uint8_t> kBegObject = {'B', 'E', 'G', 'O', 'B', 'J', 'E', 'C', 'T'};

SgRecordReader::SgRecordReader(const std::vector<uint8_t>& record) : rec_(record), start_pos_(0) {
    for (std::size_t i = 0; i + kBegObject.size() <= rec_.size(); ++i) {
        if (std::memcmp(rec_.data() + i, kBegObject.data(), kBegObject.size()) == 0) {
            start_pos_ = i + kBegObject.size();
            break;
        }
    }
}

bool SgRecordReader::is_word_boundary(uint8_t c) {
    return (c < 'A' || c > 'Z') && (c < '0' || c > '9') && c != '_';
}

bool SgRecordReader::before_word_boundary(std::size_t pos) const {
    return pos <= start_pos_ || rec_[pos - 1] != '_';
}

std::size_t SgRecordReader::find_key_exact_at(const std::string& key,
                                              std::size_t        search_from) const {
    auto        key_bytes = reinterpret_cast<const uint8_t*>(key.data());
    std::size_t key_len = key.size();

    for (std::size_t i = search_from; i + key_len <= rec_.size(); ++i) {
        if (std::memcmp(rec_.data() + i, key_bytes, key_len) == 0) {
            if (!before_word_boundary(i))
                continue;
            std::size_t after = i + key_len;
            if (after < rec_.size() && !is_word_boundary(rec_[after]))
                continue;
            return i;
        }
    }
    return std::string::npos;
}

std::size_t SgRecordReader::find_key_exact(const std::string& key) const {
    return find_key_exact_at(key, start_pos_);
}

std::size_t SgRecordReader::find_key_numeric(const std::string& key) const {
    auto        key_bytes = reinterpret_cast<const uint8_t*>(key.data());
    std::size_t key_len = key.size();

    for (std::size_t i = start_pos_; i + key_len + 4 <= rec_.size(); ++i) {
        if (std::memcmp(rec_.data() + i, key_bytes, key_len) == 0) {
            if (!before_word_boundary(i))
                continue;
            std::size_t after = i + key_len;
            if (after < rec_.size() && rec_[after] == '_')
                continue;
            return i;
        }
    }
    return std::string::npos;
}

bool SgRecordReader::has_field_exact(const std::string& key) const {
    return find_key_exact(key) != std::string::npos;
}

std::string SgRecordReader::read_string_exact(const std::string& key) const {
    std::size_t key_pos = find_key_exact(key);
    if (key_pos == std::string::npos)
        return {};

    std::size_t val_off = key_pos + key.size();
    if (val_off + 4 > rec_.size())
        return {};

    uint32_t len = 0;
    std::memcpy(&len, rec_.data() + val_off, 4);
    if (len == 0 || len > 200000 || val_off + 4 + static_cast<std::size_t>(len) > rec_.size())
        return {};

    std::string result;
    result.reserve(len);
    for (std::size_t i = 0; i < static_cast<std::size_t>(len) && rec_[val_off + 4 + i] != 0; ++i)
        result += static_cast<char>(rec_[val_off + 4 + i]);

    FieldProvenance fp;
    fp.field_name = key;
    fp.offset = key_pos;
    fp.value_offset = val_off;
    fp.value_length = 4 + len;
    fp.is_string = true;
    fp.raw_value = result;
    provenance_.push_back(fp);

    return result;
}

int SgRecordReader::read_int32_exact(const std::string& key) const {
    std::size_t key_pos = find_key_numeric(key);
    if (key_pos == std::string::npos)
        return 0;

    std::size_t val_off = key_pos + key.size();
    if (val_off + 4 > rec_.size())
        return 0;

    int32_t value = 0;
    std::memcpy(&value, rec_.data() + val_off, 4);

    FieldProvenance fp;
    fp.field_name = key;
    fp.offset = key_pos;
    fp.value_offset = val_off;
    fp.value_length = 4;
    fp.is_int = true;
    fp.raw_value = std::to_string(value);
    provenance_.push_back(fp);

    return static_cast<int>(value);
}

std::uint8_t SgRecordReader::read_uint8_exact(const std::string& key) const {
    std::size_t key_pos = find_key_numeric(key);
    if (key_pos == std::string::npos)
        return 0;

    std::size_t val_off = key_pos + key.size();
    if (val_off >= rec_.size())
        return 0;

    return rec_[val_off];
}

bool SgRecordReader::read_bool_exact(const std::string& key) const {
    return read_int32_exact(key) != 0;
}

std::vector<uint8_t> SgRecordReader::read_bytes_exact(const std::string& key) const {
    std::size_t key_pos = find_key_exact(key);
    if (key_pos == std::string::npos)
        return {};

    std::size_t val_off = key_pos + key.size();
    if (val_off + 4 > rec_.size())
        return {};

    uint32_t len = 0;
    std::memcpy(&len, rec_.data() + val_off, 4);
    if (len == 0 || len > 1000000 || val_off + 4 + static_cast<std::size_t>(len) > rec_.size())
        return {};

    return std::vector<uint8_t>(rec_.begin() + static_cast<std::ptrdiff_t>(val_off + 4),
                                rec_.begin() + static_cast<std::ptrdiff_t>(val_off + 4 + len));
}

std::vector<std::string> SgRecordReader::read_all_string_fields(const std::string& key) const {
    std::vector<std::string> results;
    auto                     key_bytes = reinterpret_cast<const uint8_t*>(key.data());
    std::size_t              key_len = key.size();

    for (std::size_t i = start_pos_; i + key_len + 4 <= rec_.size(); ++i) {
        if (std::memcmp(rec_.data() + i, key_bytes, key_len) == 0) {
            if (!before_word_boundary(i))
                continue;
            std::size_t after = i + key_len;
            if (after < rec_.size() && !is_word_boundary(rec_[after]))
                continue;

            std::size_t val_off = after;
            if (val_off + 4 > rec_.size())
                continue;
            uint32_t len = 0;
            std::memcpy(&len, rec_.data() + val_off, 4);
            if (len == 0 || len > 200000 ||
                val_off + 4 + static_cast<std::size_t>(len) > rec_.size())
                continue;

            std::string val;
            val.reserve(len);
            for (std::size_t j = 0; j < static_cast<std::size_t>(len) && rec_[val_off + 4 + j] != 0;
                 ++j)
                val += static_cast<char>(rec_[val_off + 4 + j]);
            results.push_back(val);
            i = val_off + 4 + len - 1;
        }
    }
    return results;
}

std::vector<int> SgRecordReader::read_all_int_fields(const std::string& key) const {
    std::vector<int> results;
    auto             key_bytes = reinterpret_cast<const uint8_t*>(key.data());
    std::size_t      key_len = key.size();

    for (std::size_t i = start_pos_; i + key_len + 4 <= rec_.size(); ++i) {
        if (std::memcmp(rec_.data() + i, key_bytes, key_len) == 0) {
            if (!before_word_boundary(i))
                continue;
            std::size_t after_key = i + key_len;
            if (after_key < rec_.size() && rec_[after_key] == '_')
                continue;

            std::size_t val_off = i + key_len;
            if (val_off + 4 > rec_.size())
                continue;
            int32_t value = 0;
            std::memcpy(&value, rec_.data() + val_off, 4);
            results.push_back(static_cast<int>(value));
        }
    }
    return results;
}

std::string SgRecordReader::read_object_id() const {
    std::size_t key_pos = find_key_exact_at("OBJ_ID", 0);
    if (key_pos == std::string::npos)
        return {};

    std::size_t val_off = key_pos + 6;
    if (val_off + 4 > rec_.size())
        return {};

    uint32_t len = 0;
    std::memcpy(&len, rec_.data() + val_off, 4);
    if (len == 0 || len > 128 || val_off + 4 + static_cast<std::size_t>(len) > rec_.size())
        return {};

    std::string result(reinterpret_cast<const char*>(rec_.data() + val_off + 4),
                       static_cast<std::size_t>(len));
    while (!result.empty() && result.back() == '\0')
        result.pop_back();
    return result;
}

} // namespace d2scenario
