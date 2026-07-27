#include <gtest/gtest.h>

#include "opendis2/cli_setup.hpp"
#include "opendis2/launch_options.hpp"

#include <CLI/CLI.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>
#include <vector>

namespace {

void parse(CLI::App& app, std::initializer_list<const char*> args) {
    std::vector<const char*> av{args};
    app.parse(static_cast<int>(av.size()), av.data());
}

// Create a small temp file and return its path (portable C++17, no POSIX APIs)
std::string create_temp_file() {
    auto          ts = std::chrono::steady_clock::now().time_since_epoch().count();
    auto          path = std::filesystem::temp_directory_path() /
                         ("opendis2_test_cli_" + std::to_string(ts) + ".tmp");
    std::ofstream ofs(path);
    ofs.close();
    return path.string();
}

} // namespace

TEST(OpenDis2Cli, VersionConstantIsNotEmpty) {
    EXPECT_FALSE(std::string{opendis2::kProjectVersion}.empty());
}

TEST(OpenDis2Cli, LaunchOptionsDefaultModeIsNone) {
    opendis2::LaunchOptions opts;
    EXPECT_EQ(opts.mode, opendis2::LaunchOptions::Mode::None);
}

TEST(OpenDis2Cli, HelpFlagTriggersCallForHelp) {
    CLI::App                app{"test"};
    opendis2::LaunchOptions opts;
    opendis2::setup_cli(app, opts);

    EXPECT_THROW(parse(app, {"opendis2", "--help"}), CLI::CallForHelp);
}

TEST(OpenDis2Cli, VersionFlagTriggersCallForVersion) {
    CLI::App                app{"test"};
    opendis2::LaunchOptions opts;
    opendis2::setup_cli(app, opts);

    EXPECT_THROW(parse(app, {"opendis2", "--version"}), CLI::CallForVersion);
}

TEST(OpenDis2Cli, InvalidOptionThrowsParseError) {
    CLI::App                app{"test"};
    opendis2::LaunchOptions opts;
    opendis2::setup_cli(app, opts);

    EXPECT_THROW(parse(app, {"opendis2", "--bogus"}), CLI::ParseError);
}

TEST(OpenDis2Cli, NoArgsSucceedsWithoutError) {
    CLI::App                app{"test"};
    opendis2::LaunchOptions opts;
    opendis2::setup_cli(app, opts);

    EXPECT_NO_THROW(parse(app, {"opendis2"}));
}

TEST(OpenDis2Cli, BattleViewerHelpTriggersCallForHelp) {
    CLI::App                app{"test"};
    opendis2::LaunchOptions opts;
    opendis2::setup_cli(app, opts);

    EXPECT_THROW(parse(app, {"opendis2", "battle-viewer", "--help"}), CLI::CallForHelp);
}

TEST(OpenDis2Cli, BattleViewerNoArgsFails) {
    CLI::App                app{"test"};
    opendis2::LaunchOptions opts;
    opendis2::setup_cli(app, opts);

    EXPECT_THROW(parse(app, {"opendis2", "battle-viewer"}), CLI::ParseError);
}

TEST(OpenDis2Cli, BattleViewerParses) {
    CLI::App                app{"test"};
    opendis2::LaunchOptions opts;
    opendis2::setup_cli(app, opts);

    EXPECT_NO_THROW(
        parse(app, {"opendis2", "battle-viewer", "--game-root", "/x", "--battle-script", "/y"}));
    EXPECT_EQ(opts.mode, opendis2::LaunchOptions::Mode::BattleViewer);
    EXPECT_EQ(opts.battle_viewer_config.game_root, "/x");
    EXPECT_EQ(opts.battle_viewer_config.battle_script_path, "/y");
}

TEST(OpenDis2Cli, BattleViewerMissingGameRootFails) {
    CLI::App                app{"test"};
    opendis2::LaunchOptions opts;
    opendis2::setup_cli(app, opts);

    EXPECT_THROW(parse(app, {"opendis2", "battle-viewer", "--battle-script", "/y"}),
                 CLI::ParseError);
}

TEST(OpenDis2Cli, BattleViewerMissingBattleScriptFails) {
    CLI::App                app{"test"};
    opendis2::LaunchOptions opts;
    opendis2::setup_cli(app, opts);

    EXPECT_THROW(parse(app, {"opendis2", "battle-viewer", "--game-root", "/x"}), CLI::ParseError);
}

// ── Adventure subcommand tests ──────────────────────────────────────────────

TEST(OpenDis2Cli, AdventureHelpTriggersCallForHelp) {
    CLI::App                app{"test"};
    opendis2::LaunchOptions opts;
    opendis2::setup_cli(app, opts);

    EXPECT_THROW(parse(app, {"opendis2", "adventure", "--help"}), CLI::CallForHelp);
}

TEST(OpenDis2Cli, AdventureWithoutScenarioFails) {
    CLI::App                app{"test"};
    opendis2::LaunchOptions opts;
    opendis2::setup_cli(app, opts);

    EXPECT_THROW(parse(app, {"opendis2", "adventure"}), CLI::ParseError);
}

TEST(OpenDis2Cli, AdventureParsesWithExistingScenario) {
    std::string fixture = create_temp_file();

    CLI::App                app{"test"};
    opendis2::LaunchOptions opts;
    opendis2::setup_cli(app, opts);

    EXPECT_NO_THROW(parse(app, {"opendis2", "adventure", "--scenario", fixture.c_str()}));
    EXPECT_EQ(opts.mode, opendis2::LaunchOptions::Mode::Adventure);
    EXPECT_EQ(opts.scenario_path, fixture);
    EXPECT_FALSE(opts.headless);

    std::filesystem::remove(fixture);
}

TEST(OpenDis2Cli, AdventureHeadlessFlagParses) {
    std::string fixture = create_temp_file();

    CLI::App                app{"test"};
    opendis2::LaunchOptions opts;
    opendis2::setup_cli(app, opts);

    EXPECT_NO_THROW(
        parse(app, {"opendis2", "adventure", "--scenario", fixture.c_str(), "--headless"}));
    EXPECT_EQ(opts.mode, opendis2::LaunchOptions::Mode::Adventure);
    EXPECT_EQ(opts.scenario_path, fixture);
    EXPECT_TRUE(opts.headless);

    std::filesystem::remove(fixture);
}

TEST(OpenDis2Cli, AdventureNonexistentScenarioFails) {
    CLI::App                app{"test"};
    opendis2::LaunchOptions opts;
    opendis2::setup_cli(app, opts);

    EXPECT_THROW(parse(app, {"opendis2", "adventure", "--scenario", "/nonexistent/test.sg"}),
                 CLI::ParseError);
}
