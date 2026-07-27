#include "byte_reader.hpp"
#include <cstdio>
#include <cstring>

namespace d2res {

// ---- ParseError ----------------------------------------------------------

static std::string make_parse_error_msg(const std::string& msg, std::size_t offset) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "0x%zX", offset);
    return "ParseError at offset " + std::string(buf) + ": " + msg;
}

ParseError::ParseError(const std::string& msg, std::size_t offset)
    : std::runtime_error(make_parse_error_msg(msg, offset)), offset_(offset) {}

// ---- ByteReader ----------------------------------------------------------

ByteReader::ByteReader(std::span<const uint8_t> data) : data_(data) {}

void ByteReader::require(std::size_t n) const {
    if (n > data_.size() - pos_) {
        throw ParseError("need " + std::to_string(n) + " bytes but only " +
                             std::to_string(remaining()) + " remain",
                         pos_);
    }
}

uint8_t ByteReader::u8() {
    require(1);
    return data_[pos_++];
}

uint16_t ByteReader::u16le() {
    require(2);
    auto const v = static_cast<uint16_t>(static_cast<uint32_t>(data_[pos_]) |
                                         (static_cast<uint32_t>(data_[pos_ + 1]) << 8U));
    pos_ += 2;
    return v;
}

uint32_t ByteReader::u32le() {
    require(4);
    uint32_t const v = static_cast<uint32_t>(data_[pos_]) |
                       (static_cast<uint32_t>(data_[pos_ + 1]) << 8) |
                       (static_cast<uint32_t>(data_[pos_ + 2]) << 16) |
                       (static_cast<uint32_t>(data_[pos_ + 3]) << 24);
    pos_ += 4;
    return v;
}

int32_t ByteReader::i32le() {
    return static_cast<int32_t>(u32le());
}

std::string ByteReader::fixed_string(std::size_t n) {
    require(n);
    const char* p = reinterpret_cast<const char*>(data_.data() + pos_);
    pos_ += n;
    std::size_t len = 0;
    while (len < n && p[len] != '\0')
        ++len;
    return {p, len};
}

std::string ByteReader::null_terminated_string() {
    std::string result;
    while (pos_ < data_.size()) {
        uint8_t const c = data_[pos_++];
        if (c == 0)
            return result;
        result += static_cast<char>(c);
    }
    throw ParseError("no null terminator found for string starting near pos", pos_);
}

std::vector<uint8_t> ByteReader::bytes(std::size_t n) {
    require(n);
    std::vector<uint8_t> result(data_.data() + pos_, data_.data() + pos_ + n);
    pos_ += n;
    return result;
}

void ByteReader::seek(std::size_t new_pos) {
    if (new_pos > data_.size()) {
        throw ParseError("seek to " + std::to_string(new_pos) + " beyond end (" +
                             std::to_string(data_.size()) + ")",
                         pos_);
    }
    pos_ = new_pos;
}

void ByteReader::skip(std::size_t n) {
    if (pos_ + n > data_.size())
        throw ParseError("skip " + std::to_string(n) + " bytes beyond end", pos_);
    pos_ += n;
}

bool ByteReader::peek4(char (&out)[4]) const noexcept {
    if (data_.size() - pos_ < 4)
        return false;
    std::memcpy(out, data_.data() + pos_, 4);
    return true;
}

} // namespace d2res
