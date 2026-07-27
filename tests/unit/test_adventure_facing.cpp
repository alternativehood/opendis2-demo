#include <gtest/gtest.h>

#include <d2adventure_render/adventure_render_types.hpp>
#include <d2adventure_render/map_geometry.hpp>
#include <d2adventure_render/map_preparer.hpp>
#include <d2adventure_render/prepared_adventure_map.hpp>
#include <d2engine/app/adventure_interaction_mask.hpp>
#include <d2engine/app/adventure_screen_input.hpp>
#include <d2engine/assets/render_graph_asset_collector.hpp>
#include <d2runtime/AdventureActorAnimationResolver.hpp>
#include <d2runtime/AdventureIsoDirection.hpp>
#include <d2runtime/AdventureStackPresentationResolver.hpp>
#include <d2runtime/AdventureWorldState.hpp>

#include <memory>
#include <optional>
#include <string>

using namespace d2runtime;
using namespace d2engine::adventure_render;
using namespace d2engine;

// ── AdventureIsoDirection helpers ───────────────────────────────────────

TEST(AdventureIsoDirection, DirectionIndexD0ToD7) {
    EXPECT_EQ(direction_index(AdventureIsoDirection::D0), 0);
    EXPECT_EQ(direction_index(AdventureIsoDirection::D1), 1);
    EXPECT_EQ(direction_index(AdventureIsoDirection::D2), 2);
    EXPECT_EQ(direction_index(AdventureIsoDirection::D3), 3);
    EXPECT_EQ(direction_index(AdventureIsoDirection::D4), 4);
    EXPECT_EQ(direction_index(AdventureIsoDirection::D5), 5);
    EXPECT_EQ(direction_index(AdventureIsoDirection::D6), 6);
    EXPECT_EQ(direction_index(AdventureIsoDirection::D7), 7);
}

TEST(AdventureIsoDirection, FromIndexRoundTrip) {
    for (int i = 0; i < 8; ++i) {
        EXPECT_EQ(direction_index(direction_from_index(i)), i);
    }
}

TEST(AdventureIsoDirection, FromIndexOutOfRangeThrows) {
    EXPECT_THROW(direction_from_index(-1), std::out_of_range);
    EXPECT_THROW(direction_from_index(8), std::out_of_range);
}

// ── AdventureActorAnimationResolver — full directional identity ─────────

TEST(AdventureActorAnimationResolver, UnitIdleAllDirections) {
    AdventureActorAnimationResolver resolver;
    AdventureActorPresentation      pres{.kind = AdventureActorPresentationKind::Unit};

    for (int i = 0; i < 8; ++i) {
        const auto dir = direction_from_index(i);
        auto       id =
            resolver.resolve(pres, AdventureActorAnimationRole::Idle,
                             AdventureActorAnimationLayer::Main, "G000UU0001", "G000RR0000", dir);
        ASSERT_TRUE(id.has_value()) << "missing STOP" << i;
        EXPECT_EQ(id->container_path, "Imgs/Isounit.ff");
        EXPECT_EQ(id->logical_animation_name, "G000UU0001STOP" + std::to_string(i));
    }
}

TEST(AdventureActorAnimationResolver, UnitMoveAllDirections) {
    AdventureActorAnimationResolver resolver;
    AdventureActorPresentation      pres{.kind = AdventureActorPresentationKind::Unit};

    for (int i = 0; i < 8; ++i) {
        const auto dir = direction_from_index(i);
        auto       id =
            resolver.resolve(pres, AdventureActorAnimationRole::Move,
                             AdventureActorAnimationLayer::Main, "G000UU0001", "G000RR0000", dir);
        ASSERT_TRUE(id.has_value()) << "missing MOVE" << i;
        EXPECT_EQ(id->container_path, "Imgs/Isounit.ff");
        EXPECT_EQ(id->logical_animation_name, "G000UU0001MOVE" + std::to_string(i));
    }
}

TEST(AdventureActorAnimationResolver, BoatIdleAllDirections) {
    AdventureActorAnimationResolver resolver;
    AdventureActorPresentation      pres{.kind = AdventureActorPresentationKind::Boat};

    for (int i = 0; i < 8; ++i) {
        const auto dir = direction_from_index(i);
        auto       id =
            resolver.resolve(pres, AdventureActorAnimationRole::Idle,
                             AdventureActorAnimationLayer::Main, "G000UU0001", "G000RR0000", dir);
        ASSERT_TRUE(id.has_value()) << "missing BOAT" << i;
        EXPECT_EQ(id->logical_animation_name, "G000RR0000BOAT" + std::to_string(i));
    }
}

