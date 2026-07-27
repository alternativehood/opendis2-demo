#include "sweep_runner.hpp"
#include "battle_log_writer.hpp"
#include "battle_simulator.hpp"
#include "random_action_policy.hpp"
#include "stack_catalog.hpp"
#include "stack_pair_generator.hpp"

#include <d2battle_rules/battle_bootstrap.hpp>
#include <d2battle_rules/battle_fingerprint.hpp>
#include <d2log/log.hpp>
#include <d2runtime/AdventureWorldBuilder.hpp>
#include <d2runtime/AdventureWorldState.hpp>
#include <d2scenario/SgParser.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace d2battle_sweep {

namespace {

auto kLog = d2log::get("d2battle.sweep");

constexpr std::size_t kMaxFilenameLen = 200;

[[nodiscard]] std::string sanitize_for_filename(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    for (char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '.' || c == '_' || c == '+' || c == '-')
            result += c;
        else
            result += '_';
    }
    while (result.find("__") != std::string::npos)
        result.erase(result.find("__"), 1);
    while (result.find("..") != std::string::npos)
        result.replace(result.find(".."), 2, "__");
    if (!result.empty() && result[0] == '.')
        result = "_" + result;
    return result;
}

[[nodiscard]] std::string short_hash(const std::string& s) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (char c : s) {
        hash ^= static_cast<unsigned char>(c);
        hash *= 1099511628211ULL;
    }
    std::ostringstream oss;
    oss << std::hex << (hash & 0xFFFFFFFFULL);
    return oss.str();
}

[[nodiscard]] std::string utc_timestamp() {
    auto               t = std::time(nullptr);
    auto*              gmt = std::gmtime(&t);
    std::ostringstream oss;
    oss << std::put_time(gmt, "%Y%m%dT%H%M%SZ");
    return oss.str();
}

[[nodiscard]] std::string status_string(BattleRunStatus s) {
    switch (s) {
    case BattleRunStatus::Finished:
        return "FINISHED";
    case BattleRunStatus::AbortedNoValidActions:
        return "ABORTED_NO_VALID_ACTIONS";
    case BattleRunStatus::AbortedActionLimit:
        return "ABORTED_ACTION_LIMIT";
    case BattleRunStatus::AbortedRoundLimit:
        return "ABORTED_ROUND_LIMIT";
    case BattleRunStatus::AbortedRuleException:
        return "ABORTED_RULE_EXCEPTION";
    case BattleRunStatus::AbortedBootstrapFailure:
        return "ABORTED_BOOTSTRAP_FAILURE";
    case BattleRunStatus::AbortedLogFailure:
        return "ABORTED_LOG_FAILURE";
    }
    return "UNKNOWN";
}

} // namespace

std::string format_battle_sequence(std::uint64_t sequence) {
    std::ostringstream out;
    out << std::setw(6) << std::setfill('0') << sequence;
    return out.str();
}

std::filesystem::path create_run_directory(const std::filesystem::path& output_dir,
                                           std::uint64_t                seed) {
    std::ostringstream dir_name;
    dir_name << "run_" << utc_timestamp() << "_seed_" << seed;

    fs::path dir = output_dir / dir_name.str();

    if (fs::exists(dir)) {
        for (int suffix = 1; suffix < 100; ++suffix) {
            std::ostringstream alt;
            alt << dir_name.str() << "_" << std::setw(2) << std::setfill('0') << suffix;
            dir = output_dir / alt.str();
            if (!fs::exists(dir))
                break;
        }
    }

    fs::create_directories(dir);
    return dir;
}

