#include <CLI/CLI.hpp>

#include <opendis2_battle_sweep/sweep_runner.hpp>

#include <d2buildinfo/build_info.hpp>
#include <d2log/log.hpp>

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    CLI::App app{"opendis2-dev-battle — headless stack battle sweep"};

    app.set_version_flag("--version", d2buildinfo::format_build_version());

    d2log::LogConfig log_config;
    app.add_option("--log-level", log_config.level, "Log level: trace|debug|info|warn|error|off");
    app.add_option("--log-file", log_config.file, "Log file path");

    d2battle_sweep::SweepConfig config;

    app.add_option("--scenario", config.scenario_path, "Path to .sg scenario file")
        ->required()
        ->check(CLI::ExistingFile);

    app.add_option("--game-root", config.game_root, "Path to Disciples II game root")
        ->envname("DISCIPLES2_GAME_ROOT")
        ->required();

    app.add_option("--output-dir", config.output_dir, "Output directory for battle logs")
        ->default_val("battle_logs");

    app.add_option("--seed", config.seed, "Random seed for reproducible runs")
        ->default_val(0x1A2B3C4D5E6F7890ULL);

    app.add_option("--max-actions", config.max_actions, "Maximum actions per battle")
        ->default_val(std::uint64_t{1000});

    app.add_option("--max-rounds", config.max_rounds, "Maximum rounds per battle")
        ->default_val(std::uint32_t{100});

    try {
        try {
            app.parse(argc, argv);
        } catch (const CLI::ParseError& e) {
            return app.exit(e);
        }

        d2log::init(log_config);

        auto summary = d2battle_sweep::run_sweep(config);

        std::cout << "\nSWEEP COMPLETE\n";
        std::cout << "scenario=" << summary.scenario_path << "\n";
        std::cout << "global_seed=" << summary.global_seed << "\n";
        std::cout << "eligible_stacks=" << summary.eligible_stacks << "\n";
        std::cout << "directed_pairs=" << summary.directed_pairs << "\n";
        std::cout << "run_dir=" << summary.run_dir.string() << "\n";
        std::cout << "TOTAL=" << summary.total << "\n";
        std::cout << "FINISHED=" << summary.finished << "\n";
        std::cout << "ABORTED_NO_VALID_ACTIONS=" << summary.aborted_no_valid_actions << "\n";
        std::cout << "ABORTED_ACTION_LIMIT=" << summary.aborted_action_limit << "\n";
        std::cout << "ABORTED_ROUND_LIMIT=" << summary.aborted_round_limit << "\n";
        std::cout << "ABORTED_RULE_EXCEPTION=" << summary.aborted_rule_exception << "\n";
        std::cout << "ABORTED_BOOTSTRAP_FAILURE=" << summary.aborted_bootstrap_failure << "\n";
        std::cout << "ABORTED_LOG_FAILURE=" << summary.aborted_log_failure << "\n";
        std::cout << "PARTY1_WINS=" << summary.party1_wins << "\n";
        std::cout << "PARTY2_WINS=" << summary.party2_wins << "\n";

        bool all_finished =
            summary.aborted_no_valid_actions == 0 && summary.aborted_action_limit == 0 &&
            summary.aborted_round_limit == 0 && summary.aborted_rule_exception == 0 &&
            summary.aborted_bootstrap_failure == 0 && summary.aborted_log_failure == 0;

        return all_finished ? 0 : 2;

    } catch (const std::exception& e) {
        d2log::write_fatal_stderr(e.what());
        return EXIT_FAILURE;
    } catch (...) {
        d2log::write_fatal_stderr("unknown exception");
        return EXIT_FAILURE;
    }
}
