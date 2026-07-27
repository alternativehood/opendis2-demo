#include "adventure_terrain_surface.hpp"

#include "adventure_terrain_variant_hash.hpp"
#include <d2adventure_render/image_store.hpp>
#include "terrain_asset_catalog.hpp"

#include <d2log/log.hpp>
#include <d2res/rgba_buffer.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <utility>

namespace {

// IImageStore implementation that wraps a pre-loaded image map.
// Used by AdventureTerrainSurfaceComposer(AdventureTerrainSurfaceImageMap).
class MapImageStore final : public d2engine::IImageStore {
public:
    explicit MapImageStore(d2engine::AdventureTerrainSurfaceImageMap map) : map_(std::move(map)) {}

    std::shared_ptr<const d2res::RgbaBuffer> raw_png(std::string_view container,
                                                     std::string_view record) const override {
        auto key = std::string(container) + "/" + std::string(record);
        auto it = map_.find(key);
        if (it != map_.end()) {
            // Cache the shared_ptr to keep buffer alive (callers store raw pointers
            // from .get() and rely on the store keeping the shared_ptr alive).
            auto& cached = cache_[key];
            if (!cached) {
                cached = std::make_shared<const d2res::RgbaBuffer>(it->second);
            }
            return cached;
        }
        return nullptr;
    }

    std::optional<d2res::RgbaBuffer> copy_raw_png(std::string_view container,
                                                  std::string_view record) const override {
        auto key = std::string(container) + "/" + std::string(record);
        auto it = map_.find(key);
        if (it != map_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

private:
    d2engine::AdventureTerrainSurfaceImageMap                                         map_;
    mutable std::unordered_map<std::string, std::shared_ptr<const d2res::RgbaBuffer>> cache_;
};

} // namespace

namespace d2engine {

auto kLog = d2log::get("d2.terrain"); // NOLINT(cert-err58-cpp)

// ── NE mask preparation (sparse only, preserves raw authored data) ───────────

PreparedNeMask prepare_ne_mask(const d2res::RgbaBuffer& buf) {
    PreparedNeMask mask;
    mask.width = static_cast<int>(buf.width);
    mask.height = static_cast<int>(buf.height);
    const auto count = static_cast<std::size_t>(mask.width) * static_cast<std::size_t>(mask.height);
    mask.nonzero_pixels.reserve(count / 4);
    for (int py = 0; py < mask.height; ++py) {
        for (int px = 0; px < mask.width; ++px) {
            const auto* pxp =
                buf.rgba.data() +
                (static_cast<std::size_t>(py) * buf.width + static_cast<std::size_t>(px)) * 4U;
            if (pxp[3] == 0) {
                continue;
            }
            if (d2res::is_magenta_key_pixel(pxp[0], pxp[1], pxp[2])) {
                continue;
            }
            const auto luma = static_cast<std::uint8_t>(
                (static_cast<int>(pxp[0]) + static_cast<int>(pxp[1]) + static_cast<int>(pxp[2])) /
                3);
            if (luma == 0) {
                continue;
            }
            mask.nonzero_pixels.push_back(
                {static_cast<std::uint8_t>(px), static_cast<std::uint8_t>(py), luma});
        }
    }
    return mask;
}

// ── WA sprite preparation (sparse only, NO diamond clip) ─────────────────────

PreparedWaSprite prepare_wa_sprite(const d2res::RgbaBuffer& buf) {
    PreparedWaSprite sprite;
    sprite.opaque_pixels.reserve((static_cast<std::size_t>(buf.width) * buf.height) / 4);
    for (std::uint32_t py = 0; py < buf.height; ++py) {
        for (std::uint32_t px = 0; px < buf.width; ++px) {
            const auto* sp =
                buf.rgba.data() +
                (static_cast<std::size_t>(py) * buf.width + static_cast<std::size_t>(px)) * 4U;
            if (sp[3] == 0) {
                continue;
            }
            sprite.opaque_pixels.push_back({static_cast<std::uint8_t>(px),
                                            static_cast<std::uint8_t>(py),
                                            {sp[0], sp[1], sp[2], sp[3]}});
        }
    }
    return sprite;
}

// ── Composite NE mask cache ─────────────────────────────────────────────────

PreparedCompositeNeMask
build_composite_ne_mask(const PreparedCompositeNeMaskKey&            key,
                        const std::map<std::string, PreparedNeMask>& individual_masks,
                        const TerrainDiamondSupport&                 support) {
    const auto dense_size =
        static_cast<std::size_t>(support.width) * static_cast<std::size_t>(support.height);
    std::vector<std::uint8_t> dense(dense_size, 0);

    for (const auto& rec : key.records) {
        const auto it = individual_masks.find(rec);
        if (it == individual_masks.end()) {
            continue;
        }

        if (it->second.width != support.width || it->second.height != support.height) {
            D2_LOG_WARN(kLog,
                        "terrain: NE mask dimension mismatch for {}: got {}x{}, expected {}x{}; "
                        "mask skipped",
                        rec, it->second.width, it->second.height, support.width, support.height);
            continue;
        }

        for (const auto& pp : it->second.nonzero_pixels) {
            const auto idx =
                static_cast<std::size_t>(pp.y) * static_cast<std::size_t>(support.width) +
                static_cast<std::size_t>(pp.x);
            if (pp.coverage > dense[idx]) {
                dense[idx] = pp.coverage;
            }
        }
    }

    PreparedCompositeNeMask composite;
    composite.nonzero_pixels.reserve(support.pixel_coords.size() / 2);
    for (const auto& [sx, sy] : support.pixel_coords) {
        const auto idx = static_cast<std::size_t>(sy) * static_cast<std::size_t>(support.width) +
                         static_cast<std::size_t>(sx);
        const auto v = dense[idx];
        if (v == 0) {
            continue;
        }
        composite.nonzero_pixels.push_back(
            {static_cast<std::uint8_t>(sx), static_cast<std::uint8_t>(sy), v});
    }
    return composite;
}

namespace {
// NOLINTNEXTLINE(cert-err58-cpp)

struct MissingTerrainAssetLogKey {
    std::string record_name;
    std::string family;
    std::string compose_mode;
    std::string source_material_code;
    std::string target_material_code;

