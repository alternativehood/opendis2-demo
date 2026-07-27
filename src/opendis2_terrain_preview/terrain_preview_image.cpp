#include "terrain_preview_image.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace d2terrain_preview {

namespace {

struct Color {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;
};

Color color_from_byte(uint8_t value) {
    return {.r = static_cast<uint8_t>((value * 73U) & 0xFFU),
            .g = static_cast<uint8_t>(((value * 151U) + 47U) & 0xFFU),
            .b = static_cast<uint8_t>(((value * 199U) + 91U) & 0xFFU),
            .a = 255};
}

Color color_for_family(d2runtime::AdventureTerrainFamily family) {
    switch (family) {
    case d2runtime::AdventureTerrainFamily::Human:
        return {72, 150, 63, 255};
    case d2runtime::AdventureTerrainFamily::Dwarf:
        return {145, 112, 61, 255};
    case d2runtime::AdventureTerrainFamily::Heretic:
        return {161, 61, 47, 255};
    case d2runtime::AdventureTerrainFamily::Undead:
        return {89, 72, 128, 255};
    case d2runtime::AdventureTerrainFamily::Neutral:
        return {116, 126, 101, 255};
    case d2runtime::AdventureTerrainFamily::Elf:
        return {49, 132, 101, 255};
    case d2runtime::AdventureTerrainFamily::Water:
        return {45, 106, 164, 255};
    case d2runtime::AdventureTerrainFamily::Black:
        return {26, 27, 29, 255};
    case d2runtime::AdventureTerrainFamily::Unknown:
        return {255, 0, 255, 255};
    }
    return {255, 0, 255, 255};
}

void blend_pixel(std::vector<uint8_t>& rgba, int width, int height, int x, int y, Color src) {
    if (x < 0 || y < 0 || x >= width || y >= height || src.a == 0) {
        return;
    }
    auto*     dst = rgba.data() + (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                                   static_cast<std::size_t>(x)) *
                                      4U;
    const int inv_a = 255 - src.a;
    dst[0] = static_cast<uint8_t>((src.r * src.a + dst[0] * inv_a) / 255);
    dst[1] = static_cast<uint8_t>((src.g * src.a + dst[1] * inv_a) / 255);
    dst[2] = static_cast<uint8_t>((src.b * src.a + dst[2] * inv_a) / 255);
    dst[3] = static_cast<uint8_t>(std::min(255, static_cast<int>(src.a) + dst[3] * inv_a / 255));
}

void draw_diamond(std::vector<uint8_t>& rgba, int width, int height, int ox, int oy, Color color) {
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 64; ++x) {
            if (diamond_contains(x, y)) {
                blend_pixel(rgba, width, height, ox + x, oy + y, color);
            }
        }
    }
}

std::vector<uint8_t> scaled_copy(const std::vector<uint8_t>& src, int src_w, int src_h, int dst_w,
                                 int dst_h) {
    std::vector<uint8_t> dst(static_cast<std::size_t>(dst_w) * static_cast<std::size_t>(dst_h) *
                             4U);
    if (dst_w <= 0 || dst_h <= 0 || src_w <= 0 || src_h <= 0)
        return dst;

    std::vector<int> x_map(static_cast<std::size_t>(dst_w));
    std::vector<int> y_map(static_cast<std::size_t>(dst_h));
    for (int x = 0; x < dst_w; ++x) {
        x_map[static_cast<std::size_t>(x)] =
            std::min(src_w - 1, static_cast<int>(static_cast<double>(x) * src_w / dst_w));
    }
    for (int y = 0; y < dst_h; ++y) {
        y_map[static_cast<std::size_t>(y)] =
            std::min(src_h - 1, static_cast<int>(static_cast<double>(y) * src_h / dst_h));
    }

    for (int y = 0; y < dst_h; ++y) {
        const std::size_t src_row_offset =
            static_cast<std::size_t>(y_map[static_cast<std::size_t>(y)]) *
            static_cast<std::size_t>(src_w);
        const std::size_t dst_row_offset =
            static_cast<std::size_t>(y) * static_cast<std::size_t>(dst_w);
        for (int x = 0; x < dst_w; ++x) {
            const std::size_t src_i =
                (src_row_offset + static_cast<std::size_t>(x_map[static_cast<std::size_t>(x)])) *
                4U;
            const std::size_t dst_i = (dst_row_offset + static_cast<std::size_t>(x)) * 4U;
            std::copy_n(src.data() + src_i, 4, dst.data() + dst_i);
        }
    }
    return dst;
}

