#include <gtest/gtest.h>

#include <d2adventure_render/adventure_render_types.hpp>
#include <d2adventure_render/map_geometry.hpp>
#include <d2adventure_render/map_preparer.hpp>
#include <d2adventure_render/prepared_adventure_map.hpp>
#include <d2engine/animation/animation_player.hpp>
#include <d2engine/app/adventure_animation_helpers.hpp>
#include <d2engine/app/adventure_interaction_mask.hpp>
#include <d2engine/app/adventure_pick_index.hpp>
#include <d2engine/assets/game_data_registry.hpp>
#include <d2engine/assets/adventure_stack_actor_request_resolver.hpp>
#include <d2engine/assets/iso_actor_visual_resolver.hpp>
#include <d2engine/assets/sprite_animation_catalog.hpp>
#include <d2engine/assets/image_asset_key.hpp>
#include <d2engine/assets/render_graph_asset_collector.hpp>
#include <d2runtime/AdventureIsoDirection.hpp>
#include <d2runtime/AdventureWorldState.hpp>

#include <d2res/rgba_buffer.hpp>

#include "../test_dbf_builder.hpp"
#include "../test_helpers.hpp"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace d2runtime;
using namespace d2engine;

namespace ar = d2engine::adventure_render;

static const auto kAnimGeo = ar::AdventureMapGeometry::from_source(10, 10);

// ======================================================================
// Fake ISpriteAnimationCatalog for boat visual tests
// ======================================================================

namespace {

class FakeSpriteCatalog final : public ISpriteAnimationCatalog {
public:
    std::unordered_map<std::string, AnimationSequence>   animations;
    std::unordered_map<std::string, AnimationSpriteMeta> metas;

    std::vector<std::string> animations_in(std::string_view container) const override {
        (void)container;
        std::vector<std::string> names;
        for (const auto& [key, seq] : animations) {
            (void)seq;
            names.push_back(key);
        }
        return names;
    }

    AnimationSequence animation_sequence(std::string_view container,
                                         std::string_view anim_name) const override {
        (void)container;
        auto it = animations.find(std::string(anim_name));
        if (it != animations.end())
            return it->second;
        return {};
    }

    AnimationSpriteMeta sprite_metadata(std::string_view container,
                                        std::string_view sprite_name) const override {
        (void)container;
        auto it = metas.find(std::string(sprite_name));
        if (it != metas.end()) {
            if (it->second.has_visible_pieces && !it->second.content_bounds.valid()) {
                auto meta = it->second;
                meta.content_bounds = {0, 0, meta.canvas_width, meta.canvas_height};
                return meta;
            }
            return it->second;
        }
        return {};
    }
};

std::optional<ar::AdventureActorVisual>
iso_to_adventure_visual(const std::optional<IsoActorVisual>& iso) {
    if (!iso.has_value())
        return std::nullopt;
    ar::AdventureActorVisual result;
    result.presentation_kind = iso->presentation_kind;
    result.resolved_owner_id = iso->resolved_owner_id;
    result.body.container_path = iso->body.container_path;
    result.body.logical_animation_name = iso->body.animation_name;
    for (const auto& f : iso->body.frames) {
        result.body.frames.push_back(
            {f.record_name, f.canvas_width, f.canvas_height, f.content_bounds});
    }
    result.body.native_canvas_w = iso->body.native_canvas_w;
    result.body.native_canvas_h = iso->body.native_canvas_h;
    result.body.canvas_foot_x = iso->body.canvas_foot_x;
    result.body.canvas_foot_y = iso->body.canvas_foot_y;
    result.body.content_bounds = iso->body.content_bounds;
    if (iso->shadow.has_value()) {
        ar::AdventureActorVisualLayer sl;
        sl.container_path = iso->shadow->container_path;
        sl.logical_animation_name = iso->shadow->animation_name;
        for (const auto& f : iso->shadow->frames) {
            sl.frames.push_back({f.record_name, f.canvas_width, f.canvas_height, f.content_bounds});
        }
        sl.native_canvas_w = iso->shadow->native_canvas_w;
        sl.native_canvas_h = iso->shadow->native_canvas_h;
        sl.canvas_foot_x = iso->shadow->canvas_foot_x;
        sl.canvas_foot_y = iso->shadow->canvas_foot_y;
        sl.content_bounds = iso->shadow->content_bounds;
        result.shadow = std::move(sl);
    }
    return result;
}

} // namespace

// ======================================================================
// SECTION 1: Visual resolution tests (IsoActorVisualResolver)
// ======================================================================

