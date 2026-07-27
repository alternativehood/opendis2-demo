#include "rgba_buffer.hpp"
#include "byte_reader.hpp"
#include <algorithm>
#include <lodepng.h>
#include <cctype>
#include <cstring>
#include <sstream>
#include <utility>

namespace d2res {

RgbaBuffer decode_base_png(std::span<const uint8_t> data) {
    RgbaBuffer           out;
    unsigned             w = 0;
    unsigned             h = 0;
    std::vector<uint8_t> pixels;
    const unsigned       err = lodepng::decode(pixels, w, h, data.data(), data.size());
    if (err != 0) {
        std::ostringstream msg;
        msg << "lodepng decode failed (code " << err << "): " << lodepng_error_text(err);
        throw ParseError(msg.str(), 0);
    }
    out.width = w;
    out.height = h;
    out.rgba = std::move(pixels);
    return out;
}

void apply_palette_transparent_color_to_rgba(RgbaBuffer&                                    buffer,
                                             const std::array<std::array<uint8_t, 4>, 256>& palette,
                                             uint8_t transparent_color_index) {
    const auto&   tc = palette[transparent_color_index];
    const uint8_t tr = tc[2];
    const uint8_t tg = tc[1];
    const uint8_t tb = tc[0];

    uint8_t*          px = buffer.rgba.data();
    const std::size_t n = static_cast<std::size_t>(buffer.width) * buffer.height;
    for (std::size_t i = 0; i < n; ++i, px += 4) {
        if (px[0] == tr && px[1] == tg && px[2] == tb) {
            px[0] = 0;
            px[1] = 0;
            px[2] = 0;
            px[3] = 0;
        }
    }
}

RgbaBuffer compose_image(const ImageFrame& frame, const RgbaBuffer& base,
                         uint8_t                                        transparent_color_index,
                         const std::array<std::array<uint8_t, 4>, 256>& palette,
                         int16_t                                        opacity_algorithm) {
    const auto ow = static_cast<uint32_t>(frame.output_width);
    const auto oh = static_cast<uint32_t>(frame.output_height);

    RgbaBuffer out;
    out.width = ow;
    out.height = oh;
    out.rgba.assign(static_cast<std::size_t>(ow) * oh * 4, 0);

    for (const auto& p : frame.pieces) {
        if (p.width <= 0 || p.height <= 0)
            continue;

        const auto sw = static_cast<uint32_t>(p.width);
        const auto sh = static_cast<uint32_t>(p.height);
        const auto sx = static_cast<uint32_t>(p.base_x);
        const auto sy = static_cast<uint32_t>(p.base_y);
        const auto dx = static_cast<uint32_t>(p.output_x);
        const auto dy = static_cast<uint32_t>(p.output_y);

        // Bounds-check: skip malformed pieces
        if (sx + sw > base.width || sy + sh > base.height)
            continue;
        if (dx + sw > ow || dy + sh > oh)
            continue;

        for (uint32_t row = 0; row < sh; ++row) {
            const uint8_t* src =
                base.rgba.data() + (((static_cast<std::size_t>(sy + row) * base.width) + sx) * 4U);
            uint8_t* dst =
                out.rgba.data() + (((static_cast<std::size_t>(dy + row) * ow) + dx) * 4U);
            std::memcpy(dst, src, static_cast<std::size_t>(sw) * 4U);
        }
    }

    // Authored palette transparent color — run before magenta cleanup for
    // non-additive sprites. Palette entries are BGRA; transparent RGB is
    // {tc[2], tc[1], tc[0]}. Exact match only, no approximate black heuristics.
    if (opacity_algorithm != 300 && opacity_algorithm != 400) {
        apply_palette_transparent_color_to_rgba(out, palette, transparent_color_index);
    }

    // Transparency pass — unified magenta key handling
    if (opacity_algorithm == 300 || opacity_algorithm == 400) {
        // Additive-blend mode: alpha = max(R, G, B). Black → fully transparent,
        // brighter pixels → proportionally opaque. RGB preserved for downstream compositing.
        uint8_t*          px = out.rgba.data();
        const std::size_t n = static_cast<std::size_t>(ow) * oh;
        for (std::size_t i = 0; i < n; ++i, px += 4) {
            px[3] = std::max(px[0], std::max(px[1], px[2]));
        }
        // Border heuristic: catch additive-sprites with magenta-keyed background
        if (detect_magenta_key_border(out)) {
            apply_magenta_key_to_rgba(out);
        }
    } else if (opacity_algorithm > 0) {
        // Magenta-keyed sprite: use unified alpha-mask magenta cleanup
        apply_magenta_key_to_rgba(out);
    } else {
        // opacity_algorithm == 0: no metadata-tagged transparency
        // but check border heuristic for assets with wrong/missing metadata
        if (detect_magenta_key_border(out)) {
            apply_magenta_key_to_rgba(out);
        }
    }

    return out;
}

std::string sanitize_filename(std::string_view name) {
    std::string out;
    out.reserve(name.size());
    for (const char c : name) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '.' || c == '_' || c == '-') {
            out += c;
        } else {
            out += '_';
        }
    }
    // Lowercase ".PNG" suffix → ".png"
    if (out.size() >= 4) {
        const std::size_t tail = out.size() - 4;
        if (out[tail] == '.' && (out[tail + 1] == 'P' || out[tail + 1] == 'p') &&
            (out[tail + 2] == 'N' || out[tail + 2] == 'n') &&
            (out[tail + 3] == 'G' || out[tail + 3] == 'g')) {
            out[tail + 1] = 'p';
            out[tail + 2] = 'n';
            out[tail + 3] = 'g';
        }
    }
    // Reject path-traversal pseudo-names
    if (out == "." || out == "..")
        out = "_";
    if (out.empty())
        out = "_";
    return out;
}

