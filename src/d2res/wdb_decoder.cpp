#include "wdb_decoder.hpp"
#include "byte_reader.hpp"
#include <algorithm>
#include <cctype>

namespace d2res {

namespace {

std::string to_upper_str(std::string_view s) {
    std::string r(s);
    std::ranges::transform(r, r.begin(),
                           [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return r;
}

// Parse minimal RIFF/WAVE fmt chunk info from payload.
// Returns true if WAVE detected and populates out_* fields.
bool parse_wav(const std::vector<uint8_t>& payload, uint16_t& out_format_tag,
               uint16_t& out_channels, uint32_t& out_sample_rate) {
    // Minimum RIFF/WAVE header: 12 bytes
    if (payload.size() < 12)
        return false;

    // "RIFF" at 0, "WAVE" at 8
    if (payload[0] != 'R' || payload[1] != 'I' || payload[2] != 'F' || payload[3] != 'F')
        return false;
    if (payload[8] != 'W' || payload[9] != 'A' || payload[10] != 'V' || payload[11] != 'E')
        return false;

    // Walk chunks looking for "fmt "
    std::size_t pos = 12;
    while (pos + 8 <= payload.size()) {
        const char     id0 = static_cast<char>(payload[pos + 0]);
        const char     id1 = static_cast<char>(payload[pos + 1]);
        const char     id2 = static_cast<char>(payload[pos + 2]);
        const char     id3 = static_cast<char>(payload[pos + 3]);
        const uint32_t chunk_size = static_cast<uint32_t>(payload[pos + 4]) |
                                    (static_cast<uint32_t>(payload[pos + 5]) << 8) |
                                    (static_cast<uint32_t>(payload[pos + 6]) << 16) |
                                    (static_cast<uint32_t>(payload[pos + 7]) << 24);
        pos += 8;

        if (id0 == 'f' && id1 == 'm' && id2 == 't' && id3 == ' ') {
            if (pos + 16 > payload.size())
                break;
            out_format_tag = static_cast<uint16_t>(static_cast<uint32_t>(payload[pos + 0]) |
                                                   (static_cast<uint32_t>(payload[pos + 1]) << 8U));
            out_channels = static_cast<uint16_t>(static_cast<uint32_t>(payload[pos + 2]) |
                                                 (static_cast<uint32_t>(payload[pos + 3]) << 8U));
            out_sample_rate = static_cast<uint32_t>(payload[pos + 4]) |
                              (static_cast<uint32_t>(payload[pos + 5]) << 8) |
                              (static_cast<uint32_t>(payload[pos + 6]) << 16) |
                              (static_cast<uint32_t>(payload[pos + 7]) << 24);
            return true;
        }

        // Chunks are word-aligned; guard against overflow and exceeding buffer.
        const uint32_t padded = chunk_size + (chunk_size & 1U);
        if (padded < chunk_size || pos + padded < pos || pos + padded > payload.size()) {
            break;
        }
        pos += padded;
    }
    // WAVE detected but fmt chunk not found — still report as WAV
    out_format_tag = 0;
    out_channels = 0;
    out_sample_rate = 0;
    return true;
}

} // namespace

WdbDecoder::WdbDecoder(const MqdbContainer& container) : container_(container) {}

std::vector<std::string> WdbDecoder::list_sounds() const {
    std::vector<std::string> result;
    for (const auto& rec : container_.records()) {
        if (rec.name.empty() || rec.name[0] == '-')
            continue;
        result.push_back(rec.name);
    }
    return result;
}

DecodedSound WdbDecoder::decode_sound(std::string_view name) const {
    // Case-insensitive lookup
    const std::string     upper_name = to_upper_str(name);
    std::optional<Record> found;
    for (const auto& rec : container_.records()) {
        if (to_upper_str(rec.name) == upper_name) {
            found = rec;
            break;
        }
    }
    if (!found)
        throw ParseError("WdbDecoder: sound not found: " + std::string(name), 0);

    Entry entry = container_.read_record(found->index);

    DecodedSound sound;
    sound.logical_name = found->name;
    sound.payload = std::move(entry.payload);

    // Detect format
    uint16_t format_tag = 0;
    uint16_t channels = 0;
    uint32_t sample_rate = 0;
    if (parse_wav(sound.payload, format_tag, channels, sample_rate)) {
        sound.detected_format = "WAV";
        sound.metadata = {
            {"logical_name", sound.logical_name},
            {"source_container", container_.path().string()},
            {"payload_size", static_cast<int>(sound.payload.size())},
            {"detected_format", "WAV"},
            {"format_tag", static_cast<int>(format_tag)},
            {"channels", static_cast<int>(channels)},
            {"sample_rate", static_cast<int>(sample_rate)},
        };
    } else {
        sound.detected_format = "UNKNOWN";
        sound.metadata = {
            {"logical_name", sound.logical_name},
            {"source_container", container_.path().string()},
            {"payload_size", static_cast<int>(sound.payload.size())},
            {"detected_format", "UNKNOWN"},
        };
    }

    return sound;
}

} // namespace d2res
