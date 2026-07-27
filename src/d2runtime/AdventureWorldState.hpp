#pragma once

#include "AdventureCity.hpp"
#include "AdventureIsoDirection.hpp"
#include "AdventureSite.hpp"
#include "AdventureResourceNode.hpp"
#include "AdventureRuin.hpp"
#include "AdventureTreasure.hpp"
#include "MapCellCoord.hpp"

#include <cstddef>
#include <cstdint>
#include <array>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace d2runtime {

struct AdventureTerrainTile {
    uint32_t raw_value = 0;
};

struct AdventureTerrainGrid {
    int                               width = 0;
    int                               height = 0;
    std::vector<AdventureTerrainTile> tiles;

    [[nodiscard]] bool        empty() const { return tiles.empty(); }
    [[nodiscard]] std::size_t size() const { return tiles.size(); }

    [[nodiscard]] bool contains(int x, int y) const {
        return x >= 0 && y >= 0 && x < width && y < height;
    }

    [[nodiscard]] const AdventureTerrainTile* tile_at(int x, int y) const {
        if (!contains(x, y)) {
            return nullptr;
        }
        return &tiles[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                      static_cast<std::size_t>(x)];
    }

    [[nodiscard]] AdventureTerrainTile* tile_at(int x, int y) {
        if (!contains(x, y)) {
            return nullptr;
        }
        return &tiles[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                      static_cast<std::size_t>(x)];
    }
};

struct WorldObjectEntry {
    std::string id;
    std::string kind;
};

using FootprintCell = MapCellCoord;

enum class AdventureMapObjectKind {
    Capital,
    City,
    SiteMerchant,
    SiteMercenary,
    SiteTrainer,
    SiteMage,
    Ruin,
    Bag,
    Road,
    ResourceNode,
    Mountain,
    Stack,
    Landmark
};

struct AdventureMapObject {
    std::string                id;
    AdventureMapObjectKind     kind{};
    MapCellCoord               position;
    std::vector<FootprintCell> footprint;
    int                        image = -1;
    int                        index = -1;
    int                        variant = -1;
    int                        resource = -1;
    int                        race = -1;
    int                        id_mount = -1; // MidMountains entry id (preserved from SG)
    std::string                owner;
    std::string                subrace;
    bool                       blocking = true;
};

// ── Typed Road ─────────────────────────────────────────────────────────
//
struct AdventureRoad {
    std::string id;

    int index = -1;
    int variant = -1;

    MapCellCoord position;
};

// ── Typed Mountain ──────────────────────────────────────────────────────
//
struct AdventureMountain {
    std::string id;

    int id_mount = -1; // preserved from SG MidMountains entry
    int image = -1;
    int race = -1;

    MapCellCoord position;
    int          size_x = 0; // columns
    int          size_y = 0; // rows

    // Full footprint in canonical map-cell coordinates.
    std::vector<FootprintCell> footprint;
};

// ── Typed Landmark ──────────────────────────────────────────────────────
//
struct AdventureLandmark {
    std::string id;

    std::string type_id;    // TYPE — G000MGxxxx global ID (visual selector)
    std::string map_gfx_id; // MAP_GFX (falls back to IMAGE)
    std::string image;      // raw IMAGE string

    MapCellCoord position;

    std::vector<FootprintCell> footprint;
};

struct AdventureUnitGroup {
    // members[n] = the unit ID in slot n (0..5), or nullopt for empty
    std::array<std::optional<std::string>, 6> members;

    // positions[member_index] = formation cell (0..5), -1 if not placed.
    // Derived by inverting the cell-indexed SG POS_n (POS[cell] = member_index).
    // For large units, this stores the anchor (leftmost) cell.
    std::array<int, 6> positions = {-1, -1, -1, -1, -1, -1};

    // Exact SG POS_n formation occupancy: cell_members[formation_cell] = member_index, or -1
    // For large units, multiple cells may reference the same member_index.
    std::array<int, 6> cell_members = {-1, -1, -1, -1, -1, -1};
};

struct AdventureCapital {
    std::string                id;
    std::string                owner;
    std::string                subrace;
    std::string                visiting_stack_id;
    std::string                group_id;
    MapCellCoord               position;
    std::vector<FootprintCell> footprint;
    AdventureUnitGroup         garrison;
};

enum class AdventureSettlementKind {
    Village,
    Capital,
};

struct AdventureContainedStackLocation {
    AdventureSettlementKind           kind = AdventureSettlementKind::Village;
    std::string_view                  settlement_id;
    const std::vector<FootprintCell>* footprint = nullptr;
};

struct AdventureUnitInstance {
    std::string id;
    std::string type_id;

    int serialized_level = 0;

    std::vector<std::string> modifier_ids;

    int         creation = 0;
    std::string name;