TEST(AdventureActorAnimationResolver, BoatMoveAllDirections) {
    AdventureActorAnimationResolver resolver;
    AdventureActorPresentation      pres{.kind = AdventureActorPresentationKind::Boat};

    for (int i = 0; i < 8; ++i) {
        const auto dir = direction_from_index(i);
        auto       id =
            resolver.resolve(pres, AdventureActorAnimationRole::Move,
                             AdventureActorAnimationLayer::Main, "G000UU0001", "G000RR0002", dir);
        ASSERT_TRUE(id.has_value()) << "missing BTMV" << i;
        EXPECT_EQ(id->logical_animation_name, "G000RR0002BTMV" + std::to_string(i));
    }
}

// ── InteractionMask builder (from RGBA pixels) ──────────────────────────

TEST(AdventureFacing, BuildInteractionMaskFromRgba) {
    const int         w = 4;
    const int         h = 2;
    d2res::RgbaBuffer rgba;
    rgba.width = w;
    rgba.height = h;
    rgba.rgba.resize(static_cast<std::size_t>(w * h * 4), 0);

    // Pixel (0,0): opaque (alpha=255)
    rgba.rgba[3] = 255;
    // Pixel (3,1): opaque (alpha=128)
    rgba.rgba[static_cast<std::size_t>(1 * w * 4 + 3 * 4 + 3)] = 128;

    auto mask = d2engine::build_interaction_mask(rgba);

    EXPECT_EQ(mask->width, w);
    EXPECT_EQ(mask->height, h);
    EXPECT_TRUE(mask->opaque(0, 0));
    EXPECT_TRUE(mask->opaque(3, 1));
    EXPECT_FALSE(mask->opaque(1, 0));
    EXPECT_FALSE(mask->opaque(0, 1));
    EXPECT_FALSE(mask->opaque(-1, 0));
    EXPECT_FALSE(mask->opaque(0, -1));
    EXPECT_FALSE(mask->opaque(w, 0));
    EXPECT_FALSE(mask->opaque(0, h));
}

// ── ActorAnimationResolver produces distinct identities ─────────────────

TEST(AdventureFacing, DistinctIdentityPerDirection) {
    AdventureActorAnimationResolver resolver;
    AdventureActorPresentation      pres{.kind = AdventureActorPresentationKind::Unit};

    std::string names[8];
    for (int i = 0; i < 8; ++i) {
        auto id = resolver.resolve(pres, AdventureActorAnimationRole::Idle,
                                   AdventureActorAnimationLayer::Main, "G000UU0050", "G000RR0000",
                                   direction_from_index(i));
        ASSERT_TRUE(id.has_value());
        names[i] = id->logical_animation_name;
    }

    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 8; ++j) {
            if (i != j)
                EXPECT_NE(names[i], names[j]) << "D" << i << " == D" << j;
        }
    }
}

// ── No manual facing controls via input ─────────────────────────────────

TEST(AdventureScreenInput, RProducesNoFacingAction) {
    auto action =
        d2engine::AdventureScreenInputHandler::handle(d2engine::KeyPressed{d2engine::Key::R});
    EXPECT_FALSE(action.has_value());
}

TEST(AdventureScreenInput, ShiftRProducesNoFacingAction) {
    auto action = d2engine::AdventureScreenInputHandler::handle(
        d2engine::KeyPressed{d2engine::Key::R, false, d2engine::KeyModifier::Shift});
    EXPECT_FALSE(action.has_value());
}

TEST(AdventureScreenInput, CtrlRProducesNoFacingAction) {
    auto action = d2engine::AdventureScreenInputHandler::handle(
        d2engine::KeyPressed{d2engine::Key::R, false, d2engine::KeyModifier::Ctrl});
    EXPECT_FALSE(action.has_value());
}

// ── Current-direction-only preload regression ───────────────────────────