TEST(BoatVisual, VisiblePair) {
    FakeSpriteCatalog catalog;
    catalog.animations["G000RR0003BOAT3"] = {
        .name = "G000RR0003BOAT3",
        .container_path = "Imgs/Isounit.ff",
        .frames =
            {
                {.image_name = "B0", .index = 0, .duration_ms = 100},
                {.image_name = "B1", .index = 1, .duration_ms = 100},
                {.image_name = "B2", .index = 2, .duration_ms = 100},
            },
        .is_looping = false,
        .native_canvas_w = 320,
        .native_canvas_h = 320,
        .canvas_foot_x = 40,
        .canvas_foot_y = 80,
    };
    catalog.animations["G000RR0003SBOA3"] = {
        .name = "G000RR0003SBOA3",
        .container_path = "Imgs/Isounit.ff",
        .frames =
            {
                {.image_name = "S0", .index = 0, .duration_ms = 100},
                {.image_name = "S1", .index = 1, .duration_ms = 100},
                {.image_name = "S2", .index = 2, .duration_ms = 100},
            },
        .is_looping = false,
        .native_canvas_w = 200,
        .native_canvas_h = 150,
        .canvas_foot_x = 30,
        .canvas_foot_y = 60,
    };
    catalog.metas["B0"] = {80, 90, true};
    catalog.metas["B1"] = {80, 90, true};
    catalog.metas["B2"] = {80, 90, true};
    catalog.metas["S0"] = {60, 50, true};
    catalog.metas["S1"] = {60, 50, true};
    catalog.metas["S2"] = {60, 50, true};

    TempDir tmp("d2_boat_visible_pair");
    for (const auto& name :
         {"Tglobal.dbf", "Gattacks.dbf", "Gunits.dbf", "Gupgrade.dbf", "Graces.dbf"}) {
        std::ofstream(tmp.path() / name).put(0x1a);
    }
    GameDataRegistry game_data(tmp.path());

    IsoActorVisualResolver           resolver(catalog, game_data);
    AdventureStackActorVisualRequest req;
    req.presentation = {AdventureActorPresentationKind::Boat};
    req.race_id = "G000RR0003";
    req.direction = AdventureIsoDirection::D3;
    const auto visual = resolver.resolve(req);
    ASSERT_TRUE(visual.has_value());

    EXPECT_EQ(visual->presentation_kind, AdventureActorPresentationKind::Boat);
    EXPECT_EQ(visual->resolved_owner_id, "G000RR0003");
    EXPECT_EQ(visual->shadow_presence, ExactLayerPresence::Visible);

    ASSERT_EQ(visual->body.frames.size(), 3u);
    EXPECT_EQ(visual->body.frames[0].record_name, "B0");
    EXPECT_EQ(visual->body.frames[1].record_name, "B1");
    EXPECT_EQ(visual->body.frames[2].record_name, "B2");
    EXPECT_EQ(visual->body.animation_name, "G000RR0003BOAT3");

    ASSERT_TRUE(visual->shadow.has_value());
    ASSERT_EQ(visual->shadow->frames.size(), 3u);
    EXPECT_EQ(visual->shadow->frames[0].record_name, "S0");
    EXPECT_EQ(visual->shadow->frames[1].record_name, "S1");
    EXPECT_EQ(visual->shadow->frames[2].record_name, "S2");
    EXPECT_EQ(visual->shadow->animation_name, "G000RR0003SBOA3");

    EXPECT_EQ(visual->body.native_canvas_w, 320);
    EXPECT_EQ(visual->body.native_canvas_h, 320);
    EXPECT_EQ(visual->body.canvas_foot_x, 40);
    EXPECT_EQ(visual->body.canvas_foot_y, 80);
    EXPECT_EQ(visual->shadow->native_canvas_w, 200);
    EXPECT_EQ(visual->shadow->native_canvas_h, 150);
    EXPECT_EQ(visual->shadow->canvas_foot_x, 30);
    EXPECT_EQ(visual->shadow->canvas_foot_y, 60);
}

TEST(BoatVisual, MissingShadow) {
    FakeSpriteCatalog catalog;
    catalog.animations["G000RR0003BOAT5"] = {
        .name = "G000RR0003BOAT5",
        .container_path = "Imgs/Isounit.ff",
        .frames = {{.image_name = "B0", .index = 0, .duration_ms = 100}},
        .is_looping = false,
        .native_canvas_w = 320,
        .native_canvas_h = 320,
        .canvas_foot_x = 40,
        .canvas_foot_y = 80,
    };
    catalog.metas["B0"] = {80, 90, true};

    TempDir tmp("d2_boat_missing_shadow");
    for (const auto& name :
         {"Tglobal.dbf", "Gattacks.dbf", "Gunits.dbf", "Gupgrade.dbf", "Graces.dbf"}) {
        std::ofstream(tmp.path() / name).put(0x1a);
    }
    GameDataRegistry game_data(tmp.path());

    IsoActorVisualResolver           resolver(catalog, game_data);
    AdventureStackActorVisualRequest req;
    req.presentation = {AdventureActorPresentationKind::Boat};
    req.race_id = "G000RR0003";
    req.direction = AdventureIsoDirection::D5;
    const auto visual = resolver.resolve(req);
    ASSERT_TRUE(visual.has_value());

    EXPECT_EQ(visual->body.frames.size(), 1u);
    EXPECT_FALSE(visual->shadow.has_value());
    EXPECT_EQ(visual->shadow_presence, ExactLayerPresence::Missing);
}

TEST(BoatVisual, AuthoredEmptyShadow) {
    FakeSpriteCatalog catalog;
    catalog.animations["G000RR0003BOAT2"] = {
        .name = "G000RR0003BOAT2",
        .container_path = "Imgs/Isounit.ff",
        .frames = {{.image_name = "B_VISIBLE", .index = 0, .duration_ms = 100}},
        .is_looping = false,
        .native_canvas_w = 320,
        .native_canvas_h = 320,
        .canvas_foot_x = 40,
        .canvas_foot_y = 80,
    };
    catalog.animations["G000RR0003SBOA2"] = {
        .name = "G000RR0003SBOA2",
        .container_path = "Imgs/Isounit.ff",
        .frames = {{.image_name = "EMPTY_FRAME", .index = 0, .duration_ms = 100}},
        .is_looping = false,
        .native_canvas_w = 200,
        .native_canvas_h = 150,
        .canvas_foot_x = 30,
        .canvas_foot_y = 60,
    };
    catalog.metas["B_VISIBLE"] = {80, 90, true};
    catalog.metas["EMPTY_FRAME"] = {60, 50, false};

    TempDir tmp("d2_boat_authored_empty");
    for (const auto& name :
         {"Tglobal.dbf", "Gattacks.dbf", "Gunits.dbf", "Gupgrade.dbf", "Graces.dbf"}) {
        std::ofstream(tmp.path() / name).put(0x1a);
    }
    GameDataRegistry game_data(tmp.path());

    IsoActorVisualResolver           resolver(catalog, game_data);
    AdventureStackActorVisualRequest req;
    req.presentation = {AdventureActorPresentationKind::Boat};
    req.race_id = "G000RR0003";
    req.direction = AdventureIsoDirection::D2;
    const auto visual = resolver.resolve(req);
    ASSERT_TRUE(visual.has_value());

    EXPECT_TRUE(visual->body.frames.size() == 1u);
    EXPECT_FALSE(visual->shadow.has_value());
    EXPECT_EQ(visual->shadow_presence, ExactLayerPresence::AuthoredEmpty);
}

