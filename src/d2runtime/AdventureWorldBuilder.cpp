#include "AdventureWorldBuilder.hpp"

#include "AdventureGroundClassifier.hpp"
#include "AdventureTerrainDecoder.hpp"
#include <d2scenario/ScenarioTemplate.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace d2runtime {

AdventureTerrainGrid normalize_raw_sg_terrain(const d2scenario::SgTerrainGrid& raw) {
    const int            canonical_width = raw.height;
    const int            canonical_height = raw.width;
    AdventureTerrainGrid canonical;
    canonical.width = canonical_width;
    canonical.height = canonical_height;
    canonical.tiles.assign(
        static_cast<std::size_t>(canonical_width) * static_cast<std::size_t>(canonical_height), {});
    for (int y = 0; y < canonical_height; ++y) {
        for (int x = 0; x < canonical_width; ++x) {
            canonical.tile_at(x, y)->raw_value = raw.tile_at(y, x);
        }
    }
    return canonical;
}

static void add_diag(std::vector<BuildDiagnostic>& diags, BuildDiagnosticKind kind,
                     std::string message, const std::string& oid = {},
                     const std::string& cls = {}) {
    diags.push_back({kind, std::move(message), oid, cls});
}

static AdventureMapObjectKind classify_site_kind(const std::string& kind) {
    if (kind == "MidSiteMerchant")
        return AdventureMapObjectKind::SiteMerchant;
    if (kind == "MidSiteMercs")
        return AdventureMapObjectKind::SiteMercenary;
    if (kind == "MidSiteTrainer")
        return AdventureMapObjectKind::SiteTrainer;
    if (kind == "MidSiteMage")
        return AdventureMapObjectKind::SiteMage;
    return AdventureMapObjectKind::SiteMerchant;
}

static std::optional<d2runtime::AdventureSiteKind> typed_site_kind(const std::string& kind) {
    if (kind == "MidSiteMage")
        return d2runtime::AdventureSiteKind::Mage;
    if (kind == "MidSiteMerchant")
        return d2runtime::AdventureSiteKind::Merchant;
    if (kind == "MidSiteMercs")
        return d2runtime::AdventureSiteKind::Mercenary;
    if (kind == "MidSiteTrainer")
        return d2runtime::AdventureSiteKind::Trainer;
    return std::nullopt;
}

static std::pair<int, int> site_image_range(d2runtime::AdventureSiteKind kind) {
    switch (kind) {
    case d2runtime::AdventureSiteKind::Mage:
        return {0, 3};
    case d2runtime::AdventureSiteKind::Merchant:
        return {0, 7};
    case d2runtime::AdventureSiteKind::Mercenary:
        return {0, 4};
    case d2runtime::AdventureSiteKind::Trainer:
        return {0, 3};
    }
    return {0, -1};
}

static bool is_empty_member_id(const std::string& id) {
    return id.empty() || id == "G000000000";
}

static AdventureTreasurePlacement resolve_treasure_placement(const AdventureWorldState& world,
                                                             int x, int y,
                                                             std::vector<BuildDiagnostic>& diags,
                                                             const std::string& treasure_id) {
    const auto* tile = world.terrain.tile_at(x, y);
    if (tile == nullptr) {
        add_diag(diags, BuildDiagnosticKind::MissingTreasureGroundCell,
                 "treasure " + treasure_id + " at pos=(" + std::to_string(x) + "," +
                     std::to_string(y) + ") has no terrain cell",
                 treasure_id, "MidBag");
        return AdventureTreasurePlacement::Land;
    }

    const AdventureTerrainDecoder decoder;
    const auto                    descriptor = decoder.decode_tile(tile->raw_value);
    return classify_adventure_ground(descriptor) == AdventureGroundType::Water
               ? AdventureTreasurePlacement::Water
               : AdventureTreasurePlacement::Land;
}

static AdventureSurfacePlacement
resolve_surface_placement(const AdventureTerrainDecoder& decoder, const AdventureWorldState& world,
                          int x, int y, std::vector<BuildDiagnostic>& diags,
                          const std::string& object_id, std::string_view object_class,
                          BuildDiagnosticKind missing_cell_kind) {
    const auto* tile = world.terrain.tile_at(x, y);
    if (tile == nullptr) {
        add_diag(diags, missing_cell_kind,
                 std::string(object_class) + " " + object_id + " at pos=(" + std::to_string(x) +
                     "," + std::to_string(y) + ") has no terrain cell",
                 object_id, std::string(object_class));
        return AdventureSurfacePlacement::Land;
    }

    const auto descriptor = decoder.decode_tile(tile->raw_value);
    return classify_adventure_ground(descriptor) == AdventureGroundType::Water
               ? AdventureSurfacePlacement::Water
               : AdventureSurfacePlacement::Land;
}

