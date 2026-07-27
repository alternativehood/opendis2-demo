#include "adventure_terrain_border_shape_resolver.hpp"

#include <d2log/log.hpp>

#include <algorithm>
#include <array>
#include <cstdio>
#include <optional>
#include <utility>

namespace d2engine {

namespace {

auto kLog = d2log::get("d2.terrain_border"); // NOLINT(cert-err58-cpp)

std::optional<std::uint8_t> explicit_border_shape(std::uint8_t cardinal_mask,
                                                  std::uint8_t diagonal_mask) {
    static constexpr std::array mappings = {
        std::pair<std::uint8_t, std::uint8_t>{0x10, 1},
        std::pair<std::uint8_t, std::uint8_t>{0x20, 2},
        std::pair<std::uint8_t, std::uint8_t>{0x30, 3},
        std::pair<std::uint8_t, std::uint8_t>{0x40, 4},
        std::pair<std::uint8_t, std::uint8_t>{0x5F, 5},
        std::pair<std::uint8_t, std::uint8_t>{0x67, 6},
        std::pair<std::uint8_t, std::uint8_t>{0x7F, 7},
        std::pair<std::uint8_t, std::uint8_t>{0x80, 8},
        std::pair<std::uint8_t, std::uint8_t>{0x9D, 9},
        std::pair<std::uint8_t, std::uint8_t>{0xAF, 10},
        std::pair<std::uint8_t, std::uint8_t>{0xBF, 11},
        std::pair<std::uint8_t, std::uint8_t>{0xCE, 12},
        std::pair<std::uint8_t, std::uint8_t>{0xDF, 13},
        std::pair<std::uint8_t, std::uint8_t>{0xEF, 14},
        std::pair<std::uint8_t, std::uint8_t>{0xF0, 15},
        std::pair<std::uint8_t, std::uint8_t>{0x01, 17},
        std::pair<std::uint8_t, std::uint8_t>{0x02, 18},
        std::pair<std::uint8_t, std::uint8_t>{0x03, 19},
        std::pair<std::uint8_t, std::uint8_t>{0x04, 20},
        std::pair<std::uint8_t, std::uint8_t>{0x05, 21},
        std::pair<std::uint8_t, std::uint8_t>{0x06, 22},
        std::pair<std::uint8_t, std::uint8_t>{0x07, 23},
        std::pair<std::uint8_t, std::uint8_t>{0x08, 24},
        std::pair<std::uint8_t, std::uint8_t>{0x09, 25},
        std::pair<std::uint8_t, std::uint8_t>{0x0A, 26},
        std::pair<std::uint8_t, std::uint8_t>{0x0B, 27},
        std::pair<std::uint8_t, std::uint8_t>{0x0C, 28},
        std::pair<std::uint8_t, std::uint8_t>{0x0D, 29},
        std::pair<std::uint8_t, std::uint8_t>{0x0E, 30},
        std::pair<std::uint8_t, std::uint8_t>{0x0F, 31},
    };
    const auto mask = static_cast<std::uint8_t>(cardinal_mask | (diagonal_mask << 4U));
    const auto it =
        std::ranges::find_if(mappings, [mask](const auto& entry) { return entry.first == mask; });
    if (it == mappings.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<AdventureTerrainResolvedBorderOperation>
resolve_explicit_topology_operation(const AdventureTerrainTopology3x3Key& key,
                                    AdventureTerrainBorderComposeMode     compose_mode,
                                    std::string_view                      source) {
    const auto shape = explicit_border_shape(key.cardinal_mask, key.diagonal_mask);
    if (!shape.has_value() || *shape == 0 || *shape == 16 || *shape > 31) {
        D2_LOG_WARN(
            kLog,
            "terrain_border_mapping_missing family={} center={} target={} cardinal=0x{:02x} "
            "diagonal=0x{:02x} stronger=0x{:02x}/0x{:02x} weaker=0x{:02x}/0x{:02x} "
            "out=0x{:02x}/0x{:02x}",
            key.family, key.center_code, key.target_code, key.cardinal_mask, key.diagonal_mask,
            key.stronger_cardinal_mask, key.stronger_diagonal_mask, key.weaker_cardinal_mask,
            key.weaker_diagonal_mask, key.out_of_bounds_cardinal_mask,
            key.out_of_bounds_diagonal_mask);
        return std::nullopt;
    }

    AdventureTerrainResolvedBorderOperation resolved;
    resolved.logical_shape = *shape;
    resolved.record_shape = *shape;
    resolved.family = key.family;
    resolved.compose_mode = compose_mode;
    resolved.source = std::string(source);
    resolved.drawable = true;
    return resolved;
}

std::optional<AdventureTerrainResolvedBorderOperation>
resolve_ne_topology3x3_operation(const AdventureTerrainTopology3x3Key& key) {
    return resolve_explicit_topology_operation(key, AdventureTerrainBorderComposeMode::MaskBlend,
                                               "explicit_ne_topology");
}

std::optional<AdventureTerrainResolvedBorderOperation>
resolve_wa_topology_operation(const AdventureTerrainTopology3x3Key& key) {
    return resolve_explicit_topology_operation(
        key, AdventureTerrainBorderComposeMode::ColorKeyOverlay, "explicit_wa_topology");
}

} // namespace

std::optional<AdventureTerrainResolvedBorderOperation>
AdventureTerrainBorderShapeResolver::resolve_topology3x3_operation(
    const AdventureTerrainTopology3x3Key& key) const {
    if (key.family != "NE" && key.family != "WA") {
        return std::nullopt;
    }
    if (key.family == "NE") {
        return resolve_ne_topology3x3_operation(key);
    }
    return resolve_wa_topology_operation(key);
}

} // namespace d2engine
