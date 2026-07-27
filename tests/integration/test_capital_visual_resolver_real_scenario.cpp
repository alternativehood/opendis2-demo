#include <d2adventure_render/terrain/capital_asset_catalog.hpp>
#include <d2engine/assets/capital_asset_catalog_builder.hpp>
#include <d2engine/assets/capital_visual_resolver.hpp>
#include <d2engine/assets/ff_asset_store.hpp>
#include <d2engine/assets/game_data_registry.hpp>
#include <d2runtime/AdventureWorldBuilder.hpp>
#include <d2runtime/AdventureWorldState.hpp>
#include <d2scenario/SgParser.hpp>

#include <gtest/gtest.h>

#include <cstdlib>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

fs::path find_sg_file() {
    const char* env = std::getenv("OPENDIS2_ADVENTURE_TEST_SG");
    if (env != nullptr && env[0] != '\0') {
        const fs::path p(env);
        if (fs::is_regular_file(p)) {
            return p;
        }
    }

    const char* game_root_env = std::getenv("DISCIPLES2_GAME_ROOT");
    if (game_root_env != nullptr && game_root_env[0] != '\0') {
        const fs::path p = fs::path(game_root_env) / "all_objects_demo_config_20260718_101736.sg";
        if (fs::is_regular_file(p)) {
            return p;
        }
    }

    return {};
}

std::vector<std::uint8_t> read_file(const fs::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return {};
    }
    const auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
    file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
    return data;
}

} // namespace

TEST(CapitalVisualResolverRealScenario, HumanCapitalResolvesGuardianActiveHu0) {
    const auto sg_path = find_sg_file();
    if (sg_path.empty()) {
        GTEST_SKIP() << "real SG fixture not found";
    }

    const auto data = read_file(sg_path);
    ASSERT_FALSE(data.empty());

    d2scenario::SgParser             parser(data);
    const auto                       parse_result = parser.parse();
    d2runtime::AdventureWorldBuilder builder;
    const auto                       build_result = builder.build(parse_result.scenario);

    const auto capital_it =
        std::find_if(build_result.world.capitals.begin(), build_result.world.capitals.end(),
                     [](const d2runtime::AdventureCapital& c) { return c.id == "S143FT0000"; });
    ASSERT_NE(capital_it, build_result.world.capitals.end());
    const auto& capital = *capital_it;
    const auto* subrace = build_result.world.find_subrace(capital.subrace);
    ASSERT_NE(subrace, nullptr);

    const char* game_root_env = std::getenv("DISCIPLES2_GAME_ROOT");
    if (game_root_env == nullptr || game_root_env[0] == '\0') {
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";
    }

    d2engine::FfAssetStore          store(game_root_env);
    const auto                      catalog = d2engine::build_capital_asset_catalog(store);
    d2engine::GameDataRegistry      game_data{fs::path(game_root_env)};
    d2engine::CapitalVisualResolver resolver(catalog, game_data);

    const auto* guardian = build_result.world.find_unit("S143UN0000");
    ASSERT_NE(guardian, nullptr);
    EXPECT_GT(guardian->current_hp, 0);

    const auto* race = game_data.find_race(subrace->race_id);
    ASSERT_NE(race, nullptr);
    EXPECT_EQ(race->guardian_unit_id, "g000uu3001");

    const auto resolved = resolver.resolve(build_result.world, capital, subrace->race_id);
    EXPECT_EQ(resolved.state, d2engine::adventure_render::CapitalVisualState::Active);
    EXPECT_EQ(resolved.guardian_type_id, "g000uu3001");
    EXPECT_EQ(resolved.guardian_instance_id, "S143UN0000");
    ASSERT_NE(resolved.visual, nullptr);
    EXPECT_EQ(resolved.visual->logical_animation_name, "G000FT0000HU0");
}