    auto operator<(const MissingTerrainAssetLogKey& o) const noexcept -> bool {
        if (record_name != o.record_name)
            return record_name < o.record_name;
        if (family != o.family)
            return family < o.family;
        if (compose_mode != o.compose_mode)
            return compose_mode < o.compose_mode;
        if (source_material_code != o.source_material_code)
            return source_material_code < o.source_material_code;
        return target_material_code < o.target_material_code;
    }
};

struct MissingTerrainAssetLogState {
    std::set<MissingTerrainAssetLogKey> logged;
};

void log_missing_terrain_border_asset_once(MissingTerrainAssetLogState& state,
                                           const std::string& record_name, std::string_view family,
                                           AdventureTerrainBorderComposeMode compose_mode,
                                           std::string_view                  source_material_code,
                                           std::string_view target_material_code, int tile_x,
                                           int tile_y) {
    MissingTerrainAssetLogKey key;
    key.record_name = record_name;
    key.family = family;
    switch (compose_mode) {
    case AdventureTerrainBorderComposeMode::ColorKeyOverlay:
        key.compose_mode = "ColorKeyOverlay";
        break;
    case AdventureTerrainBorderComposeMode::MaskBlend:
        key.compose_mode = "MaskBlend";
        break;
    }
    key.source_material_code = source_material_code;
    key.target_material_code = target_material_code;

    if (state.logged.find(key) != state.logged.end()) {
        return;
    }
    const std::string compose_mode_str = key.compose_mode;
    state.logged.insert(std::move(key));

    D2_LOG_WARN(kLog,
                "terrain: missing GrBorder asset for terrain border operation: "
                "record={} family={} mode={} source={} target={} tile=({},{})",
                record_name, family, compose_mode_str, source_material_code, target_material_code,
                tile_x, tile_y);
}

struct NeighborCandidate {
    d2runtime::AdventureTerrainMaterial material = d2runtime::AdventureTerrainMaterial::Unknown;
    std::string                         code = {};
    int                                 first_order = 0;
    int                                 count = 0;
};

struct MaterialChoice {
    std::string                         code = {};
    d2runtime::AdventureTerrainMaterial material = d2runtime::AdventureTerrainMaterial::Unknown;
};

struct TopologyNeighbor {
    int                                              dx = 0;
    int                                              dy = 0;
    std::uint8_t                                     cardinal_bit = 0;
    std::uint8_t                                     diagonal_bit = 0;
    const d2runtime::AdventureTerrainTileDescriptor* descriptor = nullptr;
};

struct BorderOperationPlan {
    std::string                         material_b_code = {};
    d2runtime::AdventureTerrainMaterial material_b = d2runtime::AdventureTerrainMaterial::Unknown;
    AdventureTerrainResolvedBorderOperation operation;
    AdventureTerrainTopology3x3Key          key;
    int                                     target_rank = 0;
    std::string                             dominance_relation = {};
    d2runtime::AdventureTerrainBorderKind   kind = d2runtime::AdventureTerrainBorderKind::None;
};

constexpr std::array<std::pair<int, int>, 8> kNeighborOrder = {
    std::pair{0, -1},  std::pair{1, 0},  std::pair{0, 1}, std::pair{-1, 0},
    std::pair{-1, -1}, std::pair{1, -1}, std::pair{1, 1}, std::pair{-1, 1},
};

std::string record2(std::string_view prefix, int value, std::string_view suffix) {
    char buffer[32] = {};
    std::snprintf(buffer, sizeof(buffer), "%.*s_%02d%.*s", static_cast<int>(prefix.size()),
                  prefix.data(), value, static_cast<int>(suffix.size()), suffix.data());
    return buffer;
}

d2runtime::AdventureTerrainAssetRef ground_asset(std::string_view code, int variant) {
    return {.container_path = "Imgs/Ground.ff", .record_name = record2(code, variant, ".PNG")};
}

d2runtime::AdventureTerrainAssetRef border_asset(std::string record) {
    return {.container_path = "Imgs/GrBorder.ff", .record_name = std::move(record)};
}

const d2runtime::AdventureTerrainTileDescriptor*
descriptor_at(const AdventureTerrainSurfaceInput& input, int x, int y) {
    if (x < 0 || y < 0 || x >= input.map_width || y >= input.map_height)
        return nullptr;
    const auto index = (static_cast<std::size_t>(y) * static_cast<std::size_t>(input.map_width)) +
                       static_cast<std::size_t>(x);
    if (index >= input.descriptors.size())
        return nullptr;
    return &input.descriptors[index];
}

bool is_land_material(d2runtime::AdventureTerrainMaterial material) {
    using Material = d2runtime::AdventureTerrainMaterial;
    return material == Material::Human || material == Material::Dwarf ||
           material == Material::Heretic || material == Material::Undead ||
           material == Material::Neutral || material == Material::Elf;
}

bool is_water_material(d2runtime::AdventureTerrainMaterial material) {
    return material == d2runtime::AdventureTerrainMaterial::Water;
}

// ── Straight-alpha source-over ────────────────────────────────────────────────

void source_over(AdventureTerrainSurfacePixel& dst, const AdventureTerrainSurfacePixel& src) {
    if (src.a == 0) {
        return;
    }
    if (dst.a == 0) {
        dst = src;
        return;
    }
    const int sa = static_cast<int>(src.a);
    const int da = static_cast<int>(dst.a);
    // Correct straight-alpha: out = src * sa + dst * (1 - sa)  (alpha-premultiplied)
    // But our RGB is non-premultiplied, so: out_r = (src_r * sa + dst_r * da * (255-sa)/255) / 255
    const int inv_sa = 255 - sa;
    dst.r = static_cast<std::uint8_t>(
        (static_cast<int>(src.r) * sa + static_cast<int>(dst.r) * inv_sa) / 255);
    dst.g = static_cast<std::uint8_t>(
        (static_cast<int>(src.g) * sa + static_cast<int>(dst.g) * inv_sa) / 255);
    dst.b = static_cast<std::uint8_t>(
        (static_cast<int>(src.b) * sa + static_cast<int>(dst.b) * inv_sa) / 255);
    dst.a = static_cast<std::uint8_t>(std::min(255, sa + da * inv_sa / 255));
}

// ── Precomputed topology decomposition table ────────────────────────────────

struct TopologyDecomposition {
    std::vector<std::pair<std::uint8_t, std::uint8_t>> parts;
};

const std::array<TopologyDecomposition, 256>& topology_decomposition_table() {
    static constexpr std::array<std::uint8_t, 30> keys = {
        0x10, 0x20, 0x30, 0x40, 0x5F, 0x67, 0x7F, 0x80, 0x9D, 0xAF, 0xBF, 0xCE, 0xDF, 0xEF, 0xF0,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    };

    static const auto table = []() {
        std::array<TopologyDecomposition, 256> tbl;
        std::vector<std::uint8_t>              sorted_keys(keys.begin(), keys.end());
        std::ranges::sort(sorted_keys, [](std::uint8_t lhs, std::uint8_t rhs) {
            const auto lb = std::popcount(lhs);
            const auto rb = std::popcount(rhs);
            if (lb != rb)
                return lb > rb;
            return lhs < rhs;
        });

        for (int mi = 1; mi < 256; ++mi) {
            const auto m = static_cast<std::size_t>(mi);
            const auto actual = static_cast<std::uint8_t>(mi);
            const auto cardinal = static_cast<std::uint8_t>(actual & 0x0F);
            const auto diagonal = static_cast<std::uint8_t>(actual >> 4U);
            // In isometric projection grid-diagonal offsets (dx,dy both ±1)
            // map to display-space N/E/S/W and are stored in cardinal_mask.
            // Grid-axis offsets (one of dx,dy is 0) map to display-space
            // NW/NE/SE/SW and are stored in diagonal_mask.
            // Full display-space diagonal quartet (NW+NE+SE+SW) is a
            // terminal shape 15. Any additional cardinal bits are suppressed.
            if (diagonal == 0x0F) {
                tbl[m].parts.emplace_back(0x00, 0x0F);
                continue;
            }
            if (std::ranges::find(keys, actual) != keys.end()) {
                tbl[m].parts.emplace_back(cardinal, diagonal);
                continue;
            }
            auto remaining = actual;
            while (remaining != 0) {
                const auto it = std::ranges::find_if(
                    sorted_keys, [remaining](std::uint8_t k) { return (k & remaining) == k; });
                if (it == sorted_keys.end())
                    break;
                const auto key = *it;
                tbl[m].parts.emplace_back(key & 0x0F, key >> 4U);
                remaining = static_cast<std::uint8_t>(remaining & ~key);
            }
        }
        return tbl;
    }();
    return table;
}

int land_dominance_rank(std::string_view terrain_code) {
    if (terrain_code == "DW")
        return 6;
    if (terrain_code == "EL")
        return 5;
    if (terrain_code == "HU")
        return 4;
    if (terrain_code == "UN")
        return 3;
    if (terrain_code == "HE")
        return 2;
    if (terrain_code == "NE")
        return 1;
    return 0;
}

std::uint8_t material_index(std::string_view terrain_code) {
    if (terrain_code == "WA")
        return 1;
    if (terrain_code == "NE")
        return 2;
    if (terrain_code == "HE")
        return 3;
    if (terrain_code == "UN")
        return 4;
    if (terrain_code == "HU")
        return 5;
    if (terrain_code == "EL")
        return 6;
    if (terrain_code == "DW")
        return 7;
    return 0;
}

MaterialChoice choose_material_b(const AdventureTerrainSurfaceInput&              input,
                                 const d2runtime::AdventureTerrainTileDescriptor& current, int x,
                                 int y) {
    std::map<d2runtime::AdventureTerrainMaterial, NeighborCandidate> candidates;
    for (std::size_t i = 0; i < kNeighborOrder.size(); ++i) {
        const auto* neighbor =
            descriptor_at(input, x + kNeighborOrder[i].first, y + kNeighborOrder[i].second);
        if (neighbor == nullptr || neighbor->material == current.material) {
            continue;
        }
        if (neighbor->material == d2runtime::AdventureTerrainMaterial::Water) {
            return {.code = neighbor->terrain_code, .material = neighbor->material};
        }
        auto& candidate = candidates[neighbor->material];
        if (candidate.count == 0) {
            candidate.material = neighbor->material;
            candidate.code = neighbor->terrain_code;
            candidate.first_order = static_cast<int>(i);
        }
        ++candidate.count;
    }
    if (candidates.empty()) {
        return {.code = current.terrain_code, .material = current.material};
    }
    const auto best = std::ranges::max_element(candidates, [](const auto& lhs, const auto& rhs) {
        if (lhs.second.count != rhs.second.count)
            return lhs.second.count < rhs.second.count;
        return lhs.second.first_order > rhs.second.first_order;
    });
    return {.code = best->second.code, .material = best->second.material};
}

// In isometric projection grid-diagonal offsets (dx,dy both ±1) map to
// display-space N/E/S/W and populate cardinal_mask.
// Grid-axis offsets (one of dx,dy is 0) map to display-space NW/NE/SE/SW
// and populate diagonal_mask.
std::array<TopologyNeighbor, 8> topology_neighbors(const AdventureTerrainSurfaceInput& input, int x,
                                                   int y) {
    static constexpr std::array<TopologyNeighbor, 8> specs = {
        TopologyNeighbor{-1, -1, 0x01, 0}, TopologyNeighbor{1, -1, 0x02, 0},
        TopologyNeighbor{1, 1, 0x04, 0},   TopologyNeighbor{-1, 1, 0x08, 0},
        TopologyNeighbor{-1, 0, 0, 0x01},  TopologyNeighbor{0, -1, 0, 0x02},
        TopologyNeighbor{1, 0, 0, 0x04},   TopologyNeighbor{0, 1, 0, 0x08},
    };

    std::array<TopologyNeighbor, 8> neighbors;
    for (std::size_t i = 0; i < specs.size(); ++i) {
        neighbors[i] = specs[i];
        neighbors[i].descriptor = descriptor_at(input, x + specs[i].dx, y + specs[i].dy);
    }
    return neighbors;
}

std::string neighbor_material_layout(const std::array<TopologyNeighbor, 8>& neighbors) {
    std::ostringstream out;
    bool               first = true;
    for (const auto& n : neighbors) {
        if (!first) {
            out << ",";
        }
        first = false;
        out << (n.descriptor == nullptr ? "OOB" : n.descriptor->terrain_code);
    }
    return out.str();
}

std::string dominance_layout(const d2runtime::AdventureTerrainTileDescriptor& center,
                             const std::array<TopologyNeighbor, 8>&           neighbors) {
    std::ostringstream out;
    const auto         center_rank = land_dominance_rank(center.terrain_code);
    bool               first = true;
    for (const auto& n : neighbors) {
        if (!first) {
            out << ",";
        }
        first = false;
        if (n.descriptor == nullptr) {
            out << "out";
            continue;
        }
        if (!is_land_material(center.material) || !is_land_material(n.descriptor->material)) {
            out << "non_land";
            continue;
        }
        const int other_rank = land_dominance_rank(n.descriptor->terrain_code);
        if (other_rank > center_rank) {
            out << "stronger";
        } else if (other_rank < center_rank) {
            out << "weaker";
        } else {
            out << "same";
        }
    }
    return out.str();
}

std::string topology_key_string(const d2runtime::AdventureTerrainTileDescriptor& center,
                                const std::vector<BorderOperationPlan>&          operations,
                                std::uint8_t land_cardinal, std::uint8_t land_diagonal,
                                std::uint8_t stronger_cardinal, std::uint8_t stronger_diagonal,
                                std::uint8_t weaker_cardinal, std::uint8_t weaker_diagonal,
                                std::uint8_t out_cardinal, std::uint8_t out_diagonal,
                                std::uint8_t water_cardinal, std::uint8_t water_diagonal) {
    char buffer[192] = {};
    std::snprintf(buffer, sizeof(buffer),
                  "center=%s;land=%02x/%02x;stronger=%02x/%02x;weaker=%02x/%02x;"
                  "out=%02x/%02x;water=%02x/%02x;ops=%zu",
                  center.terrain_code.c_str(), land_cardinal, land_diagonal, stronger_cardinal,
                  stronger_diagonal, weaker_cardinal, weaker_diagonal, out_cardinal, out_diagonal,
                  water_cardinal, water_diagonal, operations.size());
    return buffer;
}

void add_topology_operation(const AdventureTerrainBorderShapeResolver&       resolver,
                            std::vector<BorderOperationPlan>&                operations,
                            const d2runtime::AdventureTerrainTileDescriptor& center,
                            std::string                                      target_code,
                            d2runtime::AdventureTerrainMaterial /*target_material*/,
                            std::string family, std::uint8_t cardinal_mask,
                            std::uint8_t diagonal_mask, std::uint8_t stronger_cardinal,
                            std::uint8_t stronger_diagonal, std::uint8_t weaker_cardinal,
                            std::uint8_t weaker_diagonal, std::uint8_t out_cardinal,
                            std::uint8_t out_diagonal, std::string dominance_relation) {
    AdventureTerrainTopology3x3Key key;
    key.center_code = center.terrain_code;
    key.target_code = std::move(target_code);
    key.family = std::move(family);
    key.cardinal_mask = cardinal_mask;
    key.diagonal_mask = diagonal_mask;
    key.stronger_cardinal_mask = stronger_cardinal;
    key.stronger_diagonal_mask = stronger_diagonal;
    key.weaker_cardinal_mask = weaker_cardinal;
    key.weaker_diagonal_mask = weaker_diagonal;
    key.out_of_bounds_cardinal_mask = out_cardinal;
    key.out_of_bounds_diagonal_mask = out_diagonal;

    const auto resolved = resolver.resolve_topology3x3_operation(key);
    if (!resolved.has_value()) {
        return;
    }
    const auto rank = land_dominance_rank(key.target_code);
    operations.push_back({.material_b_code = std::move(key.target_code),
                          .material_b = d2runtime::AdventureTerrainMaterial::Unknown,
                          .operation = *resolved,
                          .key = std::move(key),
                          .target_rank = rank,
                          .dominance_relation = std::move(dominance_relation),
                          .kind = d2runtime::AdventureTerrainBorderKind::Drawable});
}

std::vector<BorderOperationPlan> build_topology3x3_operations(
    const AdventureTerrainBorderShapeResolver& resolver, const AdventureTerrainSurfaceInput& input,
    int x, int y, const d2runtime::AdventureTerrainTileDescriptor& center,
    std::uint8_t* land_cardinal, std::uint8_t* land_diagonal, std::uint8_t* stronger_cardinal,
    std::uint8_t* stronger_diagonal, std::uint8_t* weaker_cardinal, std::uint8_t* weaker_diagonal,
    std::uint8_t* out_cardinal, std::uint8_t* out_diagonal, std::uint8_t* water_cardinal,
    std::uint8_t* water_diagonal) {
    std::vector<BorderOperationPlan>                      ops;
    std::map<std::string, AdventureTerrainTopology3x3Key> land_masks;
    AdventureTerrainTopology3x3Key                        water_mask;
    water_mask.center_code = center.terrain_code;
    water_mask.family = "WA";

    const auto center_rank = land_dominance_rank(center.terrain_code);
    for (const auto& neighbor : topology_neighbors(input, x, y)) {
        if (neighbor.descriptor == nullptr) {
            *out_cardinal = static_cast<std::uint8_t>(*out_cardinal | neighbor.cardinal_bit);
            *out_diagonal = static_cast<std::uint8_t>(*out_diagonal | neighbor.diagonal_bit);
            continue;
        }
        const auto& other = *neighbor.descriptor;
        if (is_land_material(other.material) && other.terrain_code != center.terrain_code) {
            const int other_rank = land_dominance_rank(other.terrain_code);
            if (!is_land_material(center.material) || other_rank > center_rank) {
                auto& mk = land_masks[other.terrain_code];
                mk.center_code = center.terrain_code;
                mk.target_code = other.terrain_code;
                mk.family = "NE";
                mk.cardinal_mask =
                    static_cast<std::uint8_t>(mk.cardinal_mask | neighbor.cardinal_bit);
                mk.diagonal_mask =
                    static_cast<std::uint8_t>(mk.diagonal_mask | neighbor.diagonal_bit);
            }
            if (is_land_material(center.material)) {
                *land_cardinal = static_cast<std::uint8_t>(*land_cardinal | neighbor.cardinal_bit);
                *land_diagonal = static_cast<std::uint8_t>(*land_diagonal | neighbor.diagonal_bit);
                if (other_rank > center_rank) {
                    *stronger_cardinal =
                        static_cast<std::uint8_t>(*stronger_cardinal | neighbor.cardinal_bit);
                    *stronger_diagonal =
                        static_cast<std::uint8_t>(*stronger_diagonal | neighbor.diagonal_bit);
                } else if (other_rank < center_rank) {
                    *weaker_cardinal =
                        static_cast<std::uint8_t>(*weaker_cardinal | neighbor.cardinal_bit);
                    *weaker_diagonal =
                        static_cast<std::uint8_t>(*weaker_diagonal | neighbor.diagonal_bit);
                }
            }
        }
        if (is_water_material(center.material) && !is_water_material(other.material)) {
            water_mask.target_code = other.terrain_code;
            water_mask.cardinal_mask =
                static_cast<std::uint8_t>(water_mask.cardinal_mask | neighbor.cardinal_bit);
            water_mask.diagonal_mask =
                static_cast<std::uint8_t>(water_mask.diagonal_mask | neighbor.diagonal_bit);
            *water_cardinal = static_cast<std::uint8_t>(*water_cardinal | neighbor.cardinal_bit);
            *water_diagonal = static_cast<std::uint8_t>(*water_diagonal | neighbor.diagonal_bit);
        }
    }

    for (const auto& [target_code, key] : land_masks) {
        const auto& decomp = topology_decomposition_table()[static_cast<std::size_t>(
            key.cardinal_mask | (key.diagonal_mask << 4U))];
        for (const auto& [cardinal, diagonal] : decomp.parts) {
            add_topology_operation(
                resolver, ops, center, target_code, d2runtime::AdventureTerrainMaterial::Unknown,
                "NE", cardinal, diagonal, *stronger_cardinal, *stronger_diagonal, *weaker_cardinal,
                *weaker_diagonal, *out_cardinal, *out_diagonal, "stronger");
        }
    }
    if (water_mask.cardinal_mask != 0 || water_mask.diagonal_mask != 0) {
        const auto& decomp = topology_decomposition_table()[static_cast<std::size_t>(
            water_mask.cardinal_mask | (water_mask.diagonal_mask << 4U))];
        for (const auto& [cardinal, diagonal] : decomp.parts) {
            add_topology_operation(resolver, ops, center, water_mask.target_code,
                                   d2runtime::AdventureTerrainMaterial::Unknown, "WA", cardinal,
                                   diagonal, *stronger_cardinal, *stronger_diagonal,
                                   *weaker_cardinal, *weaker_diagonal, *out_cardinal, *out_diagonal,
                                   "water");
        }
    }

    std::ranges::stable_sort(ops, [](const auto& lhs, const auto& rhs) {
        if (lhs.operation.compose_mode != rhs.operation.compose_mode)
            return lhs.operation.compose_mode == AdventureTerrainBorderComposeMode::MaskBlend;
        if (lhs.target_rank != rhs.target_rank)
            return lhs.target_rank > rhs.target_rank;
        return lhs.material_b_code < rhs.material_b_code;
    });
    return ops;
}

// ── Floor div / positive mod helpers ────────────────────────────────────────

int floor_div(int a, int b) {
    if (b <= 0) {
        return 0;
    }
    if (a >= 0) {
        return a / b;
    }
    return (a - b + 1) / b;
}

int floor_mod(int a, int b) {
    if (b <= 0) {
        return 0;
    }
    int r = a % b;
    if (r < 0) {
        r += b;
    }
    return r;
}

// ── Ground variant: fixed-width uint64_t deterministic hash ─────────────────

int ground_variant_for_patch(const std::string& material_code, int patch_x, int patch_y,
                             const std::vector<int>& available_variants) {
    const int n = static_cast<int>(available_variants.size());
    if (n <= 1) {
        return n == 0 ? 0 : available_variants[0];
    }
    std::uint64_t h = 0;
    for (char c : material_code)
        h = (h * 31) + static_cast<std::uint64_t>(static_cast<char>(c));
    h = (h * 31) + static_cast<std::uint64_t>(static_cast<std::int64_t>(patch_x));
    h = (h * 31) + static_cast<std::uint64_t>(static_cast<std::int64_t>(patch_y));
    return available_variants[static_cast<std::size_t>(h % static_cast<std::uint64_t>(n))];
}

// ── Border record name: deterministic variant selection via catalog ─────────
// Returns nullopt if the catalog has no assets for (family, shape).
// Never fabricates a _00 fallback.

std::optional<std::string> select_border_record_name(const TerrainAssetCatalog& catalog,
                                                     const std::string& family, int shape,
                                                     int tile_x, int tile_y) {
    const auto it = catalog.border_variant_index.find({family, shape});
    if (it == catalog.border_variant_index.end() || it->second.empty()) {
        return std::nullopt;
    }
    const auto& variants = it->second;
    const auto  n = variants.size();
    if (n > 4) {
        throw std::logic_error("unsupported border variant count greater than 4");
    }
    const auto bucket = terrain_variant_bucket(tile_x, tile_y);
    const auto selected_variant = variants[select_border_variant_index_from_bucket(bucket, n)];
    const auto asset = catalog.find_border_asset(family, shape, selected_variant);
    if (!asset.has_value()) {
        return std::nullopt;
    }
    return asset->record_name;
}

// ── Composite NE mask cache management ──────────────────────────────────────

PreparedCompositeNeMaskKey composite_key_for_operations(const std::vector<TileOperation>& tile_ops,
                                                        const std::string& target_material_code) {
    PreparedCompositeNeMaskKey key;
    for (const auto& op : tile_ops) {
        if (op.compose_mode != AdventureTerrainBorderComposeMode::MaskBlend)
            continue;
        if (op.target_material_code != target_material_code)
            continue;
        key.records.push_back(op.record_name);
    }
    std::ranges::sort(key.records);
    key.records.erase(std::unique(key.records.begin(), key.records.end()), key.records.end());
    return key;
}

// ── Ground dimension validation ──────────────────────────────────────────────

void validate_ground_dimensions(const std::vector<std::shared_ptr<const d2res::RgbaBuffer>>& bufs,
                                const std::string& material_code, int expected_w, int expected_h) {
    for (const auto& buf : bufs) {
        if (static_cast<int>(buf->width) != expected_w ||
            static_cast<int>(buf->height) != expected_h) {
            D2_LOG_WARN(kLog,
                        "terrain: {} variant dimension mismatch: variant {}x{} != first {}x{}",
                        material_code, buf->width, buf->height, expected_w, expected_h);
        }
    }
}

// ── Simple phase timer ─────────────────────────────────────────────────────

struct PhaseTimer {
    std::chrono::steady_clock::time_point start;
    explicit PhaseTimer() : start(std::chrono::steady_clock::now()) {}
    [[nodiscard]] double elapsed_ms() const {
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
            .count();
    }
};

// ── Preparation phases ──────────────────────────────────────────────────────

void build_layout(PreparedAdventureTerrainMap& state, const AdventureTerrainSurfaceInput& input,
                  const AdventureTerrainSurfaceComposeOptions& options) {
    int min_x = 0, min_y = 0, max_x = 64, max_y = 32;
    for (int gy = 0; gy < input.map_height; ++gy) {
        for (int gx = 0; gx < input.map_width; ++gx) {
            const auto* desc = descriptor_at(input, gx, gy);
            if (desc == nullptr) {
                continue;
            }
            // Canonical logical grid (gx, gy) → isometric world:
            //   world_x = (gx - gy) * half_tw
            //   world_y = (gx + gy) * half_th
            const int wx = (gx - gy) * options.tile_width / 2;
            const int wy = (gx + gy) * options.tile_height / 2;
            if (state.placements.empty()) {
                min_x = wx;
                min_y = wy;
                max_x = wx + options.tile_width;
                max_y = wy + options.tile_height;
            } else {
                min_x = std::min(min_x, wx);
                min_y = std::min(min_y, wy);
                max_x = std::max(max_x, wx + options.tile_width);
                max_y = std::max(max_y, wy + options.tile_height);
            }
            state.placements.push_back({.grid_x = gx,
                                        .grid_y = gy,
                                        .world_x = wx,
                                        .world_y = wy,
                                        .canvas_x = wx,
                                        .canvas_y = wy,
                                        .terrain_code = desc->terrain_code,
                                        .material = desc->material});
        }
    }
    state.min_world_x = min_x;
    state.min_world_y = min_y;
    state.canvas_width = max_x - min_x;
    state.canvas_height = max_y - min_y;
    for (auto& p : state.placements) {
        p.canvas_x -= min_x;
        p.canvas_y -= min_y;
    }
}

void collect_operations(PreparedAdventureTerrainMap&                 state,
                        const AdventureTerrainBorderShapeResolver&   resolver,
                        const TerrainAssetCatalog&                   catalog,
                        const AdventureTerrainSurfaceInput&          input,
                        const AdventureTerrainSurfaceComposeOptions& options) {
    state.operations.resize(state.placements.size());
    if (!options.include_borders) {
        return;
    }

    std::set<std::pair<std::string, int>> logged_missing_shapes;

    for (std::size_t i = 0; i < state.placements.size(); ++i) {
        const auto& p = state.placements[i];
        const auto* desc = descriptor_at(input, p.grid_x, p.grid_y);
        if (!desc) {
            continue;
        }

        // Fast-path: interior tile surrounded by same material needs no topology work
        bool interior = true;
        for (const auto& [dx, dy] : kNeighborOrder) {
            const auto* neighbor = descriptor_at(input, p.grid_x + dx, p.grid_y + dy);
            if (neighbor == nullptr || neighbor->material != desc->material) {
                interior = false;
                break;
            }
        }
        if (interior) {
            continue;
        }

        std::uint8_t lc = 0, ld = 0, sc = 0, sd = 0, wc = 0, wd = 0, oc = 0, od = 0, wac = 0,
                     wad = 0;
        const auto   ops =
            build_topology3x3_operations(resolver, input, p.grid_x, p.grid_y, *desc, &lc, &ld, &sc,
                                         &sd, &wc, &wd, &oc, &od, &wac, &wad);
        for (const auto& plan : ops) {
            if (!plan.operation.drawable) {
                continue;
            }
            const auto record_name = select_border_record_name(
                catalog, plan.operation.family, plan.operation.record_shape, p.grid_x, p.grid_y);
            if (!record_name.has_value()) {
                const auto key = std::make_pair(plan.operation.family, plan.operation.record_shape);
                if (logged_missing_shapes.insert(key).second) {
                    D2_LOG_WARN(kLog, "terrain: missing border shape family={} shape={}",
                                plan.operation.family, plan.operation.record_shape);
                }
                continue;
            }
            TileOperation top;
            top.compose_mode = plan.operation.compose_mode;
            top.family = plan.operation.family;
            top.record_name = *record_name;
            top.source_material_code = p.terrain_code;
            top.target_material_code = plan.material_b_code;
            top.tile_x = p.grid_x;
            top.tile_y = p.grid_y;
            state.operations[i].push_back(std::move(top));
        }
    }
}

void prepare_border_assets(PreparedAdventureTerrainMap&           state,
                           const AdventureTerrainSurfaceComposer& composer) {
    auto&                 assets = state.border_assets;
    std::set<std::string> needed_records;
    for (const auto& tile_ops : state.operations) {
        for (const auto& op : tile_ops) {
            needed_records.insert(op.record_name);
        }
    }

    MissingTerrainAssetLogState log_state;

    // Prepare individual NE masks and WA sprites
    for (const auto& rec : needed_records) {
        if (assets.individual_ne_masks.contains(rec))
            continue;
        if (assets.wa_sprites.contains(rec))
            continue;

        const auto* buf = composer.image_view({"Imgs/GrBorder.ff", rec});
        if (buf == nullptr || buf->rgba.empty()) {
            const auto family = (rec.size() >= 2) ? rec.substr(0, 2) : "";
            for (const auto& tile_ops : state.operations) {
                for (const auto& op : tile_ops) {
                    if (op.record_name != rec)
                        continue;
                    log_missing_terrain_border_asset_once(
                        log_state, rec, family, op.compose_mode, op.source_material_code,
                        op.target_material_code, op.tile_x, op.tile_y);
                }
            }
            continue;
        }

        if (rec.size() >= 2 && rec.substr(0, 2) == "WA") {
            d2res::RgbaBuffer keyed = *buf;
            d2res::apply_magenta_key_to_rgba(keyed);
            for (std::size_t wi = 0; wi + 3 < keyed.rgba.size(); wi += 4) {
                if (keyed.rgba[wi] >= 245 && keyed.rgba[wi + 1] >= 245 &&
                    keyed.rgba[wi + 2] >= 245 && keyed.rgba[wi + 3] != 0) {
                    keyed.rgba[wi] = 0;
                    keyed.rgba[wi + 1] = 0;
                    keyed.rgba[wi + 2] = 0;
                    keyed.rgba[wi + 3] = 0;
                }
            }
            assets.wa_sprites[rec] = prepare_wa_sprite(keyed);
        } else {
            assets.individual_ne_masks[rec] = prepare_ne_mask(*buf);
        }
    }
}

// ── Ownership buffer + sparse command list (replaces coverage maps) ───────

void build_ground_fields(PreparedAdventureTerrainMap&           state,
                         const AdventureTerrainSurfaceComposer& composer,
                         const TerrainAssetCatalog&             catalog) {
    const int cw = state.canvas_width;
    const int ch = state.canvas_height;
    if (cw <= 0 || ch <= 0) {
        return;
    }

    // Compute per-material bounding boxes directly from tile placements.
    struct MaterialBBox {
        int  min_x = 0;
        int  min_y = 0;
        int  max_x = 0;
        int  max_y = 0;
        bool initialized = false;
    };
    std::map<std::string, MaterialBBox> material_bboxes;
    for (const auto& p : state.placements) {
        if (p.terrain_code.empty()) {
            continue;
        }
        const auto& code = p.terrain_code;
        auto&       bbox = material_bboxes[code];
        if (!bbox.initialized) {
            bbox.min_x = p.world_x;
            bbox.min_y = p.world_y;
            bbox.max_x = p.world_x + state.diamond.width;
            bbox.max_y = p.world_y + state.diamond.height;
            bbox.initialized = true;
        } else {
            bbox.min_x = std::min(bbox.min_x, p.world_x);
            bbox.min_y = std::min(bbox.min_y, p.world_y);
            bbox.max_x = std::max(bbox.max_x, p.world_x + state.diamond.width);
            bbox.max_y = std::max(bbox.max_y, p.world_y + state.diamond.height);
        }
    }

    // Extend target material bboxes so NE transitions can look up ground at the
    // source tile's world position.  An NE transition at tile p reads target
    // ground pixels at p's world position, so the target ground field must
    // cover that area too.
    for (std::size_t i = 0; i < state.placements.size(); ++i) {
        const auto& p = state.placements[i];
        if (p.terrain_code.empty()) {
            continue;
        }
        for (const auto& op : state.operations[i]) {
            if (op.compose_mode != AdventureTerrainBorderComposeMode::MaskBlend) {
                continue;
            }
            auto& bbox = material_bboxes[op.target_material_code];
            if (!bbox.initialized) {
                bbox.min_x = p.world_x;
                bbox.min_y = p.world_y;
                bbox.max_x = p.world_x + state.diamond.width;
                bbox.max_y = p.world_y + state.diamond.height;
                bbox.initialized = true;
            } else {
                bbox.min_x = std::min(bbox.min_x, p.world_x);
                bbox.min_y = std::min(bbox.min_y, p.world_y);
                bbox.max_x = std::max(bbox.max_x, p.world_x + state.diamond.width);
                bbox.max_y = std::max(bbox.max_y, p.world_y + state.diamond.height);
            }
        }
    }

    for (const auto& [code, bbox] : material_bboxes) {
        if (!bbox.initialized) {
            continue;
        }
        if (state.ground_fields.contains(code)) {
            continue;
        }

        const auto vit = catalog.ground_variant_index.find(code);
        if (vit == catalog.ground_variant_index.end() || vit->second.empty()) {
            continue;
        }
        const auto& variants = vit->second;

        // Load all variant PNGs via shared handles (real ownership)
        std::vector<std::shared_ptr<const d2res::RgbaBuffer>> variant_bufs;
        std::vector<int>                                      variant_indices;
        int                                                   ground_w = 0, ground_h = 0;
        for (int v : variants) {
            auto buf = composer.image_handle(ground_asset(code, v));
            if (!buf || buf->rgba.empty()) {
                continue;
            }
            if (ground_w == 0) {
                ground_w = static_cast<int>(buf->width);
                ground_h = static_cast<int>(buf->height);
            }
            variant_bufs.push_back(std::move(buf));
            variant_indices.push_back(v);
        }
        if (variant_bufs.empty()) {
            continue;
        }

        validate_ground_dimensions(variant_bufs, code, ground_w, ground_h);

        // Build regular patch grid covering the material bbox.
        // bbox.max_x/max_y are exclusive (half-open [min, max)),
        // so last covered pixel is max - 1.
        const int first_patch_x = floor_div(bbox.min_x, ground_w);
        const int last_patch_x = floor_div(bbox.max_x - 1, ground_w);
        const int first_patch_y = floor_div(bbox.min_y, ground_h);
        const int last_patch_y = floor_div(bbox.max_y - 1, ground_h);

        auto& field = state.ground_fields[code];
        field.patch_width = ground_w;
        field.patch_height = ground_h;
        field.first_patch_x = first_patch_x;
        field.first_patch_y = first_patch_y;
        field.columns = last_patch_x - first_patch_x + 1;
        field.rows = last_patch_y - first_patch_y + 1;
        field.buffers = std::move(variant_bufs);
        field.patch_grid.assign(
            static_cast<std::size_t>(field.columns) * static_cast<std::size_t>(field.rows), -1);

        for (int py = first_patch_y; py <= last_patch_y; ++py) {
            for (int px = first_patch_x; px <= last_patch_x; ++px) {
                const int   patch_variant = ground_variant_for_patch(code, px, py, variant_indices);
                std::size_t buf_idx = 0;
                for (std::size_t bi = 0; bi < variant_indices.size(); ++bi) {
                    if (variant_indices[bi] == patch_variant) {
                        buf_idx = bi;
                        break;
                    }
                }
                const int local_x = px - first_patch_x;
                const int local_y = py - first_patch_y;
                field.patch_grid[static_cast<std::size_t>(local_y) *
                                     static_cast<std::size_t>(field.columns) +
                                 static_cast<std::size_t>(local_x)] = static_cast<int>(buf_idx);
            }
        }
    }
}

void build_sparse_commands(PreparedAdventureTerrainMap&                 state,
                           const AdventureTerrainSurfaceComposeOptions& options) {
    const int cw = state.canvas_width;
    const int ch = state.canvas_height;
    if (cw <= 0 || ch <= 0) {
        return;
    }

    auto& assets = state.border_assets;

    for (std::size_t i = 0; i < state.placements.size(); ++i) {
        const auto& p = state.placements[i];
        if (p.terrain_code.empty()) {
            continue;
        }

        if (options.include_base) {
            // Pre-bind Ground field reference for this tile.
            const auto  git = state.ground_fields.find(p.terrain_code);
            const auto* ground = (git != state.ground_fields.end()) ? &git->second : nullptr;

            if (p.terrain_code == "WA") {
                state.wa_base_commands.push_back({
                    .canvas_x = p.canvas_x,
                    .canvas_y = p.canvas_y,
                    .world_x = p.world_x,
                    .world_y = p.world_y,
                    .ground = ground,
                });

                // WA sprite placements (one command per border operation)
                for (const auto& op : state.operations[i]) {
                    if (op.compose_mode != AdventureTerrainBorderComposeMode::ColorKeyOverlay) {
                        continue;
                    }
                    const auto sit = assets.wa_sprites.find(op.record_name);
                    if (sit == assets.wa_sprites.end()) {
                        continue;
                    }
                    state.wa_sprite_placements.push_back({
                        .tile_canvas_x = p.canvas_x,
                        .tile_canvas_y = p.canvas_y,
                        .sprite = &sit->second,
                    });
                }
            } else if (p.terrain_code != "BL") {
                state.land_base_commands.push_back({
                    .canvas_x = p.canvas_x,
                    .canvas_y = p.canvas_y,
                    .world_x = p.world_x,
                    .world_y = p.world_y,
                    .ground = ground,
                });
            }
        }

        // Group NE operations by target material for composite (always, borders are independent)
        std::map<std::string, std::vector<const TileOperation*>> ne_by_target;
        for (const auto& op : state.operations[i]) {
            if (op.compose_mode != AdventureTerrainBorderComposeMode::MaskBlend) {
                continue;
            }
            ne_by_target[op.target_material_code].push_back(&op);
        }

        for (const auto& [target_code, ops] : ne_by_target) {
            auto composite_key = composite_key_for_operations(state.operations[i], target_code);

            auto cit = assets.composite_ne_masks.find(composite_key);
            if (cit == assets.composite_ne_masks.end()) {
                auto inserted = assets.composite_ne_masks.emplace(
                    composite_key, build_composite_ne_mask(
                                       composite_key, assets.individual_ne_masks, state.diamond));
                cit = inserted.first;
            }

            const auto target_mat_idx = material_index(target_code);
            const auto git = state.ground_fields.find(target_code);
            state.ne_transition_commands_by_material[target_mat_idx].push_back({
                .tile_canvas_x = p.canvas_x,
                .tile_canvas_y = p.canvas_y,
                .mask = &cit->second,
                .target_ground = (git != state.ground_fields.end()) ? &git->second : nullptr,
                .target_material_index = static_cast<std::uint8_t>(target_mat_idx),
            });
        }
    }
}

} // anonymous namespace

// ── Public API ──────────────────────────────────────────────────────────────

bool terrain_surface_diamond_contains(int x, int y, int tile_width, int tile_height) {
    const double cx = (static_cast<double>(tile_width) - 3.0) / 2.0;
    const double cy = (static_cast<double>(tile_height) - 1.0) / 2.0;
    return std::abs((static_cast<double>(x) - cx) / (tile_width / 2.0)) +
               std::abs((static_cast<double>(y) - cy) / (tile_height / 2.0)) <=
           1.0;
}

TerrainDiamondSupport prepare_d2_diamond_support(int tile_width, int tile_height) {
    TerrainDiamondSupport support;
    support.width = tile_width;
    support.height = tile_height;
    const auto count = static_cast<std::size_t>(tile_width) * static_cast<std::size_t>(tile_height);
    support.alpha.assign(count, 0);
    support.pixel_coords.reserve(static_cast<std::size_t>(tile_width * tile_height / 2));
    // Temporary dense row tracking: [y] = (current_span_begin, -1 if none)
    std::vector<int> span_begin(static_cast<std::size_t>(tile_height), -1);
    for (int py = 0; py < tile_height; ++py) {
        const auto uy = static_cast<std::size_t>(py);
        for (int px = 0; px < tile_width; ++px) {
            if (terrain_surface_diamond_contains(px, py, tile_width, tile_height)) {
                const auto idx =
                    (static_cast<std::size_t>(py) * static_cast<std::size_t>(tile_width)) +
                    static_cast<std::size_t>(px);
                support.alpha[idx] = 255;
                support.pixel_coords.emplace_back(px, py);
                if (span_begin[uy] < 0)
                    span_begin[uy] = px;
            } else {
                if (span_begin[uy] >= 0) {
                    support.row_spans.push_back({py, span_begin[uy], px});
                    span_begin[uy] = -1;
                }
            }
        }
        if (span_begin[uy] >= 0) {
            support.row_spans.push_back({py, span_begin[uy], tile_width});
            span_begin[uy] = -1;
        }
    }
    return support;
}

AdventureTerrainNeighborTransitionMask
build_transition_mask(const AdventureTerrainSurfaceInput& input, int x, int y,
                      d2runtime::AdventureTerrainMaterial material_b) {
    AdventureTerrainNeighborTransitionMask mask;
    const auto                             set = [&](int order, bool value) {
        switch (order) {
        case 0:
            mask.north = value;
            break;
        case 1:
            mask.east = value;
            break;
        case 2:
            mask.south = value;
            break;
        case 3:
            mask.west = value;
            break;
        case 4:
            mask.north_west = value;
            break;
        case 5:
            mask.north_east = value;
            break;
        case 6:
            mask.south_east = value;
            break;
        case 7:
            mask.south_west = value;
            break;
        default:
            break;
        }
    };
    for (std::size_t i = 0; i < kNeighborOrder.size(); ++i) {
        const auto* neighbor =
            descriptor_at(input, x + kNeighborOrder[i].first, y + kNeighborOrder[i].second);
        set(static_cast<int>(i), neighbor != nullptr && neighbor->material == material_b);
    }
    return mask;
}

AdventureTerrainSurfaceComposer::AdventureTerrainSurfaceComposer(
    const IImageStore& store, const TerrainAssetCatalog& catalog,
    AdventureTerrainBorderShapeResolver border_shape_resolver)
    : store_(&store), catalog_(catalog), border_shape_resolver_(border_shape_resolver) {}

AdventureTerrainSurfaceComposer::AdventureTerrainSurfaceComposer(
    AdventureTerrainSurfaceImageMap images, const TerrainAssetCatalog& catalog,
    AdventureTerrainBorderShapeResolver border_shape_resolver)
    : owned_store_(std::make_shared<MapImageStore>(std::move(images))), store_(owned_store_.get()),
      catalog_(catalog), border_shape_resolver_(border_shape_resolver) {}

const d2res::RgbaBuffer* AdventureTerrainSurfaceComposer::image_view(
    const d2runtime::AdventureTerrainAssetRef& asset) const {
    if (store_ == nullptr)
        return nullptr;
    return store_->raw_png(asset.container_path, asset.record_name).get();
}

std::shared_ptr<const d2res::RgbaBuffer> AdventureTerrainSurfaceComposer::image_handle(
    const d2runtime::AdventureTerrainAssetRef& asset) const {
    if (store_ == nullptr)
        return nullptr;
    return store_->raw_png(asset.container_path, asset.record_name);
}

// ── describe_tile ───────────────────────────────────────────────────────────

AdventureTerrainSurfaceCompositionInfo AdventureTerrainSurfaceComposer::describe_tile(
    const AdventureTerrainSurfaceInput& input, int x, int y,
    const AdventureTerrainSurfaceComposeOptions& options) const {
    AdventureTerrainSurfaceCompositionInfo info;
    (void)options;
    const auto* descriptor = descriptor_at(input, x, y);
    if (descriptor == nullptr)
        return info;

    info.material_a_code = descriptor->terrain_code;
    const auto material_b = choose_material_b(input, *descriptor, x, y);
    info.material_b_code = material_b.code;
    info.has_neighbor_transition = material_b.material != descriptor->material;
    info.neighbor_mask = build_transition_mask(input, x, y, material_b.material);

    if (descriptor->border &&
        descriptor->border->kind == d2runtime::AdventureTerrainBorderKind::NonDrawableShape16) {
        info.composer_border_kind = d2runtime::AdventureTerrainBorderKind::NonDrawableShape16;
        info.border_shape_source = "non_drawable_shape_16";
        info.border_asset_found = true;
        return info;
    }

    std::uint8_t land_cardinal = 0, land_diagonal = 0;
    std::uint8_t stronger_cardinal = 0, stronger_diagonal = 0;
    std::uint8_t weaker_cardinal = 0, weaker_diagonal = 0;
    std::uint8_t out_cardinal = 0, out_diagonal = 0;
    std::uint8_t water_cardinal = 0, water_diagonal = 0;
    const auto   neighbors = topology_neighbors(input, x, y);
    const auto   operations = build_topology3x3_operations(
        border_shape_resolver_, input, x, y, *descriptor, &land_cardinal, &land_diagonal,
        &stronger_cardinal, &stronger_diagonal, &weaker_cardinal, &weaker_diagonal, &out_cardinal,
        &out_diagonal, &water_cardinal, &water_diagonal);
    info.land_cardinal_mask = land_cardinal;
    info.land_diagonal_mask = land_diagonal;
    info.stronger_cardinal_mask = stronger_cardinal;
    info.stronger_diagonal_mask = stronger_diagonal;
    info.weaker_cardinal_mask = weaker_cardinal;
    info.weaker_diagonal_mask = weaker_diagonal;
    info.out_of_bounds_cardinal_mask = out_cardinal;
    info.out_of_bounds_diagonal_mask = out_diagonal;
    info.water_cardinal_mask = water_cardinal;
    info.water_diagonal_mask = water_diagonal;
    info.neighbor_material_layout = neighbor_material_layout(neighbors);
    info.dominance_layout = dominance_layout(*descriptor, neighbors);
    info.topology_key =
        topology_key_string(*descriptor, operations, land_cardinal, land_diagonal,
                            stronger_cardinal, stronger_diagonal, weaker_cardinal, weaker_diagonal,
                            out_cardinal, out_diagonal, water_cardinal, water_diagonal);

    for (const auto& plan : operations) {
        const auto record_name = select_border_record_name(catalog_, plan.operation.family,
                                                           plan.operation.record_shape, x, y);
        const bool has_name = record_name.has_value();
        const auto expected_border =
            has_name ? border_asset(*record_name)
                     : d2runtime::AdventureTerrainAssetRef{"Imgs/GrBorder.ff", ""};
        const bool asset_found = has_name && image_view(expected_border) != nullptr;
        info.border_operations.push_back({
            .material_b_code = plan.material_b_code,
            .family = plan.operation.family,
            .record_name = has_name ? *record_name : "",
            .logical_shape = plan.operation.logical_shape,
            .record_shape = plan.operation.record_shape,
            .compose_mode = plan.operation.compose_mode,
            .source = plan.operation.source,
            .cardinal_mask = plan.key.cardinal_mask,
            .diagonal_mask = plan.key.diagonal_mask,
            .dominance_relation = plan.dominance_relation,
            .drawable = plan.operation.drawable,
        });
        if (info.composer_border_kind == d2runtime::AdventureTerrainBorderKind::None &&
            plan.operation.drawable && has_name) {
            info.material_b_code = plan.material_b_code;
            info.composer_border_kind = d2runtime::AdventureTerrainBorderKind::Drawable;
            info.composer_border_family = plan.operation.family;
            info.composer_border_record = expected_border.record_name;
            info.border_shape_source = plan.operation.source;
            info.logical_border_shape = plan.operation.logical_shape;
            info.resolved_record_shape = plan.operation.record_shape;
            info.resolved_border_record = *record_name;
            info.border_synthesized = false;
            info.border_asset_found = asset_found;
        }
    }
    return info;
}

// ── prepare_full_map ────────────────────────────────────────────────────────

PreparedAdventureTerrainMap AdventureTerrainSurfaceComposer::prepare_full_map(
    const AdventureTerrainSurfaceInput&          input,
    const AdventureTerrainSurfaceComposeOptions& options) const {
    PhaseTimer                  total_timer;
    PreparedAdventureTerrainMap state;
    state.diamond = prepare_d2_diamond_support(options.tile_width, options.tile_height);

    state.include_base = options.include_base;

    // Phase A: layout
    {
        PhaseTimer t;
        build_layout(state, input, options);
        D2_LOG_DEBUG(kLog, "terrain_prepare_layout duration_ms={:.2f}", t.elapsed_ms());
    }
    if (state.canvas_width <= 0 || state.canvas_height <= 0)
        return state;

    // Phase B: operations
    {
        PhaseTimer t;
        collect_operations(state, border_shape_resolver_, catalog_, input, options);
        D2_LOG_DEBUG(kLog, "terrain_prepare_operations duration_ms={:.2f}", t.elapsed_ms());
    }

    // Phase C: prepare border assets
    {
        PhaseTimer t;
        prepare_border_assets(state, *this);
        D2_LOG_DEBUG(kLog, "terrain_prepare_border_assets duration_ms={:.2f}", t.elapsed_ms());
    }

    // Phase D: build regular Ground field grids (O(1) lookup)
    {
        PhaseTimer t;
        build_ground_fields(state, *this, catalog_);
        D2_LOG_DEBUG(kLog, "terrain_prepare_ground_fields duration_ms={:.2f}", t.elapsed_ms());
    }

    // Phase E: build sparse transition / WA commands
    {
        PhaseTimer t;
        build_sparse_commands(state, options);
        D2_LOG_DEBUG(kLog, "terrain_prepare_sparse_commands duration_ms={:.2f}", t.elapsed_ms());
    }

    // Compute tile statistics
    std::size_t boundary_tile_count = 0;
    for (const auto& ops : state.operations) {
        if (!ops.empty())
            ++boundary_tile_count;
    }
    const std::size_t interior_tile_count = state.placements.size() - boundary_tile_count;

    std::size_t ne_command_total = 0;
    for (const auto& vec : state.ne_transition_commands_by_material)
        ne_command_total += vec.size();

    D2_LOG_DEBUG(kLog, "terrain_prepare_end duration_ms={:.2f}", total_timer.elapsed_ms());
    D2_LOG_DEBUG(
        kLog,
        "terrain_prepare_info canvas={}x{} tiles={} boundary={} interior={} "
        "ground_fields={} ne_commands={} wa_placements={} wa_base_cmds={} land_base_cmds={}",
        state.canvas_width, state.canvas_height, static_cast<int>(state.placements.size()),
        static_cast<int>(boundary_tile_count), static_cast<int>(interior_tile_count),
        static_cast<int>(state.ground_fields.size()), static_cast<int>(ne_command_total),
        static_cast<int>(state.wa_sprite_placements.size()),
        static_cast<int>(state.wa_base_commands.size()),
        static_cast<int>(state.land_base_commands.size()));

    // Log per-material field statistics
    for (const auto& [code, field] : state.ground_fields) {
        D2_LOG_DEBUG(
            kLog, "terrain_field material={} grid={}x{} patches={}", code, field.columns,
            field.rows,
            static_cast<int>(std::count_if(field.patch_grid.begin(), field.patch_grid.end(),
                                           [](int v) { return v >= 0; })));
    }

    return state;
}

namespace {

const d2res::RgbaBuffer*
lookup_ground(const PreparedAdventureTerrainMap::PreparedGroundField& field, int world_x,
              int world_y, int* out_src_x, int* out_src_y) {
    const int grid_x = floor_div(world_x, field.patch_width);
    const int grid_y = floor_div(world_y, field.patch_height);
    const int local_x = grid_x - field.first_patch_x;
    const int local_y = grid_y - field.first_patch_y;
    if (local_x < 0 || local_x >= field.columns || local_y < 0 || local_y >= field.rows)
        return nullptr;
    const int buf_idx = field.patch_grid[static_cast<std::size_t>(local_y) *
                                             static_cast<std::size_t>(field.columns) +
                                         static_cast<std::size_t>(local_x)];
    if (buf_idx < 0)
        return nullptr;
    const auto* buf = field.buffers[static_cast<std::size_t>(buf_idx)].get();
    if (buf == nullptr || buf->rgba.empty())
        return nullptr;
    *out_src_x = floor_mod(world_x, field.patch_width);
    *out_src_y = floor_mod(world_y, field.patch_height);
    return buf;
}

} // anonymous namespace

// ── render_prepared_full_map ────────────────────────────────────────────────
//     O(1) Ground lookup + dominance-ordered sparse NE + sprite-level WA.

namespace {

// Render a single base tile using diamond row spans with Ground patch fragment copies.
// Returns number of pixels written.
std::size_t render_base_tile(const PreparedAdventureTerrainMap::PreparedBaseTileCommand& cmd,
                             const PreparedAdventureTerrainMap& prepared, const int cw,
                             const int ch, std::vector<AdventureTerrainSurfacePixel>& canvas) {
    if (cmd.ground == nullptr)
        return 0;
    const auto& field = *cmd.ground;
    std::size_t writes = 0;
    for (const auto& span : prepared.diamond.row_spans) {
        const int dest_y = cmd.canvas_y + span.y;
        if (dest_y < 0 || dest_y >= ch)
            continue;
        const int world_y = cmd.world_y + span.y;
        const int grid_y = floor_div(world_y, field.patch_height);
        const int local_grid_y = grid_y - field.first_patch_y;
        if (local_grid_y < 0 || local_grid_y >= field.rows)
            continue;
        const int  local_y = world_y - grid_y * field.patch_height;
        const auto row_offset =
            static_cast<std::size_t>(local_grid_y) * static_cast<std::size_t>(field.columns);
        int x = span.x_begin;
        while (x < span.x_end) {
            const int dest_x = cmd.canvas_x + x;
            const int world_x = cmd.world_x + x;
            if (dest_x < 0 || dest_x >= cw) {
                ++x;
                continue;
            }
            const int grid_x = floor_div(world_x, field.patch_width);
            const int local_grid_x = grid_x - field.first_patch_x;
            if (local_grid_x < 0 || local_grid_x >= field.columns) {
                ++x;
                continue;
            }
            const int buf_idx =
                field.patch_grid[row_offset + static_cast<std::size_t>(local_grid_x)];
            if (buf_idx < 0) {
                ++x;
                continue;
            }
            const auto* buf = field.buffers[static_cast<std::size_t>(buf_idx)].get();
            if (buf == nullptr || buf->rgba.empty()) {
                ++x;
                continue;
            }
            // Compute fragment boundaries: within this span, within this patch, within canvas.
            const int patch_pixel_x = world_x - grid_x * field.patch_width;
            const int patch_right = (grid_x + 1) * field.patch_width - cmd.world_x;
            const int span_remain = span.x_end - x;
            const int frag_len = std::min(span_remain, patch_right - x);
            const int clamped_end = std::min(x + frag_len, span.x_end);
            const int clamped_begin = std::max(x, span.x_begin);
            const int frag_x_len = clamped_end - clamped_begin;
            if (frag_x_len <= 0) {
                x = clamped_end;
                continue;
            }
            // Contiguous copy: source pixel data [local_y][patch_pixel_x + offset] * 4 bytes each.
            const int         src_x_base = patch_pixel_x + (clamped_begin - x);
            const std::size_t base_src_idx = (static_cast<std::size_t>(local_y) * buf->width +
                                              static_cast<std::size_t>(src_x_base)) *
                                             4U;
            const std::size_t base_dst_idx =
                static_cast<std::size_t>(dest_y) * static_cast<std::size_t>(cw) +
                static_cast<std::size_t>(cmd.canvas_x + clamped_begin);
            for (int fi = 0; fi < frag_x_len; ++fi) {
                const std::size_t dst_i = base_dst_idx + static_cast<std::size_t>(fi);
                const std::size_t src_i = base_src_idx + static_cast<std::size_t>(fi) * 4U;
                canvas[dst_i] = {buf->rgba[src_i], buf->rgba[src_i + 1], buf->rgba[src_i + 2], 255};
            }
            writes += static_cast<std::size_t>(frag_x_len);
            x = clamped_end;
        }
    }
    return writes;
}

} // anonymous namespace

AdventureTerrainSurface AdventureTerrainSurfaceComposer::render_prepared_full_map(
    const PreparedAdventureTerrainMap& prepared) const {
    PhaseTimer render_timer;
    const int  cw = prepared.canvas_width;
    const int  ch = prepared.canvas_height;
    if (cw <= 0 || ch <= 0) {
        return {};
    }

    const auto canvas_pixels = static_cast<std::size_t>(cw) * static_cast<std::size_t>(ch);
    std::vector<AdventureTerrainSurfacePixel> canvas(canvas_pixels, AdventureTerrainSurfacePixel{});

    std::size_t ne_command_total = 0;
    for (const auto& vec : prepared.ne_transition_commands_by_material)
        ne_command_total += vec.size();

    D2_LOG_DEBUG(kLog,
                 "terrain_render_begin canvas_width={} canvas_height={} canvas_pixels={} "
                 "ground_fields={} ne_commands={} wa_placements={}",
                 cw, ch, canvas_pixels, prepared.ground_fields.size(), ne_command_total,
                 prepared.wa_sprite_placements.size());

    std::size_t wa_base_writes = 0;
    std::size_t land_base_writes = 0;
    std::size_t ne_pixel_writes = 0;

    if (prepared.include_base) {
        // ── Phase 1: WA base tiles ──────────────────────────────────────────
        {
            PhaseTimer timer;
            for (const auto& cmd : prepared.wa_base_commands)
                wa_base_writes += render_base_tile(cmd, prepared, cw, ch, canvas);
            D2_LOG_DEBUG(kLog, "terrain_render_wa_base duration_ms={:.2f} writes={}",
                         timer.elapsed_ms(), wa_base_writes);
        }

        // ── Phase 2: WA sprite placements ──────────────────────────────────
        {
            PhaseTimer timer;
            for (const auto& placement : prepared.wa_sprite_placements) {
                if (placement.sprite == nullptr)
                    continue;
                for (const auto& wp : placement.sprite->opaque_pixels) {
                    const int cx = placement.tile_canvas_x + static_cast<int>(wp.x);
                    const int cy = placement.tile_canvas_y + static_cast<int>(wp.y);
                    if (cx < 0 || cx >= cw || cy < 0 || cy >= ch)
                        continue;
                    const std::size_t idx =
                        static_cast<std::size_t>(cy) * static_cast<std::size_t>(cw) +
                        static_cast<std::size_t>(cx);
                    canvas[idx] = wp.pixel;
                }
            }
            D2_LOG_DEBUG(kLog, "terrain_render_wa_sprites duration_ms={:.2f}", timer.elapsed_ms());
        }

        // ── Phase 3: Land base tiles (overwrites WA sprites on land) ────────
        {
            PhaseTimer timer;
            for (const auto& cmd : prepared.land_base_commands)
                land_base_writes += render_base_tile(cmd, prepared, cw, ch, canvas);
            D2_LOG_DEBUG(kLog, "terrain_render_land_base duration_ms={:.2f} writes={}",
                         timer.elapsed_ms(), land_base_writes);
        }
    }

    // ── Phase 4: NE transitions in material dominance order ─────────────────
    {
        PhaseTimer                                   ne_timer;
        static constexpr std::array<std::uint8_t, 6> kNeDominanceOrder = {2, 3, 4, 5, 6, 7};
        for (const auto target_mat_idx : kNeDominanceOrder) {
            const auto& commands = prepared.ne_transition_commands_by_material[target_mat_idx];
            for (const auto& cmd : commands) {
                if (cmd.mask == nullptr || cmd.target_ground == nullptr)
                    continue;
                for (const auto& pp : cmd.mask->nonzero_pixels) {
                    const int cx = cmd.tile_canvas_x + static_cast<int>(pp.x);
                    const int cy = cmd.tile_canvas_y + static_cast<int>(pp.y);
                    if (cx < 0 || cx >= cw || cy < 0 || cy >= ch)
                        continue;
                    const std::size_t idx =
                        static_cast<std::size_t>(cy) * static_cast<std::size_t>(cw) +
                        static_cast<std::size_t>(cx);
                    int         src_x = 0, src_y = 0;
                    const auto* buf = lookup_ground(*cmd.target_ground, cx + prepared.min_world_x,
                                                    cy + prepared.min_world_y, &src_x, &src_y);
                    if (buf == nullptr)
                        continue;
                    const std::size_t src_idx = (static_cast<std::size_t>(src_y) * buf->width +
                                                 static_cast<std::size_t>(src_x)) *
                                                4U;
                    source_over(canvas[idx], {buf->rgba[src_idx], buf->rgba[src_idx + 1],
                                              buf->rgba[src_idx + 2], pp.coverage});
                    ++ne_pixel_writes;
                }
            }
        }
        D2_LOG_DEBUG(kLog, "terrain_render_ne_pass duration_ms={:.2f} pixels={}",
                     ne_timer.elapsed_ms(), ne_pixel_writes);
    }

    D2_LOG_DEBUG(kLog, "terrain_render_end duration_ms={:.2f} ne_pixels={}",
                 render_timer.elapsed_ms(), ne_pixel_writes);
    return {cw, ch, std::move(canvas)};
}

// ── render_full_map (one-shot convenience) ──────────────────────────────────

AdventureTerrainSurface AdventureTerrainSurfaceComposer::render_full_map(
    const AdventureTerrainSurfaceInput&          input,
    const AdventureTerrainSurfaceComposeOptions& options) const {
    return render_prepared_full_map(prepare_full_map(input, options));
}

} // namespace d2engine