    std::uint8_t                transformed = 0;
    std::optional<std::uint8_t> dynamic_level;

    int current_hp = 0;
    int xp = 0;
};

struct AdventureStack {
    std::string id;
    std::string group_id;

    std::string owner;
    std::string subrace;
    std::string inside;

    MapCellCoord position;

    int move = 0;
    int morale = 0;
    int battles_won = 0;

    std::string  leader_id;
    std::uint8_t leader_alive = 0;

    AdventureIsoDirection facing = AdventureIsoDirection::D0;

    AdventureUnitGroup group;

    std::string banner;
    std::string tome;
    std::string battle1;
    std::string battle2;
    std::string artifact1;
    std::string artifact2;
    std::string boots;
};

[[nodiscard]] inline bool is_stack_on_adventure_map(const AdventureStack& stack) {
    return stack.inside.empty() || stack.inside == "G000000000";
}

// Minimal runtime subrace record — maps scenario subrace id → numeric race info.
struct AdventureSubraceRef {
    std::string id; // scenario subrace id (e.g. "G000G00000")
    std::string player_id;
    std::string race_id;     // resolved from SgPlayer.race_id
    int         subrace = 0; // numeric subrace
    int         number = 0;
    std::string name_txt;
    int         banner = 0;
};

struct AdventureWorldState {
    std::string scenario_id;
    std::string scenario_name;

    int                  map_width = 0;
    int                  map_height = 0;
    int                  terrain_tiles = 0;
    int                  map_seed = 0;
    AdventureTerrainGrid terrain;

    std::size_t semantic_object_count = 0;
    std::size_t runtime_object_count = 0;

    std::vector<WorldObjectEntry>   objects;
    std::vector<AdventureMapObject> map_objects;

    // Scenario subrace definitions (id → numeric subrace + player)
    std::vector<AdventureSubraceRef> subraces;

    // Typed collections (migrated from generic AdventureMapObject).
    std::vector<AdventureRoad>         roads;
    std::vector<AdventureMountain>     mountains;
    std::vector<AdventureLandmark>     landmarks;
    std::vector<AdventureCity>         cities;
    std::vector<AdventureSite>         sites;
    std::vector<AdventureRuin>         ruins;
    std::vector<AdventureResourceNode> resource_nodes;
    std::vector<AdventureCapital>      capitals;
    std::vector<AdventureTreasure>     treasures;
    std::vector<AdventureUnitInstance> units;
    std::vector<AdventureStack>        stacks;

    [[nodiscard]] const AdventureUnitInstance* find_unit(const std::string& id) const {
        for (const auto& unit : units) {
            if (unit.id == id)
                return &unit;
        }
        return nullptr;
    }

    [[nodiscard]] const AdventureStack* find_stack(const std::string& id) const {
        for (const auto& stack : stacks) {
            if (stack.id == id)
                return &stack;
        }
        return nullptr;
    }

    [[nodiscard]] const AdventureSubraceRef* find_subrace(const std::string& id) const {
        for (const auto& sr : subraces) {
            if (sr.id == id)
                return &sr;
        }
        return nullptr;
    }

    [[nodiscard]] const AdventureResourceNode* find_resource_node(const std::string& id) const {
        for (const auto& rn : resource_nodes) {
            if (rn.id == id)
                return &rn;
        }
        return nullptr;
    }

    [[nodiscard]] const AdventureRuin* find_ruin(std::string_view id) const {
        for (const auto& ruin : ruins) {
            if (ruin.id == id)
                return &ruin;
        }
        return nullptr;
    }

    [[nodiscard]] AdventureRuin* find_ruin(std::string_view id) {
        for (auto& ruin : ruins) {
            if (ruin.id == id)
                return &ruin;
        }
        return nullptr;
    }

    [[nodiscard]] const AdventureTreasure* find_treasure(std::string_view id) const {
        for (const auto& treasure : treasures) {
            if (treasure.id == id)
                return &treasure;
        }
        return nullptr;
    }

    [[nodiscard]] AdventureTreasure* find_treasure(std::string_view id) {
        for (auto& treasure : treasures) {
            if (treasure.id == id)
                return &treasure;
        }
        return nullptr;
    }

    [[nodiscard]] const AdventureCity* find_city(std::string_view id) const {
        for (const auto& city : cities) {
            if (city.id == id)
                return &city;
        }
        return nullptr;
    }

    [[nodiscard]] AdventureCity* find_city(std::string_view id) {
        for (auto& city : cities) {
            if (city.id == id)
                return &city;
        }
        return nullptr;
    }

    [[nodiscard]] const AdventureSite* find_site(std::string_view id) const {
        for (const auto& site : sites) {
            if (site.id == id)
                return &site;
        }
        return nullptr;
    }

