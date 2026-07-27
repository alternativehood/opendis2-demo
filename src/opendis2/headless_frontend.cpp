#include "headless_frontend.hpp"

#include <d2log/log.hpp>

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace opendis2 {

HeadlessFrontend::HeadlessFrontend(d2game::GameSession& session) : session_(session) {}

int HeadlessFrontend::run() {
    // Send InspectWorld to get summary
    auto result = session_.handle_command(d2game::GameInspectWorldCommand{});

    // Print summary from events
    bool found_summary = false;
    for (const auto& ev : result.events) {
        if (auto* inspect = std::get_if<d2game::GameInspectResult>(&ev)) {
            std::cout << inspect->world_summary << std::endl;
            found_summary = true;
        }
    }

    // Also print structured inspect from session
    auto summary = session_.inspect();

    std::cout << "\n--- Structured Summary ---\n"
              << "Scenario path: " << summary.scenario_id << "\n"
              << "Scenario name: " << summary.scenario_name << "\n"
              << "Map dimensions: " << summary.map_width << "x" << summary.map_height << "\n"
              << "Terrain tiles: " << summary.terrain_tiles << "\n"
              << "Semantic objects: " << summary.semantic_objects << "\n"
              << "Runtime objects: " << summary.runtime_objects << "\n"
              << "Build diagnostics: " << summary.build_warnings << " warnings, "
              << summary.build_errors << " errors\n"
              << "Game mode: Adventure\n"
              << std::endl;

    // Send NoOp to exercise the command path
    auto noop_result = session_.handle_command(d2game::GameNoOpCommand{});
    (void)noop_result;

    // Send Quit to cleanly exit
    session_.handle_command(d2game::GameQuitCommand{});

    return found_summary ? 0 : 1;
}

} // namespace opendis2
