#include "png_decode.hpp"

#include <lodepng.h>
#include <sstream>

namespace d2res {

PngDecodeResult decode_png_data(const std::vector<std::uint8_t>& data) {
    PngDecodeResult      result;
    unsigned             w = 0;
    unsigned             h = 0;
    std::vector<uint8_t> pixels;
    const unsigned       err = lodepng::decode(pixels, w, h, data.data(), data.size());
    if (err != 0) {
        std::ostringstream msg;
        msg << "lodepng decode failed (code " << err << "): " << lodepng_error_text(err);
        throw std::runtime_error(msg.str());
    }
    result.width = static_cast<std::uint32_t>(w);
    result.height = static_cast<std::uint32_t>(h);
    result.rgba = std::move(pixels);
    return result;
}

PngDecodeResult decode_png_file(const std::string& path) {
    std::vector<std::uint8_t> data;
    const unsigned            err = lodepng::load_file(data, path);
    if (err != 0) {
        std::ostringstream msg;
        msg << "Failed to load PNG file '" << path << "': " << lodepng_error_text(err);
        throw std::runtime_error(msg.str());
    }
    return decode_png_data(data);
}

} // namespace d2res
