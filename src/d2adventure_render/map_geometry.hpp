#pragma once

#include "adventure_render_types.hpp"

#include <d2runtime/MapCellCoord.hpp>

#include <cstdint>
#include <optional>

namespace d2engine::adventure_render {

struct CellInteractionMetrics {
    int region_x_min = -24;
    int region_x_max = 24;
    int region_y_min = -34;
    int region_y_max = 14;

    [[nodiscard]] int  width() const { return region_x_max - region_x_min; }
    [[nodiscard]] int  height() const { return region_y_max - region_y_min; }
    [[nodiscard]] bool contains(int dx, int dy) const {
        return dx >= region_x_min && dx < region_x_max && dy >= region_y_min && dy < region_y_max;
    }
};

struct SelectionCircleGeometry {
    int center_offset_x = 0;
    int center_offset_y = -9;
    int radius_x = 21;
    int radius_y = 10;

    [[nodiscard]] bool contains(int canvas_x, int canvas_y, const ScreenPoint& cell_foot) const {
        if (radius_x <= 0 || radius_y <= 0)
            return false;
        const int64_t dx =
            static_cast<int64_t>(canvas_x) - (static_cast<int64_t>(cell_foot.x) + center_offset_x);
        const int64_t dy =
            static_cast<int64_t>(canvas_y) - (static_cast<int64_t>(cell_foot.y) + center_offset_y);
        const int64_t rx = radius_x;
        const int64_t ry = radius_y;
        return dx * dx * ry * ry + dy * dy * rx * rx <= rx * rx * ry * ry;
    }
};

struct AdventureMapGeometry {
    int map_width = 0;
    int map_height = 0;
    int tile_width = 64;
    int tile_height = 32;
    int half_tile_width = 32;
    int half_tile_height = 16;
    int min_world_x = 0;
    int min_world_y = 0;
    int canvas_width = 0;
    int canvas_height = 0;

    static AdventureMapGeometry from_source(int map_w, int map_h, int tw = 64, int th = 32);

    [[nodiscard]] ScreenPoint project_cell(MapCell cell) const {
        return {(cell.x - cell.y) * half_tile_width, (cell.x + cell.y) * half_tile_height};
    }
    [[nodiscard]] static int  iso_depth(MapCell cell) { return cell.x + cell.y; }
    [[nodiscard]] ScreenPoint cell_canvas_origin(MapCell cell) const {
        const auto tile = project_cell(cell);
        return {tile.x - min_world_x, tile.y - min_world_y};
    }
    [[nodiscard]] ScreenPoint cell_foot_anchor(MapCell cell) const {
        const auto tile = project_cell(cell);
        return {tile.x - min_world_x + half_tile_width, tile.y - min_world_y + tile_height};
    }
    [[nodiscard]] std::optional<MapCell> canvas_to_cell(int canvas_x, int canvas_y) const;
    [[nodiscard]] static MapCell         derive_depth_anchor(const GridFootprint& fp);
};

} // namespace d2engine::adventure_render
