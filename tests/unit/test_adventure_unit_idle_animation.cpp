#include <gtest/gtest.h>

#include <algorithm>

#include <d2adventure_render/adventure_render_types.hpp>
#include <d2adventure_render/map_geometry.hpp>
#include <d2adventure_render/map_preparer.hpp>
#include <d2adventure_render/prepared_adventure_map.hpp>
#include <d2engine/animation/animation_player.hpp>
#include <d2engine/app/adventure_animation_helpers.hpp>
#include <d2engine/app/adventure_selection_builder.hpp>
#include <d2engine/app/adventure_visual_resources.hpp>
#include <d2engine/assets/game_data_registry.hpp>
#include <d2engine/assets/adventure_stack_actor_request_resolver.hpp>
#include <d2engine/assets/iso_actor_visual_resolver.hpp>
#include <d2engine/assets/asset_runtime_catalog_adapter.hpp>
#include <d2engine/assets/sprite_animation_catalog.hpp>
#include <d2engine/app/adventure_interaction_mask.hpp>
#include <d2engine/app/adventure_pick_index.hpp>
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
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace d2runtime;
using namespace d2engine::adventure_render;
using namespace d2engine;

static const auto kAnimGeo = AdventureMapGeometry::from_source(10, 10);

TEST(AdventureAssetCollector, SplitsInitialAndRemainingAnimationFrames) {
    PreparedAdventureRenderPrimitive animated;
    animated.container_path = "Imgs/World.ff";
    animated.animation = AdventureAnimationData{
        .frames = {{"F0", 1, 1, 1}, {"F1", 1, 1, 1}, {"F1", 1, 1, 1}, {"F2", 1, 1, 1}}};
    PreparedAdventureRenderPrimitive duplicate = animated;
    PreparedAdventureRenderPrimitive static_primitive;
    static_primitive.container_path = "Imgs/World.ff";
    static_primitive.record_name = "STATIC";

    PreparedAdventureRenderGraph graph;
    graph.world = {animated, duplicate, static_primitive};

    const auto initial = collect_adventure_initial_asset_keys(graph);
    const auto remaining = collect_adventure_remaining_animation_asset_keys(graph);
    const auto all = collect_adventure_render_asset_keys(graph);

    ASSERT_EQ(initial.size(), 2u);
    EXPECT_EQ(initial[0].image_name, "F0");
    EXPECT_EQ(initial[1].image_name, "STATIC");
    ASSERT_EQ(remaining.size(), 2u);
    EXPECT_EQ(remaining[0].image_name, "F1");
    EXPECT_EQ(remaining[1].image_name, "F2");
    ASSERT_EQ(all.size(), 4u);
    EXPECT_EQ(all[0].image_name, "F0");
    EXPECT_EQ(all[1].image_name, "F1");
    EXPECT_EQ(all[2].image_name, "F2");
    EXPECT_EQ(all[3].image_name, "STATIC");
}

// ======================================================================
// 22. Contributor preserves full ordered sequence
// ======================================================================

TEST(AdventureUnitIdleAnimation, ContributorPreservesFullOrderedSequence) {
    auto resolver =
        [](const d2runtime::AdventureStack&        stack,
           const d2runtime::AdventureUnitInstance& leader) -> std::optional<AdventureActorVisual> {
        (void)stack;
        (void)leader;
        return AdventureActorVisual{.presentation_kind = AdventureActorPresentationKind::Unit,
                                    .resolved_owner_id = "UNKNOWN",
                                    .body =
                                        {
                                            .container_path = "Imgs/Isounit.ff",
                                            .logical_animation_name = "G000UU0003STOP3",
                                            .frames =
                                                {
                                                    {"F0", 80, 90},
                                                    {"F1", 80, 90},
                                                    {"F2", 80, 90},
                                                },
                                            .native_canvas_w = 320,
                                            .native_canvas_h = 320,
                                            .canvas_foot_x = 40,
                                            .canvas_foot_y = 80,
                                        },
                                    .shadow = std::nullopt};
    };

    AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.units.push_back({.id = "u", .type_id = "G000UU0003"});
    AdventureStack stack;
    stack.id = "S1";
    stack.position.x = 3;
    stack.position.y = 4;
    stack.leader_id = "u";
    stack.facing = AdventureIsoDirection::D3;
    world.stacks.push_back(stack);

    AdventureMapPreparer preparer(kAnimGeo);
    preparer.add_contributor(make_stack_actor_contributor(resolver));
    auto result = preparer.prepare(world);

    ASSERT_EQ(result.graph.world.size(), 1u);
    const auto& prim = result.graph.world[0];
    ASSERT_TRUE(prim.animation.has_value());
    EXPECT_EQ(prim.animation->frames.size(), 3u);
    EXPECT_EQ(prim.animation->frames[0].record_name, "F0");
    EXPECT_EQ(prim.animation->frames[1].record_name, "F1");
    EXPECT_EQ(prim.animation->frames[2].record_name, "F2");
    EXPECT_EQ(prim.animation->frames[0].canvas_width, 80);
    EXPECT_EQ(prim.animation->frames[0].canvas_height, 90);
    EXPECT_EQ(prim.animation->frames[2].canvas_width, 80);
    EXPECT_EQ(prim.record_name, "F0");
}

// ======================================================================
// 23. Contributor static sequence (one frame)
// ======================================================================

TEST(AdventureUnitIdleAnimation, ContributorStaticOneFrame) {
    auto resolver =
        [](const d2runtime::AdventureStack&        stack,
           const d2runtime::AdventureUnitInstance& leader) -> std::optional<AdventureActorVisual> {
        (void)stack;
        (void)leader;
        return AdventureActorVisual{.presentation_kind = AdventureActorPresentationKind::Unit,
                                    .resolved_owner_id = "UNKNOWN",
                                    .body =
                                        {
                                            .container_path = "Imgs/Isounit.ff",
                                            .logical_animation_name = "G000UU0003STOP0",
                                            .frames = {{"STOP0.PNG", 80, 90}},
                                            .native_canvas_w = 80,
                                            .native_canvas_h = 90,
                                            .canvas_foot_x = 40,
                                            .canvas_foot_y = 80,
                                        },
                                    .shadow = std::nullopt};
    };

    AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.units.push_back({.id = "u", .type_id = "G000UU0003"});
    AdventureStack stack;
    stack.id = "S1";
    stack.position.x = 5;
    stack.position.y = 5;
    stack.leader_id = "u";
    stack.facing = AdventureIsoDirection::D0;
    world.stacks.push_back(stack);

    AdventureMapPreparer preparer(kAnimGeo);
    preparer.add_contributor(make_stack_actor_contributor(resolver));
    auto result = preparer.prepare(world);

    ASSERT_EQ(result.graph.world.size(), 1u);
    const auto& prim = result.graph.world[0];

    EXPECT_FALSE(prim.animation.has_value());
    EXPECT_EQ(prim.record_name, "STOP0.PNG");
    EXPECT_EQ(prim.src_width, 80);
    EXPECT_EQ(prim.src_height, 90);
    EXPECT_EQ(prim.level, WorldRenderLevel::Actor);

    const auto foot = kAnimGeo.cell_foot_anchor({5, 5});
    EXPECT_EQ(prim.draw_origin.x, foot.x - 40);
    EXPECT_EQ(prim.draw_origin.y, foot.y - 80);
}

// ======================================================================
// 24. Contributor animated sequence (three frames, different dimensions)
// ======================================================================

TEST(AdventureUnitIdleAnimation, ContributorAnimatedThreeFrames) {
    auto resolver =
        [](const d2runtime::AdventureStack&        stack,
           const d2runtime::AdventureUnitInstance& leader) -> std::optional<AdventureActorVisual> {
        (void)stack;
        (void)leader;
        return AdventureActorVisual{.presentation_kind = AdventureActorPresentationKind::Unit,
                                    .resolved_owner_id = "UNKNOWN",
                                    .body =
                                        {
                                            .container_path = "Imgs/Isounit.ff",
                                            .logical_animation_name = "G000UU0003STOP5",
                                            .frames =
                                                {
                                                    {"F0.PNG", 80, 90},
                                                    {"F1.PNG", 100, 110},
                                                    {"F2.PNG", 90, 100},
                                                },
                                            .native_canvas_w = 320,
                                            .native_canvas_h = 320,
                                            .canvas_foot_x = 40,
                                            .canvas_foot_y = 80,
                                        },
                                    .shadow = std::nullopt};
    };

    AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.units.push_back({.id = "u", .type_id = "G000UU0003"});
    AdventureStack stack;
    stack.id = "S1";
    stack.position.x = 3;
    stack.position.y = 4;
    stack.leader_id = "u";
    stack.facing = AdventureIsoDirection::D5;
    world.stacks.push_back(stack);

    AdventureMapPreparer preparer(kAnimGeo);
    preparer.add_contributor(make_stack_actor_contributor(resolver));
    auto result = preparer.prepare(world);

    ASSERT_EQ(result.graph.world.size(), 1u);
    const auto& prim = result.graph.world[0];

    ASSERT_TRUE(prim.animation.has_value());
    EXPECT_EQ(prim.record_name, "F0.PNG");
    EXPECT_EQ(prim.animation->animation_name, "G000UU0003STOP5");
    EXPECT_EQ(prim.animation->frames.size(), 3u);
    EXPECT_EQ(prim.animation->frames[0].record_name, "F0.PNG");
    EXPECT_EQ(prim.animation->frames[1].record_name, "F1.PNG");
    EXPECT_EQ(prim.animation->frames[2].record_name, "F2.PNG");
    EXPECT_EQ(prim.animation->frames[0].duration_ms, 100);
    EXPECT_EQ(prim.animation->frames[1].duration_ms, 100);
    EXPECT_EQ(prim.animation->frames[2].duration_ms, 100);
    EXPECT_TRUE(prim.animation->is_looping);
    EXPECT_EQ(prim.animation->timing_source, AdventureAnimationTimingSource::ProvisionalFallback);
    EXPECT_EQ(prim.animation->frames[0].canvas_width, 80);
    EXPECT_EQ(prim.animation->frames[0].canvas_height, 90);
    EXPECT_EQ(prim.animation->frames[1].canvas_width, 100);
    EXPECT_EQ(prim.animation->frames[1].canvas_height, 110);
    EXPECT_EQ(prim.animation->frames[2].canvas_width, 90);
    EXPECT_EQ(prim.animation->frames[2].canvas_height, 100);

    // visual_bounds must cover the maximum frame dimensions
    const auto foot = kAnimGeo.cell_foot_anchor({3, 4});
    const int  expected_x = foot.x - 40;
    const int  expected_y = foot.y - 80;
    EXPECT_EQ(prim.draw_origin.x, expected_x);
    EXPECT_EQ(prim.draw_origin.y, expected_y);
    EXPECT_EQ(prim.visual_bounds.min_x, expected_x);
    EXPECT_EQ(prim.visual_bounds.min_y, expected_y);
    EXPECT_EQ(prim.visual_bounds.max_x, expected_x + 100); // max width
    EXPECT_EQ(prim.visual_bounds.max_y, expected_y + 110); // max height

    EXPECT_EQ(prim.src_width, 80);  // first frame
    EXPECT_EQ(prim.src_height, 90); // first frame
}

// ======================================================================
// 25. Stable per-frame geometry (no per-frame foot correction)
// ======================================================================

TEST(AdventureUnitIdleAnimation, StablePerFrameGeometry) {
    auto resolver =
        [](const d2runtime::AdventureStack&        stack,
           const d2runtime::AdventureUnitInstance& leader) -> std::optional<AdventureActorVisual> {
        (void)stack;
        (void)leader;
        return AdventureActorVisual{.presentation_kind = AdventureActorPresentationKind::Unit,
                                    .resolved_owner_id = "UNKNOWN",
                                    .body =
                                        {
                                            .container_path = "Imgs/Isounit.ff",
                                            .logical_animation_name = "STOP3",
                                            .frames =
                                                {
                                                    {"F0.PNG", 80, 90},
                                                    {"F1.PNG", 120, 130},
                                                    {"F2.PNG", 60, 70},
                                                },
                                            .native_canvas_w = 320,
                                            .native_canvas_h = 320,
                                            .canvas_foot_x = 50,
                                            .canvas_foot_y = 100,
                                        },
                                    .shadow = std::nullopt};
    };

    AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.units.push_back({.id = "u", .type_id = "G000UU0003"});
    AdventureStack stack;
    stack.id = "S1";
    stack.position.x = 2;
    stack.position.y = 7;
    stack.leader_id = "u";
    stack.facing = AdventureIsoDirection::D3;
    world.stacks.push_back(stack);

    AdventureMapPreparer preparer(kAnimGeo);
    preparer.add_contributor(make_stack_actor_contributor(resolver));
    auto result = preparer.prepare(world);

    ASSERT_EQ(result.graph.world.size(), 1u);
    const auto& prim = result.graph.world[0];
    ASSERT_TRUE(prim.animation.has_value());

    const auto        foot = kAnimGeo.cell_foot_anchor({2, 7});
    const ScreenPoint expected_origin{foot.x - 50, foot.y - 100};

    // Source fields must always describe the first frame
    EXPECT_EQ(prim.record_name, "F0.PNG");
    EXPECT_EQ(prim.src_width, 80);
    EXPECT_EQ(prim.src_height, 90);

    // visual_bounds must cover the maximum dimensions across all frames
    EXPECT_EQ(prim.visual_bounds.min_x, expected_origin.x);
    EXPECT_EQ(prim.visual_bounds.min_y, expected_origin.y);
    EXPECT_EQ(prim.visual_bounds.max_x, expected_origin.x + 120);
    EXPECT_EQ(prim.visual_bounds.max_y, expected_origin.y + 130);

    // Every animation frame must resolve to the same draw origin
    for (std::size_t i = 0; i < prim.animation->frames.size(); ++i) {
        const auto resolved = resolve_adventure_frame_visual(prim, i);
        EXPECT_EQ(resolved.draw_origin.x, expected_origin.x) << "frame " << i << " draw_origin.x";
        EXPECT_EQ(resolved.draw_origin.y, expected_origin.y) << "frame " << i << " draw_origin.y";
        EXPECT_EQ(resolved.record_name, prim.animation->frames[i].record_name);
        EXPECT_EQ(resolved.src_width, prim.animation->frames[i].canvas_width);
        EXPECT_EQ(resolved.src_height, prim.animation->frames[i].canvas_height);
    }

    // Static (no animation) must also produce the same origin
    {
        PreparedAdventureRenderPrimitive static_prim = prim;
        static_prim.animation = std::nullopt;
        static_prim.record_name = "STATIC.PNG";
        static_prim.src_width = 100;
        static_prim.src_height = 100;
        const auto resolved = resolve_adventure_frame_visual(static_prim, std::nullopt);
        EXPECT_EQ(resolved.draw_origin.x, expected_origin.x);
        EXPECT_EQ(resolved.draw_origin.y, expected_origin.y);
        EXPECT_EQ(resolved.record_name, "STATIC.PNG");
        EXPECT_EQ(resolved.src_width, 100);
        EXPECT_EQ(resolved.src_height, 100);
    }
}

// ======================================================================
// 26. Current-direction-only preload (no other STOP directions)
// ======================================================================

