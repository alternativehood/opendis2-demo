#include <gtest/gtest.h>
#include "d2engine/assets/asset_runtime.hpp"
#include "d2engine/assets/ff_asset_store.hpp"
#include "d2engine/assets/game_data_registry.hpp"
#include "d2engine/assets/asset_runtime_catalog_adapter.hpp"
#include "d2engine/assets/iso_actor_visual_resolver.hpp"
#include "d2engine/battle_adapters/raw_ff_animation_catalog.hpp"
#include "d2res/mqdb.hpp"
#include <algorithm>
#include <filesystem>
#include <iostream>

static const char* GAME_ROOT_ENV = [] {
    const char* env = std::getenv("DISCIPLES2_GAME_ROOT"); // NOLINT
    return (env != nullptr && env[0] != '\0') ? env : DISCIPLES2_GAME_ROOT;
}();

TEST(MqdbIsounit, OpensAndHasExpectedRecords) {
    using namespace d2res;
    const auto container =
        MqdbContainer::open(std::filesystem::path(GAME_ROOT_ENV) / "Imgs/Isounit.ff");
    const auto names = container.names();
    EXPECT_FALSE(names.empty());
}

TEST(MqdbIsounit, UnitTypeStopAnimationNamingRule) {
    using namespace d2engine;
    GameDataRegistry game_data(std::filesystem::path(GAME_ROOT_ENV) / "Globals");
    for (const auto& unit : game_data.all_units()) {
        if (unit.death_anim_id <= 0)
            continue;
        EXPECT_FALSE(unit.death_battle_ff_animation.empty())
            << unit.unit_id << " has death_anim_id=" << unit.death_anim_id
            << " but empty death_battle_ff_animation";
    }
}

TEST(MqdbIsounit, IsoActorVisualResolverUsesStopAnimation) {
    using namespace d2engine;
    d2engine::AssetRuntime     assets(std::filesystem::path(GAME_ROOT_ENV), 1);
    d2engine::GameDataRegistry game_data(std::filesystem::path(GAME_ROOT_ENV) / "Globals");
    d2engine::AssetRuntimeCatalogAdapter catalog(assets);
    d2engine::IsoActorVisualResolver     resolver(catalog, game_data);

    for (const auto& type : {"G000UU0020", "G000UU0100"}) {
        d2engine::AdventureStackActorVisualRequest req;
        req.presentation = {d2runtime::AdventureActorPresentationKind::Unit};
        req.leader_unit_type_id = type;
        req.direction = d2runtime::AdventureIsoDirection::D0;
        const auto visual = resolver.resolve(req);
        ASSERT_TRUE(visual.has_value()) << type;
        EXPECT_EQ(visual->body.container_path, "Imgs/Isounit.ff");
        EXPECT_FALSE(visual->body.frames.empty());
        EXPECT_FALSE(visual->body.frames.front().record_name.empty());
        EXPECT_GT(visual->body.frames.front().canvas_width, 0);
        EXPECT_GT(visual->body.frames.front().canvas_height, 0);
        EXPECT_GT(visual->body.native_canvas_w, 0);
        EXPECT_GT(visual->body.native_canvas_h, 0);
    }
}

TEST(MqdbIsounit, AuthoredEmptyShadowAnimationMetadataAndResolver) {
    using namespace d2engine;

    AssetRuntime               assets(std::filesystem::path(GAME_ROOT_ENV), 1);
    GameDataRegistry           game_data(std::filesystem::path(GAME_ROOT_ENV) / "Globals");
    AssetRuntimeCatalogAdapter catalog(assets);

    const auto shadow_seq = assets.animation_sequence("Imgs/Isounit.ff", "G000UU0099SSTO7");
    ASSERT_EQ(shadow_seq.frames.size(), 1u);
    EXPECT_EQ(shadow_seq.frames.front().image_name, "RJJ");
    EXPECT_FALSE(shadow_seq.frames.front().content_bounds.valid());
    EXPECT_GT(shadow_seq.native_canvas_w, 0);
    EXPECT_GT(shadow_seq.native_canvas_h, 0);

    IsoActorVisualResolver           resolver(catalog, game_data);
    AdventureStackActorVisualRequest req;
    req.presentation = {d2runtime::AdventureActorPresentationKind::Unit};
    req.leader_unit_type_id = "G000UU0099";
    req.direction = d2runtime::AdventureIsoDirection::D7;

    const auto visual = resolver.resolve(req);
    ASSERT_TRUE(visual.has_value());
    ASSERT_FALSE(visual->shadow.has_value());
    EXPECT_EQ(visual->shadow_presence, ExactLayerPresence::AuthoredEmpty);
    EXPECT_TRUE(visual->body.content_bounds.valid());
    EXPECT_GT(visual->body.native_canvas_w, 0);
    EXPECT_GT(visual->body.native_canvas_h, 0);
}

TEST(MqdbIsounit, RawCatalogLoadsLargeCorpseImageSequenceWithAnchorMetadata) {
    d2engine::AssetRuntime          assets{std::filesystem::path(GAME_ROOT_ENV), 1};
    d2engine::RawFfAnimationCatalog catalog{assets, "Imgs/Batunits.ff", "Imgs/Battle.ff"};

    const auto corpse = catalog.image_sequence("DEAD_HUMAN_LA");

    ASSERT_EQ(corpse.frames.size(), 2u);
    EXPECT_EQ(corpse.frames[0].image_name, "DEAD_HUMAN_LA00");
    EXPECT_EQ(corpse.frames[1].image_name, "DEAD_HUMAN_LA01");
    EXPECT_GT(corpse.canvas_foot_x, 0);
    EXPECT_GT(corpse.canvas_foot_y, 0);
    EXPECT_GT(corpse.native_canvas_w, 0);
    EXPECT_GT(corpse.native_canvas_h, 0);
}
