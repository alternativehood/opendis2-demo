#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace d2res {

class ParseError : public std::runtime_error {
public:
    ParseError(const std::string& msg, std::size_t offset);
    [[nodiscard]] std::size_t offset() const noexcept { return offset_; }

private:
    std::size_t offset_;
};

class ByteReader {
public:
    explicit ByteReader(std::span<const uint8_t> data);

    uint8_t  u8();
    uint16_t u16le();
    uint32_t u32le();
    int32_t  i32le();

    std::string          fixed_string(std::size_t n);
    std::string          null_terminated_string();
    std::vector<uint8_t> bytes(std::size_t n);

    void                      seek(std::size_t new_pos);
    void                      skip(std::size_t n);
    [[nodiscard]] std::size_t pos() const noexcept { return pos_; }
    [[nodiscard]] std::size_t remaining() const noexcept { return data_.size() - pos_; }
    [[nodiscard]] std::size_t size() const noexcept { return data_.size(); }

    bool peek4(char (&out)[4]) const noexcept;

private:
    void require(std::size_t n) const;

    std::span<const uint8_t> data_;
    std::size_t              pos_ = 0;
};

} // namespace d2res