static bool is_valid_city_size(int size) {
    return size >= 1 && size <= 5;
}

struct UnitGroupDiagKinds {
    BuildDiagnosticKind dangling_unit_reference;
    BuildDiagnosticKind invalid_formation_cell;
    BuildDiagnosticKind empty_member_reference;
};

template <typename HasUnitFn>
static AdventureUnitGroup
build_unit_group(const std::string& object_id, std::string_view object_class,
                 const std::vector<std::string>& serialized_units,
                 const std::vector<int>& serialized_positions, const HasUnitFn& has_unit,
                 std::vector<BuildDiagnostic>& diags, const UnitGroupDiagKinds& kinds) {
    AdventureUnitGroup group;

    for (std::size_t i = 0; i < group.members.size(); ++i) {
        const std::string member_id =
            i < serialized_units.size() ? serialized_units[i] : std::string{"G000000000"};
        if (!is_empty_member_id(member_id)) {
            group.members[i] = member_id;
            if (!has_unit(member_id)) {
                add_diag(diags, kinds.dangling_unit_reference,
                         std::string(object_class) + " " + object_id + " member[" +
                             std::to_string(i) + "] references missing unit " + member_id,
                         object_id, std::string(object_class));
            }
        }
    }

    for (std::size_t cell = 0; cell < group.cell_members.size(); ++cell) {
        const int member_idx = cell < serialized_positions.size() ? serialized_positions[cell] : -1;
        if (member_idx < -1 || member_idx > 5) {
            add_diag(diags, kinds.invalid_formation_cell,
                     std::string(object_class) + " " + object_id + " formation cell " +
                         std::to_string(cell) + " references invalid member index " +
                         std::to_string(member_idx),
                     object_id, std::string(object_class));
            continue;
        }
        if (member_idx < 0) {
            continue;
        }
        if (member_idx >= static_cast<int>(serialized_units.size())) {
            add_diag(diags, kinds.invalid_formation_cell,
                     std::string(object_class) + " " + object_id + " formation cell " +
                         std::to_string(cell) + " references out-of-range member index " +
                         std::to_string(member_idx),
                     object_id, std::string(object_class));
            continue;
        }
        const auto& member_id = serialized_units[static_cast<std::size_t>(member_idx)];
        if (is_empty_member_id(member_id)) {
            add_diag(diags, kinds.empty_member_reference,
                     std::string(object_class) + " " + object_id + " formation cell " +
                         std::to_string(cell) + " references empty member[" +
                         std::to_string(member_idx) + "]",
                     object_id, std::string(object_class));
            continue;
        }

        group.cell_members[cell] = member_idx;
        if (group.positions[static_cast<std::size_t>(member_idx)] < 0) {
            group.positions[static_cast<std::size_t>(member_idx)] = static_cast<int>(cell);
        }
    }

    return group;
}

// Build a footprint map from MidgardPlan entries: object-id -> set of occupied cells
static std::map<std::string, std::set<std::pair<int, int>>>
build_plan_footprints(const std::vector<d2scenario::SgMidgardPlan>& plans,
                      std::vector<BuildDiagnostic>&                 diags) {
    std::map<std::string, std::set<std::pair<int, int>>> fp_map;

    for (const auto& plan : plans) {
        for (const auto& entry : plan.entries) {
            auto  cell = std::make_pair(entry.pos_x, entry.pos_y);
            auto& cells = fp_map[entry.element];
            if (!cells.insert(cell).second) {
                add_diag(diags, BuildDiagnosticKind::DuplicateFootprint,
                         "duplicate footprint cell (" + std::to_string(entry.pos_x) + "," +
                             std::to_string(entry.pos_y) + ") for object " + entry.element,
                         entry.element, "MidgardPlan");
            }
        }
    }

    return fp_map;
}