TEST(BoatVisual, EmptyShadowSequenceThrows) {
    FakeSpriteCatalog catalog;
    catalog.animations["G000RR0003BOAT5"] = {
        .name = "G000RR0003BOAT5",
        .container_path = "Imgs/Isounit.ff",
        .frames = {{.image_name = "B0", .index = 0, .duration_ms = 100}},
        .is_looping = false,
        .native_canvas_w = 320,
        .native_canvas_h = 320,
        .canvas_foot_x = 40,
        .canvas_foot_y = 80,
    };
    catalog.animations["G000RR0003SBOA5"] = {
        .name = "G000RR0003SBOA5",
        .container_path = "Imgs/Isounit.ff",
        .frames = {},
        .is_looping = false,
        .native_canvas_w = 200,
        .native_canvas_h = 150,
        .canvas_foot_x = 30,
        .canvas_foot_y = 60,
    };
    catalog.metas["B0"] = {80, 90, true};

    TempDir tmp("d2_boat_empty_shadow_seq");
    for (const auto& name :
         {"Tglobal.dbf", "Gattacks.dbf", "Gunits.dbf", "Gupgrade.dbf", "Graces.dbf"}) {
        std::ofstream(tmp.path() / name).put(0x1a);
    }
    GameDataRegistry game_data(tmp.path());

    IsoActorVisualResolver           resolver(catalog, game_data);
    AdventureStackActorVisualRequest req;
    req.presentation = {AdventureActorPresentationKind::Boat};
    req.race_id = "G000RR0003";
    req.direction = AdventureIsoDirection::D5;
    EXPECT_THROW(
        {
            try {
                static_cast<void>(resolver.resolve(req));
            } catch (const std::runtime_error& e) {
                std::string m(e.what());
                EXPECT_TRUE(m.find("malformed_shadow_sequence") != std::string::npos);
                EXPECT_TRUE(m.find("G000RR0003SBOA5") != std::string::npos);
                EXPECT_TRUE(m.find("empty_sequence") != std::string::npos);
                throw;
            }
        },
        std::runtime_error);
}

TEST(BoatVisual, EmptyBodySequenceThrows) {
    FakeSpriteCatalog catalog;
    catalog.animations["G000RR0003BOAT3"] = {
        .name = "G000RR0003BOAT3",
        .container_path = "Imgs/Isounit.ff",
        .frames = {},
        .is_looping = false,
        .native_canvas_w = 320,
        .native_canvas_h = 320,
        .canvas_foot_x = 40,
        .canvas_foot_y = 80,
    };

    TempDir tmp("d2_boat_empty_body_seq");
    for (const auto& name :
         {"Tglobal.dbf", "Gattacks.dbf", "Gunits.dbf", "Gupgrade.dbf", "Graces.dbf"}) {
        std::ofstream(tmp.path() / name).put(0x1a);
    }
    GameDataRegistry game_data(tmp.path());

    IsoActorVisualResolver           resolver(catalog, game_data);
    AdventureStackActorVisualRequest req;
    req.presentation = {AdventureActorPresentationKind::Boat};
    req.race_id = "G000RR0003";
    req.direction = AdventureIsoDirection::D3;
    EXPECT_THROW(
        {
            try {
                static_cast<void>(resolver.resolve(req));
            } catch (const std::runtime_error& e) {
                std::string m(e.what());
                EXPECT_TRUE(m.find("malformed_body_sequence") != std::string::npos);
                EXPECT_TRUE(m.find("G000RR0003BOAT3") != std::string::npos);
                EXPECT_TRUE(m.find("empty_sequence") != std::string::npos);
                throw;
            }
        },
        std::runtime_error);
}

TEST(BoatVisual, MissingBoatBodyThrows) {
    FakeSpriteCatalog catalog;

    TempDir tmp("d2_boat_missing_body");
    for (const auto& name :
         {"Tglobal.dbf", "Gattacks.dbf", "Gunits.dbf", "Gupgrade.dbf", "Graces.dbf"}) {
        std::ofstream(tmp.path() / name).put(0x1a);
    }
    GameDataRegistry game_data(tmp.path());

    IsoActorVisualResolver           resolver(catalog, game_data);
    AdventureStackActorVisualRequest req;
    req.presentation = {AdventureActorPresentationKind::Boat};
    req.race_id = "G000RR0003";
    req.direction = AdventureIsoDirection::D3;
    EXPECT_THROW(
        {
            try {
                static_cast<void>(resolver.resolve(req));
            } catch (const std::runtime_error& e) {
                std::string m(e.what());
                EXPECT_TRUE(m.find("missing_boat_body") != std::string::npos);
                EXPECT_TRUE(m.find("G000RR0003") != std::string::npos)
                    << "diagnostic must contain race_id: " << m;
                EXPECT_TRUE(m.find("direction=3") != std::string::npos)
                    << "diagnostic must contain direction: " << m;
                EXPECT_TRUE(m.find("expected=G000RR0003BOAT3") != std::string::npos)
                    << "diagnostic must contain expected identity: " << m;
                throw;
            }
        },
        std::runtime_error);
}

// ======================================================================
// SECTION 2: Contributor tests
// ======================================================================

