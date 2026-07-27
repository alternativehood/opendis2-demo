#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace d2engine {

struct AdventureTerrainNeighborTransitionMask {
    bool west = false;
    bool east = false;
    bool north = false;
    bool south = false;
    bool north_west = false;
    bool north_east = false;
    bool south_west = false;
    bool south_east = false;
};

struct AdventureTerrainResolvedBorderShape {
    std::uint8_t logical_shape = 0;
    std::uint8_t record_shape = 0;
    std::string  family;
    std::string  record_name;
    bool         drawable = false;
    bool         synthesized = false;
};

enum class AdventureTerrainBorderComposeMode : std::uint8_t {
    MaskBlend,
    ColorKeyOverlay,
};

struct AdventureTerrainTopology3x3Key {
    std::string  center_code;
    std::string  target_code;
    std::string  family;
    std::uint8_t cardinal_mask = 0;
    std::uint8_t diagonal_mask = 0;
    std::uint8_t stronger_cardinal_mask = 0;
    std::uint8_t stronger_diagonal_mask = 0;
    std::uint8_t weaker_cardinal_mask = 0;
    std::uint8_t weaker_diagonal_mask = 0;
    std::uint8_t out_of_bounds_cardinal_mask = 0;
    std::uint8_t out_of_bounds_diagonal_mask = 0;
};

struct AdventureTerrainResolvedBorderOperation {
    std::uint8_t                      logical_shape = 0;
    std::uint8_t                      record_shape = 0;
    std::string                       family;
    AdventureTerrainBorderComposeMode compose_mode = AdventureTerrainBorderComposeMode::MaskBlend;
    std::string                       source = "none";
    bool                              drawable = false;
};

class AdventureTerrainBorderShapeResolver {
public:
    explicit AdventureTerrainBorderShapeResolver() = default;

    [[nodiscard]] std::optional<AdventureTerrainResolvedBorderOperation>
    resolve_topology3x3_operation(const AdventureTerrainTopology3x3Key& key) const;
};

} // namespace d2engine
