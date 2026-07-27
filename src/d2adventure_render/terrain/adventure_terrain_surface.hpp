#pragma once

#include "adventure_terrain_types.hpp"
#include "adventure_terrain_border_shape_resolver.hpp"
#include "terrain_asset_catalog.hpp"

#include <d2res/rgba_buffer.hpp>
#include <d2runtime/AdventureTerrain.hpp>

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace d2engine {

class IImageStore;

struct AdventureTerrainSurfacePixel {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 0;
};

struct AdventureTerrainSurface {
    int                                       width = 64;
    int                                       height = 32;
    std::vector<AdventureTerrainSurfacePixel> pixels{};
};

struct AdventureTerrainSurfaceComposeOptions {
    int  tile_width = 64;
    int  tile_height = 32;
    bool include_base = true;
    bool include_borders = true;
};

struct AdventureTerrainSurfaceInput {
    int                                                    map_width = 0;
    int                                                    map_height = 0;
    std::vector<d2runtime::AdventureTerrainTileDescriptor> descriptors{};
    std::vector<ResolvedAdventureTerrainTile>              resolved_tiles{};
};

struct AdventureTerrainSurfaceCompositionInfo {
    struct BorderOperationInfo {
        std::string                       material_b_code{};
        std::string                       family{};
        std::string                       record_name{};
        std::uint8_t                      logical_shape = 0;
        std::uint8_t                      record_shape = 0;
        AdventureTerrainBorderComposeMode compose_mode =
            AdventureTerrainBorderComposeMode::MaskBlend;
        std::string  source = "none";
        std::uint8_t cardinal_mask = 0;
        std::uint8_t diagonal_mask = 0;
        std::string  dominance_relation{};
        bool         drawable = false;
    };

    std::string                           material_a_code{};
    std::string                           material_b_code{};
    std::string                           composer_border_family{};
    std::string                           composer_border_record{};
    d2runtime::AdventureTerrainBorderKind composer_border_kind =
        d2runtime::AdventureTerrainBorderKind::None;
    bool                                   ground_asset_found = false;
    bool                                   border_asset_found = false;
    AdventureTerrainNeighborTransitionMask neighbor_mask;
    std::string                            border_shape_source = "none";
    std::uint8_t                           logical_border_shape = 0;
    std::uint8_t                           resolved_record_shape = 0;
    std::string                            resolved_border_record{};
    bool                                   border_synthesized = false;
    bool                                   has_neighbor_transition = false;
    std::string                            topology_key{};
    std::uint8_t                           land_cardinal_mask = 0;
    std::uint8_t                           land_diagonal_mask = 0;
    std::uint8_t                           stronger_cardinal_mask = 0;
    std::uint8_t                           stronger_diagonal_mask = 0;
    std::uint8_t                           weaker_cardinal_mask = 0;
    std::uint8_t                           weaker_diagonal_mask = 0;
    std::uint8_t                           out_of_bounds_cardinal_mask = 0;
    std::uint8_t                           out_of_bounds_diagonal_mask = 0;
    std::uint8_t                           water_cardinal_mask = 0;
    std::uint8_t                           water_diagonal_mask = 0;
    std::string                            neighbor_material_layout{};
    std::string                            dominance_layout{};
    std::vector<BorderOperationInfo>       border_operations{};
};

using AdventureTerrainSurfaceImageMap = std::unordered_map<std::string, d2res::RgbaBuffer>;

[[nodiscard]] bool terrain_surface_diamond_contains(int x, int y, int tile_width = 64,
                                                    int tile_height = 32);
[[nodiscard]] AdventureTerrainNeighborTransitionMask
build_transition_mask(const AdventureTerrainSurfaceInput& input, int x, int y,
                      d2runtime::AdventureTerrainMaterial material_b);
struct TerrainDiamondRowSpan {
    int y;
    int x_begin;
    int x_end; // exclusive
};

struct TerrainDiamondSupport {
    int                                width = 64;
    int                                height = 32;
    std::vector<std::uint8_t>          alpha{};
    std::vector<std::pair<int, int>>   pixel_coords;
    std::vector<TerrainDiamondRowSpan> row_spans;
};

[[nodiscard]] TerrainDiamondSupport prepare_d2_diamond_support(int tile_width = 64,
                                                               int tile_height = 32);

// ── Prepared terrain map (immutable after preparation) ──────────────────────

struct PreparedCoveragePixel {
    std::uint8_t x;
    std::uint8_t y;
    std::uint8_t coverage;
};

struct PreparedNeMask {
    int                                width = 0;
    int                                height = 0;
    std::vector<PreparedCoveragePixel> nonzero_pixels;
};

struct PreparedCompositeNeMaskKey {
    std::vector<std::string> records;

    auto operator<=>(const PreparedCompositeNeMaskKey&) const = default;
};

struct PreparedCompositeNeMask {
    std::vector<PreparedCoveragePixel> nonzero_pixels;
};

struct PreparedWaPixel {
    std::uint8_t                 x;
    std::uint8_t                 y;
    AdventureTerrainSurfacePixel pixel;
};

struct PreparedWaSprite {
    std::vector<PreparedWaPixel> opaque_pixels;
};

struct TilePlacement {
    int                                 grid_x = 0;
    int                                 grid_y = 0;
    int                                 world_x = 0;
    int                                 world_y = 0;
    int                                 canvas_x = 0;
    int                                 canvas_y = 0;
    std::string                         terrain_code;
    d2runtime::AdventureTerrainMaterial material = d2runtime::AdventureTerrainMaterial::Unknown;
};

struct TileOperation {
    AdventureTerrainBorderComposeMode compose_mode = AdventureTerrainBorderComposeMode::MaskBlend;
    std::string                       family;
    std::string                       record_name;
    std::string                       source_material_code;
    std::string                       target_material_code;
    int                               tile_x = 0;
    int                               tile_y = 0;
};

struct PreparedTerrainBorderAssets {
    std::map<std::string, PreparedNeMask>                         individual_ne_masks;
    std::map<std::string, PreparedWaSprite>                       wa_sprites;
    std::map<PreparedCompositeNeMaskKey, PreparedCompositeNeMask> composite_ne_masks;
};

// NE/WA preparation and composite mask functions (exposed for test access)
[[nodiscard]] PreparedNeMask   prepare_ne_mask(const d2res::RgbaBuffer& buf);
[[nodiscard]] PreparedWaSprite prepare_wa_sprite(const d2res::RgbaBuffer& buf);
[[nodiscard]] PreparedCompositeNeMask
build_composite_ne_mask(const PreparedCompositeNeMaskKey&            key,
                        const std::map<std::string, PreparedNeMask>& individual_masks,
                        const TerrainDiamondSupport&                 support);

struct PreparedAdventureTerrainMap {
    // Move-only — internal pointers (commands reference fields in ground_fields,
    // wa_sprites, composite_ne_masks) that would dangle on default copy.
    PreparedAdventureTerrainMap() = default;
    ~PreparedAdventureTerrainMap() = default;
    PreparedAdventureTerrainMap(const PreparedAdventureTerrainMap&) = delete;
    PreparedAdventureTerrainMap& operator=(const PreparedAdventureTerrainMap&) = delete;
    PreparedAdventureTerrainMap(PreparedAdventureTerrainMap&&) = default;
    PreparedAdventureTerrainMap& operator=(PreparedAdventureTerrainMap&&) = default;

    TerrainDiamondSupport                   diamond;
    int                                     canvas_width = 0;
    int                                     canvas_height = 0;
    int                                     min_world_x = 0;
    int                                     min_world_y = 0;
    bool                                    include_base = true;
    std::vector<TilePlacement>              placements;
    std::vector<std::vector<TileOperation>> operations;
    PreparedTerrainBorderAssets             border_assets;

    // Regular world-space Ground patch grid for O(1) pixel lookup.
    struct PreparedGroundField {
        int patch_width = 0;
        int patch_height = 0;
        int first_patch_x = 0;
        int first_patch_y = 0;
        int columns = 0;
        int rows = 0;
        // Buffers kept alive for the lifetime of the field.
        std::vector<std::shared_ptr<const d2res::RgbaBuffer>> buffers;
        // Row-major grid; -1 = no patch, otherwise index into buffers.
        std::vector<int> patch_grid;
    };
    std::map<std::string, PreparedGroundField> ground_fields;

    // Base tile render command with pre-bound Ground field reference.
    struct PreparedBaseTileCommand {
        int                        canvas_x;
        int                        canvas_y;
        int                        world_x;
        int                        world_y;
        const PreparedGroundField* ground;
    };

    // WA base tiles (rendered first).
    std::vector<PreparedBaseTileCommand> wa_base_commands;
    // Land base tiles (rendered after WA sprites).
    std::vector<PreparedBaseTileCommand> land_base_commands;

    // WA sprite placement commands (one per border operation, not per pixel).
    struct PreparedWaSpritePlacement {
        int                     tile_canvas_x;
        int                     tile_canvas_y;
        const PreparedWaSprite* sprite;
    };
    std::vector<PreparedWaSpritePlacement> wa_sprite_placements;

    // NE transition render commands grouped by target material dominance.
    // Index is material_index() (1..7). 0 is unused.
    struct NeTransitionCommand {
        int                            tile_canvas_x;
        int                            tile_canvas_y;
        const PreparedCompositeNeMask* mask;
        const PreparedGroundField*     target_ground;
        std::uint8_t                   target_material_index;
    };
    std::array<std::vector<NeTransitionCommand>, 8> ne_transition_commands_by_material;
};

// ── AdventureTerrainSurfaceComposer ─────────────────────────────────────────
//
// NOTE: This class does NOT own a source image cache for FF assets.
// All source asset access goes through FfAssetStore (central cache).
// images_/missing_ have been removed per architecture:
// see docs/architecture/ff_asset_access.md.
//
// Variant selection uses TerrainAssetCatalog (real inventory) — no hardcoded
// variant counts, no probing.

class AdventureTerrainSurfaceComposer {
public:
    explicit AdventureTerrainSurfaceComposer(
        const IImageStore& store, const TerrainAssetCatalog& catalog,
        AdventureTerrainBorderShapeResolver border_shape_resolver =
            AdventureTerrainBorderShapeResolver{});

    // Convenience: construct from pre-loaded image map (testing / offline use).
    // Internally wraps the map in an owned IImageStore implementation.
    explicit AdventureTerrainSurfaceComposer(
        AdventureTerrainSurfaceImageMap     images,
        const TerrainAssetCatalog&          catalog = TerrainAssetCatalog{},
        AdventureTerrainBorderShapeResolver border_shape_resolver =
            AdventureTerrainBorderShapeResolver{});

    // One-shot convenience wrapper
    [[nodiscard]] AdventureTerrainSurface
    render_full_map(const AdventureTerrainSurfaceInput&          input,
                    const AdventureTerrainSurfaceComposeOptions& options = {}) const;

    // Two-phase lifecycle: prepare once, render many
    [[nodiscard]] PreparedAdventureTerrainMap
    prepare_full_map(const AdventureTerrainSurfaceInput&          input,
                     const AdventureTerrainSurfaceComposeOptions& options = {}) const;

    [[nodiscard]] AdventureTerrainSurface
    render_prepared_full_map(const PreparedAdventureTerrainMap& prepared) const;

    // Diagnostic — describes a single tile (compose_tile deprecated in production)
    [[nodiscard]] AdventureTerrainSurfaceCompositionInfo
    describe_tile(const AdventureTerrainSurfaceInput& input, int x, int y,
                  const AdventureTerrainSurfaceComposeOptions& options = {}) const;

    // Immutable image view — looks up from central FfAssetStore cache.
    [[nodiscard]] const d2res::RgbaBuffer*
    image_view(const d2runtime::AdventureTerrainAssetRef& asset) const;

    // Shared handle to cached decoded PNG (keeps buffer alive).
    [[nodiscard]] std::shared_ptr<const d2res::RgbaBuffer>
    image_handle(const d2runtime::AdventureTerrainAssetRef& asset) const;

private:
    std::shared_ptr<IImageStore>        owned_store_;
    const IImageStore*                  store_ = nullptr;
    TerrainAssetCatalog                 catalog_;
    AdventureTerrainBorderShapeResolver border_shape_resolver_;
};

} // namespace d2engine