TEST(BoatVisual, AnimatedBodyShadowPair) {
    FakeSpriteCatalog catalog;
    catalog.animations["G000RR0003BOAT3"] = {
        .name = "G000RR0003BOAT3",
        .container_path = "Imgs/Isounit.ff",
        .frames =
            {
                {.image_name = "B0.PNG", .index = 0, .duration_ms = 100},
                {.image_name = "B1.PNG", .index = 1, .duration_ms = 100},
                {.image_name = "B2.PNG", .index = 2, .duration_ms = 100},
            },
        .is_looping = true,
        .native_canvas_w = 320,
        .native_canvas_h = 320,
        .canvas_foot_x = 40,
        .canvas_foot_y = 80,
    };
    catalog.animations["G000RR0003SBOA3"] = {
        .name = "G000RR0003SBOA3",
        .container_path = "Imgs/Isounit.ff",
        .frames =
            {
                {.image_name = "S0.PNG", .index = 0, .duration_ms = 100},
                {.image_name = "S1.PNG", .index = 1, .duration_ms = 100},
                {.image_name = "S2.PNG", .index = 2, .duration_ms = 100},
            },
        .is_looping = true,
        .native_canvas_w = 200,
        .native_canvas_h = 150,
        .canvas_foot_x = 30,
        .canvas_foot_y = 60,
    };
    catalog.metas["B0.PNG"] = {80, 90, true};
    catalog.metas["B1.PNG"] = {80, 90, true};
    catalog.metas["B2.PNG"] = {80, 90, true};
    catalog.metas["S0.PNG"] = {60, 50, true};
    catalog.metas["S1.PNG"] = {60, 50, true};
    catalog.metas["S2.PNG"] = {60, 50, true};

    TempDir tmp("d2_boat_anim_pair");
    for (const auto& name :
         {"Tglobal.dbf", "Gattacks.dbf", "Gunits.dbf", "Gupgrade.dbf", "Graces.dbf"}) {
        std::ofstream(tmp.path() / name).put(0x1a);
    }
    GameDataRegistry game_data(tmp.path());

    IsoActorVisualResolver iso_resolver(catalog, game_data);

    auto visual_resolver = [&iso_resolver](const d2runtime::AdventureStack&        stack,
                                           const d2runtime::AdventureUnitInstance& leader)
        -> std::optional<ar::AdventureActorVisual> {
        (void)leader;
        AdventureStackActorVisualRequest req;
        req.presentation = {AdventureActorPresentationKind::Boat};
        req.race_id = "G000RR0003";
        req.direction = stack.facing;
        return iso_to_adventure_visual(iso_resolver.resolve(req));
    };

    AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    d2runtime::AdventureUnitInstance un;
    un.id = "u";
    un.type_id = "G000UU0001";
    world.units.push_back(un);
    AdventureStack stack;
    stack.id = "S1";
    stack.position.x = 3;
    stack.position.y = 4;
    stack.leader_id = "u";
    stack.facing = AdventureIsoDirection::D3;
    world.stacks.push_back(stack);

    ar::AdventureMapPreparer preparer(kAnimGeo);
    preparer.add_contributor(ar::make_stack_actor_contributor(visual_resolver));
    auto result = preparer.prepare(world);

    ASSERT_EQ(result.graph.world.size(), 2u);
    ASSERT_EQ(result.pick_entries.size(), 1u);

    const ar::PreparedAdventureRenderPrimitive* body = nullptr;
    const ar::PreparedAdventureRenderPrimitive* shadow = nullptr;
    for (const auto& prim : result.graph.world) {
        if (prim.stable_id == ar::stable_render_id("Stack:S1"))
            body = &prim;
        if (prim.stable_id == ar::stable_render_id("StackShadow:S1"))
            shadow = &prim;
    }
    ASSERT_NE(body, nullptr);
    ASSERT_NE(shadow, nullptr);

    EXPECT_EQ(body->stable_id, ar::stable_render_id("Stack:S1"));
    EXPECT_EQ(body->level, ar::WorldRenderLevel::Actor);
    EXPECT_FLOAT_EQ(body->alpha, 1.0f);

    EXPECT_EQ(shadow->stable_id, ar::stable_render_id("StackShadow:S1"));
    EXPECT_EQ(shadow->level, ar::WorldRenderLevel::ActorUnderlay);
    EXPECT_FLOAT_EQ(shadow->alpha, 0.5f);

    EXPECT_EQ(body->depth_anchor.x, shadow->depth_anchor.x);
    EXPECT_EQ(body->depth_anchor.y, shadow->depth_anchor.y);

    EXPECT_NE(body->draw_origin.x, shadow->draw_origin.x);
    EXPECT_NE(body->draw_origin.y, shadow->draw_origin.y);

    ASSERT_TRUE(body->animation.has_value());
    ASSERT_TRUE(shadow->animation.has_value());

    EXPECT_TRUE(shadow->animation_sync_source_id.has_value());
    EXPECT_EQ(*shadow->animation_sync_source_id, body->stable_id);

    EXPECT_EQ(body->animation->frames.size(), 3u);
    EXPECT_EQ(shadow->animation->frames.size(), 3u);
    EXPECT_EQ(body->animation->frames[0].record_name, "B0.PNG");
    EXPECT_EQ(body->animation->frames[1].record_name, "B1.PNG");
    EXPECT_EQ(body->animation->frames[2].record_name, "B2.PNG");
    EXPECT_EQ(shadow->animation->frames[0].record_name, "S0.PNG");
    EXPECT_EQ(shadow->animation->frames[1].record_name, "S1.PNG");
    EXPECT_EQ(shadow->animation->frames[2].record_name, "S2.PNG");

    auto players = build_adventure_animation_players(result.graph);
    ASSERT_EQ(players.size(), 1u);
    ASSERT_TRUE(players.contains(body->stable_id));
}

