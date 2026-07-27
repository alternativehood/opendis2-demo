#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace d2res {

struct ImagePiece {
    int32_t output_x = 0;
    int32_t output_y = 0;
    int32_t base_x = 0;
    int32_t base_y = 0;
    int32_t width = 0;
    int32_t height = 0;
};

struct ImageFrame {
    std::string             name;
    int32_t                 output_width = 0;
    int32_t                 output_height = 0;
    std::vector<ImagePiece> pieces;
};

struct ImageBlock {
    uint8_t                                 transparent_color_index = 0;
    int16_t                                 opacity_algorithm = 0;
    int32_t                                 size_x = 0;
    int32_t                                 size_y = 0;
    std::array<std::array<uint8_t, 4>, 256> palette{}; // BGRA per entry
    std::vector<ImageFrame>                 frames;
};

struct ImageMap {
    std::vector<ImageBlock>                      blocks;
    std::unordered_map<std::string, std::size_t> frame_name_to_block; // frame name → block index
    std::vector<std::string>                     warnings;
};

struct ImagesParser {
    static ImageMap parse(std::span<const uint8_t> data);
};

} // namespace d2res
