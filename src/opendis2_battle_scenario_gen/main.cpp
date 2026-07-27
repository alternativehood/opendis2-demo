#include <d2buildinfo/build_info.hpp>
#include <d2engine/assets/game_data_registry.hpp>
#include <d2engine/assets/ff_asset_store.hpp>
#include <d2engine/battle_view/battle_slot.hpp>
#include <d2engine/battle_view/event_type_doc.hpp>
#include <d2engine/battle_view/unit_attack_summary.hpp>
#include <d2engine/battle_view/unit_id_helpers.hpp>
#include <d2log/log.hpp>

#include "formation_packer.hpp"
#include "scenario_gen_validation.hpp"

#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;
using namespace d2engine;

struct SelectedUnit {
    const UnitDef* def = nullptr;
    std::string    alias;
    std::string    slot_str;
    bool           is_large = false;
};

static void make_aliases_unique(std::vector<SelectedUnit>& units, const std::string& side_prefix) {
    std::unordered_map<std::string, int> counter;
    for (auto& u : units) {
        const std::string base = side_prefix + "_" + u.def->unit_id;
        int&              cnt = counter[base];
        u.alias = (cnt == 0) ? base : base + "_" + std::to_string(cnt);
        ++cnt;
    }
}

// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, char* argv[]) {
    CLI::App app{"opendis2-dev-scenario-gen -- Interactive scenario generator (development)"};
    app.set_version_flag("--version", d2buildinfo::format_build_version());

    std::string                 game_root;
    std::string                 output_dir = "battle_scenarios";
    int                         a_count = 6;
    int                         d_count = 6;
    int                         a_large_count = 0;
    int                         d_large_count = 0;
    std::optional<unsigned int> seed;
    std::string                 scenario_id;
    d2log::LogConfig            log_config;

    app.add_option("--game-root", game_root, "Path to Disciples II game root")->required();
    app.add_option("--output", output_dir, "Output directory (default: scenarios)");
    app.add_option("--a-count", a_count, "Total A-side logical units (default: 6)");
    app.add_option("--d-count", d_count, "Total D-side logical units (default: 6)");
    app.add_option("--a-large-count", a_large_count, "A-side large (2-cell) units (default: 0)");
    app.add_option("--d-large-count", d_large_count, "D-side large (2-cell) units (default: 0)");
    app.add_option("--seed", seed, "Random seed (optional)");
    app.add_option("--id", scenario_id, "Scenario ID/name (optional, will prompt if missing)");
    app.add_option("--log-level", log_config.level,
                   "Log level: trace|debug|info|warn|error|off (env: D2_LOG_LEVEL)");
    app.add_option("--log-file", log_config.file, "Log file path (env: D2_LOG_FILE)");

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    }

    d2log::init(log_config);
    auto kLog = d2log::get("d2.generator");

    // Clamp legacy params for backward compat; strictly validate new large-count options.
    a_count = std::clamp(a_count, 0, 6);
    d_count = std::clamp(d_count, 0, 6);

    try {
        d2gen::validate_scenario_gen_counts({a_count, d_count, a_large_count, d_large_count});
    } catch (const std::invalid_argument& e) {
        kLog->error("{}", e.what());
        return EXIT_FAILURE;
    }

    if (scenario_id.empty()) {
        std::cout << "Enter scenario name/id (default: generated): ";
        std::string line;
        std::getline(std::cin, line);
        scenario_id = line.empty() ? "generated" : line;
    }

    const fs::path         globals_dir = fs::path(game_root) / "Globals";
    GameDataRegistry const registry(globals_dir);

    d2engine::FfAssetStore store{fs::path(game_root)};

    auto has_face_portrait = [&](const std::string& unit_id) -> bool {
        std::string rid = unit_id;
        for (char& c : rid) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        return store.contains_record("Imgs/Faces.ff", rid + "FACE.PNG") ||
               store.contains_record("Imgs/Faces.ff", rid + "FACEB.PNG");
    };

    std::vector<const UnitDef*> small_pool;
    std::vector<const UnitDef*> large_pool;
    for (const auto& u : registry.all_units()) {
        if (!has_face_portrait(u.unit_id)) {
            continue;
        }
        (u.size_small ? small_pool : large_pool).push_back(&u);
    }

    const int a_small_count = a_count - a_large_count;
    const int d_small_count = d_count - d_large_count;

    if (a_small_count > 0 && small_pool.empty()) {
        kLog->error("no small units found in game data");
        return EXIT_FAILURE;
    }
    if ((a_large_count > 0 || d_large_count > 0) && large_pool.empty()) {
        kLog->error("no large units found in game data");
        return EXIT_FAILURE;
    }

    std::mt19937 rng;
    if (seed.has_value()) {
        rng.seed(static_cast<std::mt19937::result_type>(*seed));
    } else {
        std::random_device rd;
        rng.seed(rd());
    }
    std::shuffle(small_pool.begin(), small_pool.end(), rng);
    std::shuffle(large_pool.begin(), large_pool.end(), rng);

    // Walk through the shuffled pools with cursors so A and D get distinct units.
    std::size_t small_idx = 0;
    std::size_t large_idx = 0;
    auto        take_from = [](const std::vector<const UnitDef*>& pool, std::size_t& idx,
                               int n) -> std::vector<const UnitDef*> {
        std::vector<const UnitDef*> out;
        out.reserve(static_cast<std::size_t>(n));
        for (int i = 0; i < n && idx < pool.size(); ++i, ++idx) {
            out.push_back(pool[idx]);
        }
        return out;
    };

    auto a_small_defs = take_from(small_pool, small_idx, a_small_count);
    auto a_large_defs = take_from(large_pool, large_idx, a_large_count);
    auto d_small_defs = take_from(small_pool, small_idx, d_small_count);
    auto d_large_defs = take_from(large_pool, large_idx, d_large_count);

    if (static_cast<int>(a_small_defs.size()) < a_small_count ||
        static_cast<int>(d_small_defs.size()) < d_small_count) {
        kLog->error("not enough distinct small units: need {}, have {}",
                    a_small_count + d_small_count, small_pool.size());
        return EXIT_FAILURE;
    }
    if (static_cast<int>(a_large_defs.size()) < a_large_count ||
        static_cast<int>(d_large_defs.size()) < d_large_count) {
        kLog->error("not enough distinct large units: need {}, have {}",
                    a_large_count + d_large_count, large_pool.size());
        return EXIT_FAILURE;
    }

    auto a_formation = d2gen::pack_formation(BattleSide::Attacker, a_large_defs, a_small_defs, rng);
    auto d_formation = d2gen::pack_formation(BattleSide::Defender, d_large_defs, d_small_defs, rng);

    std::vector<SelectedUnit> a_units;
    std::vector<SelectedUnit> d_units;
    a_units.reserve(a_formation.size());
    d_units.reserve(d_formation.size());
    for (const auto& fa : a_formation) {
        a_units.push_back({fa.def, {}, slot_coord_to_string(fa.coord), fa.is_large});
    }
    for (const auto& fd : d_formation) {
        d_units.push_back({fd.def, {}, slot_coord_to_string(fd.coord), fd.is_large});
    }

    make_aliases_unique(a_units, "a");
    make_aliases_unique(d_units, "d");

    // ── Build JSON ──────────────────────────────────────────────────────────────
    json j;
    j["version"] = 3;
    j["id"] = scenario_id;
    j["terrain"] = "HU";

    json seq;
    seq["id"] = "s01_" + scenario_id;

    json setup_step;
    setup_step["id"] = "setup_units";
    setup_step["complete"] = "immediate";
    json events = json::array();
    // Attacker units first, then defender — per-side order preserved within each side.
    for (const auto& u : a_units) {
        events.push_back({{"id", "create_" + u.alias},
                          {"event",
                           {{"type", "UnitCreated"},
                            {"unit", u.alias},
                            {"unit_type", unit_type_from_resource_unit_id(u.def->unit_id)},
                            {"slot", u.slot_str}}}});
    }
    for (const auto& u : d_units) {
        events.push_back({{"id", "create_" + u.alias},
                          {"event",
                           {{"type", "UnitCreated"},
                            {"unit", u.alias},
                            {"unit_type", unit_type_from_resource_unit_id(u.def->unit_id)},
                            {"slot", u.slot_str}}}});
    }
    setup_step["events"] = std::move(events);

    json sel_step;
    sel_step["id"] = "select_first";
    sel_step["complete"] = "immediate";
    json        sel_events = json::array();
    std::string first_alias;
    if (!a_units.empty()) {
        first_alias = a_units[0].alias;
    } else if (!d_units.empty()) {
        first_alias = d_units[0].alias;
    }
    if (!first_alias.empty()) {
        sel_events.push_back(
            {{"id", "select_first_a"},
             {"event",
              {{"type", "ActorSelected"}, {"previous", first_alias}, {"selected", first_alias}}}});
    }
    sel_step["events"] = std::move(sel_events);

    json steps = json::array();
    steps.push_back(std::move(setup_step));
    steps.push_back(std::move(sel_step));
    seq["steps"] = std::move(steps);

    j["sequences"] = json::array();
    j["sequences"].push_back(std::move(seq));

    // ── Build comment header (centralized event doc + per-unit summaries) ────────
    std::string comment;
    comment += "// Generated by opendis2-dev-scenario-gen\n";
    comment += "// Scenario: " + scenario_id + "\n";
    comment += std::string(SCENARIO_DOC_HEADER_MARKER);
    comment += "//\n";
    comment += build_scenario_event_types_doc();
    comment += "// Unit summaries (from GameDataRegistry via unit_attack_summary.hpp):\n";

    auto all_units = a_units;
    all_units.insert(all_units.end(), d_units.begin(), d_units.end());
    for (const auto& u : all_units) {
        if (u.def == nullptr) {
            continue;
        }
        const auto summary = UnitAttackSummaryExtractor::extract(u.def, registry);
        comment += format_unit_attack_summary_comment(summary, u.alias, u.slot_str);
    }

    // ── Write output ─────────────────────────────────────────────────────────────
    const fs::path out_path = fs::path(output_dir) / scenario_id / "events.json";
    fs::create_directories(out_path.parent_path());

    std::ofstream ofs(out_path);
    if (!ofs) {
        kLog->error("cannot write {}", out_path.string());
        return EXIT_FAILURE;
    }
    ofs << comment << j.dump(2) << "\n";
    ofs.close();

    kLog->info("wrote scenario path={}", out_path.string());
    return EXIT_SUCCESS;
}