std::string make_battle_filename(int sequence, const std::string& p1_id, const std::string& p1_desc,
                                 const std::string& p2_id, const std::string& p2_desc) {
    std::string s1 = sanitize_for_filename(p1_desc);
    std::string s2 = sanitize_for_filename(p2_desc);

    std::string seq_str = format_battle_sequence(static_cast<std::uint64_t>(sequence));

    std::string full = seq_str + "__1_" + p1_id;
    if (!s1.empty())
        full += "_" + s1;
    full += "__2_" + p2_id;
    if (!s2.empty())
        full += "_" + s2;

    if (full.size() > kMaxFilenameLen) {
        full = seq_str + "__1_" + p1_id + "_";
        if (s1.size() > 40)
            full += s1.substr(0, 40) + "...";
        else
            full += s1 + "...";
        full += "__2_" + p2_id + "_";
        if (s2.size() > 40)
            full += s2.substr(0, 40) + "...";
        else
            full += s2 + "...";
        full += "__" + short_hash(s1 + s2);
    }

    full += ".log";

    return full;
}

SweepSummary run_sweep(const SweepConfig& config) {
    auto logger = d2log::get("d2battle.sweep");

    SweepSummary summary;
    summary.scenario_path = config.scenario_path.string();
    summary.global_seed = config.seed;
    summary.max_actions = config.max_actions;
    summary.max_rounds = config.max_rounds;

    D2_LOG_INFO(logger, "=== BATTLE SWEEP ===");
    D2_LOG_INFO(logger, "scenario={}", config.scenario_path.string());
    D2_LOG_INFO(logger, "game_root={}", config.game_root.string());
    D2_LOG_INFO(logger, "output_dir={}", config.output_dir.string());
    D2_LOG_INFO(logger, "seed={}", config.seed);
    D2_LOG_INFO(logger, "max_actions={}", config.max_actions);
    D2_LOG_INFO(logger, "max_rounds={}", config.max_rounds);

    std::size_t   file_size = static_cast<std::size_t>(fs::file_size(config.scenario_path));
    std::ifstream ifs(config.scenario_path, std::ios::binary);
    if (!ifs) {
        D2_LOG_ERROR(logger, "cannot open scenario file: {}", config.scenario_path.string());
        throw std::runtime_error("cannot open scenario file");
    }

    std::vector<uint8_t> sg_data(file_size);
    if (!ifs.read(reinterpret_cast<char*>(sg_data.data()),
                  static_cast<std::streamsize>(file_size))) {
        D2_LOG_ERROR(logger, "cannot read scenario file: {}", config.scenario_path.string());
        throw std::runtime_error("cannot read scenario file");
    }

    D2_LOG_INFO(logger, "parsing SG file ({} bytes)...", file_size);
    d2scenario::SgParser parser(sg_data);
    auto                 parse_result = parser.parse();

    d2runtime::AdventureWorldBuilder builder;
    auto                             build_result = builder.build(parse_result.scenario);
    const auto&                      world = build_result.world;

    if (build_result.error_count() > 0) {
        D2_LOG_WARN(logger, "world build had {} errors", build_result.error_count());
    }

    std::string globals_dir = config.game_root.string();
    if (!globals_dir.empty() && globals_dir.back() != '/' && globals_dir.back() != '\\')
        globals_dir += "/";
    globals_dir += "Globals";

    d2engine::GameDataRegistry game_data(globals_dir);

    auto catalog_result = build_stack_catalog(world, game_data);
    summary.eligible_stacks = catalog_result.entries.size();

    for (const auto& d : catalog_result.diagnostics) {
        D2_LOG_ERROR(logger, "FATAL STACK CATALOG INVARIANT stack={} reason={}", d.stack_id,
                     d.reason);
    }

    D2_LOG_INFO(logger, "eligible stacks: {}", catalog_result.entries.size());

    auto pairs = generate_directed_pairs(catalog_result.entries);
    summary.directed_pairs = pairs.size();

    D2_LOG_INFO(logger, "directed pairs: {}", pairs.size());

    summary.run_dir = create_run_directory(config.output_dir, config.seed);
    D2_LOG_INFO(logger, "run directory: {}", summary.run_dir.string());

    int seq = 0;
    for (const auto& pair : pairs) {
        seq++;

        const auto* p1_entry = [&]() -> const StackCatalogEntry* {
            auto it =
                std::find_if(catalog_result.entries.begin(), catalog_result.entries.end(),
                             [&](const auto& e) { return e.stack_id == pair.party1_stack_id; });
            if (it != catalog_result.entries.end())
                return &*it;
            return nullptr;
        }();
        const auto* p2_entry = [&]() -> const StackCatalogEntry* {
            auto it =
                std::find_if(catalog_result.entries.begin(), catalog_result.entries.end(),
                             [&](const auto& e) { return e.stack_id == pair.party2_stack_id; });
            if (it != catalog_result.entries.end())
                return &*it;
            return nullptr;
        }();

        std::string p1_desc = p1_entry ? p1_entry->human_descriptor : pair.party1_stack_id;
        std::string p2_desc = p2_entry ? p2_entry->human_descriptor : pair.party2_stack_id;

        std::string filename =
            make_battle_filename(seq, pair.party1_stack_id, p1_desc, pair.party2_stack_id, p2_desc);
        fs::path log_path = summary.run_dir / filename;

        BattleRunResult result;
        result.party1_stack_id = pair.party1_stack_id;
        result.party2_stack_id = pair.party2_stack_id;

        try {
            BattleLogWriter log_writer(log_path);

            std::uint64_t battle_seed =
                stable_hash_64(config.seed, pair.party1_stack_id, pair.party2_stack_id);

            SimulatorConfig sim_config;
            sim_config.max_actions = config.max_actions;
            sim_config.max_rounds = config.max_rounds;
            sim_config.battle_seed = battle_seed;

            RandomActionPolicy policy(battle_seed);

            d2battle::BattleState initial_state;
            try {
                initial_state = d2battle::bootstrap_battle_from_stack_ids(
                    world, pair.party1_stack_id, pair.party2_stack_id, game_data);
            } catch (const std::exception& e) {
                D2_LOG_ERROR(logger, "bootstrap failed for {} vs {}: {}", pair.party1_stack_id,
                             pair.party2_stack_id, e.what());
                result.status = BattleRunStatus::AbortedBootstrapFailure;
                result.diagnostic = std::string("bootstrap: ") + e.what();

                summary.entries.push_back({seq, pair.party1_stack_id, pair.party2_stack_id,
                                           status_string(result.status), "", "", "", filename});
                summary.aborted_bootstrap_failure++;
                std::cout << "[" << format_battle_sequence(static_cast<std::uint64_t>(seq)) << "/"
                          << pairs.size() << "] " << pair.party1_stack_id << " vs "
                          << pair.party2_stack_id << " ... ABORTED_BOOTSTRAP_FAILURE\n";
                continue;
            }

            std::string seq_str = format_battle_sequence(static_cast<std::uint64_t>(seq));

            log_writer.write_header(config.scenario_path.string(), config.seed, battle_seed,
                                    seq_str, pair.party1_stack_id, pair.party2_stack_id);

            log_writer.write_party_detail(initial_state, d2battle::BattleSide::Party1, game_data);
            log_writer.write_party_detail(initial_state, d2battle::BattleSide::Party2, game_data);

            std::string init_fp = d2battle::compute_fingerprint(initial_state);
            log_writer.write_initial_fingerprint(init_fp);

            result = run_one_battle(initial_state, game_data, policy, log_writer, sim_config);

        } catch (const std::exception& e) {
            D2_LOG_ERROR(logger, "log failure for {} vs {}: {}", pair.party1_stack_id,
                         pair.party2_stack_id, e.what());
            result.status = BattleRunStatus::AbortedLogFailure;
            result.diagnostic = std::string("log: ") + e.what();
        } catch (...) {
            D2_LOG_ERROR(logger, "unknown log failure for {} vs {}", pair.party1_stack_id,
                         pair.party2_stack_id);
            result.status = BattleRunStatus::AbortedLogFailure;
            result.diagnostic = "unknown log failure";
        }

        BattleLogEntry entry;
        entry.sequence = seq;
        entry.party1_stack_id = pair.party1_stack_id;
        entry.party2_stack_id = pair.party2_stack_id;
        entry.status = status_string(result.status);
        entry.winner =
            result.winner.has_value()
                ? (*result.winner == d2battle::BattleSide::Party1 ? "PARTY_1" : "PARTY_2")
                : "";
        entry.rounds = std::to_string(result.final_round);
        entry.actions = std::to_string(result.actions_applied);
        entry.log_filename = filename;
        summary.entries.push_back(std::move(entry));

        summary.total++;

        switch (result.status) {
        case BattleRunStatus::Finished:
            summary.finished++;
            if (result.winner.has_value()) {
                if (*result.winner == d2battle::BattleSide::Party1)
                    summary.party1_wins++;
                else
                    summary.party2_wins++;
            }
            break;
        case BattleRunStatus::AbortedNoValidActions:
            summary.aborted_no_valid_actions++;
            break;
        case BattleRunStatus::AbortedActionLimit:
            summary.aborted_action_limit++;
            break;
        case BattleRunStatus::AbortedRoundLimit:
            summary.aborted_round_limit++;
            break;
        case BattleRunStatus::AbortedRuleException:
            summary.aborted_rule_exception++;
            break;
        case BattleRunStatus::AbortedBootstrapFailure:
            summary.aborted_bootstrap_failure++;
            break;
        case BattleRunStatus::AbortedLogFailure:
            summary.aborted_log_failure++;
            break;
        }

        std::cout << "[" << format_battle_sequence(static_cast<std::uint64_t>(seq)) << "/"
                  << pairs.size() << "] " << pair.party1_stack_id << " vs " << pair.party2_stack_id
                  << " ... " << status_string(result.status);
        if (result.winner.has_value()) {
            std::cout << " "
                      << (*result.winner == d2battle::BattleSide::Party1 ? "PARTY_1" : "PARTY_2");
        }
        std::cout << "\n";
    }

    fs::path summary_path = summary.run_dir / "summary.log";
    {
        std::ofstream sum_out(summary_path, std::ios::binary);

        sum_out << "scenario=" << summary.scenario_path << "\n";
        sum_out << "global_seed=" << summary.global_seed << "\n";
        sum_out << "eligible_stacks=" << summary.eligible_stacks << "\n";
        sum_out << "directed_pairs=" << summary.directed_pairs << "\n";
        sum_out << "max_actions=" << summary.max_actions << "\n";
        sum_out << "max_rounds=" << summary.max_rounds << "\n\n";

        for (const auto& e : summary.entries) {
            sum_out << format_battle_sequence(static_cast<std::uint64_t>(e.sequence)) << "\n";
            sum_out << "P1=" << e.party1_stack_id << "\n";
            sum_out << "P2=" << e.party2_stack_id << "\n";
            sum_out << "status=" << e.status << "\n";
            if (!e.winner.empty())
                sum_out << "winner=" << e.winner << "\n";
            if (!e.rounds.empty())
                sum_out << "rounds=" << e.rounds << "\n";
            if (!e.actions.empty())
                sum_out << "actions=" << e.actions << "\n";
            sum_out << "log=" << e.log_filename << "\n\n";
        }

        sum_out << "TOTAL=" << summary.total << "\n";
        sum_out << "FINISHED=" << summary.finished << "\n";
        sum_out << "ABORTED_NO_VALID_ACTIONS=" << summary.aborted_no_valid_actions << "\n";
        sum_out << "ABORTED_ACTION_LIMIT=" << summary.aborted_action_limit << "\n";
        sum_out << "ABORTED_ROUND_LIMIT=" << summary.aborted_round_limit << "\n";
        sum_out << "ABORTED_RULE_EXCEPTION=" << summary.aborted_rule_exception << "\n";
        sum_out << "ABORTED_BOOTSTRAP_FAILURE=" << summary.aborted_bootstrap_failure << "\n";
        sum_out << "ABORTED_LOG_FAILURE=" << summary.aborted_log_failure << "\n";
        sum_out << "PARTY1_WINS=" << summary.party1_wins << "\n";
        sum_out << "PARTY2_WINS=" << summary.party2_wins << "\n";
    }

    D2_LOG_INFO(logger, "sweep complete. summary at {}", summary_path.string());
    D2_LOG_INFO(logger, "TOTAL={} FINISHED={}", summary.total, summary.finished);

    return summary;
}

} // namespace d2battle_sweep
