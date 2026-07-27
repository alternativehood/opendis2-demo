#include "adventure_launcher.hpp"
#include "adventure_scenario_loader.hpp"
#include "adventure_startup_screen.hpp"
#include "headless_frontend.hpp"

#include <d2engine/app/application.hpp>
#include <d2engine/app/app_config.hpp>

#include <d2log/log.hpp>

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>

namespace opendis2 {

int run_headless_adventure(const std::string& scenario_path) {
    d2log::init({});

    auto load_result = AdventureScenarioLoader::load_semantic(scenario_path);
    if (!load_result || !load_result->success) {
        d2log::get("d2.adventure")->error("failed_to_load_scenario path={}", scenario_path);
        return EXIT_FAILURE;
    }

    for (const auto& diag : load_result->build_diagnostics) {
        d2log::get("d2.adventure")
            ->info("build_diagnostic kind={} msg={}", static_cast<int>(diag.kind), diag.message);
    }
    d2log::get("d2.adventure")
        ->info("adventure_world units={} stacks={}", load_result->session->world().units.size(),
               load_result->session->world().stacks.size());

    HeadlessFrontend frontend(*load_result->session);
    return frontend.run();
}

int run_graphical_adventure(const std::string& scenario_path, const std::string& game_root,
                            const std::string&    debug_battle_script_path,
                            std::filesystem::path config_root_override) {
    d2log::init({});

    if (game_root.empty()) {
        d2log::get("d2.adventure")
            ->error("graphical_adventure requires --game-root or DISCIPLES2_GAME_ROOT");
        return EXIT_FAILURE;
    }

    d2engine::AppConfig config;
    config.scenario_path = scenario_path;
    config.game_root = game_root;
    config.battle_script_path = debug_battle_script_path;
    config.config_root_override = std::move(config_root_override);

    try {
        d2engine::Application app(config);

        std::function<void()> request_debug_battle;
        if (!config.battle_script_path.empty()) {
            request_debug_battle = [&app]() {
                app.start_battle_screen([&app]() { app.pop_overlay_screen(); });
            };
        }

        auto loading_screen = std::make_unique<AdventureStartupScreen>(
            app, config, [&app]() { app.request_quit(); }, std::move(request_debug_battle));

        app.start_with_screen(std::move(loading_screen));
        return app.run();
    } catch (const std::exception& e) {
        d2log::get("d2.app")->error("fatal: {}", e.what());
        return EXIT_FAILURE;
    }
}

} // namespace opendis2
