#pragma once

// Runtime audio policy:
// WDB/MQDB parsing and sound id extraction are D2-specific and stay here.
// Playback, mixing, channels, music streaming, panning and volume control should
// The decoder only extracts encoded payloads; the engine owns SDL3_mixer playback.
// Do not build a handwritten mixer/audio scheduler in this codebase.
// Keep D2 WDB/MQDB parsing here, but do not add a handwritten mixer,
// channel scheduler, music streamer or panning engine.

#include "mqdb.hpp"
#include <nlohmann/json.hpp>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace d2res {

struct DecodedSound {
    std::string          logical_name;
    std::vector<uint8_t> payload;
    std::string          detected_format; // "WAV" or "UNKNOWN"
    nlohmann::json       metadata;
};

class WdbDecoder {
public:
    explicit WdbDecoder(const MqdbContainer& container);

    // Returns names of all non-special records (names not starting with '-').
    [[nodiscard]] std::vector<std::string> list_sounds() const;

    // Decode one sound by name (case-insensitive). Throws ParseError if not found.
    [[nodiscard]] DecodedSound decode_sound(std::string_view name) const;

private:
    const MqdbContainer& container_;
};

} // namespace d2res
