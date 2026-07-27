#include <d2engine/app/adventure_interaction_mask.hpp>

#include <d2adventure_render/adventure_render_types.hpp>
#include <d2adventure_render/prepared_adventure_map.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace {

std::shared_ptr<const d2res::RgbaBuffer> make_rgba(int width, int height) {
    auto rgba = std::make_shared<d2res::RgbaBuffer>();
    rgba->width = static_cast<decltype(rgba->width)>(width);
    rgba->height = static_cast<decltype(rgba->height)>(height);
    rgba->rgba.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u,
                      0xFFu);
    return rgba;
}

d2engine::PreparedImageResult make_prepared_image(const d2engine::ImageAssetKey& key, int width,
                                                  int height) {
    auto prepared = std::make_shared<d2engine::PreparedImage>();
    prepared->key = key;
    prepared->pixels = make_rgba(width, height);
    return {
        .key = key, .image = std::move(prepared), .success = true, .error = {}, .elapsed_ms = 0.0};
}

} // namespace

TEST(AdventureInteractionMask, CollectsShieldMaskKeyButNotBannerKey) {
    d2engine::adventure_render::PreparedAdventureMap map;
    map.geometry = d2engine::adventure_render::AdventureMapGeometry::from_source(10, 10);

    const auto ordinary_id = d2engine::adventure_render::stable_render_id("StackLeader:STACK0");
    const auto shield_id =
        d2engine::adventure_render::stable_render_id("ContainedStackShield:STACK1");
    const auto banner_id =
        d2engine::adventure_render::stable_render_id("ContainedStackBanner:STACK1");

    map.pick_entries.push_back({.stable_id = ordinary_id,
                                .kind = d2engine::adventure_render::PickEntryKind::Stack,
                                .object_id = "STACK0"});
    map.pick_entries.push_back({.stable_id = shield_id,
                                .kind = d2engine::adventure_render::PickEntryKind::Stack,
                                .object_id = "STACK1"});

    d2engine::adventure_render::PreparedAdventureRenderPrimitive ordinary;
    ordinary.stable_id = ordinary_id;
    ordinary.debug_label = "StackLeader:STACK0";
    ordinary.level = d2engine::adventure_render::WorldRenderLevel::Actor;
    ordinary.container_path = "Imgs/IsoCmon.ff";
    ordinary.record_name = "ORDINARY_STACK";
    ordinary.depth_anchor = {2, 2};

    d2engine::adventure_render::PreparedAdventureRenderPrimitive shield;
    shield.stable_id = shield_id;
    shield.debug_label = "ContainedStackShield:CITY1:STACK1:g000rr0005";
    shield.level = d2engine::adventure_render::WorldRenderLevel::Actor;
    shield.container_path = "Imgs/IsoCmon.ff";
    shield.record_name = "ZC";
    shield.depth_anchor = {5, 5};

    d2engine::adventure_render::PreparedAdventureRenderPrimitive banner;
    banner.stable_id = banner_id;
    banner.debug_label = "ContainedStackBanner:CITY1:STACK1:2";
    banner.level = d2engine::adventure_render::WorldRenderLevel::Actor;
    banner.container_path = "Imgs/IsoCmon.ff";
    banner.record_name = "STACK_BANNER_0200";
    banner.depth_anchor = {5, 5};

    map.world_graph.world.push_back(ordinary);
    map.world_graph.world.push_back(shield);
    map.world_graph.world.push_back(banner);

    const auto keys = d2engine::collect_stack_mask_asset_keys(map);
    ASSERT_EQ(keys.size(), 2u);
    EXPECT_EQ(keys[0].container_path, "Imgs/IsoCmon.ff");
    EXPECT_EQ(keys[0].image_name, "ORDINARY_STACK");
    EXPECT_EQ(keys[1].container_path, "Imgs/IsoCmon.ff");
    EXPECT_EQ(keys[1].image_name, "ZC");
    EXPECT_EQ(keys.front().container_path, "Imgs/IsoCmon.ff");
    EXPECT_EQ(keys.front().kind, d2engine::ImageAssetKind::ComposedSprite);

    std::vector<d2engine::PreparedImageResult> decoded;
    decoded.push_back(make_prepared_image(keys[0], 80, 90));
    decoded.push_back(make_prepared_image(keys[1], 80, 90));

    EXPECT_EQ(d2engine::attach_stack_interaction_masks(map, decoded), 2u);
    EXPECT_NE(map.world_graph.world[0].interaction_mask, nullptr);
    EXPECT_NE(map.world_graph.world[1].interaction_mask, nullptr);
    EXPECT_EQ(map.world_graph.world[2].interaction_mask, nullptr);
}
