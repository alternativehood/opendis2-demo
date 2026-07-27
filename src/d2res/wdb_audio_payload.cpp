#include "wdb_audio_payload.hpp"

#include <limits>
#include <optional>
#include <stdexcept>
#include <string>

namespace d2res {
namespace {

std::uint16_t read_u16(const std::span<const std::uint8_t> bytes, const std::size_t offset) {
    const auto value = static_cast<std::uint32_t>(bytes[offset]) |
                       (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U);
    return static_cast<std::uint16_t>(value);
}

std::uint32_t read_u32(const std::span<const std::uint8_t> bytes, const std::size_t offset) {
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24U);
}

bool has_id(const std::span<const std::uint8_t> bytes, const std::size_t offset,
            const char (&id)[5]) {
    return bytes[offset] == static_cast<std::uint8_t>(id[0]) &&
           bytes[offset + 1] == static_cast<std::uint8_t>(id[1]) &&
           bytes[offset + 2] == static_cast<std::uint8_t>(id[2]) &&
           bytes[offset + 3] == static_cast<std::uint8_t>(id[3]);
}

[[noreturn]] void reject(const std::string& message) {
    throw std::runtime_error("invalid WDB RIFF/WAVE payload: " + message);
}

} // namespace

WdbPlaybackPayload make_wdb_playback_payload(const std::span<const std::uint8_t> source_payload) {
    if (source_payload.size() < 12)
        reject("missing RIFF/WAVE header");
    if (!has_id(source_payload, 0, "RIFF"))
        reject("missing RIFF signature");
    if (!has_id(source_payload, 8, "WAVE"))
        reject("missing WAVE signature");

    std::size_t                  position = 12;
    std::optional<std::uint16_t> format_tag;
    std::size_t                  data_offset = 0;
    std::size_t                  data_size = 0;
    bool                         found_data = false;
    while (position < source_payload.size()) {
        if (source_payload.size() - position < 8)
            reject("truncated RIFF chunk header");
        const bool          is_fmt = has_id(source_payload, position, "fmt ");
        const bool          is_data = has_id(source_payload, position, "data");
        const std::uint32_t chunk_size = read_u32(source_payload, position + 4);
        position += 8;
        if constexpr (sizeof(std::size_t) < sizeof(std::uint32_t)) {
            if (chunk_size > std::numeric_limits<std::size_t>::max())
                reject("RIFF chunk size cannot be represented");
        }
        const std::size_t size = static_cast<std::size_t>(chunk_size);
        const std::size_t padding = static_cast<std::size_t>(chunk_size & 1U);
        if (size > std::numeric_limits<std::size_t>::max() - padding) {
            reject("RIFF chunk padded size overflow");
        }
        const std::size_t padded_size = size + padding;
        if (padded_size > source_payload.size() - position) {
            reject("RIFF chunk extends beyond payload");
        }
        if (is_fmt) {
            if (chunk_size < 16)
                reject("fmt chunk is smaller than 16 bytes");
            format_tag = read_u16(source_payload, position);
        }
        if (is_data && chunk_size != 0 && !found_data) {
            data_offset = position;
            data_size = size;
            found_data = true;
        }
        position += padded_size;
    }

    if (!format_tag.has_value())
        reject("missing fmt chunk");
    if (!found_data)
        reject("missing or empty data chunk");
    if (*format_tag != 1 && *format_tag != 85) {
        reject("unsupported WAVE format tag " + std::to_string(*format_tag));
    }
    if (*format_tag == 1) {
        return {.bytes = {source_payload.begin(), source_payload.end()},
                .encoding = WdbPlaybackEncoding::Wave,
                .source_format_tag = *format_tag};
    }
    return {
        .bytes = {source_payload.begin() + static_cast<std::ptrdiff_t>(data_offset),
                  source_payload.begin() + static_cast<std::ptrdiff_t>(data_offset + data_size)},
        .encoding = WdbPlaybackEncoding::Mp3,
        .source_format_tag = *format_tag};
}

} // namespace d2res
