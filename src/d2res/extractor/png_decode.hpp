#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Intentional: extraction/comparison code uses lodepng for PNG decode.
// Runtime SDL texture loading must use SDL3_image instead.

namespace d2res {

struct PngDecodeResult {
    std::uint32_t             width = 0;
    std::uint32_t             height = 0;
    std::vector<std::uint8_t> rgba;
};

// Decode a PNG file from disk to raw RGBA pixel data.
// Throws std::runtime_error on failure.
[[nodiscard]] PngDecodeResult decode_png_file(const std::string& path);

// Decode PNG data from a memory buffer.
// Throws std::runtime_error on failure.
[[nodiscard]] PngDecodeResult decode_png_data(const std::vector<std::uint8_t>& data);

} // namespace d2res
