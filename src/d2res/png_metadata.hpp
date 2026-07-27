#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace d2res {

struct PngTextKey {
    std::string chunk;
    std::string key;
};

struct ApngFrameControl {
    uint32_t sequence_number = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t x_offset = 0;
    uint32_t y_offset = 0;
    uint16_t delay_num = 0;
    uint16_t delay_den = 0;
    uint8_t  dispose_op = 0;
    uint8_t  blend_op = 0;
};

struct PngMetadata {
    bool                          is_png = false;
    bool                          has_ihdr = false;
    uint32_t                      width = 0;
    uint32_t                      height = 0;
    uint8_t                       bit_depth = 0;
    uint8_t                       color_type = 0;
    uint8_t                       compression = 0;
    uint8_t                       filter = 0;
    uint8_t                       interlace = 0;
    bool                          has_phys = false;
    uint32_t                      pixels_per_unit_x = 0;
    uint32_t                      pixels_per_unit_y = 0;
    uint8_t                       phys_unit = 0;
    bool                          has_actl = false;
    uint32_t                      actl_num_frames = 0;
    uint32_t                      actl_num_plays = 0;
    std::vector<ApngFrameControl> fctl;
    std::vector<PngTextKey>       text_keys;
    std::vector<std::string>      unknown_chunks;
    std::vector<std::string>      warnings;
};

[[nodiscard]] PngMetadata    scan_png_metadata(std::span<const uint8_t> data);
[[nodiscard]] nlohmann::json to_json(const PngMetadata& metadata);

} // namespace d2res