TEST(AdventureUnitIdleAnimation, CurrentDirectionPreloadOnly) {
    auto resolver =
        [](const d2runtime::AdventureStack&        stack,
           const d2runtime::AdventureUnitInstance& leader) -> std::optional<AdventureActorVisual> {
        (void)stack;
        (void)leader;
        return AdventureActorVisual{.presentation_kind = AdventureActorPresentationKind::Unit,
                                    .resolved_owner_id = "UNKNOWN",
                                    .body =
                                        {
                                            .container_path = "Imgs/Isounit.ff",
                                            .logical_animation_name = "G000UU0003STOP3",
                                            .frames =
                                                {
                                                    {"STOP3_F0.PNG", 80, 90},
                                                    {"STOP3_F1.PNG", 80, 90},
                                                    {"STOP3_F2.PNG", 80, 90},
                                                },
                                            .native_canvas_w = 320,
                                            .native_canvas_h = 320,
                                            .canvas_foot_x = 40,
                                            .canvas_foot_y = 80,
                                        },
                                    .shadow = std::nullopt};
    };

    AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.units.push_back({.id = "u", .type_id = "G000UU0003"});
    AdventureStack stack;
    stack.id = "S1";
    stack.position.x = 1;
    stack.position.y = 1;
    stack.leader_id = "u";
    stack.facing = AdventureIsoDirection::D3;
    world.stacks.push_back(stack);

    AdventureMapPreparer preparer(kAnimGeo);
    preparer.add_contributor(make_stack_actor_contributor(resolver));
    auto result = preparer.prepare(world);

    ASSERT_EQ(result.graph.world.size(), 1u);
    const auto& prim = result.graph.world[0];

    // Derive expected keys from primitive's animation frames
    std::unordered_set<ImageAssetKey, std::hash<ImageAssetKey>> expected;
    if (prim.animation.has_value()) {
        for (const auto& af : prim.animation->frames)
            expected.insert(make_world_composed_sprite_key(prim.container_path, af.record_name));
    } else {
        expected.insert(make_world_composed_sprite_key(prim.container_path, prim.record_name));
    }

    const auto actual_list = collect_adventure_render_asset_keys(result.graph);
    std::unordered_set<ImageAssetKey, std::hash<ImageAssetKey>> actual(actual_list.begin(),
                                                                       actual_list.end());

    EXPECT_EQ(actual.size(), expected.size());
    for (const auto& ek : expected) {
        EXPECT_TRUE(actual.count(ek) == 1)
            << "missing expected preload key: " << ek.container_path << "/" << ek.image_name;
    }

    // Verify no STOP0, STOP1, STOP2, STOP4, STOP5, STOP6, STOP7
    for (const auto& key : actual_list) {
        const auto& n = key.image_name;
        EXPECT_TRUE(n.find("STOP0") == std::string::npos && n.find("STOP1") == std::string::npos &&
                    n.find("STOP2") == std::string::npos && n.find("STOP4") == std::string::npos &&
                    n.find("STOP5") == std::string::npos && n.find("STOP6") == std::string::npos &&
                    n.find("STOP7") == std::string::npos)
            << "unexpected direction asset: " << key.image_name;
    }
}

// ======================================================================
// 27. One-frame sequences remain static, multi-frame animated
// ======================================================================

TEST(AdventureUnitIdleAnimation, StaticVsAnimatedActors) {
    AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.units.push_back({.id = "u1", .type_id = "G000UU0001"});
    world.units.push_back({.id = "u2", .type_id = "G000UU0002"});

    AdventureStack stack1;
    stack1.id = "S1";
    stack1.position.x = 1;
    stack1.position.y = 1;
    stack1.leader_id = "u1";
    stack1.facing = AdventureIsoDirection::D0;
    world.stacks.push_back(stack1);

    AdventureStack stack2;
    stack2.id = "S2";
    stack2.position.x = 3;
    stack2.position.y = 4;
    stack2.leader_id = "u2";
    stack2.facing = AdventureIsoDirection::D5;
    world.stacks.push_back(stack2);

    auto resolver =
        [](const d2runtime::AdventureStack& /*stack*/,
           const d2runtime::AdventureUnitInstance& leader) -> std::optional<AdventureActorVisual> {
        if (leader.type_id == "G000UU0001") {
            return AdventureActorVisual{.presentation_kind = AdventureActorPresentationKind::Unit,
                                        .resolved_owner_id = "UNKNOWN",
                                        .body =
                                            {
                                                .container_path = "Imgs/Isounit.ff",
                                                .logical_animation_name = "G000UU0001STOP0",
                                                .frames = {{"F0.PNG", 80, 90}},
                                                .native_canvas_w = 80,
                                                .native_canvas_h = 90,
                                                .canvas_foot_x = 40,
                                                .canvas_foot_y = 80,
                                            },
                                        .shadow = std::nullopt};
        }
        return AdventureActorVisual{.presentation_kind = AdventureActorPresentationKind::Unit,
                                    .resolved_owner_id = "UNKNOWN",
                                    .body =
                                        {
                                            .container_path = "Imgs/Isounit.ff",
                                            .logical_animation_name = "G000UU0002STOP5",
                                            .frames =
                                                {
                                                    {"F0.PNG", 80, 90},
                                                    {"F1.PNG", 100, 110},
                                                    {"F2.PNG", 90, 100},
                                                },
                                            .native_canvas_w = 320,
                                            .native_canvas_h = 320,
                                            .canvas_foot_x = 40,
                                            .canvas_foot_y = 80,
                                        },
                                    .shadow = std::nullopt};
    };

    AdventureMapPreparer preparer(kAnimGeo);
    preparer.add_contributor(make_stack_actor_contributor(resolver));
    auto result = preparer.prepare(world);

    ASSERT_EQ(result.graph.world.size(), 2u);

    bool found_static = false;
    bool found_animated = false;

    for (const auto& prim : result.graph.world) {
        if (prim.debug_label.find("S1") != std::string::npos) {
            EXPECT_FALSE(prim.animation.has_value())
                << "one-frame stack must be static, not animated";
            found_static = true;
        } else if (prim.debug_label.find("S2") != std::string::npos) {
            ASSERT_TRUE(prim.animation.has_value())
                << "three-frame stack must be animated, not static";
            EXPECT_EQ(prim.animation->frames.size(), 3u);
            found_animated = true;
        }
    }
    EXPECT_TRUE(found_static);
    EXPECT_TRUE(found_animated);

    // Verify animation players: only animated stacks get players
    std::vector<PreparedAdventureRenderPrimitive> animated_prims;
    for (const auto& prim : result.graph.world) {
        if (prim.animation.has_value())
            animated_prims.push_back(prim);
    }
    ASSERT_EQ(animated_prims.size(), 1u);
    EXPECT_TRUE(animated_prims[0].debug_label.find("S2") != std::string::npos);
}

// ======================================================================
// 28. Union interaction mask for animated units
// ======================================================================

TEST(AdventureUnitIdleAnimation, UnionInteractionMask) {
    // Frame 0: opaque at pixel (0,0) only
    d2res::RgbaBuffer frame0;
    frame0.width = 4;
    frame0.height = 2;
    frame0.rgba.resize(static_cast<std::size_t>(4 * 2 * 4), 0);
    frame0.rgba[3] = 255; // pixel (0,0) opaque

    // Frame 1: opaque at pixel (3,1) only
    d2res::RgbaBuffer frame1;
    frame1.width = 4;
    frame1.height = 2;
    frame1.rgba.resize(static_cast<std::size_t>(4 * 2 * 4), 0);
    frame1.rgba[static_cast<std::size_t>(1 * 4 * 4 + 3 * 4 + 3)] = 128; // pixel (3,1) opaque

    std::vector<const d2res::RgbaBuffer*> frames = {&frame0, &frame1};
    auto                                  mask = build_union_interaction_mask(frames);

    ASSERT_NE(mask, nullptr);
    EXPECT_EQ(mask->width, 4);
    EXPECT_EQ(mask->height, 2);

    EXPECT_TRUE(mask->opaque(0, 0)) << "frame0 pixel A must be opaque";
    EXPECT_TRUE(mask->opaque(3, 1)) << "frame1 pixel B must be opaque";
    EXPECT_FALSE(mask->opaque(1, 0)) << "unrelated pixel C must be transparent";
    EXPECT_FALSE(mask->opaque(0, 1)) << "unrelated pixel must be transparent";
    EXPECT_FALSE(mask->opaque(2, 0)) << "unrelated pixel must be transparent";
}

// ======================================================================
// 29. Missing animated frame fails with full identity in error
// ======================================================================

TEST(AdventureUnitIdleAnimation, MissingAnimatedFrameThrows) {
    PreparedAdventureMap adv_map;
    adv_map.geometry = kAnimGeo;

    PreparedAdventureRenderPrimitive prim;
    prim.stable_id = stable_render_id("Stack:S1");
    prim.level = WorldRenderLevel::Actor;
    prim.container_path = "Imgs/Isounit.ff";
    prim.record_name = "F0.PNG";
    prim.debug_label = "StackLeader:S1";

    AdventureAnimationData anim;
    anim.animation_name = "STOP3";
    anim.is_looping = true;
    anim.frames.push_back({"F0.PNG", 100, 80, 90});
    anim.frames.push_back({"F1.PNG", 100, 80, 90});
    anim.frames.push_back({"F2.PNG", 100, 80, 90});
    prim.animation = std::move(anim);

    // Store interaction_mask before the call — must remain unchanged
    std::shared_ptr<const InteractionMask> mask_before = prim.interaction_mask;

    adv_map.world_graph.world.push_back(std::move(prim));
    adv_map.pick_entries.push_back(PickEntry{.stable_id = stable_render_id("Stack:S1"),
                                             .kind = PickEntryKind::Stack,
                                             .object_id = "S1"});

    // Provide decoded data for only F0 and F2 (missing F1)
    std::vector<PreparedImageResult> decoded;

    d2res::RgbaBuffer fb0;
    fb0.width = 4;
    fb0.height = 2;
    fb0.rgba.resize(8 * 4, 0xFF);
    auto pixels0 = std::make_shared<const d2res::RgbaBuffer>(std::move(fb0));
    auto img0 = std::make_shared<PreparedImage>(PreparedImage{
        .key = make_world_composed_sprite_key("Imgs/Isounit.ff", "F0.PNG"), .pixels = pixels0});
    decoded.push_back({.key = make_world_composed_sprite_key("Imgs/Isounit.ff", "F0.PNG"),
                       .image = img0,
                       .success = true});

    d2res::RgbaBuffer fb2;
    fb2.width = 4;
    fb2.height = 2;
    fb2.rgba.resize(8 * 4, 0xFF);
    auto pixels2 = std::make_shared<const d2res::RgbaBuffer>(std::move(fb2));
    auto img2 = std::make_shared<PreparedImage>(PreparedImage{
        .key = make_world_composed_sprite_key("Imgs/Isounit.ff", "F2.PNG"), .pixels = pixels2});
    decoded.push_back({.key = make_world_composed_sprite_key("Imgs/Isounit.ff", "F2.PNG"),
                       .image = img2,
                       .success = true});

    // Missing frame is tolerated — builds mask from available frames (F0, F2)
    std::size_t built = attach_stack_interaction_masks(adv_map, decoded);
    EXPECT_EQ(built, 1u) << "interaction mask built from available frames";
    const auto& stored = adv_map.world_graph.world[0].interaction_mask;
    EXPECT_NE(stored.get(), nullptr) << "interaction_mask must be built from partial frames";
    EXPECT_NE(stored.get(), mask_before.get()) << "interaction_mask must change on success";
}

// ======================================================================
// 30. Playback loops (AdventureAnimationData -> AnimationSequence -> player)
// ======================================================================

TEST(AdventureUnitIdleAnimation, PlaybackLoopsUsingProductionHelper) {
    AdventureAnimationData anim_data;
    anim_data.animation_name = "STOP3";
    anim_data.native_canvas_w = 320;
    anim_data.native_canvas_h = 320;
    anim_data.is_looping = true;
    anim_data.timing_source = AdventureAnimationTimingSource::ProvisionalFallback;
    anim_data.frames.push_back({"F0.PNG", 100, 80, 90});
    anim_data.frames.push_back({"F1.PNG", 100, 100, 110});
    anim_data.frames.push_back({"F2.PNG", 100, 90, 100});

    auto seq = adventure_animation_data_to_sequence("Imgs/Isounit.ff", anim_data);

    EXPECT_EQ(seq.name, "STOP3");
    EXPECT_EQ(seq.container_path, "Imgs/Isounit.ff");
    EXPECT_TRUE(seq.is_looping);
    EXPECT_EQ(seq.native_canvas_w, 320);
    EXPECT_EQ(seq.native_canvas_h, 320);
    ASSERT_EQ(seq.frames.size(), 3u);
    EXPECT_EQ(seq.frames[0].image_name, "F0.PNG");
    EXPECT_EQ(seq.frames[1].image_name, "F1.PNG");
    EXPECT_EQ(seq.frames[2].image_name, "F2.PNG");
    EXPECT_EQ(seq.frames[0].duration_ms, 100u);
    EXPECT_EQ(seq.frames[1].duration_ms, 100u);
    EXPECT_EQ(seq.frames[2].duration_ms, 100u);

    // Verify looping playback
    AnimationPlayer player(std::move(seq));
    player.play();

    EXPECT_EQ(player.current_frame_index(), 0u);
    EXPECT_EQ(player.current_frame().image_name, "F0.PNG");

    player.update(100.0f);
    EXPECT_EQ(player.current_frame_index(), 1u);
    EXPECT_EQ(player.current_frame().image_name, "F1.PNG");

    player.update(100.0f);
    EXPECT_EQ(player.current_frame_index(), 2u);
    EXPECT_EQ(player.current_frame().image_name, "F2.PNG");

    // Next advance wraps back to frame 0 (looping)
    player.update(100.0f);
    EXPECT_EQ(player.current_frame_index(), 0u);
    EXPECT_EQ(player.current_frame().image_name, "F0.PNG");
    EXPECT_EQ(player.state(), AnimationPlayerState::Playing);
}

// ======================================================================
// Edge case: one-frame static doesn't create animation player data
// ======================================================================

TEST(AdventureUnitIdleAnimation, OneFrameIsNotAnimated) {
    auto resolver =
        [](const d2runtime::AdventureStack&        stack,
           const d2runtime::AdventureUnitInstance& leader) -> std::optional<AdventureActorVisual> {
        (void)stack;
        (void)leader;
        return AdventureActorVisual{.presentation_kind = AdventureActorPresentationKind::Unit,
                                    .resolved_owner_id = "UNKNOWN",
                                    .body =
                                        {
                                            .container_path = "Imgs/Isounit.ff",
                                            .logical_animation_name = "STOP0",
                                            .frames = {{"ONLY.PNG", 80, 90}},
                                            .native_canvas_w = 80,
                                            .native_canvas_h = 90,
                                            .canvas_foot_x = 40,
                                            .canvas_foot_y = 80,
                                        },
                                    .shadow = std::nullopt};
    };

    AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.units.push_back({.id = "u", .type_id = "G000UU0001"});
    AdventureStack stack;
    stack.id = "S1";
    stack.position.x = 1;
    stack.position.y = 1;
    stack.leader_id = "u";
    stack.facing = AdventureIsoDirection::D0;
    world.stacks.push_back(stack);

    AdventureMapPreparer preparer(kAnimGeo);
    preparer.add_contributor(make_stack_actor_contributor(resolver));
    auto result = preparer.prepare(world);

    ASSERT_EQ(result.graph.world.size(), 1u);
    const auto& prim = result.graph.world[0];
    EXPECT_FALSE(prim.animation.has_value());
    EXPECT_EQ(prim.record_name, "ONLY.PNG");
}

// ======================================================================
// Fake ISpriteAnimationCatalog for production resolver test
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

} // namespace

// ======================================================================
// 32. Production resolver returns full ordered sequence through interface
// ======================================================================

TEST(AdventureUnitIdleAnimation, ResolverReturnsFullOrderedSequenceViaCatalog) {
    FakeSpriteCatalog catalog;
    catalog.animations["G000UU0003STOP3"] = {
        .name = "G000UU0003STOP3",
        .container_path = "Imgs/Isounit.ff",
        .frames =
            {
                {.image_name = "F0", .index = 0, .duration_ms = 100},
                {.image_name = "F1", .index = 1, .duration_ms = 100},
                {.image_name = "F2", .index = 2, .duration_ms = 100},
            },
        .is_looping = false,
        .native_canvas_w = 320,
        .native_canvas_h = 320,
        .canvas_foot_x = 40,
        .canvas_foot_y = 80,
    };
    catalog.metas["F0"] = {80, 90};
    catalog.metas["F1"] = {80, 90};
    catalog.metas["F2"] = {80, 90};

    TempDir tmp("d2_resolver_test");
    for (const auto& name :
         {"Tglobal.dbf", "Gattacks.dbf", "Gunits.dbf", "Gupgrade.dbf", "Graces.dbf"}) {
        std::ofstream(tmp.path() / name).put(0x1a);
    }
    GameDataRegistry game_data(tmp.path());

    IsoActorVisualResolver           resolver(catalog, game_data);
    AdventureStackActorVisualRequest req;
    req.presentation = {AdventureActorPresentationKind::Unit};
    req.leader_unit_type_id = "G000UU0003";
    req.direction = AdventureIsoDirection::D3;
    const auto visual = resolver.resolve(req);
    ASSERT_TRUE(visual.has_value());
    EXPECT_EQ(visual->body.animation_name, "G000UU0003STOP3");
    ASSERT_EQ(visual->body.frames.size(), 3u);
    EXPECT_EQ(visual->body.frames[0].record_name, "F0");
    EXPECT_EQ(visual->body.frames[1].record_name, "F1");
    EXPECT_EQ(visual->body.frames[2].record_name, "F2");
    EXPECT_EQ(visual->body.frames[0].canvas_width, 80);
    EXPECT_EQ(visual->body.frames[0].canvas_height, 90);
    EXPECT_EQ(visual->body.native_canvas_w, 320);
    EXPECT_EQ(visual->body.native_canvas_h, 320);
    EXPECT_EQ(visual->body.canvas_foot_x, 40);
    EXPECT_EQ(visual->body.canvas_foot_y, 80);
    EXPECT_FALSE(visual->body.frames[0].record_name.empty());

    // Empty source sequence returns unresolved
    {
        AdventureStackActorVisualRequest req2;
        req2.presentation = {AdventureActorPresentationKind::Unit};
        req2.leader_unit_type_id = "G000UU_NONEXIST";
        req2.direction = AdventureIsoDirection::D0;
        EXPECT_FALSE(resolver.resolve(req2).has_value());
    }
}

