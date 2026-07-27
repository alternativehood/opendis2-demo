#include "map_geometry.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace d2engine::adventure_render {

AdventureMapGeometry AdventureMapGeometry::from_source(int map_w, int map_h, int tw, int th) {
    AdventureMapGeometry g;
    g.map_width = map_w;
    g.map_height = map_h;
    g.tile_width = tw;
    g.tile_height = th;
    g.half_tile_width = tw / 2;
    g.half_tile_height = th / 2;
    g.min_world_x = -(map_h - 1) * g.half_tile_width;
    g.canvas_width = (map_w + map_h) * g.half_tile_width;
    g.canvas_height = (map_w + map_h) * g.half_tile_height;
    return g;
}

std::optional<MapCell> AdventureMapGeometry::canvas_to_cell(int canvas_x, int canvas_y) const {
    if (map_width <= 0 || map_height <= 0 || tile_width <= 0 || tile_height <= 0 ||
        half_tile_width <= 0 || half_tile_height <= 0)
        return std::nullopt;

    const auto world_x = static_cast<double>(static_cast<std::int64_t>(canvas_x) + min_world_x);
    const auto world_y = static_cast<double>(static_cast<std::int64_t>(canvas_y) + min_world_y);
    const auto projected_x = world_x / static_cast<double>(half_tile_width);
    const auto projected_y = world_y / static_cast<double>(half_tile_height);
    const auto estimate_x = static_cast<int>(std::floor((projected_x + projected_y) / 2.0));
    const auto estimate_y = static_cast<int>(std::floor((projected_y - projected_x) / 2.0));

    struct Candidate {
        MapCell cell;
        bool    inside = false;
    };
    std::array<Candidate, 9> candidates{};
    std::size_t              candidate_count = 0;
    const auto               add_candidate = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= map_width || y >= map_height)
            return;
        for (std::size_t i = 0; i < candidate_count; ++i) {
            if (candidates[i].cell == MapCell{x, y})
                return;
        }
        const auto         origin = cell_canvas_origin({x, y});
        const std::int64_t center_x = static_cast<std::int64_t>(origin.x) + half_tile_width;
        const std::int64_t center_y = static_cast<std::int64_t>(origin.y) + half_tile_height;
        const std::int64_t dx = std::llabs(static_cast<std::int64_t>(canvas_x) - center_x);
        const std::int64_t dy = std::llabs(static_cast<std::int64_t>(canvas_y) - center_y);
        const std::int64_t lhs = dx * half_tile_height + dy * half_tile_width;
        const std::int64_t rhs = static_cast<std::int64_t>(half_tile_width) * half_tile_height;
        candidates[candidate_count++] = {{x, y}, lhs <= rhs};
    };
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy)
            add_candidate(estimate_x + dx, estimate_y + dy);
    }

    std::optional<MapCell> result;
    for (std::size_t i = 0; i < candidate_count; ++i) {
        if (!candidates[i].inside)
            continue;
        if (!result || iso_depth(candidates[i].cell) > iso_depth(*result) ||
            (iso_depth(candidates[i].cell) == iso_depth(*result) &&
             (candidates[i].cell.x > result->x ||
              (candidates[i].cell.x == result->x && candidates[i].cell.y > result->y))))
            result = candidates[i].cell;
    }
    return result;
}

MapCell AdventureMapGeometry::derive_depth_anchor(const GridFootprint& fp) {
    MapCell anchor;
    int     best_depth = -1;
    for (const auto& cell : fp) {
        const int depth = iso_depth(cell);
        if (depth > best_depth || (depth == best_depth && cell.x > anchor.x) ||
            (depth == best_depth && cell.x == anchor.x && cell.y > anchor.y)) {
            best_depth = depth;
            anchor = cell;
        }
    }
    return anchor;
}

} // namespace d2engine::adventure_render