std::vector<uint8_t> scaled_copy(const d2engine::AdventureTerrainSurface& src, int dst_w,
                                 int dst_h) {
    std::vector<uint8_t> dst(static_cast<std::size_t>(dst_w) * static_cast<std::size_t>(dst_h) *
                             4U);
    if (dst_w <= 0 || dst_h <= 0 || src.width <= 0 || src.height <= 0)
        return dst;

    std::vector<int> x_map(static_cast<std::size_t>(dst_w));
    std::vector<int> y_map(static_cast<std::size_t>(dst_h));
    for (int x = 0; x < dst_w; ++x) {
        x_map[static_cast<std::size_t>(x)] =
            std::min(src.width - 1, static_cast<int>(static_cast<double>(x) * src.width / dst_w));
    }
    for (int y = 0; y < dst_h; ++y) {
        y_map[static_cast<std::size_t>(y)] =
            std::min(src.height - 1, static_cast<int>(static_cast<double>(y) * src.height / dst_h));
    }

    for (int y = 0; y < dst_h; ++y) {
        const std::size_t src_row_offset =
            static_cast<std::size_t>(y_map[static_cast<std::size_t>(y)]) *
            static_cast<std::size_t>(src.width);
        const std::size_t dst_row_offset =
            static_cast<std::size_t>(y) * static_cast<std::size_t>(dst_w);
        for (int x = 0; x < dst_w; ++x) {
            const std::size_t src_i =
                src_row_offset + static_cast<std::size_t>(x_map[static_cast<std::size_t>(x)]);
            const std::size_t dst_i = (dst_row_offset + static_cast<std::size_t>(x)) * 4U;
            const auto&       px = src.pixels[src_i];
            dst[dst_i + 0] = px.r;
            dst[dst_i + 1] = px.g;
            dst[dst_i + 2] = px.b;
            dst[dst_i + 3] = px.a;
        }
    }
    return dst;
}

} // namespace

PreviewImage preview_from_surface(const d2engine::AdventureTerrainSurface& surface, int max_size) {
    PreviewImage image;
    image.logical_width = surface.width;
    image.logical_height = surface.height;
    const auto max_wh = std::max(surface.width, surface.height);
    const auto fit =
        max_size > 0 ? std::min(1.0, static_cast<double>(max_size) / static_cast<double>(max_wh))
                     : 1.0;
    image.output_width = std::max(1, static_cast<int>(std::floor(surface.width * fit)));
    image.output_height = std::max(1, static_cast<int>(std::floor(surface.height * fit)));
    image.rgba = scaled_copy(surface, image.output_width, image.output_height);
    return image;
}