TEST(BoatVisual, BodyOnlyWithoutShadow) {
    FakeSpriteCatalog catalog;
    catalog.animations["G000RR0003BOAT3"] = {
        .name = "G000RR0003BOAT3",
        .container_path = "Imgs/Isounit.ff",
        .frames =
            {
                {.image_name = "B0.PNG", .index = 0, .duration_ms = 100},
                {.image_name = "B1.PNG", .index = 1, .duration_ms = 100},
                {.image_name = "B2.PNG", .index = 2, .duration_ms = 100},
            },
        .is_looping = true,
        .native_canvas_w = 320,
        .native_canvas_h = 320,
        .canvas_foot_x = 40,
        .canvas_foot_y = 80,
    };
    catalog.metas["B0.PNG"] = {80, 90, true};
    catalog.metas["B1.PNG"] = {80, 90, true};
    catalog.metas["B2.PNG"] = {80, 90, true};

    TempDir tmp("d2_boat_body_only");
    for (const auto& name :
         {"Tglobal.dbf", "Gattacks.dbf", "Gunits.dbf", "Gupgrade.dbf", "Graces.dbf"}) {
        std::ofstream(tmp.path() / name).put(0x1a);
    }
    GameDataRegistry game_data(tmp.path());

    IsoActorVisualResolver iso_resolver(catalog, game_data);

    auto visual_resolver = [&iso_resolver](const d2runtime::AdventureStack&        stack,
                                           const d2runtime::AdventureUnitInstance& leader)
        -> std::optional<ar::AdventureActorVisual> {
        (void)leader;
        AdventureStackActorVisualRequest req;
        req.presentation = {AdventureActorPresentationKind::Boat};
        req.race_id = "G000RR0003";
        req.direction = stack.facing;
        return iso_to_adventure_visual(iso_resolver.resolve(req));
    };

    AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    d2runtime::AdventureUnitInstance un;
    un.id = "u";
    un.type_id = "G000UU0001";
    world.units.push_back(un);
    AdventureStack stack;
    stack.id = "S1";
    stack.position.x = 3;
    stack.position.y = 4;
    stack.leader_id = "u";
    stack.facing = AdventureIsoDirection::D3;
    world.stacks.push_back(stack);

    ar::AdventureMapPreparer preparer(kAnimGeo);
    preparer.add_contributor(ar::make_stack_actor_contributor(visual_resolver));
    auto result = preparer.prepare(world);

    ASSERT_EQ(result.graph.world.size(), 1u);
    ASSERT_EQ(result.pick_entries.size(), 1u);
    EXPECT_EQ(result.graph.world[0].stable_id, ar::stable_render_id("Stack:S1"));

    auto players = build_adventure_animation_players(result.graph);
    ASSERT_EQ(players.size(), 1u);
}

// ======================================================================
// SECTION 3: Same-leader land/water cache regression
// ======================================================================

TEST(BoatVisual, SameLeaderDifferentTerrain) {
    FakeSpriteCatalog catalog;
    catalog.animations["G000UU0001STOP3"] = {
        .name = "G000UU0001STOP3",
        .container_path = "Imgs/Isounit.ff",
        .frames = {{.image_name = "STOP3_F0", .index = 0, .duration_ms = 100}},
        .is_looping = false,
        .native_canvas_w = 320,
        .native_canvas_h = 320,
        .canvas_foot_x = 40,
        .canvas_foot_y = 80,
    };
    catalog.animations["G000UU0001SSTO3"] = {
        .name = "G000UU0001SSTO3",
        .container_path = "Imgs/Isounit.ff",
        .frames = {{.image_name = "SSTO3_F0", .index = 0, .duration_ms = 100}},
        .is_looping = false,
        .native_canvas_w = 200,
        .native_canvas_h = 150,
        .canvas_foot_x = 30,
        .canvas_foot_y = 60,
    };
    catalog.animations["G000RR0003BOAT3"] = {
        .name = "G000RR0003BOAT3",
        .container_path = "Imgs/Isounit.ff",
        .frames =
            {
                {.image_name = "BOAT3_B0", .index = 0, .duration_ms = 100},
                {.image_name = "BOAT3_B1", .index = 1, .duration_ms = 100},
                {.image_name = "BOAT3_B2", .index = 2, .duration_ms = 100},
            },
        .is_looping = false,
        .native_canvas_w = 320,
        .native_canvas_h = 320,
        .canvas_foot_x = 40,
        .canvas_foot_y = 80,
    };
    catalog.animations["G000RR0003SBOA3"] = {
        .name = "G000RR0003SBOA3",
        .container_path = "Imgs/Isounit.ff",
        .frames =
            {
                {.image_name = "SBOA3_S0", .index = 0, .duration_ms = 100},
                {.image_name = "SBOA3_S1", .index = 1, .duration_ms = 100},
                {.image_name = "SBOA3_S2", .index = 2, .duration_ms = 100},
            },
        .is_looping = false,
        .native_canvas_w = 200,
        .native_canvas_h = 150,
        .canvas_foot_x = 30,
        .canvas_foot_y = 60,
    };
    catalog.metas["STOP3_F0"] = {80, 90, true};
    catalog.metas["SSTO3_F0"] = {60, 50, true};
    catalog.metas["BOAT3_B0"] = {80, 90, true};
    catalog.metas["BOAT3_B1"] = {80, 90, true};
    catalog.metas["BOAT3_B2"] = {80, 90, true};
    catalog.metas["SBOA3_S0"] = {60, 50, true};
    catalog.metas["SBOA3_S1"] = {60, 50, true};
    catalog.metas["SBOA3_S2"] = {60, 50, true};

    TempDir tmp("d2_same_leader_regression");
    {
        test_dbf::DbfBuilder gunits({
            {"UNIT_ID", 'C', 20},
            {"RACE_ID", 'C', 20},
            {"WATER_ONLY", 'C', 1},
        });
        gunits.add_record({"G000UU0001", "G000RR0003", ""});
        gunits.write(tmp.path() / "Gunits.dbf");
    }
    {
        test_dbf::DbfBuilder gmabi({
            {"UNIT_ID", 'C', 20},
            {"M_ABILITY", 'N', 3},
        });
        gmabi.write(tmp.path() / "GMabi.dbf");
    }
    {
        test_dbf::DbfBuilder races({
            {"RACE_ID", 'C', 20},
            {"NAME", 'C', 20},
        });
        races.add_record({"G000RR0003", "TestRace"});
        races.write(tmp.path() / "Graces.dbf");
    }
    for (const auto& name : {"Tglobal.dbf", "Gattacks.dbf", "Gupgrade.dbf"}) {
        std::ofstream(tmp.path() / name).put(0x1a);
    }
    GameDataRegistry game_data(tmp.path());

    AdventureStackActorRequestResolver req_resolver(game_data);
    IsoActorVisualResolver             resolver(catalog, game_data);

    // Shared world skeleton: same unit, stack, subrace
    AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.terrain.width = 10;
    world.terrain.height = 10;
    world.terrain.tiles.resize(100);

    AdventureUnitInstance unit;
    unit.id = "u";
    unit.type_id = "G000UU0001";
    world.units.push_back(unit);

    AdventureStack stack;
    stack.id = "S1";
    stack.position = {3, 4};
    stack.leader_id = "u";
    stack.subrace = "sr";
    stack.facing = AdventureIsoDirection::D3;
    world.stacks.push_back(stack);

    AdventureSubraceRef subrace;
    subrace.id = "sr";
    subrace.race_id = "G000RR0003";
    world.subraces.push_back(std::move(subrace));

    // ---- Land terrain → Unit visual (STOP/SSTO) ----
    for (auto& t : world.terrain.tiles)
        t.raw_value = 1; // land
    auto land_req = req_resolver.resolve(world, world.stacks[0]);
    EXPECT_EQ(land_req.presentation.kind, AdventureActorPresentationKind::Unit);
    auto land_vis = resolver.resolve(land_req);
    ASSERT_TRUE(land_vis.has_value());
    EXPECT_EQ(land_vis->body.animation_name, "G000UU0001STOP3");
    ASSERT_TRUE(land_vis->shadow.has_value());
    EXPECT_EQ(land_vis->shadow->animation_name, "G000UU0001SSTO3");
    EXPECT_TRUE(land_vis->body.animation_name.find("BOAT") == std::string::npos);
    EXPECT_EQ(land_vis->shadow_presence, ExactLayerPresence::Visible);

    // ---- Water terrain → Boat visual (BOAT/SBOA) ----
    for (auto& t : world.terrain.tiles)
        t.raw_value = 7; // water
    auto water_req = req_resolver.resolve(world, world.stacks[0]);
    EXPECT_EQ(water_req.presentation.kind, AdventureActorPresentationKind::Boat);
    EXPECT_EQ(water_req.race_id, "G000RR0003");
    auto water_vis = resolver.resolve(water_req);
    ASSERT_TRUE(water_vis.has_value());
    EXPECT_EQ(water_vis->body.animation_name, "G000RR0003BOAT3");
    ASSERT_TRUE(water_vis->shadow.has_value());
    EXPECT_EQ(water_vis->shadow->animation_name, "G000RR0003SBOA3");
    EXPECT_TRUE(water_vis->body.animation_name.find("STOP") == std::string::npos);
    EXPECT_EQ(water_vis->shadow_presence, ExactLayerPresence::Visible);
}