// ======================================================================
// 33. Direction-preserving base fallback — deterministic synthetic DBF
// ======================================================================

TEST(AdventureUnitIdleAnimation, DirectionPreservingBaseFallbackDeterministic) {
    using namespace test_dbf;

    TempDir tmp("d2_base_fallback_test");

    // Build a minimal Gunits.dbf with derived/base relationship
    DbfBuilder gunits({
        {"UNIT_ID", 'C', 20},
        {"NAME", 'C', 20},
        {"BASE_UNIT", 'C', 20},
    });
    gunits.add_record({"G000UU0003", "derived_unit", "G000UU0004"});
    gunits.add_record({"G000UU0004", "base_unit", ""});
    gunits.write(tmp.path() / "Gunits.dbf");

    // Stub DBFs required by GameDataRegistry
    for (const auto& name : {"Tglobal.dbf", "Gattacks.dbf", "Gupgrade.dbf", "Graces.dbf"}) {
        std::ofstream(tmp.path() / name).put(0x1a);
    }
    GameDataRegistry game_data(tmp.path());

    FakeSpriteCatalog catalog;
    // Base STOP5 present with F0, F1, F2
    catalog.animations["G000UU0004STOP5"] = {
        .name = "G000UU0004STOP5",
        .container_path = "Imgs/Isounit.ff",
        .frames =
            {
                {.image_name = "F0", .index = 0, .duration_ms = 100},
                {.image_name = "F1", .index = 1, .duration_ms = 100},
                {.image_name = "F2", .index = 2, .duration_ms = 100},
            },
        .is_looping = false,
        .native_canvas_w = 320,
        .native_canvas_h = 320,
        .canvas_foot_x = 40,
        .canvas_foot_y = 80,
    };
    // Derived STOP5 absent
    // Base STOP0 present
    catalog.animations["G000UU0004STOP0"] = {
        .name = "G000UU0004STOP0",
        .container_path = "Imgs/Isounit.ff",
        .frames = {{.image_name = "BASE_F0", .index = 0, .duration_ms = 100}},
        .is_looping = false,
        .native_canvas_w = 80,
        .native_canvas_h = 90,
        .canvas_foot_x = 40,
        .canvas_foot_y = 80,
    };
    // Derived STOP0 present (to avoid resolving to base STOP0 for D0)
    catalog.animations["G000UU0003STOP0"] = {
        .name = "G000UU0003STOP0",
        .container_path = "Imgs/Isounit.ff",
        .frames = {{.image_name = "DERIVED_F0", .index = 0, .duration_ms = 100}},
        .is_looping = false,
        .native_canvas_w = 80,
        .native_canvas_h = 90,
        .canvas_foot_x = 40,
        .canvas_foot_y = 80,
    };
    catalog.metas["F0"] = {80, 90};
    catalog.metas["F1"] = {80, 90};
    catalog.metas["F2"] = {80, 90};
    catalog.metas["BASE_F0"] = {80, 90};
    catalog.metas["DERIVED_F0"] = {80, 90};

    IsoActorVisualResolver resolver(catalog, game_data);

    // D5 must fall back from derived to base STOP5
    AdventureStackActorVisualRequest req;
    req.presentation = {AdventureActorPresentationKind::Unit};
    req.leader_unit_type_id = "G000UU0003";
    req.direction = AdventureIsoDirection::D5;
    const auto d5_visual = resolver.resolve(req);
    ASSERT_TRUE(d5_visual.has_value());
    EXPECT_EQ(d5_visual->body.animation_name, "G000UU0004STOP5");
    ASSERT_EQ(d5_visual->body.frames.size(), 3u);
    EXPECT_EQ(d5_visual->body.frames[0].record_name, "F0");
    EXPECT_EQ(d5_visual->body.frames[1].record_name, "F1");
    EXPECT_EQ(d5_visual->body.frames[2].record_name, "F2");

    // Must NOT resolve to STOP0
    EXPECT_EQ(d5_visual->body.animation_name.find("STOP0"), std::string::npos);

    // Repeated resolution returns the same D5 result
    AdventureStackActorVisualRequest req2;
    req2.presentation = {AdventureActorPresentationKind::Unit};
    req2.leader_unit_type_id = "G000UU0003";
    req2.direction = AdventureIsoDirection::D5;
    const auto d5_again = resolver.resolve(req2);
    ASSERT_TRUE(d5_again.has_value());
    EXPECT_EQ(d5_again->body.animation_name, "G000UU0004STOP5");
    ASSERT_EQ(d5_again->body.frames.size(), 3u);
}

// ======================================================================
// 34. Union mask + picking regression
// ======================================================================

TEST(AdventureUnitIdleAnimation, UnionMaskAndPickingRegression) {
    PreparedAdventureMap adv_map;
    adv_map.geometry = kAnimGeo;

    PreparedAdventureRenderPrimitive prim;
    const auto                       stack_stable = stable_render_id("Stack:S1");
    prim.stable_id = stack_stable;
    prim.debug_label = "StackLeader:S1";
    prim.level = WorldRenderLevel::Actor;
    prim.container_path = "test_cont";
    prim.record_name = "F0.PNG";
    // Position at cell (5,5) — known foot anchor
    prim.depth_anchor = {5, 5};
    const auto foot = kAnimGeo.cell_foot_anchor({5, 5});
    // Place draw_origin so alpha test pixels fall within interaction region
    // Interaction region: dx [-24,24), dy [-34,14) relative to foot
    // Place mask pixels at (30,5) and (10,10) relative to draw_origin
    prim.draw_origin = {foot.x - 24, foot.y - 34};
    prim.visual_bounds = {prim.draw_origin.x, prim.draw_origin.y, prim.draw_origin.x + 80,
                          prim.draw_origin.y + 90};
    prim.footprint.push_back({5, 5});

    AdventureAnimationData anim;
    anim.animation_name = "STOP3";
    anim.is_looping = true;
    anim.frames.push_back({"F0.PNG", 100, 80, 90});
    anim.frames.push_back({"F1.PNG", 100, 80, 90});
    prim.animation = std::move(anim);

    adv_map.world_graph.world.push_back(std::move(prim));
    adv_map.pick_entries.push_back(
        PickEntry{.stable_id = stack_stable, .kind = PickEntryKind::Stack, .object_id = "S1"});

    // Decoded buffers: frame 0 has pixel (30,5) opaque, frame 1 has pixel (10,10) opaque
    auto make_frame = [](std::string_view record, int px, int py) -> PreparedImageResult {
        d2res::RgbaBuffer fb;
        fb.width = 80;
        fb.height = 90;
        fb.rgba.resize(static_cast<std::size_t>(80 * 90 * 4), 0);
        fb.rgba[static_cast<std::size_t>(py * 80 * 4 + px * 4 + 3)] = 255;
        auto pixels = std::make_shared<const d2res::RgbaBuffer>(std::move(fb));
        auto img = std::make_shared<PreparedImage>(PreparedImage{
            .key = make_world_composed_sprite_key("test_cont", record), .pixels = pixels});
        return PreparedImageResult{.key = make_world_composed_sprite_key("test_cont", record),
                                   .image = img,
                                   .success = true};
    };
    std::vector<PreparedImageResult> decoded;
    decoded.push_back(make_frame("F0.PNG", 30, 5));
    decoded.push_back(make_frame("F1.PNG", 10, 10));

    const auto built = attach_stack_interaction_masks(adv_map, decoded);
    EXPECT_EQ(built, 1u);

    // Build pick index with zero-radius ellipse so we test alpha path only
    SelectionCircleGeometry sel_geo;
    sel_geo.center_offset_x = 0;
    sel_geo.center_offset_y = 0;
    sel_geo.radius_x = 0; // ellipse never matches
    sel_geo.radius_y = 0;

    AdventurePickIndex pick_index;
    pick_index.build(adv_map, sel_geo);
    ASSERT_FALSE(pick_index.empty());

    // Pixel (30,5) relative to draw_origin → must hit via alpha mask (frame 0)
    {
        const int  at_x = prim.draw_origin.x + 30;
        const int  at_y = prim.draw_origin.y + 5;
        const auto result = pick_index.hit_test(at_x, at_y);
        ASSERT_NE(result.interaction_target, nullptr);
        EXPECT_EQ(result.interaction_target->object_id, "S1");
    }

    // Pixel (10,10) relative to draw_origin → must hit via alpha union (frame 1)
    {
        const int  at_x = prim.draw_origin.x + 10;
        const int  at_y = prim.draw_origin.y + 10;
        const auto result = pick_index.hit_test(at_x, at_y);
        ASSERT_NE(result.interaction_target, nullptr);
        EXPECT_EQ(result.interaction_target->object_id, "S1");
    }

    // Pixel (70,70) relative to draw_origin (transparent in both frames)
    // Must NOT produce an alpha hit.
    {
        const int  at_x = prim.draw_origin.x + 70;
        const int  at_y = prim.draw_origin.y + 70;
        const auto result = pick_index.hit_test(at_x, at_y);
        EXPECT_EQ(result.interaction_target, nullptr)
            << "unrelated alpha-clear pixel must not hit via mask";
    }
}

// ======================================================================
// 35. Playback from contributor output end-to-end
// ======================================================================

TEST(AdventureUnitIdleAnimation, PlaybackFromContributorOutput) {
    auto resolver =
        [](const d2runtime::AdventureStack&        stack,
           const d2runtime::AdventureUnitInstance& leader) -> std::optional<AdventureActorVisual> {
        (void)stack;
        (void)leader;
        return AdventureActorVisual{.presentation_kind = AdventureActorPresentationKind::Unit,
                                    .resolved_owner_id = "UNKNOWN",
                                    .body =
                                        {
                                            .container_path = "Imgs/Isounit.ff",
                                            .logical_animation_name = "STOP3",
                                            .frames =
                                                {
                                                    {"F0.PNG", 80, 90},
                                                    {"F1.PNG", 100, 110},
                                                    {"F2.PNG", 90, 100},
                                                },
                                            .native_canvas_w = 320,
                                            .native_canvas_h = 320,
                                            .canvas_foot_x = 40,
                                            .canvas_foot_y = 80,
                                        },
                                    .shadow = std::nullopt};
    };

    AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.units.push_back({.id = "u", .type_id = "G000UU0003"});
    AdventureStack stack;
    stack.id = "S1";
    stack.position.x = 5;
    stack.position.y = 5;
    stack.leader_id = "u";
    stack.facing = AdventureIsoDirection::D0;
    world.stacks.push_back(stack);

    AdventureMapPreparer preparer(kAnimGeo);
    preparer.add_contributor(make_stack_actor_contributor(resolver));
    auto result = preparer.prepare(world);

    ASSERT_EQ(result.graph.world.size(), 1u);
    const auto& prim = result.graph.world[0];
    ASSERT_TRUE(prim.animation.has_value());
    ASSERT_EQ(prim.animation->frames.size(), 3u);

    // One-frame visual must NOT have animation data
    {
        auto one_frame = [](const d2runtime::AdventureStack&        stack,
                            const d2runtime::AdventureUnitInstance& leader)
            -> std::optional<AdventureActorVisual> {
            (void)stack;
            (void)leader;
            return AdventureActorVisual{.presentation_kind = AdventureActorPresentationKind::Unit,
                                        .resolved_owner_id = "UNKNOWN",
                                        .body =
                                            {
                                                .container_path = "Imgs/Isounit.ff",
                                                .logical_animation_name = "STOP0",
                                                .frames = {{"ONLY.PNG", 80, 90}},
                                                .native_canvas_w = 80,
                                                .native_canvas_h = 90,
                                                .canvas_foot_x = 40,
                                                .canvas_foot_y = 80,
                                            },
                                        .shadow = std::nullopt};
        };
        AdventureWorldState single_world;
        single_world.map_width = 10;
        single_world.map_height = 10;
        single_world.units.push_back({.id = "u2", .type_id = "G000UU0001"});
        AdventureStack s2;
        s2.id = "S2";
        s2.position.x = 1;
        s2.position.y = 1;
        s2.leader_id = "u2";
        s2.facing = AdventureIsoDirection::D0;
        single_world.stacks.push_back(s2);

        AdventureMapPreparer p2(kAnimGeo);
        p2.add_contributor(make_stack_actor_contributor(one_frame));
        auto r2 = p2.prepare(single_world);
        ASSERT_EQ(r2.graph.world.size(), 1u);
        EXPECT_FALSE(r2.graph.world[0].animation.has_value());
    }

    // Convert to sequence and verify production playback loop
    auto seq = adventure_animation_data_to_sequence(prim.container_path, *prim.animation);
    EXPECT_EQ(seq.name, "STOP3");
    EXPECT_EQ(seq.container_path, "Imgs/Isounit.ff");
    EXPECT_TRUE(seq.is_looping);
    ASSERT_EQ(seq.frames.size(), 3u);
    EXPECT_EQ(seq.frames[0].image_name, "F0.PNG");
    EXPECT_EQ(seq.frames[1].image_name, "F1.PNG");
    EXPECT_EQ(seq.frames[2].image_name, "F2.PNG");

    AnimationPlayer player(std::move(seq));
    player.play();

    EXPECT_EQ(player.current_frame_index(), 0u);
    EXPECT_EQ(player.current_frame().image_name, "F0.PNG");

    player.update(100.0f);
    EXPECT_EQ(player.current_frame_index(), 1u);
    EXPECT_EQ(player.current_frame().image_name, "F1.PNG");

    player.update(100.0f);
    EXPECT_EQ(player.current_frame_index(), 2u);
    EXPECT_EQ(player.current_frame().image_name, "F2.PNG");

    // Wrap around (looping)
    player.update(100.0f);
    EXPECT_EQ(player.current_frame_index(), 0u);
    EXPECT_EQ(player.current_frame().image_name, "F0.PNG");
    EXPECT_EQ(player.state(), AnimationPlayerState::Playing);
}

// ======================================================================
// 36. Current-direction loading: exact key set match
// ======================================================================