TEST(AdventureFacing, CurrentDirectionPreloadExcludesOtherDirections) {
    const AdventureMapGeometry geo = AdventureMapGeometry::from_source(10, 10);

    d2runtime::AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;

    const std::string                unit_id = "G000UU0001";
    d2runtime::AdventureUnitInstance unit;
    unit.id = unit_id;
    unit.type_id = unit_id;
    world.units.push_back(unit);

    auto resolver =
        [&](const d2runtime::AdventureStack&        stack,
            const d2runtime::AdventureUnitInstance& leader) -> std::optional<AdventureActorVisual> {
        const auto suffix = "STOP" + std::to_string(direction_index(stack.facing));
        return AdventureActorVisual{
            .resolved_owner_id = "UNKNOWN",
            .body = {.container_path = "Imgs/Isounit.ff",
                     .logical_animation_name = std::string(leader.type_id) + suffix,
                     .frames = {{suffix, 80, 90}},
                     .native_canvas_w = 80,
                     .native_canvas_h = 90,
                     .canvas_foot_x = 40,
                     .canvas_foot_y = 80},
            .shadow = std::nullopt};
    };

    d2runtime::AdventureStack stack;
    stack.id = "S1";
    stack.position.x = 1;
    stack.position.y = 1;
    stack.leader_id = unit_id;
    stack.inside = "";
    stack.facing = d2runtime::AdventureIsoDirection::D3;
    world.stacks.push_back(stack);

    AdventureMapPreparer preparer(geo);
    preparer.add_contributor(make_stack_actor_contributor(resolver));
    auto result = preparer.prepare(world);

    ASSERT_EQ(result.graph.world.size(), 1u);
    const auto& prim = result.graph.world[0];
    ASSERT_FALSE(prim.animation.has_value());
    EXPECT_EQ(prim.record_name, "STOP3");

    // The generic asset collector must find the current STOP3 asset
    auto asset_keys = collect_adventure_render_asset_keys(result.graph);
    bool found_stop3 = false;
    for (const auto& key : asset_keys) {
        if (key.image_name.find("STOP3") != std::string::npos)
            found_stop3 = true;
        // No STOP0, STOP1, etc. should appear via any facing-specific path
        EXPECT_TRUE(key.image_name.find("STOP") == std::string::npos || key.image_name == "STOP3")
            << "unexpected direction asset: " << key.image_name;
    }
    EXPECT_TRUE(found_stop3);
}

// ── Production unit contributor with different initial facings ──────────

TEST(AdventureFacingContributor, DifferentInitialFacingsProduceCorrectStopDirection) {
    const AdventureMapGeometry geo = AdventureMapGeometry::from_source(10, 10);

    d2runtime::AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;

    const std::string                unit_id = "G000UU0001";
    d2runtime::AdventureUnitInstance unit;
    unit.id = unit_id;
    unit.type_id = unit_id;
    world.units.push_back(unit);

    auto resolver =
        [&](const d2runtime::AdventureStack&        stack,
            const d2runtime::AdventureUnitInstance& leader) -> std::optional<AdventureActorVisual> {
        const auto suffix = "STOP" + std::to_string(direction_index(stack.facing));
        return AdventureActorVisual{
            .resolved_owner_id = "UNKNOWN",
            .body = {.container_path = "Imgs/Isounit.ff",
                     .logical_animation_name = std::string(leader.type_id) + suffix,
                     .frames = {{suffix, 80, 90}},
                     .native_canvas_w = 80,
                     .native_canvas_h = 90,
                     .canvas_foot_x = 40,
                     .canvas_foot_y = 80},
            .shadow = std::nullopt};
    };

    auto add_stack = [&](const std::string& id, d2runtime::AdventureIsoDirection facing, int px,
                         int py) {
        d2runtime::AdventureStack stack;
        stack.id = id;
        stack.position.x = px;
        stack.position.y = py;
        stack.leader_id = unit_id;
        stack.inside = "";
        stack.facing = facing;
        world.stacks.push_back(stack);
    };

    add_stack("SA", d2runtime::AdventureIsoDirection::D0, 1, 1);
    add_stack("SB", d2runtime::AdventureIsoDirection::D3, 2, 2);
    add_stack("SC", d2runtime::AdventureIsoDirection::D7, 3, 3);

    AdventureMapPreparer preparer(geo);
    preparer.add_contributor(make_stack_actor_contributor(resolver));
    auto result = preparer.prepare(world);

    ASSERT_EQ(result.graph.world.size(), 3u);

    for (const auto& prim : result.graph.world) {
        ASSERT_FALSE(prim.animation.has_value());
        if (prim.debug_label.find("SA") != std::string::npos) {
            EXPECT_EQ(prim.record_name, "STOP0");
        } else if (prim.debug_label.find("SB") != std::string::npos) {
            EXPECT_EQ(prim.record_name, "STOP3");
        } else if (prim.debug_label.find("SC") != std::string::npos) {
            EXPECT_EQ(prim.record_name, "STOP7");
        }
    }
}
