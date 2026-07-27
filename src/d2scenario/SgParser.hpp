#pragma once

#include "SgTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace d2scenario {

class SgParser {
public:
    explicit SgParser(std::span<const uint8_t> data);

    SgParseResult parse();

    static std::string short_class_name(const std::string& full_class);
    static std::string decode_cp1251(std::span<const uint8_t> bytes);

    bool        has_field_exact(const std::vector<uint8_t>& rec, const std::string& key) const;
    std::string read_string_field(const std::vector<uint8_t>& rec, const std::string& key) const;
    int         read_int_field(const std::vector<uint8_t>& rec, const std::string& key) const;
    bool        read_bool_field(const std::vector<uint8_t>& rec, const std::string& key) const;
    std::vector<uint8_t>     read_bytes_field(const std::vector<uint8_t>& rec,
                                              const std::string&          key) const;
    std::vector<std::string> read_all_string_fields(const std::vector<uint8_t>& rec,
                                                    const std::string&          key) const;
    std::vector<int>         read_all_int_fields(const std::vector<uint8_t>& rec,
                                                 const std::string&          key) const;

    static uint32_t read_le32(const std::vector<uint8_t>& data, std::size_t offset);

    SgObjectClassification   classify_object(const std::string& short_cls) const;
    std::span<const uint8_t> data_;
    std::vector<std::string> warnings_;

    void            add_warning(const std::string& msg);
    void            validate_signature();
    SgScenarioInfo  parse_scenario_info(const std::vector<uint8_t>& rec);
    SgPlayer        parse_player(const std::vector<uint8_t>& rec);
    SgSubRace       parse_subrace(const std::vector<uint8_t>& rec);
    SgStack         parse_stack(const std::vector<uint8_t>& rec);
    SgCityOrVillage parse_city(const std::vector<uint8_t>& rec, const std::string& kind);
    SgRuin          parse_ruin(const std::vector<uint8_t>& rec);
    SgBag           parse_bag(const std::vector<uint8_t>& rec);
    SgLocation      parse_location(const std::vector<uint8_t>& rec);
    SgEvent         parse_event(const std::vector<uint8_t>& rec);
    SgItem          parse_item(const std::vector<uint8_t>& rec);
    SgLandmark      parse_landmark(const std::vector<uint8_t>& rec);
    SgRoad          parse_road(const std::vector<uint8_t>& rec);
    SgCrystal       parse_crystal(const std::vector<uint8_t>& rec);
    SgSite          parse_site(const std::vector<uint8_t>& rec, const std::string& kind);
    SgMapBlock      parse_map_block(const std::vector<uint8_t>& rec, const std::string& oid_str);
    SgTerrainGrid   reconstruct_terrain(const std::vector<SgMapBlock>& blocks, int map_size);

    SgStackTemplate      parse_mid_stack_template(const std::vector<uint8_t>& rec);
    SgMidScenVariables   parse_mid_scen_variables(const std::vector<uint8_t>& rec);
    SgMidDiplomacy       parse_mid_diplomacy(const std::vector<uint8_t>& rec);
    SgMidTalismanCharges parse_mid_talisman_charges(const std::vector<uint8_t>& rec);
    SgMidgardPlan        parse_midgard_plan(const std::vector<uint8_t>& rec);
    SgMidMountains       parse_mid_mountains(const std::vector<uint8_t>& rec);
    SgMapFog             parse_midgard_map_fog(const std::vector<uint8_t>& rec);
    SgPlayerKnownSpells  parse_player_known_spells(const std::vector<uint8_t>& rec);
    SgPlayerBuildings    parse_player_buildings(const std::vector<uint8_t>& rec);
    SgTurnSummary        parse_turn_summary(const std::vector<uint8_t>& rec);
    bool has_semantic_content(const std::vector<uint8_t>&     rec,
                              const std::vector<std::string>& expected_fields) const;

    void collect_global_id_usages(const SgParseResult&          parse_result,
                                  std::vector<SgGlobalIdUsage>& usages) const;
};

} // namespace d2scenario