TEST(AdventureUnitIdleAnimation, CurrentDirectionExactPreloadKeys) {
    auto resolver =
        [](const d2runtime::AdventureStack&        stack,
           const d2runtime::AdventureUnitInstance& leader) -> std::optional<AdventureActorVisual> {
        (void)stack;
        (void)leader;
        return AdventureActorVisual{.presentation_kind = AdventureActorPresentationKind::Unit,
                                    .resolved_owner_id = "UNKNOWN",
                                    .body =
                                        {
                                            .container_path = "Imgs/Isounit.ff",
                                            .logical_animation_name = "G000UU0003STOP3",
                                            .frames =
                                                {
                                                    {"STOP3_F0.PNG", 80, 90},
                                                    {"STOP3_F1.PNG", 80, 90},
                                                    {"STOP3_F2.PNG", 80, 90},
                                                },
                                            .native_canvas_w = 320,
                                            .native_canvas_h = 320,
                                            .canvas_foot_x = 40,
                                            .canvas_foot_y = 80,
                                        },
                                    .shadow = std::nullopt};
    };

    AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.units.push_back({.id = "u", .type_id = "G000UU0003"});
    AdventureStack stack;
    stack.id = "S1";
    stack.position.x = 1;
    stack.position.y = 1;
    stack.leader_id = "u";
    stack.facing = AdventureIsoDirection::D3;
    world.stacks.push_back(stack);

    AdventureMapPreparer preparer(kAnimGeo);
    preparer.add_contributor(make_stack_actor_contributor(resolver));
    auto result = preparer.prepare(world);

    ASSERT_EQ(result.graph.world.size(), 1u);
    const auto& prim = result.graph.world[0];
    ASSERT_TRUE(prim.animation.has_value());

    // Collect expected keys from the primitive's animation frames
    std::unordered_set<ImageAssetKey, std::hash<ImageAssetKey>> expected_keys;
    for (const auto& af : prim.animation->frames) {
        expected_keys.insert(make_world_composed_sprite_key(prim.container_path, af.record_name));
    }

    // Collect actual preloaded keys
    const auto actual_key_list = collect_adventure_render_asset_keys(result.graph);
    std::unordered_set<ImageAssetKey, std::hash<ImageAssetKey>> actual_key_set(
        actual_key_list.begin(), actual_key_list.end());

    EXPECT_EQ(actual_key_set.size(), expected_keys.size());
    for (const auto& ek : expected_keys) {
        EXPECT_TRUE(actual_key_set.count(ek) == 1)
            << "missing expected key: " << ek.container_path << "/" << ek.image_name;
    }

    // Verify no STOP0/STOP1/STOP2/STOP4/STOP5/STOP6/STOP7
    for (const auto& key : actual_key_list) {
        EXPECT_TRUE(key.image_name.find("STOP0") == std::string::npos &&
                    key.image_name.find("STOP1") == std::string::npos &&
                    key.image_name.find("STOP2") == std::string::npos &&
                    key.image_name.find("STOP4") == std::string::npos &&
                    key.image_name.find("STOP5") == std::string::npos &&
                    key.image_name.find("STOP6") == std::string::npos &&
                    key.image_name.find("STOP7") == std::string::npos)
            << "unexpected direction asset: " << key.image_name;
    }
}

// ======================================================================
// 37. Directional cache isolation — resolve D0, D3, D5, D0 again
// ======================================================================

TEST(AdventureUnitIdleAnimation, DirectionalCacheIsolation) {
    FakeSpriteCatalog catalog;
    // Register distinct sequences for each direction
    catalog.animations["G000UU0003STOP0"] = {
        .name = "G000UU0003STOP0",
        .container_path = "Imgs/Isounit.ff",
        .frames = {{.image_name = "DIR0_F0", .index = 0, .duration_ms = 100}},
        .is_looping = false,
        .native_canvas_w = 80,
        .native_canvas_h = 90,
        .canvas_foot_x = 40,
        .canvas_foot_y = 80,
    };
    catalog.animations["G000UU0003STOP3"] = {
        .name = "G000UU0003STOP3",
        .container_path = "Imgs/Isounit.ff",
        .frames =
            {
                {.image_name = "DIR3_F0", .index = 0, .duration_ms = 100},
                {.image_name = "DIR3_F1", .index = 1, .duration_ms = 100},
            },
        .is_looping = true,
        .native_canvas_w = 320,
        .native_canvas_h = 320,
        .canvas_foot_x = 40,
        .canvas_foot_y = 80,
    };
    catalog.animations["G000UU0003STOP5"] = {
        .name = "G000UU0003STOP5",
        .container_path = "Imgs/Isounit.ff",
        .frames =
            {
                {.image_name = "DIR5_F0", .index = 0, .duration_ms = 100},
                {.image_name = "DIR5_F1", .index = 1, .duration_ms = 100},
                {.image_name = "DIR5_F2", .index = 2, .duration_ms = 100},
            },
        .is_looping = true,
        .native_canvas_w = 320,
        .native_canvas_h = 320,
        .canvas_foot_x = 40,
        .canvas_foot_y = 80,
    };
    catalog.metas["DIR0_F0"] = {80, 90};
    catalog.metas["DIR3_F0"] = {80, 90};
    catalog.metas["DIR3_F1"] = {80, 90};
    catalog.metas["DIR5_F0"] = {80, 90};
    catalog.metas["DIR5_F1"] = {80, 90};
    catalog.metas["DIR5_F2"] = {80, 90};

    TempDir tmp("d2_cache_isolation_test");
    for (const auto& name :
         {"Tglobal.dbf", "Gattacks.dbf", "Gunits.dbf", "Gupgrade.dbf", "Graces.dbf"}) {
        std::ofstream(tmp.path() / name).put(0x1a);
    }
    GameDataRegistry game_data(tmp.path());

    IsoActorVisualResolver resolver(catalog, game_data);

    // D0 → D0 frames
    AdventureStackActorVisualRequest req;
    req.presentation = {AdventureActorPresentationKind::Unit};
    req.leader_unit_type_id = "G000UU0003";
    req.direction = AdventureIsoDirection::D0;
    const auto d0 = resolver.resolve(req);
    ASSERT_TRUE(d0.has_value());
    EXPECT_EQ(d0->body.animation_name, "G000UU0003STOP0");
    ASSERT_EQ(d0->body.frames.size(), 1u);
    EXPECT_EQ(d0->body.frames[0].record_name, "DIR0_F0");

    // D3 → D3 frames (different)
    req.direction = AdventureIsoDirection::D3;
    const auto d3 = resolver.resolve(req);
    ASSERT_TRUE(d3.has_value());
    EXPECT_EQ(d3->body.animation_name, "G000UU0003STOP3");
    ASSERT_EQ(d3->body.frames.size(), 2u);
    EXPECT_EQ(d3->body.frames[0].record_name, "DIR3_F0");
    EXPECT_EQ(d3->body.frames[1].record_name, "DIR3_F1");

    // D5 → D5 frames (different again)
    req.direction = AdventureIsoDirection::D5;
    const auto d5 = resolver.resolve(req);
    ASSERT_TRUE(d5.has_value());
    EXPECT_EQ(d5->body.animation_name, "G000UU0003STOP5");
    ASSERT_EQ(d5->body.frames.size(), 3u);
    EXPECT_EQ(d5->body.frames[0].record_name, "DIR5_F0");

    // Second D0 must return D0 frames (not cached as D3 or D5)
    req.direction = AdventureIsoDirection::D0;
    const auto d0_again = resolver.resolve(req);
    ASSERT_TRUE(d0_again.has_value());
    EXPECT_EQ(d0_again->body.animation_name, "G000UU0003STOP0");
    ASSERT_EQ(d0_again->body.frames.size(), 1u);
    EXPECT_EQ(d0_again->body.frames[0].record_name, "DIR0_F0");
}

// ======================================================================
// 38. Strict frame-resolution tests for resolve_adventure_frame_visual()
// ======================================================================

TEST(AdventureUnitIdleAnimation, AnimatedPrimitiveNoIndexThrows) {
    PreparedAdventureRenderPrimitive prim;
    prim.stable_id = stable_render_id("Stack:S1");
    prim.debug_label = "StackLeader:S1";
    prim.animation = AdventureAnimationData{};
    prim.animation->animation_name = "STOP3";
    prim.animation->frames.push_back({"F0", 100, 80, 90});
    prim.animation->frames.push_back({"F1", 100, 80, 90});

    EXPECT_THROW(
        {
            try {
                auto r = resolve_adventure_frame_visual(prim, std::nullopt);
                (void)r;
            } catch (const std::logic_error& e) {
                std::string msg(e.what());
                EXPECT_TRUE(msg.find("S1") != std::string::npos) << msg;
                EXPECT_TRUE(msg.find("StackLeader:S1") != std::string::npos) << msg;
                throw;
            }
        },
        std::logic_error);
}

TEST(AdventureUnitIdleAnimation, AnimatedPrimitiveIndexEqualsCountThrows) {
    PreparedAdventureRenderPrimitive prim;
    prim.stable_id = stable_render_id("Stack:S1");
    prim.debug_label = "StackLeader:S1";
    prim.animation = AdventureAnimationData{};
    prim.animation->animation_name = "STOP3";
    prim.animation->frames.push_back({"F0", 100, 80, 90});
    prim.animation->frames.push_back({"F1", 100, 80, 90});

    EXPECT_THROW(
        {
            try {
                auto r = resolve_adventure_frame_visual(prim, 2);
                (void)r;
            } catch (const std::logic_error& e) {
                std::string msg(e.what());
                EXPECT_TRUE(msg.find(std::to_string(prim.stable_id)) != std::string::npos) << msg;
                EXPECT_TRUE(msg.find("S1") != std::string::npos) << msg;
                throw;
            }
        },
        std::logic_error);
}

TEST(AdventureUnitIdleAnimation, AnimatedPrimitiveIndexLargerThrows) {
    PreparedAdventureRenderPrimitive prim;
    prim.stable_id = stable_render_id("Stack:S1");
    prim.debug_label = "StackLeader:S1";
    prim.animation = AdventureAnimationData{};
    prim.animation->animation_name = "STOP3";
    prim.animation->frames.push_back({"F0", 100, 80, 90});

    EXPECT_THROW(
        {
            try {
                auto r = resolve_adventure_frame_visual(prim, 999);
                (void)r;
            } catch (const std::logic_error& e) {
                std::string msg(e.what());
                EXPECT_TRUE(msg.find(std::to_string(prim.stable_id)) != std::string::npos) << msg;
                EXPECT_TRUE(msg.find("S1") != std::string::npos) << msg;
                throw;
            }
        },
        std::logic_error);
}

TEST(AdventureUnitIdleAnimation, StaticPrimitiveWithIndexThrows) {
    PreparedAdventureRenderPrimitive prim;
    prim.stable_id = stable_render_id("Stack:S1");
    prim.debug_label = "StackLeader:S1";
    prim.record_name = "STATIC.PNG";
    prim.src_width = 80;
    prim.src_height = 90;

    EXPECT_THROW(
        {
            try {
                auto r = resolve_adventure_frame_visual(prim, 0);
                (void)r;
            } catch (const std::logic_error& e) {
                std::string msg(e.what());
                EXPECT_TRUE(msg.find("S1") != std::string::npos) << msg;
                EXPECT_TRUE(msg.find("StackLeader:S1") != std::string::npos) << msg;
                throw;
            }
        },
        std::logic_error);
}

TEST(AdventureUnitIdleAnimation, AnimatedPrimitiveValidIndexReturnsExactRecord) {
    PreparedAdventureRenderPrimitive prim;
    prim.stable_id = stable_render_id("Stack:S1");
    prim.debug_label = "StackLeader:S1";
    prim.draw_origin = {100, 200};
    prim.animation = AdventureAnimationData{};
    prim.animation->animation_name = "STOP3";
    prim.animation->frames.push_back({"F0.PNG", 100, 80, 90});
    prim.animation->frames.push_back({"F1.PNG", 100, 100, 110});

    const auto resolved = resolve_adventure_frame_visual(prim, 1);
    EXPECT_EQ(resolved.record_name, "F1.PNG");
    EXPECT_EQ(resolved.draw_origin.x, 100);
    EXPECT_EQ(resolved.draw_origin.y, 200);
    EXPECT_EQ(resolved.src_width, 100);
    EXPECT_EQ(resolved.src_height, 110);
}

TEST(AdventureUnitIdleAnimation, StaticPrimitiveNullIndexReturnsExactRecord) {
    PreparedAdventureRenderPrimitive prim;
    prim.stable_id = stable_render_id("Stack:S1");
    prim.debug_label = "StackLeader:S1";
    prim.draw_origin = {100, 200};
    prim.record_name = "STATIC.PNG";
    prim.src_width = 80;
    prim.src_height = 90;

    const auto resolved = resolve_adventure_frame_visual(prim, std::nullopt);
    EXPECT_EQ(resolved.record_name, "STATIC.PNG");
    EXPECT_EQ(resolved.draw_origin.x, 100);
    EXPECT_EQ(resolved.draw_origin.y, 200);
    EXPECT_EQ(resolved.src_width, 80);
    EXPECT_EQ(resolved.src_height, 90);
}

// ======================================================================
// 39. Empty-sequence resolver test
// ======================================================================

TEST(AdventureUnitIdleAnimation, EmptySequenceIdentityResolvesToNullopt) {
    FakeSpriteCatalog catalog;
    // Register an animation with empty frames
    catalog.animations["G000UU0003STOP0"] = {
        .name = "G000UU0003STOP0",
        .container_path = "Imgs/Isounit.ff",
        .frames = {},
        .is_looping = false,
        .native_canvas_w = 320,
        .native_canvas_h = 320,
        .canvas_foot_x = 40,
        .canvas_foot_y = 80,
    };

    TempDir tmp("d2_empty_seq_test");
    for (const auto& name :
         {"Tglobal.dbf", "Gattacks.dbf", "Gunits.dbf", "Gupgrade.dbf", "Graces.dbf"}) {
        std::ofstream(tmp.path() / name).put(0x1a);
    }
    GameDataRegistry game_data(tmp.path());

    IsoActorVisualResolver resolver(catalog, game_data);

    // Empty frames → fatal malformed metadata per canonical classification
    {
        AdventureStackActorVisualRequest req;
        req.presentation = {AdventureActorPresentationKind::Unit};
        req.leader_unit_type_id = "G000UU0003";
        req.direction = AdventureIsoDirection::D0;
        EXPECT_THROW((void)resolver.resolve(req), std::runtime_error);
    }
}

TEST(AdventureUnitIdleAnimation, AbsentAnimationIdentityResolvesToNullopt) {
    FakeSpriteCatalog catalog;
    TempDir           tmp("d2_absent_anim_test");
    for (const auto& name :
         {"Tglobal.dbf", "Gattacks.dbf", "Gunits.dbf", "Gupgrade.dbf", "Graces.dbf"}) {
        std::ofstream(tmp.path() / name).put(0x1a);
    }
    GameDataRegistry game_data(tmp.path());

    IsoActorVisualResolver resolver(catalog, game_data);

    // Completely absent animation identity → unresolved
    {
        AdventureStackActorVisualRequest req;
        req.presentation = {AdventureActorPresentationKind::Unit};
        req.leader_unit_type_id = "G000UU_NONEXIST";
        req.direction = AdventureIsoDirection::D0;
        EXPECT_FALSE(resolver.resolve(req).has_value());
    }
}

// ======================================================================
// 40. Malformed visual failure through production contributor
// ======================================================================

namespace {

struct MalformedVisualFixture {
    AdventureWorldState  world;
    AdventureMapPreparer preparer;

    MalformedVisualFixture() : preparer(kAnimGeo) {
        world.map_width = 10;
        world.map_height = 10;
        world.units.push_back({.id = "u", .type_id = "G000UU0003"});
        AdventureStack stack;
        stack.id = "S1";
        stack.position.x = 5;
        stack.position.y = 5;
        stack.leader_id = "u";
        stack.facing = AdventureIsoDirection::D0;
        world.stacks.push_back(stack);
    }

    void check_throw(const std::string& expected_substring) {
        EXPECT_THROW(
            {
                try {
                    auto result = preparer.prepare(world);
                    (void)result;
                } catch (const std::runtime_error& e) {
                    std::string msg(e.what());
                    EXPECT_TRUE(msg.find("S1") != std::string::npos)
                        << "missing stack ID in: " << msg;
                    EXPECT_TRUE(msg.find("u") != std::string::npos)
                        << "missing leader ID in: " << msg;
                    EXPECT_TRUE(msg.find("G000UU0003") != std::string::npos)
                        << "missing type ID in: " << msg;
                    EXPECT_TRUE(msg.find(expected_substring) != std::string::npos)
                        << "missing reason in: " << msg;
                    throw;
                }
            },
            std::runtime_error);
    }
};

} // namespace

TEST(AdventureUnitIdleAnimation, MalformedVisualEmptyContainer) {
    MalformedVisualFixture fix;
    fix.preparer.add_contributor(make_stack_actor_contributor(
        [](const d2runtime::AdventureStack&,
           const d2runtime::AdventureUnitInstance&) -> std::optional<AdventureActorVisual> {
            return AdventureActorVisual{.presentation_kind = AdventureActorPresentationKind::Unit,
                                        .resolved_owner_id = "UNKNOWN",
                                        .body =
                                            {
                                                .container_path = "",
                                                .logical_animation_name = "STOP0",
                                                .frames = {{"F0.PNG", 80, 90}},
                                                .native_canvas_w = 80,
                                                .native_canvas_h = 90,
                                                .canvas_foot_x = 40,
                                                .canvas_foot_y = 80,
                                            },
                                        .shadow = std::nullopt};
        }));
    fix.check_throw("empty_container_path");
}

