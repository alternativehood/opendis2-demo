#pragma once
#include "image_decoder.hpp"
#include <filesystem>
#include <vector>

namespace d2res {

// Encode RGBA frames into an animated GIF file.
// frame_delay_ms: milliseconds per frame (default 100 = 10 fps).
// Transparent pixels (alpha == 0) are written with a black background.
// Throws std::runtime_error on file creation failure.
void encode_gif(const std::vector<DecodedImage>& frames, const std::filesystem::path& out_path,
                int frame_delay_ms = 100);

} // namespace d2res
