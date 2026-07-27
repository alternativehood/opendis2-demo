#include "commands_sg_inspect.hpp"

#include <d2analysis/ScenarioGlobalIdReport.hpp>
#include <d2gamedata/GlobalIdResolver.hpp>
#include <d2gamedata/RefTypes.hpp>
#include <d2gamedata/SourceKind.hpp>
#include <d2scenario/SgParser.hpp>
#include <d2scenario/SgTypes.hpp>
#include <d2log/log.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// ── TODO: Decompose sg-inspect dumpers ───────────────────────────────────
// This file is too large. The following should be extracted into libd2analysis:
//   1. SgJsonDumper       — full SG → JSON serialization
//   2. SgCsvWriter        — terrain + objects CSV writing
//   3. GlobalIdReportJsonWriter — global_id_resolutions JSON output
//
// The CLI should only wire up subcommands and call these extracted components.
// See docs/architecture/cli-responsibilities.md.
// ──────────────────────────────────────────────────────────────────────────

namespace fs = std::filesystem;

namespace {

template <typename T>
void print_category(std::ostream& os, const std::string& label, const std::vector<T>& items) {
    os << "  " << label << ": " << items.size() << "\n";
}

void write_terrain_csv(const std::string& path, const std::vector<std::vector<uint32_t>>& tiles) {
    std::ofstream f(path);
    if (!f) {
        std::cerr << "ERROR: Cannot write terrain CSV to " << path << "\n";
        return;
    }
    for (const auto& row : tiles) {
        for (std::size_t j = 0; j < row.size(); ++j) {
            if (j > 0)
                f << ",";
            f << row[j];
        }
        f << "\n";
    }
}

void write_terrain_debug_csv(const std::string&                        path,
                             const std::vector<std::vector<uint32_t>>& tiles) {
    std::ofstream f(path);
    if (!f) {
        std::cerr << "ERROR: Cannot write terrain debug CSV to " << path << "\n";
        return;
    }
    f << "x,y,raw_u32,raw_hex,low_tile_id,high24\n";
    for (std::size_t y = 0; y < tiles.size(); ++y) {
        for (std::size_t x = 0; x < tiles[y].size(); ++x) {
            uint32_t val = tiles[y][x];
            f << x << "," << y << "," << val << ",0x" << std::hex << val << std::dec << ","
              << (val & 0xFF) << "," << (val >> 8) << "\n";
        }
    }
}

void dump_json(const std::string& path, const d2scenario::SgParseResult& scenario,
               const d2gamedata::GlobalIdResolver* resolver) {
    nlohmann::json j;
    const auto&    sg = scenario.scenario;

    j["file_path"] = scenario.file_path;
    j["file_size"] = scenario.file_size;
    j["parse_warnings"] = scenario.parse_warnings;

    auto& info = j["scenario_info"];
    info["id"] = sg.info.id;
    info["name"] = sg.info.name;
    info["creator"] = sg.info.creator;
    info["briefing"] = sg.info.briefing;
    info["description"] = sg.info.description;
    info["map_size"] = sg.info.map_size;
    info["map_seed"] = sg.info.map_seed;
    info["current_turn"] = sg.info.current_turn;
    info["campaign"] = sg.info.campaign;
    info["brief_long"] = sg.info.brief_long;
    info["debunk_loss"] = sg.info.debunk_loss;
    info["debunk_win"] = sg.info.debunk_win;

    for (const auto& pl : sg.players) {
        auto& pj = j["players"].emplace_back();
        pj["id"] = pl.id;
        pj["name"] = pl.name;
        pj["description"] = pl.description;
        pj["lord_id"] = pl.lord_id;
        pj["race_id"] = pl.race_id;
        pj["fog_id"] = pl.fog_id;
        pj["known_id"] = pl.known_id;
        pj["builds_id"] = pl.builds_id;
        pj["face"] = pl.face;
        pj["is_human"] = pl.is_human;
        pj["bank"] = pl.bank;
        pj["spell_bank"] = pl.spell_bank;
        pj["attitude"] = pl.attitude;
        pj["always_ai"] = pl.always_ai;
    }

    for (const auto& sr : sg.subraces) {
        auto& sj = j["subraces"].emplace_back();
        sj["id"] = sr.id;
        sj["subrace"] = sr.subrace;
        sj["player_id"] = sr.player_id;
        sj["number"] = sr.number;
        sj["name_txt"] = sr.name_txt;
        sj["banner"] = sr.banner;
    }

    for (const auto& u : sg.units) {
        auto& uj = j["units"].emplace_back();
        uj["id"] = u.id;
        uj["type"] = u.type_id;
        uj["level"] = u.level_raw_i32;
        uj["hp"] = u.hp;
        uj["xp"] = u.xp;
        uj["creation"] = u.creation;
        uj["name"] = u.name;
    }

    for (const auto& s : sg.stacks) {
        auto& sj = j["stacks"].emplace_back();
        sj["id"] = s.id;
        sj["owner"] = s.owner;
        sj["pos_x"] = s.pos_x;
        sj["pos_y"] = s.pos_y;
        sj["leader_id"] = s.leader_id;
        sj["leader_alive"] = s.leader_alive;
        sj["move"] = s.move;
        sj["morale"] = s.morale;
        sj["inside"] = s.inside;
        sj["subrace"] = s.subrace;
        sj["invisible"] = s.invisible;
        sj["order"] = s.order;
        sj["units"] = s.units;
        sj["positions"] = s.positions;
    }

    for (const auto& c : sg.cities) {
        auto& cj = j["cities"].emplace_back();
        cj["id"] = c.id;
        cj["kind"] = c.kind;
        cj["name"] = c.name;
        cj["description"] = c.description;
        cj["owner"] = c.owner;
        cj["subrace"] = c.subrace;
        cj["stack"] = c.stack;
        cj["pos_x"] = c.pos_x;
        cj["pos_y"] = c.pos_y;
        cj["group_id"] = c.group_id;
        cj["ai_priority"] = c.ai_priority;
        cj["size"] = c.size;
        cj["unit_ids"] = c.unit_ids;
        cj["positions"] = c.positions;
        cj["item_ids"] = c.item_ids;
    }

    for (const auto& s : sg.sites) {
        auto& sj = j["sites"].emplace_back();
        sj["id"] = s.id;
        sj["kind"] = s.kind;
        sj["title"] = s.title;
        sj["description"] = s.description;
        sj["img_iso"] = s.image_iso;
        sj["img_intf"] = s.image_interface;
        sj["pos_x"] = s.pos_x;
        sj["pos_y"] = s.pos_y;
        sj["visitor"] = s.visitor;
        sj["ai_priority"] = s.ai_priority;
        if (!s.items.empty())
            sj["items"] = s.items;
        if (!s.missions.empty())
            sj["missions"] = s.missions;
        if (!s.units.empty())
            sj["units"] = s.units;
    }

    for (const auto& r : sg.ruins) {
        auto& rj = j["ruins"].emplace_back();
        rj["id"] = r.id;
        rj["title"] = r.title;
        rj["description"] = r.description;
        rj["image"] = r.image;
        rj["pos_x"] = r.pos_x;
        rj["pos_y"] = r.pos_y;
        rj["cash"] = r.cash;
        rj["item"] = r.item;
        rj["looter"] = r.looter;
        rj["ai_priority"] = r.ai_priority;
        rj["unit_ids"] = r.unit_ids;
        rj["positions"] = r.positions;
    }

    for (const auto& b : sg.bags) {
        auto& bj = j["bags"].emplace_back();
        bj["id"] = b.id;
        bj["pos_x"] = b.pos_x;
        bj["pos_y"] = b.pos_y;
        bj["image"] = b.image;
        bj["cash"] = b.cash;
        bj["items"] = b.items;
        bj["looter"] = b.looter;
        bj["ai_priority"] = b.ai_priority;
    }

    for (const auto& l : sg.locations) {
        auto& lj = j["locations"].emplace_back();
        lj["id"] = l.id;
        lj["name"] = l.name;
        lj["pos_x"] = l.pos_x;
        lj["pos_y"] = l.pos_y;
        lj["radius"] = l.radius;
    }

    for (const auto& e : sg.events) {
        auto& ej = j["events"].emplace_back();
        ej["id"] = e.id;
        ej["name"] = e.name;
        ej["enabled"] = e.enabled;
        ej["occur_once"] = e.occur_once;
        ej["chance"] = e.chance;
        ej["order"] = e.order;
        ej["cond_qty"] = e.cond_qty;
        ej["effect_qty"] = e.effect_qty;
        ej["frequency"] = e.frequency;
        ej["locations"] = e.locations;
        ej["players"] = e.players;
        ej["popup_texts"] = e.popup_texts;
        ej["category_values"] = e.category_values;
        ej["num_values"] = e.num_values;
    }

    for (const auto& it : sg.items) {
        auto& ij = j["items"].emplace_back();
        ij["id"] = it.id;
        ij["type"] = it.type;
    }

    for (const auto& lm : sg.landmarks) {
        auto& lmj = j["landmarks"].emplace_back();
        lmj["id"] = lm.id;
        lmj["pos_x"] = lm.pos_x;
        lmj["pos_y"] = lm.pos_y;
        lmj["name"] = lm.name;
        lmj["type"] = lm.type;
        lmj["map_gfx_id"] = lm.map_gfx_id;
        lmj["image"] = lm.image;
    }

    for (const auto& rd : sg.roads) {
        auto& rdj = j["roads"].emplace_back();
        rdj["id"] = rd.id;
        rdj["index"] = rd.index;
        rdj["variant"] = rd.variant;
        rdj["pos_x"] = rd.pos_x;
        rdj["pos_y"] = rd.pos_y;
    }

    for (const auto& cr : sg.crystals) {
        auto& crj = j["crystals"].emplace_back();
        crj["id"] = cr.id;
        crj["pos_x"] = cr.pos_x;
        crj["pos_y"] = cr.pos_y;
        crj["resource"] = cr.resource;
        crj["type"] = cr.type;
        crj["owner"] = cr.owner;
    }

    auto& tj = j["terrain"];
    tj["block_count"] = sg.map.blocks.size();
    tj["grid_width"] = sg.map.terrain.width;
    tj["grid_height"] = sg.map.terrain.height;

    for (const auto& st : sg.stack_templates) {
        auto& sj = j["stack_templates"].emplace_back();
        sj["id"] = st.id;
        sj["owner"] = st.owner;
        sj["leader"] = st.leader;
        sj["leader_level"] = st.leader_level;
        sj["name_txt"] = st.name_txt;
        sj["subrace"] = st.subrace;
        sj["order"] = st.order;
        sj["order_target"] = st.order_target;
        sj["use_facing"] = st.use_facing;
        sj["facing"] = st.facing;
        sj["ai_priority"] = st.ai_priority;
        sj["pos_x"] = st.pos_x;
        sj["pos_y"] = st.pos_y;
        sj["modifier_id"] = st.modifier_id;
        sj["unit_pos"] = st.unit_pos;
        for (const auto& ut : st.units) {
            auto& uj = sj["units"].emplace_back();
            uj["unit_id"] = ut.unit_id;
            uj["level"] = ut.level;
            uj["position"] = ut.position;
        }
    }

    for (const auto& sv : sg.scen_variables) {
        auto& svj = j["scen_variables"].emplace_back();
        svj["id"] = sv.id;
        for (const auto& v : sv.variables) {
            auto& vj = svj["variables"].emplace_back();
            vj["name"] = v.name;
            vj["value"] = v.value;
            if (!v.value2.empty())
                vj["value2"] = v.value2;
        }
    }

    for (const auto& d : sg.diplomacy) {
        auto& dj = j["diplomacy"].emplace_back();
        dj["id"] = d.id;
        for (const auto& rel : d.relations) {
            auto& rj = dj["relations"].emplace_back();
            rj["race_1"] = rel.race1;
            rj["race_2"] = rel.race2;
            rj["relation"] = rel.relation;
        }
    }

    for (const auto& tc : sg.talisman_charges) {
        auto& tcj = j["talisman_charges"].emplace_back();
        tcj["id"] = tc.id;
        for (const auto& ch : tc.charges) {
            auto& chj = tcj["charges"].emplace_back();
            chj["item_id"] = ch.item_id;
            chj["charges"] = ch.charges;
        }
    }

    for (const auto& plan : sg.plans) {
        auto& pj = j["plans"].emplace_back();
        pj["id"] = plan.id;
        for (const auto& entry : plan.entries) {
            auto& ej = pj["entries"].emplace_back();
            ej["element"] = entry.element;
            ej["pos_x"] = entry.pos_x;
            ej["pos_y"] = entry.pos_y;
        }
    }

    for (const auto& mt : sg.mountains) {
        auto& mj = j["mountains"].emplace_back();
        mj["id"] = mt.id;
        for (const auto& entry : mt.entries) {
            auto& ej = mj["entries"].emplace_back();
            ej["id_mount"] = entry.id_mount;
            ej["pos_x"] = entry.pos_x;
            ej["pos_y"] = entry.pos_y;
            ej["size_x"] = entry.size_x;
            ej["size_y"] = entry.size_y;
            ej["image"] = entry.image;
            ej["race"] = entry.race;
        }
    }

    // Diagnostic empty hulls (verified-empty initial state objects).
    // These are NOT semantic scenario data — they exist only for debugging.
    // Full evidence is available under classification.verified_empty_initial_state_classes
    // and the inline verified_empty_objects array.
    if (!scenario.spell_casts.empty() || !scenario.spell_effects.empty() ||
        !scenario.stacks_destroyed.empty() || !scenario.quest_logs.empty()) {
        auto& hulls = j["parse_diagnostics"]["empty_hulls"];
        for (const auto& sc : scenario.spell_casts)
            hulls["MidSpellCast"].push_back(sc.id);
        for (const auto& se : scenario.spell_effects)
            hulls["MidSpellEffects"].push_back(se.id);
        for (const auto& sd : scenario.stacks_destroyed)
            hulls["MidStackDestroyed"].push_back(sd.id);
        for (const auto& ql : scenario.quest_logs)
            hulls["MidQuestLog"].push_back(ql.id);
    }

    for (const auto& ts : sg.turn_summaries) {
        auto& tsj = j["turn_summaries"].emplace_back();
        tsj["id"] = ts.id;
        tsj["turn"] = ts.turn;
    }

    for (const auto& ks : sg.known_spells) {
        auto& ksj = j["known_spells"].emplace_back();
        ksj["id"] = ks.id;
        ksj["player_id"] = ks.player_id;
        ksj["spell_ids"] = ks.spell_ids;
    }

    for (const auto& pb : sg.buildings) {
        auto& pbj = j["buildings"].emplace_back();
        pbj["id"] = pb.id;
        pbj["player_id"] = pb.player_id;
        pbj["build_data_size"] = pb.build_data.size();
    }

    for (const auto& fog : sg.map_fogs) {
        auto& fj = j["map_fogs"].emplace_back();
        fj["id"] = fog.id;
        fj["player_id"] = fog.player_id;
        fj["fog_data_size"] = fog.fog_data.size();
        fj["bytes_per_row"] = fog.bytes_per_row;
        fj["map_width_tiles"] = fog.map_width_tiles;
        fj["map_height_tiles"] = fog.map_height_tiles;
        fj["encoding_hypothesis"] = fog.encoding_hypothesis;
        fj["row_count"] = fog.rows.size();
    }

    // Global ID Usages
    for (const auto& u : scenario.global_id_usages) {
        auto& uj = j["global_id_usages"].emplace_back();
        uj["value"] = u.value.value;
        uj["object_id"] = u.object_id.value;
        uj["class_name"] = u.class_name;
        uj["field_name"] = u.field_name;
        if (u.pos_x >= 0)
            uj["pos_x"] = u.pos_x;
        if (u.pos_y >= 0)
            uj["pos_y"] = u.pos_y;

        if (resolver) {
            auto resolved = resolver->resolve_raw(u.value.value);
            if (resolved.status == d2gamedata::ResolutionStatus::Resolved ||
                resolved.status == d2gamedata::ResolutionStatus::NullRef) {
                auto& rj = uj["resolved"] = nlohmann::json::object();
                rj["resolved"] = (resolved.status == d2gamedata::ResolutionStatus::Resolved);
                rj["category"] = resolved.category_kind;
                rj["source_kind"] = d2gamedata::to_string(resolved.source_kind);
                if (resolved.primary_match.has_value()) {
                    rj["source_table"] = resolved.primary_match->table_name;
                    rj["row_index"] = resolved.primary_match->row_index;
                    rj["source_field"] = resolved.primary_match->field_name;
                }
                if (resolved.name.has_value()) {
                    rj["name_text_id"] = resolved.name->id.value;
                    rj["name_resolved"] = resolved.name->resolved;
                    rj["name_value"] = resolved.name->resolved ? resolved.name->value : "";
                    if (!resolved.name->resolved && !resolved.name->reason_unresolved.empty())
                        rj["name_unresolved_reason"] = resolved.name->reason_unresolved;
                }
                if (resolved.description.has_value()) {
                    rj["description_text_id"] = resolved.description->id.value;
                    rj["description_resolved"] = resolved.description->resolved;
                    rj["description_value"] =
                        resolved.description->resolved ? resolved.description->value : "";
                    if (!resolved.description->resolved &&
                        !resolved.description->reason_unresolved.empty()) {
                        rj["description_unresolved_reason"] =
                            resolved.description->reason_unresolved;
                    }
                }
                rj["confidence"] = (resolved.status == d2gamedata::ResolutionStatus::Resolved)
                                       ? "high"
                                       : "certain";
            } else {
                auto& rj = uj["resolved"] = nlohmann::json::object();
                rj["resolved"] = false;
                rj["reason"] = resolved.unresolved_reason;
            }
        }
    }

    // Classification summary
    j["classification"] = nlohmann::json::object();
    j["classification"]["parsed_classes"] = nlohmann::json::object();
    for (const auto& [cls, entries] : scenario.parsed_objects)
        j["classification"]["parsed_classes"][cls] = entries.size();
    j["classification"]["verified_empty_initial_state_classes"] = nlohmann::json::object();
    for (const auto& [cls, entries] : scenario.verified_empty_objects)
        j["classification"]["verified_empty_initial_state_classes"][cls] = entries.size();
    j["classification"]["unknown_classes"] = nlohmann::json::object();
    for (const auto& [cls, entries] : scenario.unknown_objects)
        j["classification"]["unknown_classes"][cls] = entries.size();

    if (!scenario.verified_empty_objects.empty()) {
        auto& ve_arr = j["verified_empty_objects"] = nlohmann::json::array();
        for (const auto& [cls, entries] : scenario.verified_empty_objects) {
            for (const auto& entry : entries) {
                auto& vej = ve_arr.emplace_back();
                vej["id"] = entry.obj_id;
                vej["class_name"] = entry.class_name;
                vej["offset"] = entry.offset;
                vej["length"] = entry.length;
                vej["reason"] = "Verified empty initial state: " + cls +
                                " object has no semantic payload (no player_id/data fields)";
            }
        }
    }

    for (const auto& entry : scenario.object_index) {
        auto& oij = j["object_index"].emplace_back();
        oij["offset"] = entry.offset;
        oij["length"] = entry.length;
        oij["class"] = entry.class_name;
        oij["obj_id"] = entry.obj_id;
        switch (entry.classification) {
        case d2scenario::SgObjectClassification::Parsed:
            oij["classification"] = "parsed";
            break;
        case d2scenario::SgObjectClassification::VerifiedEmptyInitialState:
            oij["classification"] = "verified_empty_initial_state";
            break;
        case d2scenario::SgObjectClassification::Unknown:
            oij["classification"] = "unknown";
            break;
        }
    }

    for (const auto& raw : scenario.raw_objects) {
        auto& roj = j["raw_objects"].emplace_back();
        roj["offset"] = raw.offset;
        roj["length"] = raw.length;
        roj["class"] = raw.class_name;
        roj["obj_id"] = raw.obj_id;
        roj["raw_size"] = raw.raw_bytes.size();
    }

    // Resolver summary — moved to report layer
    if (resolver) {
        d2analysis::ScenarioGlobalIdReport report(*resolver, scenario);
        auto                               sum = report.summarize();
        auto&                              rsj = j["global_id_summary"] = nlohmann::json::object();
        rsj["total_usages"] = sum.total_usages;
        rsj["unique_ids"] = sum.unique_ids;
        rsj["resolved_usages"] = sum.resolved_usages;
        rsj["unresolved_usages"] = sum.unresolved_usages;
        rsj["null_ref_usages"] = sum.null_ref_usages;
        rsj["resolved_unique_ids"] = sum.resolved_unique_ids;
        rsj["unresolved_unique_ids"] = sum.unresolved_unique_ids;
        rsj["null_ref_unique_ids"] = sum.null_ref_unique_ids;

        rsj["dbf_files_loaded"] = sum.dbf_files_loaded;
        rsj["dbf_files_failed"] = sum.dbf_files_failed;
        rsj["dbf_rows_scanned"] = sum.dbf_rows_scanned;
        rsj["dbf_field_values_scanned"] = sum.dbf_field_values_scanned;
        rsj["indexed_global_values"] = sum.indexed_global_values;
        rsj["unique_global_ids"] = sum.unique_global_ids;
        rsj["tables_with_global_ids"] = sum.tables_with_global_ids;
        rsj["text_rows_loaded"] = sum.text_rows_loaded;
        rsj["asset_fallback_configured"] = sum.asset_fallback_configured;
        rsj["asset_manifests_scanned"] = sum.asset_manifests_scanned;

        auto& by_prefix = rsj["by_prefix"] = nlohmann::json::object();
        for (const auto& [p, c] : sum.by_prefix)
            by_prefix[p] = c;

        auto& by_class = rsj["by_class"] = nlohmann::json::object();
        for (const auto& [c, cnt] : sum.by_class)
            by_class[c] = cnt;

        auto& by_field = rsj["by_field"] = nlohmann::json::object();
        for (const auto& [f, cnt] : sum.by_field)
            by_field[f] = cnt;

        auto& by_source_kind = rsj["by_source_kind"] = nlohmann::json::object();
        for (const auto& [sk, cnt] : sum.by_source_kind)
            by_source_kind[sk] = cnt;

        auto& by_domain_category = rsj["by_domain_category"] = nlohmann::json::object();
        for (const auto& [dc, cnt] : sum.by_domain_category)
            by_domain_category[dc] = cnt;

        // Global ID resolutions: detailed per-unique-ID map
        auto  resolutions = report.build_resolution_map();
        auto& resolutions_obj = j["global_id_resolutions"] = nlohmann::json::object();
        for (const auto& [raw_id, entry] : resolutions) {
            auto& rj = resolutions_obj[raw_id] = nlohmann::json::object();
            rj["raw_id"] = raw_id;
            rj["resolved"] = entry.resolved;
            rj["source_kind"] = entry.source_kind_str;
            if (entry.resolved) {
                rj["source_table"] = entry.source_table;
                rj["source_file_path"] = entry.source_file_path;
                rj["row_index"] = entry.row_index;
                rj["source_field"] = entry.source_field;
                rj["category"] = entry.category;
                rj["confidence"] = entry.confidence;

                if (!entry.all_matches.empty()) {
                    auto& am_arr = rj["all_matches"] = nlohmann::json::array();
                    for (const auto& match : entry.all_matches) {
                        auto& mj = am_arr.emplace_back();
                        mj["source_table"] = match.table_name;
                        mj["source_file_path"] = match.file_path.string();
                        mj["row_index"] = match.row_index;
                        mj["source_field"] = match.field_name;
                        auto& rf = mj["row_fields"] = nlohmann::json::object();
                        for (const auto& [k, v] : match.row_fields)
                            rf[k] = v;
                    }
                }
            } else {
                rj["reason"] = entry.reason;
            }

            // Honest name text fields (present for all entries, resolved or not).
            if (!entry.name_text_id.empty()) {
                rj["name_text_id"] = entry.name_text_id;
                rj["name_resolved"] = entry.name_resolved;
                rj["name_value"] = entry.name_resolved ? entry.name_value : "";
                if (!entry.name_resolved && !entry.name_unresolved_reason.empty())
                    rj["name_unresolved_reason"] = entry.name_unresolved_reason;
            }

            if (!entry.description_text_id.empty()) {
                rj["description_text_id"] = entry.description_text_id;
                rj["description_resolved"] = entry.description_resolved;
                rj["description_value"] = entry.description_resolved ? entry.description_value : "";
                if (!entry.description_resolved && !entry.description_unresolved_reason.empty())
                    rj["description_unresolved_reason"] = entry.description_unresolved_reason;
            }
        }

        // Unresolved IDs detail (with usage counts and examples)
        auto  ureport = report.build_unresolved_report();
        auto& unresolved_arr = j["unresolved_global_ids"] = nlohmann::json::array();
        if (ureport.unresolved_usages > 0) {
            for (const auto& ur : ureport.unresolved) {
                auto& uj = unresolved_arr.emplace_back();
                uj["id"] = ur.id.value;
                uj["reason"] = ur.reason;
                uj["usage_count"] = ur.usage_count;
                auto& examples = uj["usage_examples"] = nlohmann::json::array();
                for (const auto& ex : ur.usage_examples) {
                    auto& ej = examples.emplace_back();
                    ej["object_id"] = ex.object_id;
                    ej["class_name"] = ex.class_name;
                    ej["field_name"] = ex.field_name;
                }
            }
        }
    }

    std::ofstream f(path);
    if (!f) {
        std::cerr << "Error: Cannot write JSON to " << path << "\n";
        return;
    }
    f << j.dump(2);
}

void dump_objects_csv(const std::string& path, const d2scenario::SgParseResult& scenario) {
    std::ofstream f(path);
    if (!f) {
        std::cerr << "Error: Cannot write objects CSV to " << path << "\n";
        return;
    }
    f << "offset,length,class,obj_id,classification\n";
    for (const auto& entry : scenario.object_index) {
        std::string cls_label;
        switch (entry.classification) {
        case d2scenario::SgObjectClassification::Parsed:
            cls_label = "parsed";
            break;
        case d2scenario::SgObjectClassification::VerifiedEmptyInitialState:
            cls_label = "verified_empty_initial_state";
            break;
        case d2scenario::SgObjectClassification::Unknown:
            cls_label = "unknown";
            break;
        }
        f << entry.offset << "," << entry.length << "," << entry.class_name << "," << entry.obj_id
          << "," << cls_label << "\n";
    }
}

void print_summary(std::ostream& os, const d2scenario::SgParseResult& scenario) {
    const auto& sg = scenario.scenario;
    os << "=== .SG Scenario Inspector Summary ===\n";
    os << "File: " << scenario.file_path << "\n";
    os << "Size: " << scenario.file_size << " bytes\n";
    os << "\n";

    const auto& info = sg.info;
    os << "Scenario Name: " << info.name << "\n";
    os << "Creator: " << info.creator << "\n";
    os << "Map Size: " << info.map_size << "x" << info.map_size << "\n";
    os << "Map Seed: " << info.map_seed << "\n";
    os << "Campaign: " << info.campaign << "\n";
    os << "Current Turn: " << info.current_turn << "\n";
    os << "\n";

    os << "=== Object Counts ===\n";
    os << "Total parsed objects: " << scenario.object_index.size() << "\n";
    os << "  Parsed: " << scenario.parsed_objects.size() << " classes\n";
    os << "  Verified empty-initial-state: " << scenario.verified_empty_objects.size()
       << " classes\n";
    os << "  Unknown: " << scenario.unknown_objects.size() << " classes\n";
    print_category(os, "Players", sg.players);
    print_category(os, "SubRaces", sg.subraces);
    print_category(os, "Units", sg.units);
    print_category(os, "Stacks", sg.stacks);
    print_category(os, "Cities/Villages", sg.cities);
    print_category(os, "Sites", sg.sites);
    print_category(os, "Ruins", sg.ruins);
    print_category(os, "Bags", sg.bags);
    print_category(os, "Locations", sg.locations);
    print_category(os, "Events", sg.events);
    print_category(os, "Items", sg.items);
    print_category(os, "Landmarks", sg.landmarks);
    print_category(os, "Roads", sg.roads);
    print_category(os, "Crystals", sg.crystals);
    print_category(os, "Map Blocks", sg.map.blocks);

    os << "\n";
    os << "=== Semantic Object Counts ===\n";
    print_category(os, "Stack Templates", sg.stack_templates);
    print_category(os, "Scenario Variables", sg.scen_variables);
    print_category(os, "Diplomacy Records", sg.diplomacy);
    print_category(os, "Talisman Charges", sg.talisman_charges);
    print_category(os, "Plans", sg.plans);
    print_category(os, "Mountains", sg.mountains);
    print_category(os, "Turn Summaries", sg.turn_summaries);
    print_category(os, "Known Spells", sg.known_spells);
    print_category(os, "Buildings", sg.buildings);
    print_category(os, "Map Fogs", sg.map_fogs);

    os << "\n";
    os << "=== Verified Empty Diagnostic Hulls ===\n";
    os << "  (Verified-empty initial-state objects — no semantic payload)\n";
    print_category(os, "Spell Casts", scenario.spell_casts);
    print_category(os, "Spell Effects", scenario.spell_effects);
    print_category(os, "Stacks Destroyed", scenario.stacks_destroyed);
    print_category(os, "Quest Logs", scenario.quest_logs);

    os << "\n";

    os << "=== Object Counts by Class ===\n";
    std::map<std::string, int> class_counts;
    for (const auto& entry : scenario.object_index) {
        class_counts[entry.class_name]++;
    }
    for (const auto& [cls, count] : class_counts) {
        os << "  " << cls << ": " << count << "\n";
    }
    os << "\n";

    os << "=== Players ===\n";
    for (const auto& pl : sg.players) {
        os << "  " << pl.id << " " << pl.name << " race=" << pl.race_id << " lord=" << pl.lord_id
           << " bank=" << pl.bank << " human=" << (pl.is_human ? "1" : "0") << "\n";
    }
    os << "\n";

    os << "=== Map Fogs ===\n";
    for (const auto& fog : sg.map_fogs) {
        os << "  " << fog.id << " rows=" << fog.rows.size()
           << " bytes_per_row=" << fog.bytes_per_row << " map_width=" << fog.map_width_tiles
           << " map_height=" << fog.map_height_tiles << " data_size=" << fog.fog_data.size()
           << "\n";
    }
    os << "\n";

    if (!scenario.unknown_objects.empty()) {
        os << "=== Unknown Object Classes ===\n";
        for (const auto& [cls, entries] : scenario.unknown_objects) {
            os << "  " << cls << ": " << entries.size() << " objects\n";
        }
        os << "\n";
    }

    if (!scenario.parse_warnings.empty()) {
        os << "=== Parse Warnings ===\n";
        for (const auto& w : scenario.parse_warnings) {
            os << "  [WARN] " << w << "\n";
        }
        os << "\n";
    }

    os << "=== Global ID Usage Summary ===\n";
    os << "  Total global IDs referenced: " << scenario.global_id_usages.size() << "\n";

    std::set<std::string> unique_global_ids;
    for (const auto& u : scenario.global_id_usages)
        unique_global_ids.insert(u.value.value);

    os << "  Unique IDs: " << unique_global_ids.size() << "\n";

    std::map<std::string, int> gid_by_prefix;
    for (const auto& u : scenario.global_id_usages) {
        std::string prefix = u.value.prefix();
        gid_by_prefix[prefix]++;
    }
    for (const auto& [prefix, cnt] : gid_by_prefix) {
        os << "    " << prefix << "*: " << cnt << " references\n";
    }
    os << "\n";
}

} // anonymous namespace

