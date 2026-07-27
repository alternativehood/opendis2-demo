#include "commands_research.hpp"

#include "d2res/anim_decoder.hpp"
#include "d2res/dbf_reader.hpp"
#include "d2res/mqdb.hpp"
#include "d2res/opt_maps.hpp"
#include "d2res/raw_inventory.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <set>
#include <sstream>

using json = nlohmann::json;

namespace {

std::string upper(std::string_view text) {
    std::string out{text};
    for (char& c : out) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return out;
}

std::string lower(std::string_view text) {
    std::string out{text};
    for (char& c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

std::string type_to_unit_id(std::string_view unit_type) {
    return "g000" + lower(unit_type);
}

std::string unit_id_to_type(std::string_view unit_id) {
    if (unit_id.size() <= 4u) {
        return {};
    }
    return upper(unit_id.substr(4));
}

std::string field(const std::map<std::string, std::string>& row, std::string_view key) {
    const auto it = row.find(std::string{key});
    return it == row.end() ? std::string{} : it->second;
}

std::vector<uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) {
        return {};
    }
    const auto size = in.tellg();
    if (size <= 0) {
        return {};
    }
    std::vector<uint8_t> data(static_cast<std::size_t>(size));
    in.seekg(0);
    in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return data;
}

std::map<std::string, std::string> load_base_units(const std::string& game_root) {
    std::map<std::string, std::string> result;
    if (game_root.empty()) {
        return result;
    }
    const auto data = read_file(std::filesystem::path{game_root} / "Globals" / "Gunits.dbf");
    if (data.empty()) {
        return result;
    }
    d2res::DbfReader const reader{data};
    for (const auto& row : reader.read_records()) {
        const auto        unit = row.find("UNIT_ID");
        const auto        base = row.find("BASE_UNIT");
        const std::string base_id = base == row.end() ? std::string{} : lower(base->second);
        if (unit != row.end() && !unit->second.empty() && !base_id.empty() && base_id != "000000" &&
            base_id != "g000000000") {
            result[lower(unit->second)] = base_id;
        }
    }
    return result;
}

std::map<std::string, std::map<std::string, std::string>>
load_table_by_id(const std::string& game_root, const std::string& filename,
                 const std::string& id_field, json& diagnostics) {
    std::map<std::string, std::map<std::string, std::string>> result;
    if (game_root.empty()) {
        diagnostics.push_back({{"level", "info"},
                               {"message", "game_root not provided; gameplay DBF linkage skipped"},
                               {"table", filename}});
        return result;
    }
    const auto data = read_file(std::filesystem::path{game_root} / "Globals" / filename);
    if (data.empty()) {
        diagnostics.push_back(
            {{"level", "warning"}, {"message", "DBF table missing or empty"}, {"table", filename}});
        return result;
    }
    d2res::DbfReader const reader{data};
    for (const auto& row : reader.read_records()) {
        const std::string id = lower(field(row, id_field));
        if (!id.empty()) {
            result[id] = row;
        }
    }
    return result;
}

std::string role_token(std::string_view animation_name, std::string_view unit_type) {
    const std::string anim = upper(animation_name);
    const std::string unit = upper(unit_type);
    const auto        pos = anim.find(unit);
    if (pos == std::string::npos) {
        return {};
    }
    const auto role_begin = pos + unit.size();
    const auto role_end = anim.find('1', role_begin);
    if (role_end == std::string::npos || role_end == role_begin) {
        return {};
    }
    return anim.substr(role_begin, role_end - role_begin);
}

std::map<std::string, std::vector<std::string>>
build_animation_index(const std::vector<std::string>& animations) {
    std::map<std::string, std::vector<std::string>> index;
    for (const auto& name : animations) {
        const std::string uname = upper(name);
        const auto        pos = uname.find("UU");
        if (pos == std::string::npos || pos + 6u > uname.size()) {
            continue;
        }
        const std::string unit = uname.substr(pos, 6);
        if (std::ranges::all_of(unit.substr(2), [](unsigned char c) { return std::isdigit(c); })) {
            index[unit].push_back(name);
        }
    }
    return index;
}

std::vector<std::string>
animations_for_unit(const std::map<std::string, std::vector<std::string>>& index,
                    std::string_view requested_type, std::string_view base_type) {
    std::vector<std::string> out;
    for (const auto& unit : {upper(requested_type), upper(base_type)}) {
        const auto it = index.find(unit);
        if (it != index.end()) {
            out.insert(out.end(), it->second.begin(), it->second.end());
        }
    }
    std::ranges::sort(out);
    out.erase(std::ranges::unique(out).begin(), out.end());
    return out;
}

json frame_summary(const d2res::DecodedAnimation& animation) {
    json        frames = json::array();
    const auto& meta_frames = animation.metadata.at("frames");
    for (const auto& frame : meta_frames) {
        frames.push_back(frame);
    }
    return frames;
}

json decode_animation_report(const d2res::AnimationDecoder& decoder, const std::string& name,
                             std::map<std::string, json>& cache) {
    const auto it = cache.find(name);
    if (it != cache.end()) {
        return it->second;
    }
    json out;
    out["frames"] = json::array();
    out["warnings"] = json::array();
    try {
        auto decoded = decoder.decode_animation_with_research_metadata(name);
        out["frame_count"] = decoded.frames.size();
        out["frames"] = frame_summary(decoded);
        for (const auto& warning : decoded.warnings) {
            out["warnings"].push_back(warning);
        }
    } catch (const std::exception& e) {
        out["frame_count"] = 0;
        out["warnings"].push_back(e.what());
    }
    cache[name] = out;
    return out;
}

std::string suffix_after_role(std::string_view animation_name, std::string_view unit_type,
                              std::string_view role) {
    const std::string anim = upper(animation_name);
    const std::string unit = upper(unit_type);
    const auto        pos = anim.find(unit);
    if (pos == std::string::npos) {
        return {};
    }
    const auto suffix_begin = pos + unit.size() + role.size();
    if (suffix_begin >= anim.size()) {
        return {};
    }
    return anim.substr(suffix_begin);
}

json make_hypotheses(const json& observed_facts) {
    json       hypotheses = json::array();
    const auto labels = observed_facts.value("morphology_labels", json::array());
    const bool full_canvas =
        std::ranges::find(labels, "full_canvas_overlay_candidate") != labels.end();
    const bool additive = std::ranges::find(labels, "additive_effect_candidate") != labels.end();
    if (full_canvas || additive) {
        hypotheses.push_back({{"statement", "animation may be a broad battle visual overlay"},
                              {"evidence", json::array({"observed_facts.role_token",
                                                        "observed_facts.morphology_labels",
                                                        "observed_facts.output_size_range",
                                                        "observed_facts.transparency_modes"})},
                              {"confidence", full_canvas && additive ? "medium" : "low"},
                              {"conflicting_evidence", json::array()}});
    }
    return hypotheses;
}

json make_matrix_row(const json& unit_report) {
    json row;
    row["unit_id"] = unit_report["unit_id"];
    row["base_unit_id"] = unit_report["base_unit_id"];
    row["observed_facts"] = unit_report;
    row["diagnostics"] = unit_report.value("warnings", json::array());
    row["hypotheses"] = json::array();
    for (const auto& animation : unit_report["animations"]) {
        const auto hypotheses = make_hypotheses(animation.value("observed_facts", json::object()));
        for (const auto& hypothesis : hypotheses) {
            row["hypotheses"].push_back(hypothesis);
        }
    }
    return row;
}

json aggregate_matrix(const json& rows) {
    std::map<std::string, int> attack_class;
    std::map<std::string, int> attack_source;
    std::map<std::string, int> attack_reach;
    std::map<std::string, int> role_token;
    std::map<std::string, int> morphology_label;
    for (const auto& row : rows) {
        const auto gameplay = row["observed_facts"].value("gameplay_linkage", json::object());
        const auto attack = gameplay.value("primary_attack", json::object());
        if (!attack.empty()) {
            ++attack_class[attack.value("CLASS", "")];
            ++attack_source[attack.value("SOURCE", "")];
            ++attack_reach[attack.value("REACH", "")];
        }
        for (const auto& animation : row["observed_facts"]["animations"]) {
            const std::string role = animation.value("role_token", "");
            if (!role.empty()) {
                ++role_token[role];
            }
            for (const auto& label : animation.value("morphology_labels", json::array())) {
                ++morphology_label[label.get<std::string>()];
            }
        }
    }
    return {{"by_attack_class", attack_class},
            {"by_attack_source", attack_source},
            {"by_attack_reach", attack_reach},
            {"by_role_token", role_token},
            {"by_morphology_label", morphology_label}};
}

json make_unit_report(
    const std::string& unit_type, const std::map<std::string, std::string>& bases,
    const d2res::AnimationDecoder&                                   decoder,
    const std::map<std::string, std::vector<std::string>>&           animation_index,
    const std::map<std::string, std::map<std::string, std::string>>& units_by_id,
    const std::map<std::string, std::map<std::string, std::string>>& attacks_by_id,
    std::map<std::string, json>&                                     decode_cache) {
    const std::string requested_type = upper(unit_type);
    const std::string requested_id = type_to_unit_id(requested_type);
    const auto        base_it = bases.find(requested_id);
    const std::string base_id = base_it == bases.end() ? requested_id : base_it->second;
    const std::string base_type = unit_id_to_type(base_id);

    json out;
    out["unit_id"] = requested_type;
    out["resource_unit_id"] = base_type.empty() ? requested_type : base_type;
    out["base_unit_id"] = base_id;
    out["role_label_policy"] = "name-derived observation";
    out["animations"] = json::array();
    out["warnings"] = json::array();
    out["gameplay_linkage"] =
        d2cli_research::build_gameplay_linkage(requested_type, units_by_id, attacks_by_id);
    for (const auto& diagnostic : out["gameplay_linkage"].value("diagnostics", json::array())) {
        out["warnings"].push_back(diagnostic);
    }

    const auto animations = animations_for_unit(animation_index, requested_type, base_type);
    const std::vector<std::string>     needles = {requested_type, base_type};
    std::map<std::string, std::size_t> role_counts;
    for (const auto& name : animations) {
        const std::string uname = upper(name);
        const bool        match = std::ranges::any_of(needles, [&](const std::string& needle) {
            return !needle.empty() && uname.find(needle) != std::string::npos;
        });
        if (!match) {
            continue;
        }
        const std::string token = role_token(
            name, uname.find(requested_type) != std::string::npos ? requested_type : base_type);
        if (!token.empty()) {
            ++role_counts[token];
        }
    }
    std::vector<std::string> found_roles;
    for (const auto& name : animations) {
        const std::string uname = upper(name);
        const bool        match = std::ranges::any_of(needles, [&](const std::string& needle) {
            return !needle.empty() && uname.find(needle) != std::string::npos;
        });
        if (!match) {
            continue;
        }
        json item;
        item["name"] = name;
        item["role_token"] = role_token(
            name, uname.find(requested_type) != std::string::npos ? requested_type : base_type);
        item["role_token_semantics"] = "name-derived only";
        item["name_derived_observation"] = true;
        const std::string suffix = suffix_after_role(
            name, uname.find(requested_type) != std::string::npos ? requested_type : base_type,
            item["role_token"].get<std::string>());
        item["direction_suffix"] = suffix.size() >= 2u ? suffix.substr(0, 2) : "";
        item["variant_suffix"] = suffix.size() >= 3u ? suffix.substr(1) : suffix;
        const auto decoded = decode_animation_report(decoder, name, decode_cache);
        item["frame_count"] = decoded["frame_count"];
        const json& frames = decoded["frames"];
        item["frame_metadata_sample"] = json::array();
        for (std::size_t i = 0; i < std::min<std::size_t>(frames.size(), 3u); ++i) {
            item["frame_metadata_sample"].push_back(frames.at(i));
        }
        for (const auto& warning : decoded["warnings"]) {
            item["warnings"].push_back(warning);
        }
        if (item["role_token"].is_string() && !item["role_token"].get<std::string>().empty()) {
            found_roles.push_back(item["role_token"].get<std::string>());
        }
        item["morphology"] = d2cli_research::summarize_role_morphology(
            frames, role_counts[item["role_token"].get<std::string>()]);
        item["morphology_labels"] = item["morphology"]["labels"];
        item["observed_facts"] = {
            {"role_token", item["role_token"]},
            {"role_token_semantics", item["role_token_semantics"]},
            {"frame_count", item["frame_count"]},
            {"direction_suffix", item["direction_suffix"]},
            {"variant_suffix", item["variant_suffix"]},
            {"morphology_labels", item["morphology_labels"]},
            {"output_size_range", item["morphology"]["output_size_range"]},
            {"transparency_modes", item["morphology"]["transparency_modes"]},
            {"piece_destination_bounds", item["morphology"]["piece_destination_bounds"]}};
        out["animations"].push_back(std::move(item));
    }

    out["missing_role_like_names"] = json::array();
    for (const std::string role : {"IDLEA", "HMOVA", "HHITA", "STILA", "HEFFA", "TUCHA"}) {
        if (std::ranges::find(found_roles, role) == found_roles.end()) {
            out["missing_role_like_names"].push_back(role);
        }
    }
    return out;
}

void write_text(const std::string& path, const std::string& text) {
    if (path.empty()) {
        return;
    }
    const auto parent = std::filesystem::path{path}.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
    std::ofstream out(path);
    out << text;
}

std::string markdown_report(const json& report) {
    std::ostringstream out;
    out << "# Battle FX Research Report\n\n";
    out << "Role labels are name-derived observations only.\n\n";
    for (const auto& unit : report["units"]) {
        out << "## " << unit["unit_id"].get<std::string>() << "\n\n";
        out << "- base_unit_id: `" << unit["base_unit_id"].get<std::string>() << "`\n";
        out << "- animations: " << unit["animations"].size() << "\n";
        out << "- diagnostics: " << unit["warnings"].size() << "\n";
        out << "- missing_role_like_names: `" << unit["missing_role_like_names"].dump() << "`\n\n";
    }
    out << "## Visual Intent Matrix\n\n";
    out << "Observed facts and hypotheses are separated in JSON. Viewer resolver behavior is a "
           "future change after these hypotheses are reviewed.\n\n";
    out << "### Aggregate Counts\n\n";
    out << "```json\n" << report["visual_intent_matrix"]["aggregates"].dump(2) << "\n```\n\n";
    out << "### Regeneration\n\n";
    out << "```bash\n";
    out << "build/verify/opendis2-dev-extractor battle-fx-report "
           "\"$DISCIPLES2_GAME_ROOT/Imgs/BatUnits.ff\" "
           "--game-root \"$DISCIPLES2_GAME_ROOT\"";
    for (const auto& unit : report["units"]) {
        out << " --unit " << unit["unit_id"].get<std::string>();
    }
    out << " --out research/battle_visual_intent_matrix_seed.json "
           "--markdown research/battle_visual_intent_matrix_seed.md\n";
    out << "```\n";
    return out.str();
}

} // namespace