MagentaCleanupStats apply_magenta_key_to_rgba(RgbaBuffer& buffer) {
    MagentaCleanupStats stats;
    uint8_t*            px = buffer.rgba.data();
    const std::size_t   n = static_cast<std::size_t>(buffer.width) * buffer.height;
    stats.checked_pixels = n;
    for (std::size_t i = 0; i < n; ++i, px += 4) {
        if (is_magenta_key_pixel(px[0], px[1], px[2])) {
            px[0] = 0;
            px[1] = 0;
            px[2] = 0;
            px[3] = 0;
            ++stats.keyed_pixels;
        }
    }
    stats.cleanup_applied = stats.keyed_pixels > 0;
    return stats;
}

bool detect_magenta_key_border(const RgbaBuffer& buffer) {
    if (buffer.width == 0 || buffer.height == 0)
        return false;

    const std::size_t w = buffer.width;
    const std::size_t h = buffer.height;

    // Check 3x3 corner clusters — return true if any has >= 3 magenta pixels
    const auto corner_magenta_count = [&](std::size_t cx, std::size_t cy) -> int {
        int count = 0;
        for (std::size_t dy = 0; dy < 3 && cy + dy < h; ++dy) {
            for (std::size_t dx = 0; dx < 3 && cx + dx < w; ++dx) {
                const std::size_t idx = (((cy + dy) * w) + (cx + dx)) * 4;
                if (is_magenta_key_pixel(buffer.rgba[idx], buffer.rgba[idx + 1],
                                         buffer.rgba[idx + 2])) {
                    ++count;
                }
            }
        }
        return count;
    };

    if (corner_magenta_count(0, 0) >= 3)
        return true;
    if (w >= 3 && corner_magenta_count(w - 3, 0) >= 3)
        return true;
    if (h >= 3 && corner_magenta_count(0, h - 3) >= 3)
        return true;
    if (w >= 3 && h >= 3 && corner_magenta_count(w - 3, h - 3) >= 3)
        return true;

    // Scan outer 1-2 pixel border
    const int      border_width = (w > 4 && h > 4) ? 2 : 1;
    int            total_border = 0;
    int            magenta_border = 0;
    const uint8_t* rgba = buffer.rgba.data();

    for (std::size_t y = 0; y < h; ++y) {
        for (int b = 0; b < border_width; ++b) {
            // Left edge
            if (std::cmp_less(b, w)) {
                const std::size_t i = ((y * w) + static_cast<std::size_t>(b)) * 4;
                if (is_magenta_key_pixel(rgba[i], rgba[i + 1], rgba[i + 2]))
                    ++magenta_border;
                ++total_border;
            }
            // Right edge (avoid double-counting corners when border_width > 1)
            if ((b > 0 || border_width == 1) && std::cmp_greater(w, b)) {
                const std::size_t i = ((y * w) + w - static_cast<std::size_t>(b)) * 4;
                if (is_magenta_key_pixel(rgba[i], rgba[i + 1], rgba[i + 2]))
                    ++magenta_border;
                ++total_border;
            }
        }
    }
    for (auto x = static_cast<std::size_t>(border_width);
         x + static_cast<std::size_t>(border_width) < w; ++x) {
        for (int b = 0; b < border_width; ++b) {
            // Top edge (skip corners already counted above)
            if (std::cmp_less(b, h)) {
                const std::size_t i = ((static_cast<std::size_t>(b) * w) + x) * 4;
                if (is_magenta_key_pixel(rgba[i], rgba[i + 1], rgba[i + 2]))
                    ++magenta_border;
                ++total_border;
            }
            // Bottom edge (avoid double-counting corners when border_width > 1)
            if ((b > 0 || border_width == 1) && std::cmp_greater(h, b)) {
                const std::size_t i = (((h - static_cast<std::size_t>(b)) * w) + x) * 4;
                if (is_magenta_key_pixel(rgba[i], rgba[i + 1], rgba[i + 2]))
                    ++magenta_border;
                ++total_border;
            }
        }
    }

    if (total_border == 0)
        return false;

    // At least 4 border pixels AND at least 1% of border pixels
    return magenta_border >= 4 && (magenta_border * 100 / total_border >= 1);
}

} // namespace d2res
