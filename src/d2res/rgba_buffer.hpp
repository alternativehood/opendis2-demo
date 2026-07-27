#pragma once
#include "opt_images.hpp"
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace d2res {

struct RgbaBuffer {
    uint32_t             width = 0;
    uint32_t             height = 0;
    std::vector<uint8_t> rgba; // RGBA8, row-major, 4 bytes per pixel
};

enum class TransparencyMode : uint8_t {
    None,
    ColorKeyMagentaRange,
    AdditiveBlend,
    PaletteIndexAlpha,
    ShadowAlpha128,
    Unknown
};

// Decode a PNG payload (lodepng) → RgbaBuffer. Throws ParseError on failure.
RgbaBuffer decode_base_png(std::span<const uint8_t> data);

// Composite a logical image: blit ImagePiece crops from base into an output
// buffer of frame.output_width × frame.output_height, then apply transparency.
RgbaBuffer compose_image(const ImageFrame& frame, const RgbaBuffer& base,
                         uint8_t                                        transparent_color_index,
                         const std::array<std::array<uint8_t, 4>, 256>& palette,
                         int16_t                                        opacity_algorithm);

// Map internal resource name to a safe filesystem name.
// Replaces chars outside [A-Za-z0-9._-] with '_', lowercases ".PNG" → ".png".
// Never returns empty string (returns "_" when all chars are replaced).
std::string sanitize_filename(std::string_view name);

// ── Magenta key helpers (unified production path) ──────────────────────────

// Loose magenta-key pixel predicate: detects exact 255/0/255 and all
// dirty/near-magenta variants observed in Disciples II game assets.
// Covers values such as 252/3/252, 241/5/239, 214/7/214, 168/2/169.
// Avoids false-positives on normal purple/dark art.
[[nodiscard]] inline bool is_magenta_key_pixel(uint8_t r, uint8_t g, uint8_t b) noexcept {
    return r >= 145 && b >= 145 && g <= 2 && static_cast<int>(r) - static_cast<int>(b) <= 80 &&
           static_cast<int>(b) - static_cast<int>(r) <= 80 &&
           static_cast<int>(r) - static_cast<int>(g) >= 80 &&
           static_cast<int>(b) - static_cast<int>(g) >= 80;
}

// Non-destructive alpha-mask magenta cleanup: sets alpha=0 and rgb=0,0,0
// for matching pixels. Does not copy neighboring RGB, does not blur, does
// not alter non-magenta opaque art. This is the single production path.
// Returns stats about the cleanup pass.
struct MagentaCleanupStats {
    std::size_t checked_pixels = 0;
    std::size_t keyed_pixels = 0;
    bool        cleanup_applied = false;
};

MagentaCleanupStats apply_magenta_key_to_rgba(RgbaBuffer& buffer);

// Apply the authored palette transparent color: for non-additive composed sprites,
// any pixel whose RGB matches the transparent palette entry (BGRA → {tc[2],tc[1],tc[0]})
// is set to RGBA(0,0,0,0).
void apply_palette_transparent_color_to_rgba(RgbaBuffer&                                    buffer,
                                             const std::array<std::array<uint8_t, 4>, 256>& palette,
                                             uint8_t transparent_color_index);

// Border heuristic: returns true if the image has a magenta-keyed background.
// Scans outer 1-2 pixel border for loose-magenta pixels. Returns true when:
//   - Any 3×3 corner cluster has >= 3 magenta pixels, or
//   - At least 4 border pixels match and >= 1% of border pixels match.
// Designed to catch UI/sprite assets with wrong/missing opacity metadata
// while avoiding false-positives on interior purple art.
[[nodiscard]] bool detect_magenta_key_border(const RgbaBuffer& buffer);

} // namespace d2res
