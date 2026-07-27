#include "png_metadata.hpp"

#include <array>
#include <cstring>
#include <unordered_set>

namespace d2res {
namespace {

constexpr std::array<uint8_t, 8> kPngSignature = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};

uint32_t u32be(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24U) | (static_cast<uint32_t>(p[1]) << 16U) |
           (static_cast<uint32_t>(p[2]) << 8U) | static_cast<uint32_t>(p[3]);
}

uint16_t u16be(const uint8_t* p) {
    return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8U) | static_cast<uint16_t>(p[1]));
}

std::string chunk_type(const uint8_t* p) {
    return {reinterpret_cast<const char*>(p), 4};
}

std::string text_key(std::span<const uint8_t> payload) {
    for (std::size_t i = 0; i < payload.size(); ++i) {
        if (payload[i] == 0) {
            return {reinterpret_cast<const char*>(payload.data()), i};
        }
    }
    return {};
}

} // namespace

PngMetadata scan_png_metadata(std::span<const uint8_t> data) {
    PngMetadata result;
    if (data.size() < kPngSignature.size() ||
        !std::equal(kPngSignature.begin(), kPngSignature.end(), data.begin())) {
        result.warnings.emplace_back("not a PNG signature");
        return result;
    }
    result.is_png = true;

    const std::unordered_set<std::string> known = {"IHDR", "PLTE", "IDAT", "IEND", "tRNS", "gAMA",
                                                   "cHRM", "sRGB", "iCCP", "pHYs", "tEXt", "zTXt",
                                                   "iTXt", "bKGD", "tIME", "acTL", "fcTL", "fdAT"};
    std::size_t                           pos = kPngSignature.size();
    while (pos < data.size()) {
        if (data.size() - pos < 12u) {
            result.warnings.emplace_back("truncated PNG chunk header");
            break;
        }
        const uint32_t    len = u32be(data.data() + pos);
        const std::string type = chunk_type(data.data() + pos + 4u);
        pos += 8u;
        if (static_cast<std::size_t>(len) > data.size() - pos - 4u) {
            result.warnings.push_back("truncated PNG chunk data: " + type);
            break;
        }
        const auto payload = std::span<const uint8_t>(data.data() + pos, len);
        if (type == "IHDR") {
            if (payload.size() >= 13u) {
                result.has_ihdr = true;
                result.width = u32be(payload.data());
                result.height = u32be(payload.data() + 4u);
                result.bit_depth = payload[8];
                result.color_type = payload[9];
                result.compression = payload[10];
                result.filter = payload[11];
                result.interlace = payload[12];
            } else {
                result.warnings.emplace_back("truncated IHDR payload");
            }
        } else if (type == "pHYs") {
            if (payload.size() >= 9u) {
                result.has_phys = true;
                result.pixels_per_unit_x = u32be(payload.data());
                result.pixels_per_unit_y = u32be(payload.data() + 4u);
                result.phys_unit = payload[8];
            } else {
                result.warnings.emplace_back("truncated pHYs payload");
            }
        } else if (type == "acTL") {
            if (payload.size() >= 8u) {
                result.has_actl = true;
                result.actl_num_frames = u32be(payload.data());
                result.actl_num_plays = u32be(payload.data() + 4u);
            } else {
                result.warnings.emplace_back("truncated acTL payload");
            }
        } else if (type == "fcTL") {
            if (payload.size() >= 26u) {
                result.fctl.push_back(ApngFrameControl{
                    .sequence_number = u32be(payload.data()),
                    .width = u32be(payload.data() + 4u),
                    .height = u32be(payload.data() + 8u),
                    .x_offset = u32be(payload.data() + 12u),
                    .y_offset = u32be(payload.data() + 16u),
                    .delay_num = u16be(payload.data() + 20u),
                    .delay_den = u16be(payload.data() + 22u),
                    .dispose_op = payload[24],
                    .blend_op = payload[25],
                });
            } else {
                result.warnings.emplace_back("truncated fcTL payload");
            }
        } else if (type == "tEXt" || type == "zTXt" || type == "iTXt") {
            result.text_keys.push_back(PngTextKey{.chunk = type, .key = text_key(payload)});
        } else if (!known.contains(type)) {
            result.unknown_chunks.push_back(type);
        }
        pos += static_cast<std::size_t>(len) + 4u; // data + CRC
        if (type == "IEND") {
            break;
        }
    }

    return result;
}

nlohmann::json to_json(const PngMetadata& metadata) {
    nlohmann::json result;
    result["is_png"] = metadata.is_png;
    result["warnings"] = metadata.warnings;
    if (!metadata.is_png) {
        return result;
    }
    if (metadata.has_ihdr) {
        result["ihdr"] = {{"width", metadata.width},
                          {"height", metadata.height},
                          {"bit_depth", metadata.bit_depth},
                          {"color_type", metadata.color_type},
                          {"compression", metadata.compression},
                          {"filter", metadata.filter},
                          {"interlace", metadata.interlace}};
    }
    if (metadata.has_phys) {
        result["phys"] = {{"pixels_per_unit_x", metadata.pixels_per_unit_x},
                          {"pixels_per_unit_y", metadata.pixels_per_unit_y},
                          {"unit", metadata.phys_unit}};
    }
    if (metadata.has_actl) {
        result["actl"] = {{"num_frames", metadata.actl_num_frames},
                          {"num_plays", metadata.actl_num_plays}};
    }
    result["fctl"] = nlohmann::json::array();
    for (const auto& frame : metadata.fctl) {
        result["fctl"].push_back({{"sequence_number", frame.sequence_number},
                                  {"width", frame.width},
                                  {"height", frame.height},
                                  {"x_offset", frame.x_offset},
                                  {"y_offset", frame.y_offset},
                                  {"delay_num", frame.delay_num},
                                  {"delay_den", frame.delay_den},
                                  {"dispose_op", frame.dispose_op},
                                  {"blend_op", frame.blend_op}});
    }
    result["text_keys"] = nlohmann::json::array();
    for (const auto& key : metadata.text_keys) {
        result["text_keys"].push_back({{"chunk", key.chunk}, {"key", key.key}});
    }
    result["unknown_chunks"] = metadata.unknown_chunks;
    return result;
}

} // namespace d2res