PreviewLayout make_preview_layout(int grid_width, int grid_height) {
    PreviewLayout layout;
    if (grid_width <= 0 || grid_height <= 0) {
        return layout;
    }

    layout.positions.reserve(static_cast<std::size_t>(grid_width) *
                             static_cast<std::size_t>(grid_height));
    for (int y = 0; y < grid_height; ++y) {
        for (int x = 0; x < grid_width; ++x) {
            const int sx = (x - y) * layout.tile_width / 2;
            const int sy = (x + y) * layout.tile_height / 2;
            layout.positions.push_back({sx, sy});
        }
    }
    const auto min_x = std::min_element(layout.positions.begin(), layout.positions.end(),
                                        [](const auto& a, const auto& b) { return a.x < b.x; })
                           ->x;
    const auto min_y = std::min_element(layout.positions.begin(), layout.positions.end(),
                                        [](const auto& a, const auto& b) { return a.y < b.y; })
                           ->y;
    const auto max_x = std::max_element(layout.positions.begin(), layout.positions.end(),
                                        [tw = layout.tile_width](const auto& a, const auto& b) {
                                            return (a.x + tw) < (b.x + tw);
                                        })
                           ->x +
                       layout.tile_width;
    const auto max_y = std::max_element(layout.positions.begin(), layout.positions.end(),
                                        [th = layout.tile_height](const auto& a, const auto& b) {
                                            return (a.y + th) < (b.y + th);
                                        })
                           ->y +
                       layout.tile_height;
    for (auto& pos : layout.positions) {
        pos.x -= min_x;
        pos.y -= min_y;
    }
    layout.logical_width = max_x - min_x;
    layout.logical_height = max_y - min_y;
    return layout;
}

bool diamond_contains(int x, int y, int tile_width, int tile_height) {
    return d2engine::terrain_surface_diamond_contains(x, y, tile_width, tile_height);
}

PreviewImage render_preview_image(int grid_width, int grid_height,
                                  const d2engine::AdventureTerrainSurfaceInput&    surface_input,
                                  const d2engine::AdventureTerrainSurfaceComposer& composer,
                                  const PreviewRenderOptions&                      options) {
    const auto   layout = make_preview_layout(grid_width, grid_height);
    PreviewImage image;
    image.logical_width = layout.logical_width;
    image.logical_height = layout.logical_height;
    if (layout.logical_width <= 0 || layout.logical_height <= 0) {
        return image;
    }

    const auto scale_factor = std::max(1, options.scale);
    const auto requested_w = layout.logical_width * scale_factor;
    const auto requested_h = layout.logical_height * scale_factor;
    const auto max_wh = std::max(requested_w, requested_h);
    const auto fit =
        options.max_size > 0
            ? std::min(1.0, static_cast<double>(options.max_size) / static_cast<double>(max_wh))
            : 1.0;
    image.output_width = std::max(1, static_cast<int>(std::floor(requested_w * fit)));
    image.output_height = std::max(1, static_cast<int>(std::floor(requested_h * fit)));

    if (options.mode == PreviewMode::Assets) {
        const auto full = composer.render_full_map(
            surface_input,
            {.tile_width = 64, .tile_height = 32, .include_base = true, .include_borders = true});
        if (!full.pixels.empty() && full.width > 0 && full.height > 0) {
            image.rgba = scaled_copy(full, image.output_width, image.output_height);
        }
        return image;
    }

    std::vector<uint8_t> logical(static_cast<std::size_t>(layout.logical_width) *
                                 static_cast<std::size_t>(layout.logical_height) * 4U);
    const auto           count = layout.positions.size();
    for (std::size_t i = 0; i < count; ++i) {
        if (i >= surface_input.resolved_tiles.size()) {
            continue;
        }
        const auto& resolved = surface_input.resolved_tiles[i];
        const auto& tile = resolved.descriptor;
        const auto& pos = layout.positions[i];
        if (options.mode == PreviewMode::LowByte) {
            draw_diamond(logical, layout.logical_width, layout.logical_height, pos.x, pos.y,
                         color_from_byte(tile.raw.low_byte));
        } else if (options.mode == PreviewMode::BorderShape) {
            draw_diamond(logical, layout.logical_width, layout.logical_height, pos.x, pos.y,
                         tile.raw.border_shape == 0 ? Color{24, 24, 24, 255}
                                                    : color_from_byte(tile.raw.border_shape));
        } else {
            draw_diamond(logical, layout.logical_width, layout.logical_height, pos.x, pos.y,
                         color_for_family(tile.family));
        }
    }

    image.rgba = scaled_copy(logical, layout.logical_width, layout.logical_height,
                             image.output_width, image.output_height);
    return image;
}

} // namespace d2terrain_preview