// ======================================================================
// SECTION 4: Preload and picking tests
// ======================================================================

TEST(BoatVisual, ExactPreloadAssets) {
    FakeSpriteCatalog catalog;
    catalog.animations["G000RR0003BOAT3"] = {
        .name = "G000RR0003BOAT3",
        .container_path = "Imgs/Isounit.ff",
        .frames =
            {
                {.image_name = "B0.PNG", .index = 0, .duration_ms = 100},
                {.image_name = "B1.PNG", .index = 1, .duration_ms = 100},
                {.image_name = "B2.PNG", .index = 2, .duration_ms = 100},
            },
        .is_looping = true,
        .native_canvas_w = 320,
        .native_canvas_h = 320,
        .canvas_foot_x = 40,
        .canvas_foot_y = 80,
    };
    catalog.animations["G000RR0003SBOA3"] = {
        .name = "G000RR0003SBOA3",
        .container_path = "Imgs/Isounit.ff",
        .frames =
            {
                {.image_name = "S0.PNG", .index = 0, .duration_ms = 100},
                {.image_name = "S1.PNG", .index = 1, .duration_ms = 100},
                {.image_name = "S2.PNG", .index = 2, .duration_ms = 100},
            },
        .is_looping = true,
        .native_canvas_w = 200,
        .native_canvas_h = 150,
        .canvas_foot_x = 30,
        .canvas_foot_y = 60,
    };
    catalog.metas["B0.PNG"] = {80, 90, true};
    catalog.metas["B1.PNG"] = {80, 90, true};
    catalog.metas["B2.PNG"] = {80, 90, true};
    catalog.metas["S0.PNG"] = {60, 50, true};
    catalog.metas["S1.PNG"] = {60, 50, true};
    catalog.metas["S2.PNG"] = {60, 50, true};

    TempDir tmp("d2_boat_preload");
    for (const auto& name :
         {"Tglobal.dbf", "Gattacks.dbf", "Gunits.dbf", "Gupgrade.dbf", "Graces.dbf"}) {
        std::ofstream(tmp.path() / name).put(0x1a);
    }
    GameDataRegistry game_data(tmp.path());

    IsoActorVisualResolver iso_resolver(catalog, game_data);

    // Resolve once, store by stack id
    AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    d2runtime::AdventureUnitInstance un;
    un.id = "u";
    un.type_id = "G000UU0001";
    world.units.push_back(un);
    AdventureStack stack;
    stack.id = "S1";
    stack.position.x = 3;
    stack.position.y = 4;
    stack.leader_id = "u";
    stack.facing = AdventureIsoDirection::D3;
    world.stacks.push_back(stack);

    AdventureStackActorVisualRequest req;
    req.presentation = {AdventureActorPresentationKind::Boat};
    req.race_id = "G000RR0003";
    req.direction = AdventureIsoDirection::D3;
    const auto iso_vis = iso_resolver.resolve(req);
    ASSERT_TRUE(iso_vis.has_value());
    ASSERT_TRUE(iso_vis->shadow.has_value());

    std::unordered_map<std::string, ar::AdventureActorVisual> visual_cache;
    visual_cache[stack.id] = *iso_to_adventure_visual(iso_vis);

    ar::AdventureMapPreparer preparer(kAnimGeo);
    preparer.add_contributor(ar::make_stack_actor_contributor(
        [&](const d2runtime::AdventureStack& s,
            const d2runtime::AdventureUnitInstance&) -> std::optional<ar::AdventureActorVisual> {
            auto it = visual_cache.find(s.id);
            return it != visual_cache.end() ? std::optional(it->second) : std::nullopt;
        }));
    auto result = preparer.prepare(world);

    // Derive expected key set from the resolved IsoActorVisual
    std::unordered_set<ImageAssetKey, std::hash<ImageAssetKey>> expected;
    for (const auto& f : iso_vis->body.frames) {
        expected.insert(make_world_composed_sprite_key("Imgs/Isounit.ff", f.record_name));
    }
    // Only visible SBOA contributes shadow keys
    if (iso_vis->shadow.has_value() && iso_vis->shadow_presence == ExactLayerPresence::Visible) {
        for (const auto& f : iso_vis->shadow->frames) {
            expected.insert(make_world_composed_sprite_key("Imgs/Isounit.ff", f.record_name));
        }
    }

    const auto actual_keys = collect_adventure_render_asset_keys(result.graph);
    std::unordered_set<ImageAssetKey, std::hash<ImageAssetKey>> actual_set(actual_keys.begin(),
                                                                           actual_keys.end());

    EXPECT_EQ(actual_set, expected)
        << "actual preload keys (size=" << actual_set.size()
        << ") must exactly match expected (size=" << expected.size() << ")";
    EXPECT_EQ(actual_set.size(), 6u) << "3 body + 3 visible shadow = 6 keys";

    for (const auto& key : actual_keys) {
        EXPECT_TRUE(key.image_name.find("STOP") == std::string::npos)
            << "unexpected STOP asset: " << key.image_name;
        EXPECT_TRUE(key.image_name.find("SSTO") == std::string::npos)
            << "unexpected SSTO asset: " << key.image_name;
        EXPECT_TRUE(key.image_name.find("BTMV") == std::string::npos)
            << "unexpected BTMV asset: " << key.image_name;
        EXPECT_TRUE(key.image_name.find("SBTM") == std::string::npos)
            << "unexpected SBTM asset: " << key.image_name;
        EXPECT_TRUE(key.image_name.find("BBTMV") == std::string::npos)
            << "unexpected BBTMV asset: " << key.image_name;
    }
}