TEST(AdventureUnitIdleAnimation, MalformedVisualEmptyAnimationName) {
    MalformedVisualFixture fix;
    fix.preparer.add_contributor(make_stack_actor_contributor(
        [](const d2runtime::AdventureStack&,
           const d2runtime::AdventureUnitInstance&) -> std::optional<AdventureActorVisual> {
            return AdventureActorVisual{.presentation_kind = AdventureActorPresentationKind::Unit,
                                        .resolved_owner_id = "UNKNOWN",
                                        .body =
                                            {
                                                .container_path = "Imgs/Isounit.ff",
                                                .logical_animation_name = "",
                                                .frames = {{"F0.PNG", 80, 90}},
                                                .native_canvas_w = 80,
                                                .native_canvas_h = 90,
                                                .canvas_foot_x = 40,
                                                .canvas_foot_y = 80,
                                            },
                                        .shadow = std::nullopt};
        }));
    fix.check_throw("empty_animation_name");
}

TEST(AdventureUnitIdleAnimation, MalformedVisualZeroFrames) {
    MalformedVisualFixture fix;
    fix.preparer.add_contributor(make_stack_actor_contributor(
        [](const d2runtime::AdventureStack&,
           const d2runtime::AdventureUnitInstance&) -> std::optional<AdventureActorVisual> {
            return AdventureActorVisual{.presentation_kind = AdventureActorPresentationKind::Unit,
                                        .resolved_owner_id = "UNKNOWN",
                                        .body =
                                            {
                                                .container_path = "Imgs/Isounit.ff",
                                                .logical_animation_name = "STOP0",
                                                .frames = {},
                                                .native_canvas_w = 80,
                                                .native_canvas_h = 90,
                                                .canvas_foot_x = 40,
                                                .canvas_foot_y = 80,
                                            },
                                        .shadow = std::nullopt};
        }));
    fix.check_throw("zero_frames");
}

TEST(AdventureUnitIdleAnimation, MalformedVisualZeroCanvas) {
    MalformedVisualFixture fix;
    fix.preparer.add_contributor(make_stack_actor_contributor(
        [](const d2runtime::AdventureStack&,
           const d2runtime::AdventureUnitInstance&) -> std::optional<AdventureActorVisual> {
            return AdventureActorVisual{.presentation_kind = AdventureActorPresentationKind::Unit,
                                        .resolved_owner_id = "UNKNOWN",
                                        .body =
                                            {
                                                .container_path = "Imgs/Isounit.ff",
                                                .logical_animation_name = "STOP0",
                                                .frames = {{"F0.PNG", 80, 90}},
                                                .native_canvas_w = 0,
                                                .native_canvas_h = 0,
                                                .canvas_foot_x = 40,
                                                .canvas_foot_y = 80,
                                            },
                                        .shadow = std::nullopt};
        }));
    fix.check_throw("invalid_canvas");
}

TEST(AdventureUnitIdleAnimation, MalformedVisualEmptyFrameRecord) {
    MalformedVisualFixture fix;
    fix.preparer.add_contributor(make_stack_actor_contributor(
        [](const d2runtime::AdventureStack&,
           const d2runtime::AdventureUnitInstance&) -> std::optional<AdventureActorVisual> {
            return AdventureActorVisual{.presentation_kind = AdventureActorPresentationKind::Unit,
                                        .resolved_owner_id = "UNKNOWN",
                                        .body =
                                            {
                                                .container_path = "Imgs/Isounit.ff",
                                                .logical_animation_name = "STOP0",
                                                .frames = {{"", 80, 90}},
                                                .native_canvas_w = 80,
                                                .native_canvas_h = 90,
                                                .canvas_foot_x = 40,
                                                .canvas_foot_y = 80,
                                            },
                                        .shadow = std::nullopt};
        }));
    fix.check_throw("empty_frame_record");
}

TEST(AdventureUnitIdleAnimation, MalformedVisualZeroFrameWidth) {
    MalformedVisualFixture fix;
    fix.preparer.add_contributor(make_stack_actor_contributor(
        [](const d2runtime::AdventureStack&,
           const d2runtime::AdventureUnitInstance&) -> std::optional<AdventureActorVisual> {
            return AdventureActorVisual{.presentation_kind = AdventureActorPresentationKind::Unit,
                                        .resolved_owner_id = "UNKNOWN",
                                        .body =
                                            {
                                                .container_path = "Imgs/Isounit.ff",
                                                .logical_animation_name = "STOP0",
                                                .frames = {{"F0.PNG", 80, 0}},
                                                .native_canvas_w = 80,
                                                .native_canvas_h = 90,
                                                .canvas_foot_x = 40,
                                                .canvas_foot_y = 80,
                                            },
                                        .shadow = std::nullopt};
        }));
    fix.check_throw("invalid_frame_dimensions");
}

TEST(AdventureUnitIdleAnimation, MalformedVisualZeroFrameHeight) {
    MalformedVisualFixture fix;
    fix.preparer.add_contributor(make_stack_actor_contributor(
        [](const d2runtime::AdventureStack&,
           const d2runtime::AdventureUnitInstance&) -> std::optional<AdventureActorVisual> {
            return AdventureActorVisual{.presentation_kind = AdventureActorPresentationKind::Unit,
                                        .resolved_owner_id = "UNKNOWN",
                                        .body =
                                            {
                                                .container_path = "Imgs/Isounit.ff",
                                                .logical_animation_name = "STOP0",
                                                .frames = {{"F0.PNG", 80, 0}},
                                                .native_canvas_w = 80,
                                                .native_canvas_h = 90,
                                                .canvas_foot_x = 40,
                                                .canvas_foot_y = 80,
                                            },
                                        .shadow = std::nullopt};
        }));
    fix.check_throw("invalid_frame_dimensions");
}

// ======================================================================
// 41. No primitive/pick entry committed before malformed visual failure
// ======================================================================

TEST(AdventureUnitIdleAnimation, MalformedVisualNoPrimitiveCommitted) {
    AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.units.push_back({.id = "u", .type_id = "G000UU0003"});
    AdventureStack stack;
    stack.id = "S1";
    stack.position.x = 5;
    stack.position.y = 5;
    stack.leader_id = "u";
    stack.facing = AdventureIsoDirection::D0;
    world.stacks.push_back(stack);

    AdventureMapPreparer preparer(kAnimGeo);
    preparer.add_contributor(make_stack_actor_contributor(
        [](const d2runtime::AdventureStack&,
           const d2runtime::AdventureUnitInstance&) -> std::optional<AdventureActorVisual> {
            return AdventureActorVisual{.presentation_kind = AdventureActorPresentationKind::Unit,
                                        .resolved_owner_id = "UNKNOWN",
                                        .body =
                                            {
                                                .container_path = "",
                                                .logical_animation_name = "STOP0",
                                                .frames = {{"F0.PNG", 80, 90}},
                                                .native_canvas_w = 80,
                                                .native_canvas_h = 90,
                                                .canvas_foot_x = 40,
                                                .canvas_foot_y = 80,
                                            },
                                        .shadow = std::nullopt};
        }));

    EXPECT_THROW(
        {
            auto result = preparer.prepare(world);
            (void)result;
        },
        std::runtime_error);
    // After the prepare() call fails at contributor time, the PrepareResult
    // is never constructed — the exception propagates directly.
}

// ======================================================================
// 42. Invalid RGBA buffer via attach_stack_interaction_masks — zero-sized
// ======================================================================

TEST(AdventureUnitIdleAnimation, ZeroSizedFrameBuffer) {
    PreparedAdventureMap adv_map;
    adv_map.geometry = kAnimGeo;

    PreparedAdventureRenderPrimitive prim;
    prim.stable_id = stable_render_id("Stack:S1");
    prim.debug_label = "StackLeader:S1";
    prim.level = WorldRenderLevel::Actor;
    prim.container_path = "test_cont";
    prim.record_name = "F0.PNG";
    prim.depth_anchor = {5, 5};
    prim.draw_origin = {0, 0};

    AdventureAnimationData anim;
    anim.animation_name = "STOP3";
    anim.is_looping = true;
    anim.frames.push_back({"F0.PNG", 100, 80, 90});
    anim.frames.push_back({"F1.PNG", 100, 80, 90});
    prim.animation = std::move(anim);

    const auto* mask_before = prim.interaction_mask.get();

    adv_map.world_graph.world.push_back(std::move(prim));
    adv_map.pick_entries.push_back(PickEntry{.stable_id = stable_render_id("Stack:S1"),
                                             .kind = PickEntryKind::Stack,
                                             .object_id = "S1"});

    // Zero-sized decoded buffer for F0 (width=0)
    d2res::RgbaBuffer fb;
    fb.width = 0;
    fb.height = 1;
    fb.rgba.resize(4, 0xFF);
    auto pixels = std::make_shared<const d2res::RgbaBuffer>(std::move(fb));
    auto img = std::make_shared<PreparedImage>(PreparedImage{
        .key = make_world_composed_sprite_key("test_cont", "F0.PNG"), .pixels = pixels});
    std::vector<PreparedImageResult> decoded;
    decoded.push_back({.key = make_world_composed_sprite_key("test_cont", "F0.PNG"),
                       .image = img,
                       .success = true});

    // Zero-sized buffer is tolerated — no valid frames, mask not built
    std::size_t built = attach_stack_interaction_masks(adv_map, decoded);
    EXPECT_EQ(built, 0u) << "no mask built from zero-sized frame";
    const auto& stored = adv_map.world_graph.world[0].interaction_mask;
    EXPECT_EQ(stored.get(), mask_before) << "interaction_mask must not change when no frames match";
}

// ======================================================================
// 43. Invalid RGBA buffer via attach_stack_interaction_masks — undersized
// ======================================================================

TEST(AdventureUnitIdleAnimation, UndersizedFrameBuffer) {
    PreparedAdventureMap adv_map;
    adv_map.geometry = kAnimGeo;

    PreparedAdventureRenderPrimitive prim;
    prim.stable_id = stable_render_id("Stack:S1");
    prim.debug_label = "StackLeader:S1";
    prim.level = WorldRenderLevel::Actor;
    prim.container_path = "test_cont";
    prim.record_name = "F0.PNG";
    prim.depth_anchor = {5, 5};
    prim.draw_origin = {0, 0};

    AdventureAnimationData anim;
    anim.animation_name = "STOP3";
    anim.is_looping = true;
    anim.frames.push_back({"F0.PNG", 100, 80, 90});
    anim.frames.push_back({"F1.PNG", 100, 80, 90});
    prim.animation = std::move(anim);

    const auto* mask_before = prim.interaction_mask.get();

    adv_map.world_graph.world.push_back(std::move(prim));
    adv_map.pick_entries.push_back(PickEntry{.stable_id = stable_render_id("Stack:S1"),
                                             .kind = PickEntryKind::Stack,
                                             .object_id = "S1"});

    // Undersized decoded buffer for F0 (RGBA smaller than width*height*4)
    d2res::RgbaBuffer fb;
    fb.width = 80;
    fb.height = 90;
    fb.rgba.resize(100, 0xFF); // only 100 bytes, need 28800
    auto pixels = std::make_shared<const d2res::RgbaBuffer>(std::move(fb));
    auto img = std::make_shared<PreparedImage>(PreparedImage{
        .key = make_world_composed_sprite_key("test_cont", "F0.PNG"), .pixels = pixels});
    std::vector<PreparedImageResult> decoded;
    decoded.push_back({.key = make_world_composed_sprite_key("test_cont", "F0.PNG"),
                       .image = img,
                       .success = true});

    // Undersized buffer is tolerated — no valid frames, mask not built
    std::size_t built = attach_stack_interaction_masks(adv_map, decoded);
    EXPECT_EQ(built, 0u) << "no mask built from undersized frame";
    const auto& stored = adv_map.world_graph.world[0].interaction_mask;
    EXPECT_EQ(stored.get(), mask_before) << "interaction_mask must not change when no frames match";
}

// ======================================================================
// 42. Deterministic exact-owner composite resolver test
// ======================================================================