namespace d2cli_research {

json summarize_role_morphology(const json& frames, std::size_t variant_count) {
    json out;
    out["frame_count"] = frames.size();
    out["output_size_range"] = {
        {"min_w", nullptr}, {"min_h", nullptr}, {"max_w", nullptr}, {"max_h", nullptr}};
    out["transparency_modes"] = json::array();
    out["piece_destination_bounds"] = {
        {"min_x", nullptr}, {"min_y", nullptr}, {"max_x", nullptr}, {"max_y", nullptr}};
    out["labels"] = json::array();
    out["evidence_gaps"] = json::array();

    if (frames.empty()) {
        out["labels"].push_back("no_effect_role");
        out["labels"].push_back("unknown");
        out["evidence_gaps"].push_back("no decoded frame metadata");
        return out;
    }

    std::optional<int>    min_w;
    std::optional<int>    min_h;
    std::optional<int>    max_w;
    std::optional<int>    max_h;
    std::optional<int>    min_x;
    std::optional<int>    min_y;
    std::optional<int>    max_x;
    std::optional<int>    max_y;
    std::set<std::string> modes;
    bool                  all_battle_size = true;
    bool                  any_parts = false;

    for (const auto& frame : frames) {
        const int w = frame.value("width", 0);
        const int h = frame.value("height", 0);
        if (w != 800 || h != 600) {
            all_battle_size = false;
        }
        min_w = min_w ? std::min(*min_w, w) : w;
        min_h = min_h ? std::min(*min_h, h) : h;
        max_w = max_w ? std::max(*max_w, w) : w;
        max_h = max_h ? std::max(*max_h, h) : h;

        const auto metadata = frame.value("research_metadata", json::object());
        const auto mode = metadata.value("transparency_mode", "");
        if (!mode.empty()) {
            modes.insert(mode);
        }
        for (const auto& part : metadata.value("parts", json::array())) {
            const auto dest = part.value("dest", json::object());
            const auto src = part.value("source_rect", json::object());
            const int  x = dest.value("x", 0);
            const int  y = dest.value("y", 0);
            const int  w2 = src.value("w", 0);
            const int  h2 = src.value("h", 0);
            any_parts = true;
            min_x = min_x ? std::min(*min_x, x) : x;
            min_y = min_y ? std::min(*min_y, y) : y;
            max_x = max_x ? std::max(*max_x, x + w2) : x + w2;
            max_y = max_y ? std::max(*max_y, y + h2) : y + h2;
        }
    }

    out["output_size_range"] = {
        {"min_w", *min_w}, {"min_h", *min_h}, {"max_w", *max_w}, {"max_h", *max_h}};
    out["transparency_modes"] = json::array();
    for (const auto& mode : modes) {
        out["transparency_modes"].push_back(mode);
    }
    if (any_parts) {
        out["piece_destination_bounds"] =
            json{{"min_x", *min_x}, {"min_y", *min_y}, {"max_x", *max_x}, {"max_y", *max_y}};
    } else {
        out["evidence_gaps"].push_back("no piece destination metadata");
    }

    if (all_battle_size) {
        out["labels"].push_back("full_canvas_overlay_candidate");
    }
    if (modes.contains("AdditiveBlend")) {
        out["labels"].push_back("additive_effect_candidate");
    }
    if (variant_count > 1u) {
        out["labels"].push_back("multi_variant_role");
    }
    if (!all_battle_size && any_parts) {
        out["labels"].push_back("unit_local_candidate");
        out["labels"].push_back("target_local_candidate");
    }
    if (out["labels"].empty()) {
        out["labels"].push_back("unknown");
    }
    return out;
}

json build_gameplay_linkage(
    std::string_view                                                 unit_type,
    const std::map<std::string, std::map<std::string, std::string>>& units_by_id,
    const std::map<std::string, std::map<std::string, std::string>>& attacks_by_id) {
    json              out;
    const std::string unit_id = type_to_unit_id(upper(unit_type));
    out["gunit_id"] = unit_id;
    out["diagnostics"] = json::array();

    const auto unit_it = units_by_id.find(unit_id);
    if (unit_it == units_by_id.end()) {
        out["diagnostics"].push_back({{"level", "warning"},
                                      {"message", "missing Gunits unit record"},
                                      {"unit_id", unit_id}});
        return out;
    }

    const auto& row = unit_it->second;
    const auto  base_unit_id = lower(field(row, "BASE_UNIT"));
    const auto  attack_id = lower(field(row, "ATTACK_ID"));
    const auto  attack2_id = lower(field(row, "ATTACK2_ID"));
    out["base_unit_id"] = base_unit_id.empty() ? unit_id : base_unit_id;
    out["attack_id"] = attack_id;
    out["secondary_attack_id"] = attack2_id;
    out["death_animation"] = field(row, "DEATH_ANIM");

    auto attach_attack = [&](const std::string& id, const char* key) {
        if (id.empty()) {
            return;
        }
        const auto attack_it = attacks_by_id.find(id);
        if (attack_it == attacks_by_id.end()) {
            out["diagnostics"].push_back(
                {{"level", "warning"}, {"message", "missing Gattacks record"}, {"attack_id", id}});
            return;
        }
        json attack;
        for (const auto& name : {"ATT_ID", "SOURCE", "CLASS", "REACH", "ALT_ATTACK", "QTY_DAM",
                                 "QTY_HEAL", "POWER", "INFINITE", "CRIT_HIT"}) {
            attack[name] = field(attack_it->second, name);
        }
        out[key] = std::move(attack);
    };
    attach_attack(attack_id, "primary_attack");
    attach_attack(attack2_id, "secondary_attack");
    return out;
}

} // namespace d2cli_research

