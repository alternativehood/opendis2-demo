#pragma once

#include <d2engine/assets/adventure_terrain_asset_resolver.hpp>
#include <d2adventure_render/terrain/adventure_terrain_surface.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace d2terrain_preview {

enum class PreviewMode {
    Assets,
    LowByte,
    BorderShape,
    FamilyId,
};

struct TileScreenPosition {
    int x = 0;
    int y = 0;
};

struct PreviewLayout {
    int                             tile_width = 64;
    int                             tile_height = 32;
    int                             logical_width = 0;
    int                             logical_height = 0;
    std::vector<TileScreenPosition> positions;
};

struct PreviewRenderOptions {
    PreviewMode mode = PreviewMode::Assets;
    int         scale = 1;
    int         max_size = 4096;
};

struct PreviewImage {
    int                  logical_width = 0;
    int                  logical_height = 0;
    int                  output_width = 0;
    int                  output_height = 0;
    std::vector<uint8_t> rgba;
};

[[nodiscard]] PreviewLayout make_preview_layout(int grid_width, int grid_height);
[[nodiscard]] bool diamond_contains(int x, int y, int tile_width = 64, int tile_height = 32);
[[nodiscard]] PreviewImage render_preview_image(
    int grid_width, int grid_height, const d2engine::AdventureTerrainSurfaceInput& surface_input,
    const d2engine::AdventureTerrainSurfaceComposer& composer, const PreviewRenderOptions& options);

[[nodiscard]] PreviewImage preview_from_surface(const d2engine::AdventureTerrainSurface& surface,
                                                int                                      max_size);

} // namespace d2terrain_preview
