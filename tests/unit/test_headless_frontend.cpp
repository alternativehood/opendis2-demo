#include <opendis2/headless_frontend.hpp>
#include <d2game/GameSession.hpp>
#include <d2runtime/AdventureWorldBuilder.hpp>
#include <d2runtime/AdventureWorldState.hpp>
#include <d2scenario/ScenarioTemplate.hpp>

#include <gtest/gtest.h>

#include <sstream>
#include <string>

using namespace d2game;
using namespace d2runtime;

// ── HeadlessFrontend runs against a minimal session ────────────────────────

TEST(HeadlessFrontend, RunsAgainstMinimalSession) {
    d2scenario::ScenarioTemplate tmpl;
    tmpl.info.id = "scn_headless";
    tmpl.info.name = "Headless Test";
    tmpl.map.terrain.width = 48;
    tmpl.map.terrain.height = 48;
    tmpl.map.terrain.tiles.assign(48, std::vector<uint32_t>(48, 0));

    AdventureWorldBuilder builder;
    auto                  build_result = builder.build(tmpl);

    GameSession session(std::move(build_result.world), build_result.warning_count(),
                        build_result.error_count());

    opendis2::HeadlessFrontend frontend(session);
    int                        exit_code = frontend.run();

    EXPECT_EQ(exit_code, 0);
}