TEST(AdventureUnitIdleShadow, ExactOwnerCompositeResolution) {
    FakeSpriteCatalog catalog;
    catalog.animations["G000UU0001STOP3"] = {
        .name = "G000UU0001STOP3",
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
    catalog.animations["G000UU0001SSTO3"] = {
        .name = "G000UU0001SSTO3",
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
    catalog.metas["B0"] = {80, 90};
    catalog.metas["B1"] = {80, 90};
    catalog.metas["B2"] = {80, 90};
    catalog.metas["S0"] = {60, 50};
    catalog.metas["S1"] = {60, 50};
    catalog.metas["S2"] = {60, 50};

    TempDir tmp("d2_exact_owner_test");
    for (const auto& name :
         {"Tglobal.dbf", "Gattacks.dbf", "Gunits.dbf", "Gupgrade.dbf", "Graces.dbf"})
        std::ofstream(tmp.path() / name).put(0x1a);
    GameDataRegistry game_data(tmp.path());

    IsoActorVisualResolver           resolver(catalog, game_data);
    AdventureStackActorVisualRequest req;
    req.presentation = {AdventureActorPresentationKind::Unit};
    req.leader_unit_type_id = "G000UU0001";
    req.direction = AdventureIsoDirection::D3;
    const auto visual = resolver.resolve(req);
    ASSERT_TRUE(visual.has_value());
    EXPECT_EQ(visual->resolved_owner_id, "g000uu0001");
    EXPECT_EQ(visual->body.animation_name, "G000UU0001STOP3");
    ASSERT_EQ(visual->body.frames.size(), 3u);
    EXPECT_EQ(visual->body.frames[0].record_name, "B0");
    EXPECT_EQ(visual->body.frames[1].record_name, "B1");
    EXPECT_EQ(visual->body.frames[2].record_name, "B2");

    ASSERT_TRUE(visual->shadow.has_value());
    EXPECT_EQ(visual->shadow->animation_name, "G000UU0001SSTO3");
    ASSERT_EQ(visual->shadow->frames.size(), 3u);
    EXPECT_EQ(visual->shadow->frames[0].record_name, "S0");
    EXPECT_EQ(visual->shadow->frames[1].record_name, "S1");
    EXPECT_EQ(visual->shadow->frames[2].record_name, "S2");
    EXPECT_EQ(visual->shadow->native_canvas_w, 200);
    EXPECT_EQ(visual->shadow->native_canvas_h, 150);
    EXPECT_EQ(visual->shadow->canvas_foot_x, 30);
    EXPECT_EQ(visual->shadow->canvas_foot_y, 60);

    EXPECT_EQ(visual->body.native_canvas_w, 320);
    EXPECT_EQ(visual->body.native_canvas_h, 320);
    EXPECT_EQ(visual->body.canvas_foot_x, 40);
    EXPECT_EQ(visual->body.canvas_foot_y, 80);
}

// ======================================================================
// 43. Body-fallback/shadow-owner: shadow follows actual body owner
// ======================================================================

TEST(AdventureUnitIdleShadow, BodyFallbackAndShadowOwner) {
    using namespace test_dbf;
    FakeSpriteCatalog catalog;
    catalog.animations["G000UU0005STOP5"] = {
        .name = "G000UU0005STOP5",
        .container_path = "Imgs/Isounit.ff",
        .frames = {{.image_name = "BF0", .index = 0, .duration_ms = 100}},
        .is_looping = false,
        .native_canvas_w = 320,
        .native_canvas_h = 320,
        .canvas_foot_x = 40,
        .canvas_foot_y = 80,
    };
    catalog.animations["G000UU0005SSTO5"] = {
        .name = "G000UU0005SSTO5",
        .container_path = "Imgs/Isounit.ff",
        .frames = {{.image_name = "BS0", .index = 0, .duration_ms = 100}},
        .is_looping = false,
        .native_canvas_w = 200,
        .native_canvas_h = 150,
        .canvas_foot_x = 30,
        .canvas_foot_y = 60,
    };
    catalog.metas["BF0"] = {80, 90};
    catalog.metas["BS0"] = {60, 50};

    TempDir    tmp("d2_body_fallback_test");
    DbfBuilder gunits({{"UNIT_ID", 'C', 20}, {"NAME", 'C', 20}, {"BASE_UNIT", 'C', 20}});
    gunits.add_record({"G000UU0003", "derived", "G000UU0005"});
    gunits.add_record({"G000UU0005", "base", ""});
    gunits.write(tmp.path() / "Gunits.dbf");
    for (const auto& name : {"Tglobal.dbf", "Gattacks.dbf", "Gupgrade.dbf", "Graces.dbf"})
        std::ofstream(tmp.path() / name).put(0x1a);
    GameDataRegistry game_data(tmp.path());

    IsoActorVisualResolver           resolver(catalog, game_data);
    AdventureStackActorVisualRequest req;
    req.presentation = {AdventureActorPresentationKind::Unit};
    req.leader_unit_type_id = "G000UU0003";
    req.direction = AdventureIsoDirection::D5;
    const auto visual = resolver.resolve(req);
    ASSERT_TRUE(visual.has_value());
    EXPECT_EQ(visual->resolved_owner_id, "g000uu0005");
    EXPECT_EQ(visual->body.animation_name, "G000UU0005STOP5");
    ASSERT_TRUE(visual->shadow.has_value());
    EXPECT_EQ(visual->shadow->animation_name, "G000UU0005SSTO5");
    ASSERT_EQ(visual->body.frames.size(), 1u);
    EXPECT_EQ(visual->body.frames[0].record_name, "BF0");
    ASSERT_EQ(visual->shadow->frames.size(), 1u);
    EXPECT_EQ(visual->shadow->frames[0].record_name, "BS0");
}

// ======================================================================
// 44. No independent shadow fallback regression
// ======================================================================

TEST(AdventureUnitIdleShadow, NoIndependentShadowFallback) {
    using namespace test_dbf;
    FakeSpriteCatalog catalog;
    catalog.animations["G000UU0003STOP3"] = {
        .name = "G000UU0003STOP3",
        .container_path = "Imgs/Isounit.ff",
        .frames = {{.image_name = "DF0", .index = 0, .duration_ms = 100}},
        .is_looping = false,
        .native_canvas_w = 320,
        .native_canvas_h = 320,
        .canvas_foot_x = 40,
        .canvas_foot_y = 80,
    };
    catalog.animations["G000UU0005STOP3"] = {
        .name = "G000UU0005STOP3",
        .container_path = "Imgs/Isounit.ff",
        .frames = {{.image_name = "BF0", .index = 0, .duration_ms = 100}},
        .is_looping = false,
        .native_canvas_w = 320,
        .native_canvas_h = 320,
        .canvas_foot_x = 40,
        .canvas_foot_y = 80,
    };
    catalog.animations["G000UU0005SSTO3"] = {
        .name = "G000UU0005SSTO3",
        .container_path = "Imgs/Isounit.ff",
        .frames = {{.image_name = "BS0", .index = 0, .duration_ms = 100}},
        .is_looping = false,
        .native_canvas_w = 200,
        .native_canvas_h = 150,
        .canvas_foot_x = 30,
        .canvas_foot_y = 60,
    };
    catalog.metas["DF0"] = {80, 90};
    catalog.metas["BF0"] = {80, 90};
    catalog.metas["BS0"] = {60, 50};

    TempDir    tmp("d2_no_shadow_fallback_test");
    DbfBuilder gunits({{"UNIT_ID", 'C', 20}, {"NAME", 'C', 20}, {"BASE_UNIT", 'C', 20}});
    gunits.add_record({"G000UU0003", "derived", "G000UU0005"});
    gunits.add_record({"G000UU0005", "base", ""});
    gunits.write(tmp.path() / "Gunits.dbf");
    for (const auto& name : {"Tglobal.dbf", "Gattacks.dbf", "Gupgrade.dbf", "Graces.dbf"})
        std::ofstream(tmp.path() / name).put(0x1a);
    GameDataRegistry game_data(tmp.path());

    IsoActorVisualResolver           resolver(catalog, game_data);
    AdventureStackActorVisualRequest req;
    req.presentation = {AdventureActorPresentationKind::Unit};
    req.leader_unit_type_id = "G000UU0003";
    req.direction = AdventureIsoDirection::D3;
    const auto visual = resolver.resolve(req);
    ASSERT_TRUE(visual.has_value());
    EXPECT_EQ(visual->resolved_owner_id, "g000uu0003");
    EXPECT_EQ(visual->body.animation_name, "G000UU0003STOP3");
    EXPECT_FALSE(visual->shadow.has_value())
        << "shadow must be absent when owner lacks SSTOn, even if base has it";
}

// ======================================================================
// 45. Present-but-malformed shadow regression
// ======================================================================

TEST(AdventureUnitIdleShadow, PresentMalformedShadowThrows) {
    {
        FakeSpriteCatalog catalog;
        catalog.animations["G000UU0001STOP3"] = {
            .name = "G000UU0001STOP3",
            .container_path = "Imgs/Isounit.ff",
            .frames = {{.image_name = "B0", .index = 0, .duration_ms = 100}},
            .is_looping = false,
            .native_canvas_w = 320,
            .native_canvas_h = 320,
            .canvas_foot_x = 40,
            .canvas_foot_y = 80,
        };
        catalog.animations["G000UU0001SSTO3"] = {
            .name = "G000UU0001SSTO3",
            .container_path = "Imgs/Isounit.ff",
            .frames = {},
            .is_looping = false,
            .native_canvas_w = 200,
            .native_canvas_h = 150,
            .canvas_foot_x = 30,
            .canvas_foot_y = 60,
        };
        catalog.metas["B0"] = {80, 90};
        TempDir tmp("d2_malformed_shadow_1");
        for (const auto& n :
             {"Tglobal.dbf", "Gattacks.dbf", "Gunits.dbf", "Gupgrade.dbf", "Graces.dbf"})
            std::ofstream(tmp.path() / n).put(0x1a);
        GameDataRegistry       gd(tmp.path());
        IsoActorVisualResolver r(catalog, gd);
        EXPECT_THROW(
            {
                try {
                    AdventureStackActorVisualRequest req;
                    req.presentation = {AdventureActorPresentationKind::Unit};
                    req.leader_unit_type_id = "G000UU0001";
                    req.direction = AdventureIsoDirection::D3;
                    static_cast<void>(r.resolve(req));
                } catch (const std::runtime_error& e) {
                    std::string m(e.what());
                    EXPECT_NE(m.find("G000UU0001SSTO3"), std::string::npos);
                    EXPECT_NE(m.find("empty_sequence"), std::string::npos);
                    throw;
                }
            },
            std::runtime_error);
    }
    {
        FakeSpriteCatalog catalog;
        catalog.animations["G000UU0002STOP3"] = {
            .name = "G000UU0002STOP3",
            .container_path = "Imgs/Isounit.ff",
            .frames = {{.image_name = "B0", .index = 0, .duration_ms = 100}},
            .is_looping = false,
            .native_canvas_w = 320,
            .native_canvas_h = 320,
            .canvas_foot_x = 40,
            .canvas_foot_y = 80,
        };
        catalog.animations["G000UU0002SSTO3"] = {
            .name = "G000UU0002SSTO3",
            .container_path = "Imgs/Isounit.ff",
            .frames = {{.image_name = "S0", .index = 0, .duration_ms = 100}},
            .is_looping = false,
            .native_canvas_w = 0,
            .native_canvas_h = 0,
            .canvas_foot_x = 30,
            .canvas_foot_y = 60,
        };
        catalog.metas["B0"] = {80, 90};
        catalog.metas["S0"] = {60, 50};
        TempDir tmp("d2_malformed_shadow_2");
        for (const auto& n :
             {"Tglobal.dbf", "Gattacks.dbf", "Gunits.dbf", "Gupgrade.dbf", "Graces.dbf"})
            std::ofstream(tmp.path() / n).put(0x1a);
        GameDataRegistry       gd(tmp.path());
        IsoActorVisualResolver r(catalog, gd);
        EXPECT_THROW(
            {
                try {
                    AdventureStackActorVisualRequest req;
                    req.presentation = {AdventureActorPresentationKind::Unit};
                    req.leader_unit_type_id = "G000UU0002";
                    req.direction = AdventureIsoDirection::D3;
                    static_cast<void>(r.resolve(req));
                } catch (const std::runtime_error& e) {
                    std::string m(e.what());
                    EXPECT_NE(m.find("G000UU0002SSTO3"), std::string::npos);
                    EXPECT_NE(m.find("invalid_native_canvas"), std::string::npos);
                    throw;
                }
            },
            std::runtime_error);
    }
}

// ======================================================================
// 46. Contributor static body-shadow pair
// ======================================================================

TEST(AdventureUnitIdleShadow, ContributorStaticPair) {
    auto resolver =
        [](const d2runtime::AdventureStack&,
           const d2runtime::AdventureUnitInstance&) -> std::optional<AdventureActorVisual> {
        return AdventureActorVisual{
            .presentation_kind = AdventureActorPresentationKind::Unit,
            .resolved_owner_id = "G000UU0001",
            .body = {.container_path = "Imgs/Isounit.ff",
                     .logical_animation_name = "G000UU0001STOP3",
                     .frames = {{"B_STOP3.PNG", 80, 90}},
                     .native_canvas_w = 320,
                     .native_canvas_h = 320,
                     .canvas_foot_x = 40,
                     .canvas_foot_y = 80},
            .shadow = AdventureActorVisualLayer{.container_path = "Imgs/Isounit.ff",
                                                .logical_animation_name = "G000UU0001SSTO3",
                                                .frames = {{"S_STOP3.PNG", 60, 50}},
                                                .native_canvas_w = 200,
                                                .native_canvas_h = 150,
                                                .canvas_foot_x = 30,
                                                .canvas_foot_y = 60},
        };
    };
    AdventureWorldState world;
    world.units.push_back({.id = "u1", .type_id = "G000UU0001"});
    AdventureStack stack;
    stack.id = "S1";
    stack.position.x = 3;
    stack.position.y = 4;
    stack.leader_id = "u1";
    stack.facing = AdventureIsoDirection::D3;
    world.stacks.push_back(stack);
    AdventureMapPreparer preparer(kAnimGeo);
    preparer.add_contributor(make_stack_actor_contributor(resolver));
    auto result = preparer.prepare(world);
    ASSERT_EQ(result.graph.world.size(), 2u);
    ASSERT_EQ(result.pick_entries.size(), 1u);

    const PreparedAdventureRenderPrimitive* body = nullptr;
    const PreparedAdventureRenderPrimitive* shadow = nullptr;
    for (const auto& prim : result.graph.world) {
        if (prim.stable_id == stable_render_id("Stack:S1"))
            body = &prim;
        if (prim.stable_id == stable_render_id("StackShadow:S1"))
            shadow = &prim;
    }
    ASSERT_NE(body, nullptr);
    ASSERT_NE(shadow, nullptr);
    EXPECT_EQ(body->level, WorldRenderLevel::Actor);
    EXPECT_EQ(shadow->level, WorldRenderLevel::ActorUnderlay);
    EXPECT_EQ(shadow->local_suborder, kActorShadowSuborder);
    EXPECT_FALSE(shadow->animation.has_value());
    EXPECT_FALSE(body->animation.has_value());
    EXPECT_EQ(shadow->record_name, "S_STOP3.PNG");
    EXPECT_EQ(body->record_name, "B_STOP3.PNG");
    EXPECT_NE(body->draw_origin.x, shadow->draw_origin.x);
    EXPECT_NE(body->draw_origin.y, shadow->draw_origin.y);
    EXPECT_EQ(body->depth_anchor.x, shadow->depth_anchor.x);
    EXPECT_EQ(shadow->interaction_mask, nullptr);
}

// ======================================================================
// 47. Contributor animated pair test
// ======================================================================

TEST(AdventureUnitIdleShadow, ContributorAnimatedPair) {
    auto resolver =
        [](const d2runtime::AdventureStack&,
           const d2runtime::AdventureUnitInstance&) -> std::optional<AdventureActorVisual> {
        return AdventureActorVisual{
            .presentation_kind = AdventureActorPresentationKind::Unit,
            .resolved_owner_id = "G000UU0002",
            .body = {.container_path = "Imgs/Isounit.ff",
                     .logical_animation_name = "G000UU0002STOP3",
                     .frames = {{"B0.PNG", 80, 90}, {"B1.PNG", 85, 95}, {"B2.PNG", 82, 92}},
                     .native_canvas_w = 400,
                     .native_canvas_h = 400,
                     .canvas_foot_x = 50,
                     .canvas_foot_y = 100},
            .shadow =
                AdventureActorVisualLayer{
                    .container_path = "Imgs/Isounit.ff",
                    .logical_animation_name = "G000UU0002SSTO3",
                    .frames = {{"S0.PNG", 60, 45}, {"S1.PNG", 62, 47}, {"S2.PNG", 61, 46}},
                    .native_canvas_w = 200,
                    .native_canvas_h = 150,
                    .canvas_foot_x = 35,
                    .canvas_foot_y = 70},
        };
    };
    AdventureWorldState world;
    world.units.push_back({.id = "u2", .type_id = "G000UU0002"});
    AdventureStack stack;
    stack.id = "S2";
    stack.position.x = 5;
    stack.position.y = 6;
    stack.leader_id = "u2";
    stack.facing = AdventureIsoDirection::D3;
    world.stacks.push_back(stack);
    AdventureMapPreparer preparer(kAnimGeo);
    preparer.add_contributor(make_stack_actor_contributor(resolver));
    auto result = preparer.prepare(world);
    ASSERT_EQ(result.graph.world.size(), 2u);

    const PreparedAdventureRenderPrimitive* body = nullptr;
    const PreparedAdventureRenderPrimitive* shadow = nullptr;
    for (const auto& prim : result.graph.world) {
        if (prim.stable_id == stable_render_id("Stack:S2"))
            body = &prim;
        if (prim.stable_id == stable_render_id("StackShadow:S2"))
            shadow = &prim;
    }
    ASSERT_NE(body, nullptr);
    ASSERT_NE(shadow, nullptr);
    ASSERT_TRUE(body->animation.has_value());
    ASSERT_TRUE(shadow->animation.has_value());
    EXPECT_EQ(body->animation->frames.size(), 3u);
    for (const auto& f : body->animation->frames)
        EXPECT_EQ(f.duration_ms, 100);
    EXPECT_TRUE(body->animation->is_looping);
    EXPECT_TRUE(shadow->animation->is_looping);
    EXPECT_FALSE(body->animation_sync_source_id.has_value());
    ASSERT_TRUE(shadow->animation_sync_source_id.has_value());
    EXPECT_EQ(*shadow->animation_sync_source_id, stable_render_id("Stack:S2"));
    EXPECT_EQ(body->record_name, "B0.PNG");
    EXPECT_EQ(shadow->record_name, "S0.PNG");
}

// ======================================================================
// 48. Frame-count mismatch regression
// ======================================================================

TEST(AdventureUnitIdleShadow, FrameCountMismatchUsesIndependentShadowClock) {
    auto resolver =
        [](const d2runtime::AdventureStack&,
           const d2runtime::AdventureUnitInstance&) -> std::optional<AdventureActorVisual> {
        return AdventureActorVisual{
            .presentation_kind = AdventureActorPresentationKind::Unit,
            .resolved_owner_id = "G000UU0003",
            .body = {.container_path = "Imgs/Isounit.ff",
                     .logical_animation_name = "G000UU0003STOP3",
                     .frames = {{"B0.PNG", 80, 90},
                                {"B1.PNG", 80, 90},
                                {"B2.PNG", 80, 90},
                                {"B3.PNG", 80, 90},
                                {"B4.PNG", 80, 90},
                                {"B5.PNG", 80, 90},
                                {"B6.PNG", 80, 90},
                                {"B7.PNG", 80, 90}},
                     .native_canvas_w = 320,
                     .native_canvas_h = 320,
                     .canvas_foot_x = 40,
                     .canvas_foot_y = 80},
            .shadow = AdventureActorVisualLayer{.container_path = "Imgs/Isounit.ff",
                                                .logical_animation_name = "G000UU0003SSTO3",
                                                .frames = {{"S0.PNG", 60, 50},
                                                           {"S1.PNG", 60, 50},
                                                           {"S2.PNG", 60, 50},
                                                           {"S3.PNG", 60, 50},
                                                           {"S4.PNG", 60, 50},
                                                           {"S5.PNG", 60, 50},
                                                           {"S6.PNG", 60, 50},
                                                           {"S7.PNG", 60, 50},
                                                           {"S8.PNG", 60, 50},
                                                           {"S9.PNG", 60, 50},
                                                           {"S10.PNG", 60, 50},
                                                           {"S11.PNG", 60, 50},
                                                           {"S12.PNG", 60, 50},
                                                           {"S13.PNG", 60, 50},
                                                           {"S14.PNG", 60, 50},
                                                           {"S15.PNG", 60, 50}},
                                                .native_canvas_w = 200,
                                                .native_canvas_h = 150,
                                                .canvas_foot_x = 30,
                                                .canvas_foot_y = 60},
        };
    };
    AdventureWorldState world;
    world.units.push_back({.id = "u3", .type_id = "G000UU0003"});
    AdventureStack stack;
    stack.id = "S3";
    stack.position.x = 3;
    stack.position.y = 4;
    stack.leader_id = "u3";
    stack.facing = AdventureIsoDirection::D3;
    world.stacks.push_back(stack);
    AdventureMapPreparer preparer(kAnimGeo);
    preparer.add_contributor(make_stack_actor_contributor(resolver));
    const auto prepared = preparer.prepare(world);
    const auto shadow = std::find_if(
        prepared.graph.world.begin(), prepared.graph.world.end(), [](const auto& primitive) {
            return primitive.stable_id == stable_render_id("StackShadow:S3");
        });
    ASSERT_NE(shadow, prepared.graph.world.end());
    EXPECT_FALSE(shadow->animation_sync_source_id.has_value());
}

// ======================================================================
// 49. Shared animation-clock test
// ======================================================================

TEST(AdventureUnitIdleShadow, SharedAnimationClock) {
    auto resolver =
        [](const d2runtime::AdventureStack&,
           const d2runtime::AdventureUnitInstance&) -> std::optional<AdventureActorVisual> {
        return AdventureActorVisual{
            .presentation_kind = AdventureActorPresentationKind::Unit,
            .resolved_owner_id = "G000UU0004",
            .body = {.container_path = "Imgs/Isounit.ff",
                     .logical_animation_name = "G000UU0004STOP3",
                     .frames = {{"B0.PNG", 80, 90}, {"B1.PNG", 80, 90}, {"B2.PNG", 80, 90}},
                     .native_canvas_w = 320,
                     .native_canvas_h = 320,
                     .canvas_foot_x = 40,
                     .canvas_foot_y = 80},
            .shadow =
                AdventureActorVisualLayer{
                    .container_path = "Imgs/Isounit.ff",
                    .logical_animation_name = "G000UU0004SSTO3",
                    .frames = {{"S0.PNG", 60, 50}, {"S1.PNG", 60, 50}, {"S2.PNG", 60, 50}},
                    .native_canvas_w = 200,
                    .native_canvas_h = 150,
                    .canvas_foot_x = 30,
                    .canvas_foot_y = 60},
        };
    };
    AdventureWorldState world;
    world.units.push_back({.id = "u4", .type_id = "G000UU0004"});
    AdventureStack stack;
    stack.id = "S4";
    stack.position.x = 5;
    stack.position.y = 6;
    stack.leader_id = "u4";
    stack.facing = AdventureIsoDirection::D3;
    world.stacks.push_back(stack);
    AdventureMapPreparer preparer(kAnimGeo);
    preparer.add_contributor(make_stack_actor_contributor(resolver));
    auto result = preparer.prepare(world);
    auto players = build_adventure_animation_players(result.graph);
    ASSERT_EQ(players.size(), 1u);
    const auto body_id = stable_render_id("Stack:S4");
    ASSERT_TRUE(players.contains(body_id));
    auto& player = players.at(body_id);
    EXPECT_EQ(player.current_frame_index(), 0u);
    player.update(100.0f);
    EXPECT_EQ(player.current_frame_index(), 1u);
    player.update(100.0f);
    EXPECT_EQ(player.current_frame_index(), 2u);
    player.update(100.0f);
    EXPECT_EQ(player.current_frame_index(), 0u);

    const PreparedAdventureRenderPrimitive* body = nullptr;
    const PreparedAdventureRenderPrimitive* shadow = nullptr;
    for (const auto& prim : result.graph.world) {
        if (prim.stable_id == body_id)
            body = &prim;
        if (prim.stable_id == stable_render_id("StackShadow:S4"))
            shadow = &prim;
    }
    ASSERT_NE(body, nullptr);
    ASSERT_NE(shadow, nullptr);
    EXPECT_EQ(adventure_animation_player_id(*body), adventure_animation_player_id(*shadow));
}

// ======================================================================
// 50. Missing-shadow contributor regression
// ======================================================================

TEST(AdventureUnitIdleShadow, MissingShadowContributor) {
    auto resolver =
        [](const d2runtime::AdventureStack&,
           const d2runtime::AdventureUnitInstance&) -> std::optional<AdventureActorVisual> {
        return AdventureActorVisual{
            .presentation_kind = AdventureActorPresentationKind::Unit,
            .resolved_owner_id = "G000UU0001",
            .body = {.container_path = "Imgs/Isounit.ff",
                     .logical_animation_name = "G000UU0001STOP3",
                     .frames = {{"B0.PNG", 80, 90}, {"B1.PNG", 80, 90}},
                     .native_canvas_w = 320,
                     .native_canvas_h = 320,
                     .canvas_foot_x = 40,
                     .canvas_foot_y = 80},
            .shadow = std::nullopt,
        };
    };
    AdventureWorldState world;
    world.units.push_back({.id = "u5", .type_id = "G000UU0001"});
    AdventureStack stack;
    stack.id = "S5";
    stack.position.x = 3;
    stack.position.y = 4;
    stack.leader_id = "u5";
    stack.facing = AdventureIsoDirection::D3;
    world.stacks.push_back(stack);
    AdventureMapPreparer preparer(kAnimGeo);
    preparer.add_contributor(make_stack_actor_contributor(resolver));
    auto result = preparer.prepare(world);
    ASSERT_EQ(result.graph.world.size(), 1u);
    EXPECT_EQ(result.graph.world[0].stable_id, stable_render_id("Stack:S5"));
    EXPECT_EQ(result.graph.world[0].level, WorldRenderLevel::Actor);
    ASSERT_EQ(result.pick_entries.size(), 1u);
    auto players = build_adventure_animation_players(result.graph);
    ASSERT_EQ(players.size(), 1u);
}

// ======================================================================
// 51. Invalid sync topology — missing source
// ======================================================================

TEST(AdventureUnitIdleShadow, InvalidSyncTopologyMissingSource) {
    PreparedAdventureRenderGraph     graph;
    PreparedAdventureRenderPrimitive prim;
    prim.stable_id = 42;
    prim.animation = AdventureAnimationData{
        .animation_name = "test",
        .frames = {{"F0", 100, 80, 90}},
        .native_canvas_w = 320,
        .native_canvas_h = 320,
        .is_looping = true,
        .timing_source = AdventureAnimationTimingSource::ProvisionalFallback,
    };
    prim.animation_sync_source_id = 999;
    graph.world.push_back(prim);
    EXPECT_THROW(
        {
            try {
                static_cast<void>(build_adventure_animation_players(graph));
            } catch (const std::runtime_error& e) {
                EXPECT_NE(std::string(e.what()).find("missing_sync_source"), std::string::npos);
                throw;
            }
        },
        std::runtime_error);
}

// ======================================================================
// 52. Invalid sync topology — static source
// ======================================================================

TEST(AdventureUnitIdleShadow, InvalidSyncTopologyStaticSource) {
    PreparedAdventureRenderGraph     graph;
    PreparedAdventureRenderPrimitive source;
    source.stable_id = 1;
    source.animation = std::nullopt;
    graph.world.push_back(source);
    PreparedAdventureRenderPrimitive follower;
    follower.stable_id = 2;
    follower.animation = AdventureAnimationData{
        .animation_name = "test",
        .frames = {{"F0", 100, 80, 90}},
        .native_canvas_w = 320,
        .native_canvas_h = 320,
        .is_looping = true,
        .timing_source = AdventureAnimationTimingSource::ProvisionalFallback,
    };
    follower.animation_sync_source_id = 1;
    graph.world.push_back(follower);
    EXPECT_THROW(
        {
            try {
                static_cast<void>(build_adventure_animation_players(graph));
            } catch (const std::runtime_error& e) {
                EXPECT_NE(std::string(e.what()).find("static_sync_source"), std::string::npos);
                throw;
            }
        },
        std::runtime_error);
}

// ======================================================================
// 53. Invalid sync topology — frame count mismatch
// ======================================================================

TEST(AdventureUnitIdleShadow, InvalidSyncTopologyFrameCountMismatch) {
    PreparedAdventureRenderGraph     graph;
    PreparedAdventureRenderPrimitive source;
    source.stable_id = 1;
    source.animation = AdventureAnimationData{
        .animation_name = "src",
        .frames = {{"F0", 100, 80, 90}, {"F1", 100, 80, 90}},
        .native_canvas_w = 320,
        .native_canvas_h = 320,
        .is_looping = true,
        .timing_source = AdventureAnimationTimingSource::ProvisionalFallback,
    };
    graph.world.push_back(source);
    PreparedAdventureRenderPrimitive follower;
    follower.stable_id = 2;
    follower.animation = AdventureAnimationData{
        .animation_name = "fol",
        .frames = {{"F0", 100, 80, 90}},
        .native_canvas_w = 320,
        .native_canvas_h = 320,
        .is_looping = true,
        .timing_source = AdventureAnimationTimingSource::ProvisionalFallback,
    };
    follower.animation_sync_source_id = 1;
    graph.world.push_back(follower);
    EXPECT_THROW(
        {
            try {
                static_cast<void>(build_adventure_animation_players(graph));
            } catch (const std::runtime_error& e) {
                EXPECT_NE(std::string(e.what()).find("sync_frame_count_mismatch"),
                          std::string::npos);
                throw;
            }
        },
        std::runtime_error);
}

// ======================================================================
// 54. Valid sync with different record names (allowed for body/shadow)
// ======================================================================

TEST(AdventureUnitIdleShadow, ValidSyncWithDifferentRecordNames) {
    PreparedAdventureRenderGraph     graph;
    PreparedAdventureRenderPrimitive source;
    source.stable_id = 1;
    source.animation = AdventureAnimationData{
        .animation_name = "body",
        .frames = {{"body_frame", 100, 80, 90}},
        .native_canvas_w = 320,
        .native_canvas_h = 320,
        .is_looping = true,
        .timing_source = AdventureAnimationTimingSource::ProvisionalFallback,
    };
    graph.world.push_back(source);
    PreparedAdventureRenderPrimitive follower;
    follower.stable_id = 2;
    follower.animation = AdventureAnimationData{
        .animation_name = "shadow",
        .frames = {{"shadow_frame", 100, 60, 50}},
        .native_canvas_w = 200,
        .native_canvas_h = 150,
        .is_looping = true,
        .timing_source = AdventureAnimationTimingSource::ProvisionalFallback,
    };
    follower.animation_sync_source_id = 1;
    graph.world.push_back(follower);
    auto players = build_adventure_animation_players(graph);
    ASSERT_EQ(players.size(), 1u);
    ASSERT_TRUE(players.contains(1));
}

// ======================================================================
// 55. Authored-empty shadow — resolves to nullopt shadow, no crash
// ======================================================================

TEST(AdventureUnitIdleShadow, AuthoredEmptyShadowResolvesNoShadow) {
    FakeSpriteCatalog catalog;
    catalog.animations["G000UU0001STOP7"] = {
        .name = "G000UU0001STOP7",
        .container_path = "Imgs/Isounit.ff",
        .frames = {{.image_name = "BODY_FRAME", .index = 0, .duration_ms = 100}},
        .is_looping = false,
        .native_canvas_w = 320,
        .native_canvas_h = 320,
        .canvas_foot_x = 40,
        .canvas_foot_y = 80,
    };
    catalog.animations["G000UU0001SSTO7"] = {
        .name = "G000UU0001SSTO7",
        .container_path = "Imgs/Isounit.ff",
        .frames = {{.image_name = "EMPTY_SHADOW_FRAME", .index = 0, .duration_ms = 100}},
        .is_looping = false,
        .native_canvas_w = 800,
        .native_canvas_h = 600,
        .canvas_foot_x = 400,
        .canvas_foot_y = 600,
    };
    catalog.metas["BODY_FRAME"] = {80, 90, true};
    catalog.metas["EMPTY_SHADOW_FRAME"] = {800, 600, false};

    TempDir tmp("d2_authored_empty_test");
    for (const auto& name :
         {"Tglobal.dbf", "Gattacks.dbf", "Gunits.dbf", "Gupgrade.dbf", "Graces.dbf"})
        static_cast<void>(std::ofstream(tmp.path() / name).put(0x1a));
    GameDataRegistry game_data(tmp.path());

    IsoActorVisualResolver           resolver(catalog, game_data);
    AdventureStackActorVisualRequest req;
    req.presentation = {AdventureActorPresentationKind::Unit};
    req.leader_unit_type_id = "G000UU0001";
    req.direction = AdventureIsoDirection::D7;
    const auto visual = resolver.resolve(req);
    ASSERT_TRUE(visual.has_value());
    EXPECT_EQ(visual->resolved_owner_id, "g000uu0001");
    EXPECT_EQ(visual->body.animation_name, "G000UU0001STOP7");
    ASSERT_EQ(visual->body.frames.size(), 1u);
    EXPECT_EQ(visual->body.frames[0].record_name, "BODY_FRAME");

    EXPECT_FALSE(visual->shadow.has_value()) << "authored-empty shadow must produce nullopt";

    const auto presence = resolver.shadow_presence("g000uu0001", AdventureIsoDirection::D7);
    EXPECT_EQ(presence, ExactLayerPresence::AuthoredEmpty);
}

// ======================================================================
// 56. Complete 8-direction authored-empty shadow family
// ======================================================================

TEST(AdventureUnitIdleShadow, AuthoredEmptyEightDirectionFamily) {
    FakeSpriteCatalog catalog;

    for (int d = 0; d < 8; ++d) {
        const auto d_str = std::to_string(d);
        catalog.animations["G000UU0001STOP" + d_str] = {
            .name = "G000UU0001STOP" + d_str,
            .container_path = "Imgs/Isounit.ff",
            .frames = {{.image_name = "BODY_D" + d_str, .index = 0, .duration_ms = 100}},
            .is_looping = false,
            .native_canvas_w = 320,
            .native_canvas_h = 320,
            .canvas_foot_x = 40,
            .canvas_foot_y = 80,
        };
        catalog.animations["G000UU0001SSTO" + d_str] = {
            .name = "G000UU0001SSTO" + d_str,
            .container_path = "Imgs/Isounit.ff",
            .frames = {{.image_name = "EMPTY_D" + d_str, .index = 0, .duration_ms = 100}},
            .is_looping = false,
            .native_canvas_w = 800,
            .native_canvas_h = 600,
            .canvas_foot_x = 400,
            .canvas_foot_y = 600,
        };
        catalog.metas["BODY_D" + d_str] = {80, 90, true};
        catalog.metas["EMPTY_D" + d_str] = {800, 600, false};
    }

    TempDir tmp("d2_empty_family_test");
    for (const auto& name :
         {"Tglobal.dbf", "Gattacks.dbf", "Gunits.dbf", "Gupgrade.dbf", "Graces.dbf"})
        static_cast<void>(std::ofstream(tmp.path() / name).put(0x1a));
    GameDataRegistry game_data(tmp.path());

    IsoActorVisualResolver resolver(catalog, game_data);

    for (int d = 0; d < 8; ++d) {
        const auto                       dir = direction_from_index(d);
        AdventureStackActorVisualRequest req;
        req.presentation = {AdventureActorPresentationKind::Unit};
        req.leader_unit_type_id = "G000UU0001";
        req.direction = dir;
        const auto visual = resolver.resolve(req);
        ASSERT_TRUE(visual.has_value()) << "D" << d;
        EXPECT_EQ(visual->resolved_owner_id, "g000uu0001") << "D" << d;
        EXPECT_FALSE(visual->shadow.has_value()) << "D" << d << " shadow must be absent";

        const auto presence = resolver.shadow_presence("g000uu0001", dir);
        EXPECT_EQ(presence, ExactLayerPresence::AuthoredEmpty) << "D" << d;
    }
}

// ======================================================================
// 57. Mixed visibility shadow throws
// ======================================================================

TEST(AdventureUnitIdleShadow, MixedVisibilityShadowThrows) {
    FakeSpriteCatalog catalog;
    catalog.animations["G000UU0001STOP3"] = {
        .name = "G000UU0001STOP3",
        .container_path = "Imgs/Isounit.ff",
        .frames = {{.image_name = "BODY", .index = 0, .duration_ms = 100}},
        .is_looping = false,
        .native_canvas_w = 320,
        .native_canvas_h = 320,
        .canvas_foot_x = 40,
        .canvas_foot_y = 80,
    };
    catalog.animations["G000UU0001SSTO3"] = {
        .name = "G000UU0001SSTO3",
        .container_path = "Imgs/Isounit.ff",
        .frames =
            {
                {.image_name = "VISIBLE_FRAME", .index = 0, .duration_ms = 100},
                {.image_name = "EMPTY_FRAME", .index = 1, .duration_ms = 100},
            },
        .is_looping = false,
        .native_canvas_w = 320,
        .native_canvas_h = 320,
        .canvas_foot_x = 40,
        .canvas_foot_y = 80,
    };
    catalog.metas["BODY"] = {80, 90, true};
    catalog.metas["VISIBLE_FRAME"] = {80, 90, true};
    catalog.metas["EMPTY_FRAME"] = {800, 600, false};

    TempDir tmp("d2_mixed_vis_test");
    for (const auto& name :
         {"Tglobal.dbf", "Gattacks.dbf", "Gunits.dbf", "Gupgrade.dbf", "Graces.dbf"})
        static_cast<void>(std::ofstream(tmp.path() / name).put(0x1a));
    GameDataRegistry game_data(tmp.path());

    IsoActorVisualResolver resolver(catalog, game_data);
    EXPECT_THROW(
        {
            try {
                AdventureStackActorVisualRequest req;
                req.presentation = {AdventureActorPresentationKind::Unit};
                req.leader_unit_type_id = "G000UU0001";
                req.direction = AdventureIsoDirection::D3;
                static_cast<void>(resolver.resolve(req));
            } catch (const std::runtime_error& e) {
                std::string m(e.what());
                EXPECT_NE(m.find("mixed_visibility_shadow"), std::string::npos);
                EXPECT_NE(m.find("G000UU0001SSTO3"), std::string::npos);
                EXPECT_NE(m.find("visible_frames=1"), std::string::npos);
                EXPECT_NE(m.find("empty_frames=1"), std::string::npos);
                EXPECT_NE(m.find("empty_frame_index=1"), std::string::npos);
                EXPECT_NE(m.find("empty_frame_record=EMPTY_FRAME"), std::string::npos);
                throw;
            }
        },
        std::runtime_error);
}

// ======================================================================
// 58. Empty body frame throws
// ======================================================================

TEST(AdventureUnitIdleShadow, EmptyBodyFrameThrows) {
    FakeSpriteCatalog catalog;
    catalog.animations["G000UU0001STOP3"] = {
        .name = "G000UU0001STOP3",
        .container_path = "Imgs/Isounit.ff",
        .frames = {{.image_name = "EMPTY_BODY_FRAME", .index = 0, .duration_ms = 100}},
        .is_looping = false,
        .native_canvas_w = 320,
        .native_canvas_h = 320,
        .canvas_foot_x = 40,
        .canvas_foot_y = 80,
    };
    catalog.metas["EMPTY_BODY_FRAME"] = {800, 600, false};

    TempDir tmp("d2_empty_body_test");
    for (const auto& name :
         {"Tglobal.dbf", "Gattacks.dbf", "Gunits.dbf", "Gupgrade.dbf", "Graces.dbf"})
        static_cast<void>(std::ofstream(tmp.path() / name).put(0x1a));
    GameDataRegistry game_data(tmp.path());

    IsoActorVisualResolver resolver(catalog, game_data);
    EXPECT_THROW(
        {
            try {
                AdventureStackActorVisualRequest req;
                req.presentation = {AdventureActorPresentationKind::Unit};
                req.leader_unit_type_id = "G000UU0001";
                req.direction = AdventureIsoDirection::D3;
                static_cast<void>(resolver.resolve(req));
            } catch (const std::runtime_error& e) {
                std::string m(e.what());
                EXPECT_NE(m.find("malformed_body_sequence"), std::string::npos);
                EXPECT_NE(m.find("empty_body_frame"), std::string::npos);
                EXPECT_NE(m.find("G000UU0001STOP3"), std::string::npos);
                throw;
            }
        },
        std::runtime_error);
}

// ======================================================================
// 59. Authored-empty shadow contributor produces body-only output
// ======================================================================

TEST(AdventureUnitIdleShadow, AuthoredEmptyShadowContributorBodyOnly) {
    auto resolver =
        [](const d2runtime::AdventureStack&,
           const d2runtime::AdventureUnitInstance&) -> std::optional<AdventureActorVisual> {
        return AdventureActorVisual{
            .presentation_kind = AdventureActorPresentationKind::Unit,
            .resolved_owner_id = "G000UU0001",
            .body = {.container_path = "Imgs/Isounit.ff",
                     .logical_animation_name = "G000UU0001STOP7",
                     .frames = {{"B_D7.PNG", 80, 90}},
                     .native_canvas_w = 320,
                     .native_canvas_h = 320,
                     .canvas_foot_x = 40,
                     .canvas_foot_y = 80},
            .shadow = std::nullopt,
        };
    };
    AdventureWorldState world;
    world.units.push_back({.id = "u1", .type_id = "G000UU0001"});
    AdventureStack stack;
    stack.id = "S1";
    stack.position.x = 3;
    stack.position.y = 4;
    stack.leader_id = "u1";
    stack.facing = AdventureIsoDirection::D7;
    world.stacks.push_back(stack);
    AdventureMapPreparer preparer(kAnimGeo);
    preparer.add_contributor(make_stack_actor_contributor(resolver));
    auto result = preparer.prepare(world);
    ASSERT_EQ(result.graph.world.size(), 1u);
    ASSERT_EQ(result.pick_entries.size(), 1u);
    EXPECT_EQ(result.graph.world[0].stable_id, stable_render_id("Stack:S1"));
    EXPECT_EQ(result.graph.world[0].level, WorldRenderLevel::Actor);

    // Preload must contain body frame only
    auto keys = collect_adventure_render_asset_keys(result.graph);
    EXPECT_NE(std::find_if(keys.begin(), keys.end(),
                           [](const auto& k) { return k.image_name == "B_D7.PNG"; }),
              keys.end());

    // No animation player for absent shadow
    auto players = build_adventure_animation_players(result.graph);
    ASSERT_EQ(players.size(), 0u);
}

// ======================================================================
// 60. Static shadow alpha regression
// ======================================================================

TEST(AdventureUnitIdleShadow, StaticBodyShadowAlpha) {
    auto resolver =
        [](const d2runtime::AdventureStack&,
           const d2runtime::AdventureUnitInstance&) -> std::optional<AdventureActorVisual> {
        return AdventureActorVisual{
            .presentation_kind = AdventureActorPresentationKind::Unit,
            .resolved_owner_id = "G000UU0001",
            .body =
                {
                    .container_path = "Imgs/Isounit.ff",
                    .logical_animation_name = "G000UU0001STOP3",
                    .frames = {{"B_STOP3.PNG", 80, 90}},
                    .native_canvas_w = 320,
                    .native_canvas_h = 320,
                    .canvas_foot_x = 40,
                    .canvas_foot_y = 80,
                },
            .shadow =
                AdventureActorVisualLayer{
                    .container_path = "Imgs/Isounit.ff",
                    .logical_animation_name = "G000UU0001SSTO3",
                    .frames = {{"S_STOP3.PNG", 60, 50}},
                    .native_canvas_w = 200,
                    .native_canvas_h = 150,
                    .canvas_foot_x = 30,
                    .canvas_foot_y = 60,
                },
        };
    };

    AdventureWorldState world;
    world.units.push_back({.id = "u1", .type_id = "G000UU0001"});
    AdventureStack stack;
    stack.id = "S1";
    stack.position.x = 3;
    stack.position.y = 4;
    stack.leader_id = "u1";
    stack.facing = AdventureIsoDirection::D3;
    world.stacks.push_back(stack);

    AdventureMapPreparer preparer(kAnimGeo);
    preparer.add_contributor(make_stack_actor_contributor(resolver));
    auto result = preparer.prepare(world);

    ASSERT_EQ(result.graph.world.size(), 2u);

    const PreparedAdventureRenderPrimitive* body = nullptr;
    const PreparedAdventureRenderPrimitive* shadow = nullptr;
    for (const auto& prim : result.graph.world) {
        if (prim.stable_id == stable_render_id("Stack:S1"))
            body = &prim;
        if (prim.stable_id == stable_render_id("StackShadow:S1"))
            shadow = &prim;
    }
    ASSERT_NE(body, nullptr);
    ASSERT_NE(shadow, nullptr);

    EXPECT_FLOAT_EQ(body->alpha, 1.0f);
    EXPECT_FLOAT_EQ(shadow->alpha, 0.5f);
    EXPECT_EQ(body->level, WorldRenderLevel::Actor);
    EXPECT_EQ(shadow->level, WorldRenderLevel::ActorUnderlay);
    EXPECT_EQ(shadow->local_suborder, kActorShadowSuborder);
    EXPECT_EQ(body->record_name, "B_STOP3.PNG");
    EXPECT_EQ(shadow->record_name, "S_STOP3.PNG");
    EXPECT_NE(body->draw_origin.x, shadow->draw_origin.x);
    EXPECT_EQ(shadow->interaction_mask, nullptr);

    // Verify resolved frames carry alpha
    auto body_frame = resolve_adventure_frame_visual(*body, std::nullopt);
    auto shadow_frame = resolve_adventure_frame_visual(*shadow, std::nullopt);
    EXPECT_FLOAT_EQ(body_frame.alpha, 1.0f);
    EXPECT_FLOAT_EQ(shadow_frame.alpha, 0.5f);
}

// ======================================================================
// 61. Animated shadow alpha regression
// ======================================================================

TEST(AdventureUnitIdleShadow, AnimatedBodyShadowAlpha) {
    auto resolver =
        [](const d2runtime::AdventureStack&,
           const d2runtime::AdventureUnitInstance&) -> std::optional<AdventureActorVisual> {
        return AdventureActorVisual{
            .presentation_kind = AdventureActorPresentationKind::Unit,
            .resolved_owner_id = "G000UU0002",
            .body =
                {
                    .container_path = "Imgs/Isounit.ff",
                    .logical_animation_name = "G000UU0002STOP3",
                    .frames =
                        {
                            {"B0.PNG", 80, 90},
                            {"B1.PNG", 85, 95},
                            {"B2.PNG", 82, 92},
                        },
                    .native_canvas_w = 400,
                    .native_canvas_h = 400,
                    .canvas_foot_x = 50,
                    .canvas_foot_y = 100,
                },
            .shadow =
                AdventureActorVisualLayer{
                    .container_path = "Imgs/Isounit.ff",
                    .logical_animation_name = "G000UU0002SSTO3",
                    .frames =
                        {
                            {"S0.PNG", 60, 45},
                            {"S1.PNG", 62, 47},
                            {"S2.PNG", 61, 46},
                        },
                    .native_canvas_w = 200,
                    .native_canvas_h = 150,
                    .canvas_foot_x = 35,
                    .canvas_foot_y = 70,
                },
        };
    };

    AdventureWorldState world;
    world.units.push_back({.id = "u2", .type_id = "G000UU0002"});
    AdventureStack stack;
    stack.id = "S2";
    stack.position.x = 5;
    stack.position.y = 6;
    stack.leader_id = "u2";
    stack.facing = AdventureIsoDirection::D3;
    world.stacks.push_back(stack);

    AdventureMapPreparer preparer(kAnimGeo);
    preparer.add_contributor(make_stack_actor_contributor(resolver));
    auto result = preparer.prepare(world);

    const PreparedAdventureRenderPrimitive* body = nullptr;
    const PreparedAdventureRenderPrimitive* shadow = nullptr;
    for (const auto& prim : result.graph.world) {
        if (prim.stable_id == stable_render_id("Stack:S2"))
            body = &prim;
        if (prim.stable_id == stable_render_id("StackShadow:S2"))
            shadow = &prim;
    }
    ASSERT_NE(body, nullptr);
    ASSERT_NE(shadow, nullptr);

    EXPECT_FLOAT_EQ(body->alpha, 1.0f);
    EXPECT_FLOAT_EQ(shadow->alpha, 0.5f);

    ASSERT_TRUE(body->animation.has_value());
    ASSERT_TRUE(shadow->animation.has_value());
    ASSERT_EQ(body->animation->frames.size(), 3u);

    // Alpha stable across all animation frames
    for (std::size_t i = 0; i < 3u; ++i) {
        auto body_frame = resolve_adventure_frame_visual(*body, i);
        auto shadow_frame = resolve_adventure_frame_visual(*shadow, i);
        EXPECT_FLOAT_EQ(body_frame.alpha, 1.0f) << "body frame " << i;
        EXPECT_FLOAT_EQ(shadow_frame.alpha, 0.5f) << "shadow frame " << i;
    }
}

// ======================================================================
// 62. Default primitive opacity regression
// ======================================================================

TEST(AdventureUnitIdleShadow, DefaultPrimitiveOpacity) {
    // Static primitive with default alpha
    PreparedAdventureRenderPrimitive static_prim;
    static_prim.container_path = "Imgs/Isounit.ff";
    static_prim.record_name = "TEST.PNG";
    static_prim.src_width = 80;
    static_prim.src_height = 90;
    static_prim.stable_id = 1;

    EXPECT_FLOAT_EQ(static_prim.alpha, 1.0f);
    auto static_frame = resolve_adventure_frame_visual(static_prim, std::nullopt);
    EXPECT_FLOAT_EQ(static_frame.alpha, 1.0f);

    // Animated primitive with default alpha
    PreparedAdventureRenderPrimitive anim_prim;
    anim_prim.container_path = "Imgs/Isounit.ff";
    anim_prim.animation = AdventureAnimationData{
        .animation_name = "TEST",
        .frames = {{"F0", 100, 80, 90}, {"F1", 100, 80, 90}},
        .native_canvas_w = 320,
        .native_canvas_h = 320,
        .is_looping = true,
        .timing_source = AdventureAnimationTimingSource::ProvisionalFallback,
    };
    anim_prim.stable_id = 2;

    EXPECT_FLOAT_EQ(anim_prim.alpha, 1.0f);
    auto anim_frame = resolve_adventure_frame_visual(anim_prim, 0);
    EXPECT_FLOAT_EQ(anim_frame.alpha, 1.0f);
}

// ======================================================================
// 63. Selection-circle remains fully opaque
// ======================================================================

TEST(AdventureUnitIdleShadow, SelectionCircleRemainsOpaque) {
    AdventureStackRef ref;
    ref.stable_id = 1;
    ref.object_id = "S1";
    ref.cell = {3, 4};

    AdventureWorldVisualResources visuals;
    visuals.select_no.key.container_path = "Imgs/IsoCmon.ff";
    visuals.select_no.key.image_name = "SELECT.PNG";
    visuals.select_no.src_width = 80;
    visuals.select_no.src_height = 80;
    visuals.select_no.canvas_foot_x = 40;
    visuals.select_no.canvas_foot_y = 40;

    auto prim = build_selection_primitive(ref, ref.cell, visuals.select_no,
                                          stable_render_id("Select:S1"), kAnimGeo);

    EXPECT_EQ(prim.level, WorldRenderLevel::ActorUnderlay)
        << "selection must still be ActorUnderlay";
    EXPECT_EQ(prim.local_suborder, kActorSelectionSuborder);
    EXPECT_FLOAT_EQ(prim.alpha, 1.0f) << "selection circle must remain fully opaque";
}