AdventureWorldBuildResult
AdventureWorldBuilder::build(const d2scenario::ScenarioTemplate& scenario) {
    AdventureWorldState          world;
    std::vector<BuildDiagnostic> diags;

    world.scenario_id = scenario.info.id;
    world.scenario_name = scenario.info.name;
    world.map_seed = scenario.info.map_seed;

    // Map dimension priority:
    //   1. Valid terrain grid dimensions
    //   2. Fallback to info.map_size > 0
    //   3. Otherwise diagnostics
    bool have_terrain = scenario.map.terrain.width > 0 && scenario.map.terrain.height > 0;

    if (have_terrain) {
        std::size_t stored = 0;
        for (const auto& row : scenario.map.terrain.tiles) {
            stored += row.size();
        }

        world.terrain = normalize_raw_sg_terrain(scenario.map.terrain);
        world.map_width = world.terrain.width;
        world.map_height = world.terrain.height;
        world.terrain_tiles = static_cast<int>(world.terrain.size());

        if (stored != 0 && static_cast<std::size_t>(world.terrain_tiles) != stored) {
            add_diag(diags, BuildDiagnosticKind::ParserWarning,
                     "terrain tile count mismatch: expected " +
                         std::to_string(world.terrain_tiles) + " tiles, stored " +
                         std::to_string(stored),
                     scenario.map.id, "SgMap");
        }
    } else if (scenario.info.map_size > 0) {
        world.map_width = scenario.info.map_size;
        world.map_height = scenario.info.map_size;
        world.terrain_tiles = 0;
    } else {
        add_diag(diags, BuildDiagnosticKind::MissingMapDimensions,
                 "no valid map dimensions: terrain grid absent and info.map_size is zero",
                 scenario.info.id);
        world.map_width = 0;
        world.map_height = 0;
    }

    // Build footprint map from MidgardPlan
    auto plan_footprints = build_plan_footprints(scenario.plans, diags);

    auto get_footprint = [&](const std::string& oid, int /*px*/,
                             int /*py*/) -> std::vector<FootprintCell> {
        auto it = plan_footprints.find(oid);
        if (it != plan_footprints.end() && !it->second.empty()) {
            std::vector<FootprintCell> cells;
            cells.reserve(it->second.size());
            for (const auto& c : it->second)
                cells.emplace_back(MapCellCoord{c.first, c.second});
            return cells;
        }
        return {};
    };

    // Count semantic objects
    world.semantic_object_count = 0;

    auto count_vec = [&](const auto& vec) { world.semantic_object_count += vec.size(); };

    count_vec(scenario.players);
    count_vec(scenario.subraces);
    count_vec(scenario.units);
    count_vec(scenario.stacks);
    count_vec(scenario.cities);
    count_vec(scenario.sites);
    count_vec(scenario.ruins);
    count_vec(scenario.bags);
    count_vec(scenario.locations);
    count_vec(scenario.events);
    count_vec(scenario.items);
    count_vec(scenario.landmarks);
    count_vec(scenario.roads);
    count_vec(scenario.crystals);
    count_vec(scenario.stack_templates);
    count_vec(scenario.scen_variables);
    count_vec(scenario.diplomacy);
    count_vec(scenario.talisman_charges);
    count_vec(scenario.plans);
    count_vec(scenario.mountains);
    count_vec(scenario.turn_summaries);
    count_vec(scenario.known_spells);
    count_vec(scenario.buildings);
    count_vec(scenario.map_fogs);

    // Populate runtime object index entries
    world.objects.reserve(scenario.players.size() + scenario.units.size() + scenario.stacks.size() +
                          scenario.cities.size());

    auto add_obj = [&](const std::string& id, const std::string& kind) {
        if (id.empty()) {
            add_diag(diags, BuildDiagnosticKind::EmptyObjectId, kind + " has empty id", id, kind);
            return;
        }
        world.objects.push_back({id, kind});
    };

    for (const auto& p : scenario.players)
        add_obj(p.id, "player");
    for (const auto& u : scenario.units)
        add_obj(u.id, "unit");
    for (const auto& s : scenario.stacks)
        add_obj(s.id, "stack");
    for (const auto& c : scenario.cities)
        add_obj(c.id, "city");

    std::set<std::string> unit_ids;
    world.units.reserve(scenario.units.size());
    for (const auto& su : scenario.units) {
        AdventureUnitInstance unit;
        unit.id = su.id;
        unit.type_id = su.type_id;
        unit.serialized_level = su.level_raw_i32;
        unit.modifier_ids.assign(su.modifier_ids.begin(), su.modifier_ids.end());
        unit.creation = su.creation;
        unit.name = su.name;
        unit.transformed = su.transformed;
        unit.dynamic_level = su.dynamic_level;
        unit.current_hp = su.hp;
        unit.xp = su.xp;
        if (!unit.id.empty())
            unit_ids.insert(unit.id);
        world.units.push_back(std::move(unit));
    }

    auto has_unit = [&](const std::string& id) { return unit_ids.count(id) > 0; };

    // Build AdventureMapObjects
    auto& map_objs = world.map_objects;

    // Capitals and Villages
    world.capitals.reserve(scenario.cities.size());
    for (const auto& c : scenario.cities) {
        AdventureMapObject mo;
        mo.id = c.id;
        mo.kind =
            (c.kind == "Capital") ? AdventureMapObjectKind::Capital : AdventureMapObjectKind::City;
        mo.position.x = c.pos_x;
        mo.position.y = c.pos_y;
        mo.footprint = get_footprint(c.id, c.pos_x, c.pos_y);
        mo.owner = c.owner;
        mo.subrace = c.subrace;
        if (mo.footprint.empty()) {
            add_diag(diags, BuildDiagnosticKind::MissingFootprint,
                     "no MidgardPlan footprint for " + c.kind + " " + c.id, c.id, c.kind);
        }
        const auto city_footprint = mo.footprint;
        map_objs.push_back(std::move(mo));

        if (c.kind == "Capital") {
            AdventureCapital capital;
            capital.id = c.id;
            capital.owner = c.owner;
            capital.subrace = c.subrace;
            capital.visiting_stack_id = c.stack;
            capital.group_id = c.group_id;
            capital.position.x = c.pos_x;
            capital.position.y = c.pos_y;
            capital.footprint = get_footprint(c.id, c.pos_x, c.pos_y);

            capital.garrison =
                build_unit_group(capital.id, "MidCapital", c.unit_ids, c.positions, has_unit, diags,
                                 {BuildDiagnosticKind::DanglingCapitalGarrisonUnitReference,
                                  BuildDiagnosticKind::InvalidCapitalFormationCell,
                                  BuildDiagnosticKind::CapitalPositionAssignedToEmptyMember});

            world.capitals.push_back(std::move(capital));
        }
        if (c.kind == "MidVillage") {
            if (!is_valid_city_size(c.size)) {
                add_diag(diags, BuildDiagnosticKind::InvalidCitySize,
                         "city " + c.id + " pos=(" + std::to_string(c.pos_x) + "," +
                             std::to_string(c.pos_y) + ") size=" + std::to_string(c.size) +
                             " expected=1..5",
                         c.id, c.kind);
                continue;
            }

            AdventureCity city;
            city.id = c.id;
            city.name_txt_id = c.name;
            city.description_txt_id = c.description;
            city.owner_id = c.owner;
            city.subrace_id = c.subrace;
            city.stack_id = c.stack;
            city.group_id = c.group_id;
            city.position.x = c.pos_x;
            city.position.y = c.pos_y;
            city.ai_priority = c.ai_priority;
            city.size = c.size;
            city.footprint = city_footprint;
            world.cities.push_back(std::move(city));
        }
    }

    const AdventureTerrainDecoder terrain_decoder;

    // Sites
    world.sites.reserve(scenario.sites.size());
    for (const auto& s : scenario.sites) {
        AdventureMapObject mo;
        mo.id = s.id;
        mo.kind = classify_site_kind(s.kind);
        mo.position.x = s.pos_x;
        mo.position.y = s.pos_y;
        mo.footprint = get_footprint(s.id, s.pos_x, s.pos_y);
        mo.image = s.image_iso;
        if (mo.footprint.empty()) {
            add_diag(diags, BuildDiagnosticKind::MissingFootprint,
                     "no MidgardPlan footprint for site " + s.id, s.id, s.kind);
        }
        map_objs.push_back(std::move(mo));

        const auto kind = typed_site_kind(s.kind);
        if (!kind.has_value()) {
            continue;
        }

        const auto [min_image, max_image] = site_image_range(*kind);
        if (s.image_iso < min_image || s.image_iso > max_image) {
            add_diag(
                diags, BuildDiagnosticKind::InvalidSiteImageIndex,
                "site " + s.id + " class=" + s.kind + " pos=(" + std::to_string(s.pos_x) + "," +
                    std::to_string(s.pos_y) + ") IMG_ISO=" + std::to_string(s.image_iso) +
                    " expected=" + std::to_string(min_image) + ".." + std::to_string(max_image),
                s.id, s.kind);
            continue;
        }

        AdventureSite site;
        site.id = s.id;
        site.kind = *kind;
        site.title = s.title;
        site.description = s.description;
        site.image_iso = s.image_iso;
        site.image_interface = s.image_interface;
        site.position.x = s.pos_x;
        site.position.y = s.pos_y;
        site.visitor_id = s.visitor;
        site.ai_priority = s.ai_priority;
        site.footprint = get_footprint(s.id, s.pos_x, s.pos_y);

        switch (site.kind) {
        case AdventureSiteKind::Mage: {
            AdventureMageSiteData payload;
            payload.declared_spell_count = s.qty_spell;
            payload.spell_ids.assign(s.spells.begin(), s.spells.end());
            site.payload = std::move(payload);
            break;
        }
        case AdventureSiteKind::Merchant: {
            AdventureMerchantSiteData payload;
            payload.buy_armor = s.buy_armor;
            payload.buy_jewel = s.buy_jewel;
            payload.buy_weapon = s.buy_weapon;
            payload.buy_banner = s.buy_banner;
            payload.buy_potion = s.buy_potion;
            payload.buy_scroll = s.buy_scroll;
            payload.buy_wand = s.buy_wand;
            payload.buy_value = s.buy_value;
            payload.declared_item_count = s.qty_item;
            payload.item_ids.assign(s.items.begin(), s.items.end());
            payload.mission_ids.assign(s.missions.begin(), s.missions.end());
            site.payload = std::move(payload);
            break;
        }
        case AdventureSiteKind::Mercenary: {
            AdventureMercenarySiteData payload;
            payload.declared_unit_count = s.qty_unit;
            payload.unit_ids.assign(s.units.begin(), s.units.end());
            site.payload = std::move(payload);
            break;
        }
        case AdventureSiteKind::Trainer: {
            site.payload = AdventureTrainerSiteData{};
            break;
        }
        }

        world.sites.push_back(std::move(site));
    }

    // Ruins
    world.ruins.reserve(scenario.ruins.size());
    for (const auto& r : scenario.ruins) {
        AdventureMapObject mo;
        mo.id = r.id;
        mo.kind = AdventureMapObjectKind::Ruin;
        mo.position.x = r.pos_x;
        mo.position.y = r.pos_y;
        mo.footprint = get_footprint(r.id, r.pos_x, r.pos_y);
        mo.image = r.image;
        if (mo.footprint.empty()) {
            add_diag(diags, BuildDiagnosticKind::MissingFootprint,
                     "no MidgardPlan footprint for ruin " + r.id, r.id, "MidRuin");
        }
        map_objs.push_back(std::move(mo));

        if (r.image < 0 || r.image > 10) {
            add_diag(diags, BuildDiagnosticKind::InvalidRuinImage,
                     "ruin " + r.id + " pos=(" + std::to_string(r.pos_x) + "," +
                         std::to_string(r.pos_y) + ") image=" + std::to_string(r.image) +
                         " expected=0..10",
                     r.id, "MidRuin");
            continue;
        }

        AdventureRuin ruin;
        ruin.id = r.id;
        ruin.title = r.title;
        ruin.description = r.description;
        ruin.cash = r.cash;
        ruin.item_id = r.item;
        ruin.looter_id = r.looter;
        ruin.ai_priority = r.ai_priority;
        ruin.image = r.image;
        ruin.position.x = r.pos_x;
        ruin.position.y = r.pos_y;
        ruin.placement =
            resolve_surface_placement(terrain_decoder, world, r.pos_x, r.pos_y, diags, r.id,
                                      "MidRuin", BuildDiagnosticKind::MissingRuinGroundCell);
        ruin.defender_unit_ids = r.unit_ids;
        ruin.formation_positions = r.positions;
        ruin.footprint = get_footprint(r.id, r.pos_x, r.pos_y);
        world.ruins.push_back(std::move(ruin));
    }

    // Bags
    world.treasures.reserve(scenario.bags.size());
    for (const auto& b : scenario.bags) {
        AdventureMapObject mo;
        mo.id = b.id;
        mo.kind = AdventureMapObjectKind::Bag;
        mo.position.x = b.pos_x;
        mo.position.y = b.pos_y;
        mo.footprint = get_footprint(b.id, b.pos_x, b.pos_y);
        mo.image = b.image;
        if (mo.footprint.empty()) {
            // Bag footprint defaults to 1x1 at pos
            mo.footprint.emplace_back(MapCellCoord{b.pos_x, b.pos_y});
        }
        map_objs.push_back(std::move(mo));

        AdventureTreasure treasure;
        treasure.id = b.id;
        treasure.looter_id = b.looter;
        treasure.item_ids.reserve(b.items.size());
        for (const auto& item_id : b.items) {
            treasure.item_ids.push_back(item_id);
        }
        treasure.position.x = b.pos_x;
        treasure.position.y = b.pos_y;
        treasure.image = b.image;
        treasure.placement = resolve_treasure_placement(world, b.pos_x, b.pos_y, diags, b.id);
        treasure.footprint = map_objs.back().footprint;
        world.treasures.push_back(std::move(treasure));
    }

    // Roads
    world.roads.reserve(scenario.roads.size());
    for (const auto& rd : scenario.roads) {
        AdventureMapObject mo;
        mo.id = rd.id;
        mo.kind = AdventureMapObjectKind::Road;
        mo.position.x = rd.pos_x;
        mo.position.y = rd.pos_y;
        mo.footprint = get_footprint(rd.id, rd.pos_x, rd.pos_y);
        mo.index = rd.index;
        mo.variant = rd.variant;
        mo.blocking = false;
        if (mo.footprint.empty()) {
            mo.footprint.emplace_back(MapCellCoord{rd.pos_x, rd.pos_y});
        }
        map_objs.push_back(std::move(mo));

        // Typed AdventureRoad
        AdventureRoad aroad;
        aroad.id = rd.id;
        aroad.index = rd.index;
        aroad.variant = rd.variant;
        aroad.position.x = rd.pos_x;
        aroad.position.y = rd.pos_y;
        world.roads.push_back(std::move(aroad));
    }

    // ── ResourceKind normalization ──────────────────────────────────────────
    auto normalize_resource_kind = [&diags](int raw, const std::string& oid, int pos_x, int pos_y,
                                            const std::string& raw_type,
                                            const std::string& owner) -> AdventureResourceKind {
        switch (raw) {
        case 0:
            return AdventureResourceKind::GoldMine;
        case 1:
            return AdventureResourceKind::RedMana;
        case 2:
            return AdventureResourceKind::YellowMana;
        case 3:
            return AdventureResourceKind::OrangeMana;
        case 4:
            return AdventureResourceKind::WhiteMana;
        case 5:
            return AdventureResourceKind::BlueMana;
        default: {
            std::string msg = "unsupported RESOURCE value " + std::to_string(raw);
            msg += " for resource node " + oid;
            msg += " category=ResourceNode";
            msg += " position=(" + std::to_string(pos_x) + "," + std::to_string(pos_y) + ")";
            msg += " raw_RESOURCE=" + std::to_string(raw);
            msg += " raw_TYPE=" + (raw_type.empty() ? "<empty>" : raw_type);
            msg += " raw_OWNER=" + (owner.empty() ? "<empty>" : owner);
            msg += " expected domain 0..5";
            msg += " reason=unsupported_resource_value";
            add_diag(diags, BuildDiagnosticKind::UnsupportedResourceNodeValue, msg, oid,
                     "MidCrystal");
            return AdventureResourceKind::GoldMine;
        }
        }
    };

    // Crystals → ResourceNodes
    world.resource_nodes.reserve(scenario.crystals.size());
    for (const auto& cr : scenario.crystals) {
        AdventureResourceNode node;
        node.id = cr.id;
        node.position.x = cr.pos_x;
        node.position.y = cr.pos_y;
        node.raw_resource = cr.resource;
        node.resource_kind =
            normalize_resource_kind(cr.resource, cr.id, cr.pos_x, cr.pos_y, cr.type, cr.owner);
        node.raw_type = cr.type;
        node.owner = cr.owner;
        node.ai_priority = cr.ai_priority;

        // Plan footprint validation
        auto it = plan_footprints.find(cr.id);
        if (it == plan_footprints.end() || it->second.empty()) {
            std::string msg = "resource node " + cr.id;
            msg += " pos=(" + std::to_string(cr.pos_x) + "," + std::to_string(cr.pos_y) + ")";
            msg += " missing plan footprint";
            add_diag(diags, BuildDiagnosticKind::ResourceNodePlanFootprintMismatch, msg, cr.id,
                     "MidCrystal");
        } else if (it->second.size() != 1) {
            std::string msg = "resource node " + cr.id;
            msg += " pos=(" + std::to_string(cr.pos_x) + "," + std::to_string(cr.pos_y) + ")";
            msg += " plan footprint contains " + std::to_string(it->second.size()) +
                   " cells (expected exactly 1)";
            add_diag(diags, BuildDiagnosticKind::ResourceNodePlanFootprintMismatch, msg, cr.id,
                     "MidCrystal");
        } else {
            const auto& cell = *it->second.begin();
            if (cell.first != cr.pos_x || cell.second != cr.pos_y) {
                std::string msg = "resource node " + cr.id;
                msg += " pos=(" + std::to_string(cr.pos_x) + "," + std::to_string(cr.pos_y) + ")";
                msg += " plan footprint cell (" + std::to_string(cell.first) + "," +
                       std::to_string(cell.second) + ")";
                msg += " inconsistent with MidCrystal position";
                add_diag(diags, BuildDiagnosticKind::ResourceNodePlanFootprintMismatch, msg, cr.id,
                         "MidCrystal");
            }
            for (const auto& c : it->second)
                node.footprint.emplace_back(MapCellCoord{c.first, c.second});
        }

        // Compatibility AdventureMapObject entry (not used for rendering) —
        // build from source values before moving node.
        AdventureMapObject mo;
        mo.id = cr.id;
        mo.kind = AdventureMapObjectKind::ResourceNode;
        mo.position.x = cr.pos_x;
        mo.position.y = cr.pos_y;
        mo.footprint = node.footprint;
        mo.resource = cr.resource;
        mo.owner = cr.owner;
        if (mo.footprint.empty()) {
            mo.footprint.emplace_back(MapCellCoord{cr.pos_x, cr.pos_y});
        }
        map_objs.push_back(std::move(mo));

        world.resource_nodes.push_back(std::move(node));
    }

    // Mountains (from MidMountains, not MidgardPlan)
    for (const auto& mt : scenario.mountains) {
        for (const auto& entry : mt.entries) {
            AdventureMapObject mo;
            const std::string  mountain_id = mt.id + "/" + std::to_string(entry.id_mount);
            mo.id = mountain_id;
            mo.kind = AdventureMapObjectKind::Mountain;
            mo.position.x = entry.pos_x;
            mo.position.y = entry.pos_y;
            mo.image = entry.image;
            mo.race = entry.race;
            mo.id_mount = entry.id_mount;
            for (int dy = 0; dy < entry.size_y; ++dy) {
                for (int dx = 0; dx < entry.size_x; ++dx) {
                    mo.footprint.emplace_back(MapCellCoord{entry.pos_x + dx, entry.pos_y + dy});
                }
            }
            if (mo.footprint.empty()) {
                mo.footprint.emplace_back(MapCellCoord{entry.pos_x, entry.pos_y});
            }

            // Typed AdventureMountain — build before moving mo out
            AdventureMountain amount;
            amount.id = mountain_id;
            amount.id_mount = entry.id_mount;
            amount.image = entry.image;
            amount.race = entry.race;
            amount.position.x = entry.pos_x;
            amount.position.y = entry.pos_y;
            amount.size_x = entry.size_x;
            amount.size_y = entry.size_y;
            amount.footprint = mo.footprint;
            world.mountains.push_back(std::move(amount));

            map_objs.push_back(std::move(mo));
        }
    }

    // Stacks (visible on adventure map)
    for (const auto& st : scenario.stacks) {
        AdventureStack stack;
        stack.id = st.id;
        stack.group_id = st.group_id;
        stack.owner = st.owner;
        stack.subrace = st.subrace;
        stack.inside = st.inside;
        stack.position.x = st.pos_x;
        stack.position.y = st.pos_y;
        stack.move = st.move;
        stack.morale = st.morale;
        stack.battles_won = st.battles_won;
        stack.leader_id = st.leader_id;
        stack.leader_alive = st.leader_alive;
        stack.banner = st.banner;
        stack.tome = st.tome;
        stack.battle1 = st.battle1;
        stack.battle2 = st.battle2;
        stack.artifact1 = st.artifact1;
        stack.artifact2 = st.artifact2;
        stack.boots = st.boots;

        if (st.facing < 0 || st.facing > 7) {
            add_diag(diags, BuildDiagnosticKind::InvalidFacingValue,
                     "stack " + st.id + " pos=(" + std::to_string(st.pos_x) + "," +
                         std::to_string(st.pos_y) +
                         ") has invalid FACING=" + std::to_string(st.facing) + " (must be 0..7)",
                     st.id, "MidStack");
        } else {
            stack.facing = direction_from_index(st.facing);
        }

        stack.group =
            build_unit_group(stack.id, "MidStack", st.units, st.positions, has_unit, diags,
                             {BuildDiagnosticKind::DanglingStackUnitReference,
                              BuildDiagnosticKind::InvalidFormationCell,
                              BuildDiagnosticKind::PositionAssignedToEmptyMember});

        if (!is_empty_member_id(stack.leader_id)) {
            if (!has_unit(stack.leader_id)) {
                add_diag(diags, BuildDiagnosticKind::DanglingStackLeader,
                         "stack " + st.id + " leader references missing unit " + stack.leader_id,
                         st.id, "MidStack");
            }

            bool leader_in_group = false;
            for (const auto& member : stack.group.members) {
                if (member.has_value() && *member == stack.leader_id) {
                    leader_in_group = true;
                    break;
                }
            }
            if (!leader_in_group) {
                add_diag(diags, BuildDiagnosticKind::LeaderNotInStackGroup,
                         "stack " + st.id + " leader " + stack.leader_id +
                             " is not present in member table",
                         st.id, "MidStack");
            }
        }
        world.stacks.push_back(std::move(stack));

        AdventureMapObject mo;
        mo.id = st.id;
        mo.kind = AdventureMapObjectKind::Stack;
        mo.position.x = st.pos_x;
        mo.position.y = st.pos_y;
        mo.footprint = get_footprint(st.id, st.pos_x, st.pos_y);
        mo.owner = st.owner;
        mo.subrace = st.subrace;
        if (mo.footprint.empty()) {
            mo.footprint.emplace_back(MapCellCoord{st.pos_x, st.pos_y});
        }
        map_objs.push_back(std::move(mo));
    }

    // Landmarks
    for (const auto& lm : scenario.landmarks) {
        AdventureMapObject mo;
        mo.id = lm.id;
        mo.kind = AdventureMapObjectKind::Landmark;
        mo.position.x = lm.pos_x;
        mo.position.y = lm.pos_y;
        mo.footprint = get_footprint(lm.id, lm.pos_x, lm.pos_y);
        if (mo.footprint.empty()) {
            mo.footprint.emplace_back(MapCellCoord{lm.pos_x, lm.pos_y});
        }

        // Typed AdventureLandmark — build before moving mo out
        AdventureLandmark alandmark;
        alandmark.id = lm.id;
        alandmark.type_id = lm.type;
        alandmark.map_gfx_id = lm.map_gfx_id;
        alandmark.image = lm.image;
        alandmark.position.x = lm.pos_x;
        alandmark.position.y = lm.pos_y;
        alandmark.footprint = mo.footprint;
        world.landmarks.push_back(std::move(alandmark));

        map_objs.push_back(std::move(mo));
    }

    // Track object IDs that are present in plan but have no semantic object
    std::set<std::string> semantic_ids;
    for (const auto& c : scenario.cities)
        semantic_ids.insert(c.id);
    for (const auto& s : scenario.sites)
        semantic_ids.insert(s.id);
    for (const auto& r : scenario.ruins)
        semantic_ids.insert(r.id);
    for (const auto& b : scenario.bags)
        semantic_ids.insert(b.id);
    for (const auto& rd : scenario.roads)
        semantic_ids.insert(rd.id);
    for (const auto& cr : scenario.crystals)
        semantic_ids.insert(cr.id);
    for (const auto& st : scenario.stacks)
        semantic_ids.insert(st.id);
    for (const auto& lm : scenario.landmarks)
        semantic_ids.insert(lm.id);

    for (const auto& [plan_oid, cells] : plan_footprints) {
        bool found = semantic_ids.count(plan_oid) > 0;
        // Check mountain entries
        for (const auto& mt : scenario.mountains) {
            if (mt.id == plan_oid) {
                found = true;
                break;
            }
        }
        if (!found) {
            add_diag(diags, BuildDiagnosticKind::UnknownPlanReference,
                     "MidgardPlan entry references unknown object " + plan_oid, plan_oid,
                     "MidgardPlan");
        }
    }

    // Subraces
    world.subraces.reserve(scenario.subraces.size());
    for (const auto& sr : scenario.subraces) {
        AdventureSubraceRef ref;
        ref.id = sr.id;
        ref.player_id = sr.player_id;

        for (const auto& p : scenario.players) {
            if (p.id == sr.player_id) {
                ref.race_id = p.race_id;
                break;
            }
        }

        ref.subrace = sr.subrace;
        ref.number = sr.number;
        ref.name_txt = sr.name_txt;
        ref.banner = sr.banner;
        world.subraces.push_back(std::move(ref));
    }

    world.runtime_object_count = world.objects.size() + world.map_objects.size();

    return {std::move(world), std::move(diags)};
}

std::size_t AdventureWorldBuildResult::warning_count() const {
    return static_cast<std::size_t>(
        std::count_if(diagnostics.begin(), diagnostics.end(),
                      [](const BuildDiagnostic& d) { return !is_build_error(d.kind); }));
}

std::size_t AdventureWorldBuildResult::error_count() const {
    return static_cast<std::size_t>(
        std::count_if(diagnostics.begin(), diagnostics.end(),
                      [](const BuildDiagnostic& d) { return is_build_error(d.kind); }));
}

} // namespace d2runtime
