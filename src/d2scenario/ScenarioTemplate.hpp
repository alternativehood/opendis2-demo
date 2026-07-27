#pragma once

// ── Runtime semantic scenario model ──────────────────────────────────────
//
// This header defines the domain types consumed by runtime code (engine,
// analysis, reports). It intentionally does NOT include SgTypes.hpp — code
// that only needs the semantic model should include this file, not the full
// parser/diagnostics header.
//
// ScenarioTemplate — runtime-consumable domain data (no raw bytes, no
//                    diagnostics, no empty hulls, no object classification)
// SgParseResult    — defined in SgTypes.hpp; contains ScenarioTemplate
//                    scenario member + diagnostics

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace d2scenario {

// ── Core ID type ─────────────────────────────────────────────────────────
//
// Identifies a scenario object (player, unit, stack, etc.) within an .sg file.
using SgObjectId = std::string;

// ── Semantic scenario structs ────────────────────────────────────────────

struct SgScenarioInfo {
    SgObjectId               id;
    std::string              name;
    std::string              creator;
    std::string              briefing;
    std::string              description;
    int                      map_size = 0;
    int                      map_seed = 0;
    int                      current_turn = 0;
    int                      max_unit = 0;
    int                      max_spell = 0;
    int                      max_leader = 0;
    int                      max_city = 0;
    int                      diff_scenario = 0;
    int                      diff_game = 0;
    int                      suggested_level = 0;
    std::vector<std::string> brief_long;
    std::vector<std::string> debunk_win;
    std::string              debunk_loss;
    std::string              campaign;
};

struct SgPlayer {
    SgObjectId  id;
    std::string name;
    std::string description;
    SgObjectId  lord_id;
    SgObjectId  race_id;
    SgObjectId  fog_id;
    SgObjectId  known_id;
    SgObjectId  builds_id;
    int         face = 0;
    bool        is_human = false;
    std::string bank;
    std::string spell_bank;
    bool        attitude = false;
    bool        always_ai = false;
};

struct SgSubRace {
    SgObjectId  id;
    int         subrace = 0;
    SgObjectId  player_id;
    int         number = 0;
    std::string name_txt;
    int         banner = 0;
};

struct SgUnit {
    SgObjectId                  id;
    SgObjectId                  type_id;
    int                         level_raw_i32 = 0;
    std::vector<SgObjectId>     modifier_ids;
    std::uint8_t                transformed = 0;
    std::optional<std::uint8_t> dynamic_level;
    int                         hp = 0;
    int                         xp = 0;
    int                         creation = 0;
    std::string                 name;
};

struct SgStack {
    SgObjectId              id;
    SgObjectId              group_id;
    SgObjectId              owner;
    std::vector<SgObjectId> units;
    // positions[formation_cell] = member_index (cell-indexed; POS[cell] = member_idx)
    // -1 means the formation cell is empty; 0..5 indexes the corresponding UNIT_n member.
    std::vector<int> positions;
    SgObjectId       leader_id;
    std::uint8_t     leader_alive = 0;
    int              pos_x = 0;
    int              pos_y = 0;
    int              move = 0;
    int              morale = 0;
    SgObjectId       inside;
    SgObjectId       subrace;
    bool             invisible = false;
    bool             ai_ignore = false;
    int              order = 0;
    int              ai_order = 0;
    int              ai_priority = 0;
    int              create_level = 0;
    int              battles_won = 0;
    SgObjectId       banner;
    SgObjectId       tome;
    SgObjectId       battle1;
    SgObjectId       battle2;
    SgObjectId       artifact1;
    SgObjectId       artifact2;
    SgObjectId       boots;
    int              facing = 0;
};

struct SgCityOrVillage {
    SgObjectId              id;
    std::string             kind;
    std::string             name;
    std::string             description;
    SgObjectId              owner;
    SgObjectId              subrace;
    SgObjectId              stack;
    int                     pos_x = 0;
    int                     pos_y = 0;
    SgObjectId              group_id;
    int                     ai_priority = 0;
    int                     size = 0;
    std::vector<SgObjectId> unit_ids;
    // positions[formation_cell] = member_index (cell-indexed; POS[cell] = member_idx)
    std::vector<int>        positions;
    std::vector<SgObjectId> item_ids;
};

struct SgRuin {
    SgObjectId              id;
    std::string             title;
    std::string             description;
    int                     image = 0;
    int                     pos_x = 0;
    int                     pos_y = 0;
    std::string             cash;
    SgObjectId              item;
    SgObjectId              looter;
    int                     ai_priority = 0;
    std::vector<SgObjectId> unit_ids;
    // positions[formation_cell] = member_index (cell-indexed; POS[cell] = member_idx)
    std::vector<int> positions;
};

struct SgBag {
    SgObjectId              id;
    int                     pos_x = 0;
    int                     pos_y = 0;
    int                     image = 0;
    std::string             cash;
    std::vector<SgObjectId> items;
    SgObjectId              looter;
    int                     ai_priority = 0;
};

struct SgLocation {
    SgObjectId  id;
    int         pos_x = 0;
    int         pos_y = 0;
    std::string name;
    int         radius = 0;
};

struct SgEvent {
    SgObjectId               id;
    std::string              name;
    bool                     enabled = true;
    bool                     occur_once = false;
    int                      chance = 0;
    int                      order = 0;
    int                      cond_qty = 0;
    int                      effect_qty = 0;
    int                      frequency = 0;
    std::vector<SgObjectId>  locations;
    std::vector<SgObjectId>  players;
    std::vector<std::string> popup_texts;
    std::vector<int>         category_values;
    std::vector<int>         num_values;
    std::vector<std::string> refs;
};

struct SgItem {
    SgObjectId id;
    SgObjectId type;
};

struct SgLandmark {
    SgObjectId  id;
    int         pos_x = 0;
    int         pos_y = 0;
    std::string type;
    SgObjectId  map_gfx_id;
    std::string image;
    std::string name;
};

struct SgRoad {
    SgObjectId id;
    int        index = 0;
    int        variant = 0;
    int        pos_x = 0;
    int        pos_y = 0;
};

struct SgCrystal {
    SgObjectId  id;
    int         pos_x = 0;
    int         pos_y = 0;
    int         resource = 0;
    std::string type;
    SgObjectId  owner;
    int         ai_priority = 0;
};

struct SgSite {
    SgObjectId  id;
    std::string kind;
    std::string title;
    std::string description;
    int         image_iso = 0;
    int         image_interface = 0;
    int         pos_x = 0;
    int         pos_y = 0;
    std::string visitor;
    int         ai_priority = 0;

    // Merchant-specific
    std::string             buy_armor;
    std::string             buy_jewel;
    std::string             buy_weapon;
    std::string             buy_banner;
    std::string             buy_potion;
    std::string             buy_scroll;
    std::string             buy_wand;
    int                     buy_value = 0;
    int                     qty_item = 0;
    std::vector<SgObjectId> items;
    std::vector<SgObjectId> missions;

    // Mercs-specific
    int                     qty_unit = 0;
    std::vector<SgObjectId> units;

    // Trainer-specific
    // (all fields shared with common set)

    // Mage-specific
    int                     qty_spell = 0;
    std::vector<SgObjectId> spells;
};

struct SgMapBlock {
    SgObjectId            id;
    int                   grid_x = -1;
    int                   grid_y = -1;
    std::vector<uint32_t> values;
    std::vector<uint8_t>  raw_data;
};

struct SgTerrainGrid {
    int                                width = 0;
    int                                height = 0;
    std::vector<std::vector<uint32_t>> tiles;

    uint32_t tile_at(int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height)
            return 0;
        return tiles[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
    }
};

struct SgMap {
    SgObjectId              id;
    SgTerrainGrid           terrain;
    std::vector<SgMapBlock> blocks;
};

struct SgStackTemplateUnit {
    SgObjectId unit_id;
    int        level = 0;
    int        position = 0;
};

struct SgStackTemplate {
    SgObjectId                       id;
    SgObjectId                       owner;
    SgObjectId                       leader;
    int                              leader_level = 0;
    std::string                      name_txt;
    std::string                      resolved_name;
    SgObjectId                       subrace;
    int                              order = 0;
    SgObjectId                       order_target;
    bool                             use_facing = false;
    int                              facing = 0;
    int                              ai_priority = 0;
    int                              pos_x = 0;
    int                              pos_y = 0;
    SgObjectId                       modifier_id;
    int                              unit_pos = 0;
    std::vector<SgStackTemplateUnit> units;
};

struct SgMountainEntry {
    int id_mount = 0;
    int pos_x = 0;
    int pos_y = 0;
    int size_x = 0;
    int size_y = 0;
    int image = 0;
    int race = 0;
};

struct SgMidMountains {
    SgObjectId                   id;
    std::vector<SgMountainEntry> entries;
};

struct SgPlanEntry {
    SgObjectId element;
    int        pos_x = 0;
    int        pos_y = 0;
};

struct SgMidgardPlan {
    SgObjectId               id;
    std::vector<SgPlanEntry> entries;
};

struct SgNameValuePair {
    std::string name;
    std::string value;
    std::string value2;
};

struct SgMidScenVariables {
    SgObjectId                   id;
    std::vector<SgNameValuePair> variables;
};

struct SgDiplomacyRelation {
    SgObjectId race1;
    SgObjectId race2;
    int        relation = 0;
};

struct SgMidDiplomacy {
    SgObjectId                       id;
    std::vector<SgDiplomacyRelation> relations;
};

struct SgTalismanCharge {
    SgObjectId item_id;
    int        charges = 0;
};

struct SgMidTalismanCharges {
    SgObjectId                    id;
    std::vector<SgTalismanCharge> charges;
};

struct SgFogRow {
    int                  pos_y = 0;
    std::vector<uint8_t> raw_bytes;
};

struct SgMapFog {
    SgObjectId            id;
    std::string           player_id;
    std::vector<uint8_t>  fog_data;
    std::vector<uint8_t>  fog_bits;
    int                   map_width_tiles = 0;
    int                   map_height_tiles = 0;
    int                   bytes_per_row = 0;
    std::vector<SgFogRow> rows;
    std::string           encoding_hypothesis;
};

struct SgPlayerKnownSpells {
    SgObjectId              id;
    std::string             player_id;
    std::vector<SgObjectId> spell_ids;
};

struct SgPlayerBuildings {
    SgObjectId           id;
    std::string          player_id;
    std::vector<uint8_t> build_data;
};

struct SgTurnSummary {
    SgObjectId id;
    int        turn = 0;
};

// ── Semantic-only scenario domain data ────────────────────────────────────
//
// Contains only the parsed scenario data meaningful at runtime.
// No raw bytes, parse warnings, or object classification maps.
// No empty hull types that lack semantic content.
//
struct ScenarioTemplate {
    SgScenarioInfo               info;
    std::vector<SgPlayer>        players;
    std::vector<SgSubRace>       subraces;
    std::vector<SgUnit>          units;
    std::vector<SgStack>         stacks;
    std::vector<SgCityOrVillage> cities;
    std::vector<SgSite>          sites;
    std::vector<SgRuin>          ruins;
    std::vector<SgBag>           bags;
    std::vector<SgLocation>      locations;
    std::vector<SgEvent>         events;
    std::vector<SgItem>          items;
    std::vector<SgLandmark>      landmarks;
    std::vector<SgRoad>          roads;
    std::vector<SgCrystal>       crystals;

    SgMap map;

    std::vector<SgStackTemplate>      stack_templates;
    std::vector<SgMidScenVariables>   scen_variables;
    std::vector<SgMidDiplomacy>       diplomacy;
    std::vector<SgMidTalismanCharges> talisman_charges;
    std::vector<SgMidgardPlan>        plans;
    std::vector<SgMidMountains>       mountains;
    std::vector<SgTurnSummary>        turn_summaries;
    std::vector<SgPlayerKnownSpells>  known_spells;
    std::vector<SgPlayerBuildings>    buildings;
    std::vector<SgMapFog>             map_fogs;
};

} // namespace d2scenario
