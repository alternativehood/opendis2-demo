#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace d2res {

enum class WdbPlaybackEncoding { Wave, Mp3 };

struct WdbPlaybackPayload {
    std::vector<std::uint8_t> bytes;
    WdbPlaybackEncoding       encoding = WdbPlaybackEncoding::Wave;
    std::uint16_t             source_format_tag = 0;
};

[[nodiscard]] WdbPlaybackPayload
make_wdb_playback_payload(std::span<const std::uint8_t> source_payload);

} // namespace d2res
