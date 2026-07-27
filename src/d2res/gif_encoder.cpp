#include "gif_encoder.hpp"
// gif.h defines all functions in the header with external linkage —
// include it in exactly one translation unit.
#include <gif.h>
#include <stdexcept>
#include <vector>

namespace d2res {

void encode_gif(const std::vector<DecodedImage>& frames, const std::filesystem::path& out_path,
                int frame_delay_ms) {
    if (frames.empty())
        throw std::runtime_error("encode_gif: no frames to encode");

    const uint32_t width = frames[0].width;
    const uint32_t height = frames[0].height;
    // gif.h delay is in 1/100 s units
    const auto delay_cs = static_cast<uint32_t>(frame_delay_ms / 10);

    GifWriter writer{};
    if (!GifBegin(&writer, out_path.string().c_str(), width, height, delay_cs))
        throw std::runtime_error("encode_gif: could not open file " + out_path.string());

    struct GifGuard { // NOLINT(cppcoreguidelines-special-member-functions) ; local RAII, never
                      // copied
        // cppcheck-suppress uninitMemberVarNoCtor
        GifWriter& w; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
        bool       done = false;
        ~GifGuard() {
            if (!done && (w.f != nullptr))
                GifEnd(&w);
        }
    } guard{.w = writer};

    // Scratch buffer: replace alpha=0 pixels with black so they render as
    // a clean background rather than magenta (the pre-transparency RGB value).
    std::vector<uint8_t> buf;

    for (const auto& frame : frames) {
        const uint32_t fw = (frame.width != 0u) ? frame.width : width;
        const uint32_t fh = (frame.height != 0u) ? frame.height : height;

        buf.assign(frame.rgba.begin(), frame.rgba.end());
        // Pad/trim to canonical dimensions if needed (should not happen normally)
        buf.resize(static_cast<std::size_t>(fw) * fh * 4, 0);

        // Replace transparent pixels with white so sprites are visible on a neutral background
        for (std::size_t i = 0; i + 3 < buf.size(); i += 4) {
            if (buf[i + 3] == 0) {
                buf[i + 0] = 255;
                buf[i + 1] = 255;
                buf[i + 2] = 255;
                buf[i + 3] = 255;
            }
        }

        if (!GifWriteFrame(&writer, buf.data(), fw, fh, delay_cs))
            throw std::runtime_error("encode_gif: GifWriteFrame failed");
    }

    // cppcheck-suppress unreadVariable
    guard.done = true;
    GifEnd(&writer);
}

} // namespace d2res
