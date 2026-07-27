#include <gtest/gtest.h>

#include "opendis2/battle_viewer_cli.hpp"

#include <CLI/CLI.hpp>

namespace {

d2engine::AppConfig parse_config(std::initializer_list<const char*> args) {
    CLI::App            app{"test"};
    d2engine::AppConfig config;
    d2battle_viewer::configure_cli(app, config);
    app.parse(static_cast<int>(args.size()), args.begin());
    return config;
}

} // namespace

TEST(BattleViewerCli, ParsesBattleScriptPath) {
    const auto config = parse_config({"opendis2 battle-viewer", "--game-root", "/game",
                                      "--battle-script", "some/path/events.json"});

    EXPECT_EQ(config.battle_script_path, "some/path/events.json");
}

TEST(BattleViewerCli, RejectsMissingBattleScript) {
    CLI::App            app{"test"};
    d2engine::AppConfig config;
    d2battle_viewer::configure_cli(app, config);

    EXPECT_THROW(app.parse({"opendis2 battle-viewer", "--game-root", "/game"}), CLI::ParseError);
}

TEST(BattleViewerCli, RejectsEmptyGameRoot) {
    CLI::App            app{"test"};
    d2engine::AppConfig config;
    d2battle_viewer::configure_cli(app, config);

    EXPECT_THROW(app.parse({"opendis2 battle-viewer", "--game-root", ""}), CLI::ParseError);
}

// ── Mode parsing ────────────────────────────────────────────────

TEST(BattleViewerCli, DefaultModeIsNormal) {
    EXPECT_FALSE(d2engine::AppConfig{}.debug_mode);
}

TEST(BattleViewerCli, ParsesModeNormal) {
    const auto config = parse_config({"opendis2 battle-viewer", "--game-root", "/game",
                                      "--battle-script", "test.json", "--mode", "normal"});
    EXPECT_FALSE(config.debug_mode);
}

TEST(BattleViewerCli, ParsesModeDebug) {
    const auto config = parse_config({"opendis2 battle-viewer", "--game-root", "/game",
                                      "--battle-script", "test.json", "--mode", "debug"});
    EXPECT_TRUE(config.debug_mode);
}

TEST(BattleViewerCli, RejectsInvalidMode) {
    CLI::App            app{"test"};
    d2engine::AppConfig config;
    d2battle_viewer::configure_cli(app, config);

    EXPECT_THROW(app.parse({"opendis2 battle-viewer", "--game-root", "/game", "--battle-script",
                            "test.json", "--mode", "garbage"}),
                 CLI::ParseError);
}
