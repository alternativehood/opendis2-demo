#include <d2game/GameSession.hpp>
#include <d2game/GameCommand.hpp>
#include <d2game/GameEvent.hpp>

#include <d2runtime/AdventureWorldBuilder.hpp>
#include <d2runtime/AdventureWorldState.hpp>

#include <d2scenario/ScenarioTemplate.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <string>

using namespace d2game;
using namespace d2runtime;

static d2scenario::ScenarioTemplate make_minimal_tmpl() {
    d2scenario::ScenarioTemplate tmpl;
    tmpl.info.id = d2scenario::SgObjectId("scn_session");
    tmpl.info.name = "Session Test";
    tmpl.map.terrain.width = 36;
    tmpl.map.terrain.height = 36;
    tmpl.map.terrain.tiles.assign(36, std::vector<uint32_t>(36, 0));
    return tmpl;
}

static GameSession make_session() {
    AdventureWorldBuilder builder;
    auto                  build_result = builder.build(make_minimal_tmpl());
    return GameSession(std::move(build_result.world), build_result.warning_count(),
                       build_result.error_count());
}

// ── Quit ───────────────────────────────────────────────────────────────────

TEST(GameSession, AcceptsQuit) {
    auto session = make_session();
    auto result = session.handle_command(GameQuitCommand{});

    EXPECT_TRUE(result.quit_requested);

    bool found_quit = false;
    for (const auto& ev : result.events) {
        if (std::holds_alternative<GameQuitRequested>(ev)) {
            found_quit = true;
            break;
        }
    }
    EXPECT_TRUE(found_quit);
}

// ── InspectWorld produces deterministic result ─────────────────────────────

TEST(GameSession, InspectWorldProducesDeterministicResult) {
    auto session = make_session();
    auto result1 = session.handle_command(GameInspectWorldCommand{});
    auto result2 = session.handle_command(GameInspectWorldCommand{});

    EXPECT_FALSE(result1.quit_requested);
    EXPECT_FALSE(result2.quit_requested);

    std::size_t inspect_count_1 = 0;
    std::size_t inspect_count_2 = 0;
    for (const auto& ev : result1.events) {
        if (std::holds_alternative<GameInspectResult>(ev))
            ++inspect_count_1;
    }
    for (const auto& ev : result2.events) {
        if (std::holds_alternative<GameInspectResult>(ev))
            ++inspect_count_2;
    }
    EXPECT_EQ(inspect_count_1, 1u);
    EXPECT_EQ(inspect_count_2, 1u);
}

// ── InspectWorld summary contains key info ──────────────────────────────────

TEST(GameSession, InspectSummaryContainsKeyInfo) {
    auto session = make_session();
    auto summary = session.inspect();

    EXPECT_EQ(summary.scenario_id, "scn_session");
    EXPECT_EQ(summary.scenario_name, "Session Test");
    EXPECT_EQ(summary.map_width, 36);
    EXPECT_EQ(summary.map_height, 36);
}