int cmd_sg_inspect(const std::string& file_path, bool summary, const std::string& dump_json_path,
                   const std::string& dump_terrain_csv_path,
                   const std::string& dump_objects_csv_path, const std::string& globals_dir,
                   bool report_global_ids, bool annotate_ids,
                   const std::string& dump_terrain_debug_csv_path) {
    fs::path path(file_path);
    if (!fs::exists(path)) {
        std::cerr << "Error: File not found: " << file_path << "\n";
        return 1;
    }

    std::size_t file_size = static_cast<std::size_t>(fs::file_size(path));

    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        std::cerr << "Error: Cannot open file: " << file_path << "\n";
        return 1;
    }

    std::vector<uint8_t> data(file_size);
    if (!ifs.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(file_size))) {
        std::cerr << "Error: Cannot read file: " << file_path << "\n";
        return 1;
    }

    try {
        d2scenario::SgParser parser(data);
        auto                 parse_result = parser.parse();
        parse_result.file_path = file_path;
        parse_result.file_size = file_size;
        const auto& sg = parse_result.scenario;

        d2gamedata::GlobalIdResolver resolver;
        bool                         has_resolver = false;
        if (!globals_dir.empty() && fs::is_directory(globals_dir)) {
            resolver.load_game_data(globals_dir);
            has_resolver = true;
            if (resolver.scanned_table_count() == 0) {
                std::cout << "\n=== WARNING ===\n";
                std::cout << "  No DBF tables found at: " << globals_dir << "\n";
                std::cout << "  Provide a game root (containing Globals/), a Globals directory,\n";
                std::cout << "  or a directory containing .dbf files directly.\n";
                std::cout << "  Global ID resolution will not be available.\n";
            }
        }

        if (summary) {
            print_summary(std::cout, parse_result);

            if (has_resolver) {
                d2analysis::ScenarioGlobalIdReport report(resolver, parse_result);
                auto                               sum = report.summarize();
                std::cout << "\n=== Global ID Resolver ===\n";
                std::cout << "  Scanned " << resolver.scanned_table_count() << " DBF files\n";
                std::cout << "  Resolution rate: " << sum.resolved_usages << "/"
                          << (sum.total_usages - sum.null_ref_usages) << " non-null usages\n";
            }

            if (report_global_ids && has_resolver) {
                d2analysis::ScenarioGlobalIdReport report(resolver, parse_result);
                auto                               ureport = report.build_unresolved_report();
                std::cout << ureport.to_string();
            }

            if (annotate_ids && has_resolver) {
                d2analysis::ScenarioGlobalIdReport report(resolver, parse_result);
                std::cout << "\n=== Annotated Global IDs ===\n";
                for (const auto& u : parse_result.global_id_usages) {
                    if (u.value.is_null())
                        continue;
                    auto resolved = resolver.resolve_raw(u.value.value);
                    std::cout << "  " << u.value.value;
                    if (resolved.status == d2gamedata::ResolutionStatus::Resolved) {
                        std::cout << "  -> " << resolved.primary_match->table_name << " row["
                                  << resolved.primary_match->row_index << "]";
                        if (resolved.name.has_value() && resolved.name->resolved)
                            std::cout << " \"" << resolved.name->value << "\"";
                    } else {
                        std::cout << "  -> UNRESOLVED";
                    }
                    std::cout << " | " << u.class_name << "." << u.field_name << " (obj "
                              << u.object_id.value << ")";
                    if (u.pos_x >= 0 || u.pos_y >= 0)
                        std::cout << " pos=(" << u.pos_x << "," << u.pos_y << ")";
                    std::cout << "\n";
                }
            }
        }

        if (!dump_json_path.empty()) {
            dump_json(dump_json_path, parse_result, has_resolver ? &resolver : nullptr);
            std::cout << "Wrote JSON dump to: " << dump_json_path << "\n";
        }

        if (!dump_terrain_csv_path.empty()) {
            write_terrain_csv(dump_terrain_csv_path, sg.map.terrain.tiles);
            std::cout << "Wrote terrain CSV to: " << dump_terrain_csv_path << "\n";
        }

        if (!dump_terrain_debug_csv_path.empty()) {
            write_terrain_debug_csv(dump_terrain_debug_csv_path, sg.map.terrain.tiles);
            std::cout << "Wrote terrain debug CSV to: " << dump_terrain_debug_csv_path << "\n";
        }

        if (!dump_objects_csv_path.empty()) {
            dump_objects_csv(dump_objects_csv_path, parse_result);
            std::cout << "Wrote objects CSV to: " << dump_objects_csv_path << "\n";
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Parse error: " << e.what() << "\n";
        return 1;
    }
}