    [[nodiscard]] AdventureSite* find_site(std::string_view id) {
        for (auto& site : sites) {
            if (site.id == id)
                return &site;
        }
        return nullptr;
    }

    [[nodiscard]] const AdventureCapital* find_capital(std::string_view id) const {
        for (const auto& capital : capitals) {
            if (capital.id == id)
                return &capital;
        }
        return nullptr;
    }

    [[nodiscard]] AdventureCapital* find_capital(std::string_view id) {
        for (auto& capital : capitals) {
            if (capital.id == id)
                return &capital;
        }
        return nullptr;
    }

    [[nodiscard]] std::optional<AdventureContainedStackLocation>
    find_contained_stack_location(const AdventureStack& stack) const {
        if (is_stack_on_adventure_map(stack)) {
            return std::nullopt;
        }
        if (stack.inside.empty()) {
            return std::nullopt;
        }

        if (const auto* city = find_city(stack.inside); city != nullptr) {
            if (city->stack_id != stack.id) {
                return std::nullopt;
            }
            return AdventureContainedStackLocation{AdventureSettlementKind::Village, city->id,
                                                   &city->footprint};
        }

        if (const auto* capital = find_capital(stack.inside); capital != nullptr) {
            if (capital->visiting_stack_id != stack.id) {
                return std::nullopt;
            }
            return AdventureContainedStackLocation{AdventureSettlementKind::Capital, capital->id,
                                                   &capital->footprint};
        }

        return std::nullopt;
    }
};

enum class BuildDiagnosticKind {
    MissingMapDimensions,
    InvalidMapDimensions,
    IgnoredObjectClass,
    EmptyObjectId,
    UnknownObjectClass,
    ParserWarning,
    MissingFootprint,
    DuplicateFootprint,
    UnsupportedResourceNodeValue,
    UnresolvedSprite,
    InvalidSiteImageIndex,
    UnknownPlanReference,
    ResourceNodePlanFootprintMismatch,
    InvalidCitySize,
    InvalidRuinImage,
    MissingTreasureGroundCell,
    MissingRuinGroundCell,
    DanglingStackUnitReference,
    InvalidFormationCell,
    PositionAssignedToEmptyMember,
    DanglingStackLeader,
    LeaderNotInStackGroup,
    DanglingCapitalGarrisonUnitReference,
    InvalidCapitalFormationCell,
    CapitalPositionAssignedToEmptyMember,
    InvalidFacingValue
};

struct BuildDiagnostic {
    BuildDiagnosticKind kind;
    std::string         message;
    std::string         object_id;
    std::string         object_class;
};

struct AdventureWorldBuildResult {
    AdventureWorldState          world;
    std::vector<BuildDiagnostic> diagnostics;

    [[nodiscard]] std::size_t warning_count() const;
    [[nodiscard]] std::size_t error_count() const;
};

[[nodiscard]] inline bool is_build_error(BuildDiagnosticKind kind) {
    switch (kind) {
    case BuildDiagnosticKind::MissingMapDimensions:
    case BuildDiagnosticKind::InvalidMapDimensions:
    case BuildDiagnosticKind::EmptyObjectId:
    case BuildDiagnosticKind::InvalidFacingValue:
    case BuildDiagnosticKind::UnsupportedResourceNodeValue:
    case BuildDiagnosticKind::ResourceNodePlanFootprintMismatch:
    case BuildDiagnosticKind::InvalidCitySize:
    case BuildDiagnosticKind::InvalidRuinImage:
        return true;
    case BuildDiagnosticKind::IgnoredObjectClass:
    case BuildDiagnosticKind::UnknownObjectClass:
    case BuildDiagnosticKind::ParserWarning:
    case BuildDiagnosticKind::MissingFootprint:
    case BuildDiagnosticKind::DuplicateFootprint:
    case BuildDiagnosticKind::UnresolvedSprite:
    case BuildDiagnosticKind::UnknownPlanReference:
    case BuildDiagnosticKind::DanglingStackUnitReference:
    case BuildDiagnosticKind::InvalidFormationCell:
    case BuildDiagnosticKind::PositionAssignedToEmptyMember:
    case BuildDiagnosticKind::DanglingStackLeader:
    case BuildDiagnosticKind::LeaderNotInStackGroup:
    case BuildDiagnosticKind::DanglingCapitalGarrisonUnitReference:
    case BuildDiagnosticKind::InvalidCapitalFormationCell:
    case BuildDiagnosticKind::CapitalPositionAssignedToEmptyMember:
    case BuildDiagnosticKind::MissingTreasureGroundCell:
    case BuildDiagnosticKind::MissingRuinGroundCell:
        return false;
    case BuildDiagnosticKind::InvalidSiteImageIndex:
        return true;
    }
    return false;
}

} // namespace d2runtime