int cmd_mqrc_inventory(const std::string& container_path) {
    const auto container = d2res::MqdbContainer::open(container_path);
    std::cout << d2res::to_json(d2res::inspect_raw_records(container), container).dump(2) << '\n';
    return 0;
}

int cmd_battle_fx_report(const std::string& container_path, const std::string& game_root,
                         const std::vector<std::string>& units, const std::string& out_json,
                         const std::string& out_markdown, const std::string& contact_sheet) {
    const auto                    container = d2res::MqdbContainer::open(container_path);
    const auto                    maps = d2res::parse_opt_maps(container);
    const d2res::AnimationDecoder decoder{container, maps};
    const auto                    animations = decoder.list_animations();
    const auto                    animation_index = build_animation_index(animations);
    const auto                    base_units = load_base_units(game_root);
    json                          diagnostics = json::array();
    const auto units_by_id = load_table_by_id(game_root, "Gunits.dbf", "UNIT_ID", diagnostics);
    const auto attacks_by_id = load_table_by_id(game_root, "Gattacks.dbf", "ATT_ID", diagnostics);

    json report;
    report["container"] = container_path;
    report["layout_authority"] = "OPT ImageMap";
    report["role_label_policy"] = "name-derived observation; no HEFF/TUCH/DAMA semantics";
    report["diagnostics"] = diagnostics;
    report["units"] = json::array();
    std::map<std::string, json> decode_cache;
    for (const auto& unit : units) {
        report["units"].push_back(make_unit_report(unit, base_units, decoder, animation_index,
                                                   units_by_id, attacks_by_id, decode_cache));
    }
    report["visual_intent_matrix"] = {{"schema_version", 1},
                                      {"role_token_policy", "name-derived observation only"},
                                      {"rows", json::array()}};
    for (const auto& unit : report["units"]) {
        report["visual_intent_matrix"]["rows"].push_back(make_matrix_row(unit));
    }
    report["visual_intent_matrix"]["aggregates"] =
        aggregate_matrix(report["visual_intent_matrix"]["rows"]);
    if (!out_json.empty()) {
        write_text(out_json, report.dump(2) + "\n");
    } else {
        std::cout << report.dump(2) << '\n';
    }
    if (!out_markdown.empty()) {
        write_text(out_markdown, markdown_report(report));
    }
    if (!contact_sheet.empty()) {
        write_text(contact_sheet,
                   "Contact sheet rendering is not required for JSON research data.\n");
    }
    return 0;
}