TEST(BoatVisual, ShadowDoesNotContributeToPicking) {
    FakeSpriteCatalog catalog;
    catalog.animations["G000RR0003BOAT3"] = {
        .name = "G000RR0003BOAT3",
        .container_path = "Imgs/Isounit.ff",
        .frames =
            {
                {.image_name = "B0.PNG", .index = 0, .duration_ms = 100},
            },
        .is_looping = true,
        .native_canvas_w = 64,
        .native_canvas_h = 64,
        .canvas_foot_x = 32,
        .canvas_foot_y = 32,
    };
    catalog.animations["G000RR0003SBOA3"] = {
        .name = "G000RR0003SBOA3",
        .container_path = "Imgs/Isounit.ff",
        .frames =
            {
                {.image_name = "S0.PNG", .index = 0, .duration_ms = 100},
            },
        .is_looping = true,
        .native_canvas_w = 64,
        .native_canvas_h = 64,
        .canvas_foot_x = 32,
        .canvas_foot_y = 32,
    };
    catalog.metas["B0.PNG"] = {64, 64, true};
    catalog.metas["S0.PNG"] = {64, 64, true};

    TempDir tmp("d2_boat_picking");
    for (const auto& name :
         {"Tglobal.dbf", "Gattacks.dbf", "Gunits.dbf", "Gupgrade.dbf", "Graces.dbf"}) {
        std::ofstream(tmp.path() / name).put(0x1a);
    }
    GameDataRegistry game_data(tmp.path());

    IsoActorVisualResolver iso_resolver(catalog, game_data);

    // Resolve once, store by stack id
    AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    d2runtime::AdventureUnitInstance un;
    un.id = "u";
    un.type_id = "G000UU0001";
    world.units.push_back(un);
    AdventureStack stack;
    stack.id = "S1";
    stack.position.x = 3;
    stack.position.y = 4;
    stack.leader_id = "u";
    stack.facing = AdventureIsoDirection::D3;
    world.stacks.push_back(stack);

    AdventureStackActorVisualRequest req;
    req.presentation = {AdventureActorPresentationKind::Boat};
    req.race_id = "G000RR0003";
    req.direction = AdventureIsoDirection::D3;
    const auto iso_vis = iso_resolver.resolve(req);
    ASSERT_TRUE(iso_vis.has_value());
    ASSERT_TRUE(iso_vis->shadow.has_value());

    std::unordered_map<std::string, ar::AdventureActorVisual> visual_cache;
    visual_cache[stack.id] = *iso_to_adventure_visual(iso_vis);

    ar::AdventureMapPreparer preparer(kAnimGeo);
    preparer.add_contributor(ar::make_stack_actor_contributor(
        [&](const d2runtime::AdventureStack& s,
            const d2runtime::AdventureUnitInstance&) -> std::optional<ar::AdventureActorVisual> {
            auto it = visual_cache.find(s.id);
            return it != visual_cache.end() ? std::optional(it->second) : std::nullopt;
        }));
    auto result = preparer.prepare(world);

    ASSERT_EQ(result.pick_entries.size(), 1u);
    EXPECT_EQ(result.pick_entries[0].kind, ar::PickEntryKind::Stack);
    EXPECT_EQ(result.pick_entries[0].object_id, "S1");

    const ar::PreparedAdventureRenderPrimitive* body = nullptr;
    const ar::PreparedAdventureRenderPrimitive* shadow = nullptr;
    for (const auto& prim : result.graph.world) {
        if (prim.stable_id == ar::stable_render_id("Stack:S1"))
            body = &prim;
        if (prim.stable_id == ar::stable_render_id("StackShadow:S1"))
            shadow = &prim;
    }
    ASSERT_NE(body, nullptr);
    ASSERT_NE(shadow, nullptr);

    // Build RGBA buffers with deterministic pixel opacities.
    // Point P (10,10): opaque only in SBOA shadow, transparent in BOAT body.
    // Point Q (20,20): opaque only in BOAT body, transparent in SBOA shadow.
    auto make_buffer = [](int w, int h, int opaque_x, int opaque_y, bool add_extra_opaque) {
        auto buf = std::make_shared<d2res::RgbaBuffer>();
        buf->width = static_cast<uint32_t>(w);
        buf->height = static_cast<uint32_t>(h);
        buf->rgba.resize(static_cast<std::size_t>(w * h * 4), 0);
        auto set_pixel = [&](int px, int py) {
            std::size_t idx = (static_cast<std::size_t>(py) * static_cast<std::size_t>(w) +
                               static_cast<std::size_t>(px)) *
                              4;
            buf->rgba[idx + 0] = 255;
            buf->rgba[idx + 1] = 255;
            buf->rgba[idx + 2] = 255;
            buf->rgba[idx + 3] = 255;
        };
        set_pixel(opaque_x, opaque_y);
        if (add_extra_opaque)
            set_pixel(30, 30);
        return buf;
    };

    auto body_buffer = make_buffer(64, 64, 20, 20, true); // Q(20,20) + (30,30) opaque
    auto shd_buffer = make_buffer(64, 64, 10, 10, false); // only P(10,10) opaque

    // Build decoded_world_images for the asset runtime pipeline
    std::vector<d2engine::PreparedImageResult> decoded;
    auto make_result = [&](const std::string&                       record,
                           std::shared_ptr<const d2res::RgbaBuffer> pixels) {
        d2engine::PreparedImageResult p;
        p.key = d2engine::make_world_composed_sprite_key("Imgs/Isounit.ff", record);
        p.image = std::make_shared<d2engine::PreparedImage>();
        const_cast<d2engine::ImageAssetKey&>(p.image->key) = p.key;
        const_cast<std::shared_ptr<const d2res::RgbaBuffer>&>(p.image->pixels) = pixels;
        p.success = true;
        return p;
    };
    decoded.push_back(make_result("B0.PNG", body_buffer));
    decoded.push_back(make_result("S0.PNG", shd_buffer));

    // Attach interaction masks through production path
    ar::PreparedAdventureMap pam;
    pam.geometry = kAnimGeo;
    pam.world_graph = std::move(result.graph);
    pam.pick_entries = std::move(result.pick_entries);
    const auto built = d2engine::attach_stack_interaction_masks(pam, decoded);
    EXPECT_EQ(built, 1u) << "only body should get an interaction mask";

    // Re-extract primitives from the prepared map
    const ar::PreparedAdventureRenderPrimitive* body2 = nullptr;
    const ar::PreparedAdventureRenderPrimitive* shadow2 = nullptr;
    for (const auto& prim : pam.world_graph.world) {
        if (prim.stable_id == ar::stable_render_id("Stack:S1"))
            body2 = &prim;
        if (prim.stable_id == ar::stable_render_id("StackShadow:S1"))
            shadow2 = &prim;
    }
    ASSERT_NE(body2, nullptr);
    ASSERT_NE(shadow2, nullptr);

    EXPECT_NE(body2->interaction_mask, nullptr) << "body must have interaction mask";
    EXPECT_EQ(shadow2->interaction_mask, nullptr) << "shadow must NOT have interaction mask";

    const auto foot = kAnimGeo.cell_foot_anchor({3, 4});
    const int  ox = foot.x - 32; // draw_origin.x = foot.x - canvas_foot_x
    const int  oy = foot.y - 32; // draw_origin.y = foot.y - canvas_foot_y

    d2engine::AdventurePickIndex index;
    ar::SelectionCircleGeometry  sel_geo;
    sel_geo.center_offset_x = 0;
    sel_geo.center_offset_y = -9;
    sel_geo.radius_x = 21;
    sel_geo.radius_y = 10;
    index.build(pam, sel_geo);

    // Point P(10,10) relative to draw_origin → only opaque in SBOA → no body hit
    auto hit_p = index.hit_test(ox + 10, oy + 10);
    EXPECT_EQ(hit_p.interaction_target, nullptr) << "P (SBOA-only pixel) must NOT hit body";

    // Point Q(20,20) relative to draw_origin → opaque in BOAT → body hit
    auto hit_q = index.hit_test(ox + 20, oy + 20);
    ASSERT_NE(hit_q.interaction_target, nullptr) << "Q (BOAT pixel) must hit body";
    EXPECT_EQ(hit_q.interaction_target->object_id, "S1");

    // Shadow stable_id must NOT appear in the pick index targets
    bool shadow_in_index = false;
    for (std::size_t ti = 0; ti < index.size(); ++ti) {
        (void)ti;
        // Can't inspect targets directly from AdventurePickIndex;
        // verify via hit test that shadow region doesn't hit
        auto hit_shadow_region = index.hit_test(ox + 10, oy + 10);
        EXPECT_EQ(hit_shadow_region.interaction_target, nullptr)
            << "shadow-only region must not produce a hit";
        if (hit_shadow_region.interaction_target != nullptr)
            shadow_in_index = true;
    }
    EXPECT_FALSE(shadow_in_index) << "shadow stable_id must be absent from PickIndex";
}
