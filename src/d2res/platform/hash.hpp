#pragma once
#include <cstdint>
#include <span>
#include <string>

namespace d2res::platform {

// Returns lowercase hex SHA-256 digest (64 chars) of the given bytes.
[[nodiscard]] std::string sha256_hex(std::span<const uint8_t> data);

// File-hashing convenience: reads the file in chunks and returns its digest.
// Returns empty string if the file cannot be opened.
[[nodiscard]] std::string sha256_file(const std::string& path);

} // namespace d2res::platform
