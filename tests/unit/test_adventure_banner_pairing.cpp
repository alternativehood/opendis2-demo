#include <d2adventure_render/adventure_render_types.hpp>
#include <d2adventure_render/prepared_adventure_map.hpp>

#include "opendis2/adventure_startup_screen.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ar = d2engine::adventure_render;
namespace odetail = opendis2::detail;

namespace {

ar::PreparedAdventureRenderPrimitive make_primitive(ar::AdventurePrimitiveRole role,
                                                    std::string object_id, std::string debug_label,
                                                    bool is_banner, std::string container,
                                                    std::string record) {
    ar::PreparedAdventureRenderPrimitive prim;
    prim.stable_id = ar::stable_render_id(debug_label);
    prim.debug_label = std::move(debug_label);
    prim.semantic_role = role;
    prim.semantic_object_id = std::move(object_id);
    prim.container_path = std::move(container);
    prim.record_name = std::move(record);
    prim.visibility_group = is_banner ? ar::AdventureRenderVisibilityGroup::Banners
                                      : ar::AdventureRenderVisibilityGroup::Default;
    prim.content_bounds = {0, 0, 10, 10};
    prim.src_width = 10;
    prim.src_height = 10;
    return prim;
}

void add_pair(std::vector<ar::PreparedAdventureRenderPrimitive>& collection,
              ar::AdventurePrimitiveRole body_role, ar::AdventurePrimitiveRole banner_role,
              const std::string& object_id, const std::string& debug_prefix,
              const std::string& container, const std::string& record) {
    collection.push_back(
        make_primitive(body_role, object_id, debug_prefix + ":body", false, container, record));
    collection.push_back(
        make_primitive(banner_role, object_id, debug_prefix + ":banner", true, container, record));
}

ar::PreparedAdventureRenderPrimitive make_banner_only(ar::AdventurePrimitiveRole role,
                                                      const std::string&         object_id,
                                                      const std::string&         debug_label,
                                                      const std::string&         container,
                                                      const std::string&         record) {
    auto prim = make_primitive(role, object_id, debug_label, true, container, record);
    prim.interaction_mask = nullptr;
    return prim;
}

} // namespace

TEST(AdventureBannerPairing, MapStackPairCounts) {
    ar::PreparedAdventureRenderGraph graph;
    add_pair(graph.world, ar::AdventurePrimitiveRole::MapStackBody,
             ar::AdventurePrimitiveRole::MapStackBanner, "stack-1", "unrelated-stack", "a", "A");

    const auto stats = odetail::collect_adventure_banner_pairing_statistics(graph);
    EXPECT_EQ(stats.map_stack_bodies, 1u);
    EXPECT_EQ(stats.map_stack_banners, 1u);
    EXPECT_EQ(stats.total_bodies, 1u);
    EXPECT_EQ(stats.total_banners, 1u);
}

TEST(AdventureBannerPairing, SitePairCounts) {
    ar::PreparedAdventureRenderGraph graph;
    add_pair(graph.world, ar::AdventurePrimitiveRole::SiteBody,
             ar::AdventurePrimitiveRole::SiteBanner, "site-1", "unrelated-site", "a", "A");

    const auto stats = odetail::collect_adventure_banner_pairing_statistics(graph);
    EXPECT_EQ(stats.site_bodies, 1u);
    EXPECT_EQ(stats.site_banners, 1u);
}

TEST(AdventureBannerPairing, RuinPairCounts) {
    ar::PreparedAdventureRenderGraph graph;
    add_pair(graph.world, ar::AdventurePrimitiveRole::RuinBody,
             ar::AdventurePrimitiveRole::RuinBanner, "ruin-1", "unrelated-ruin", "a", "A");

    const auto stats = odetail::collect_adventure_banner_pairing_statistics(graph);
    EXPECT_EQ(stats.ruin_bodies, 1u);
    EXPECT_EQ(stats.ruin_banners, 1u);
}

TEST(AdventureBannerPairing, RuinBodyWithoutBannerFails) {
    ar::PreparedAdventureRenderGraph graph;
    graph.world.push_back(make_primitive(ar::AdventurePrimitiveRole::RuinBody, "ruin-1",
                                         "body-only", false, "a", "A"));

    EXPECT_THROW(static_cast<void>(odetail::collect_adventure_banner_pairing_statistics(graph)),
                 std::runtime_error);
}

TEST(AdventureBannerPairing, RuinBannerWithoutBodyFails) {
    ar::PreparedAdventureRenderGraph graph;
    graph.world.push_back(make_banner_only(ar::AdventurePrimitiveRole::RuinBanner, "ruin-1",
                                           "banner-only", "a", "A"));

    EXPECT_THROW(static_cast<void>(odetail::collect_adventure_banner_pairing_statistics(graph)),
                 std::runtime_error);
}

TEST(AdventureBannerPairing, DuplicateRuinBannersFail) {
    ar::PreparedAdventureRenderGraph graph;
    graph.world.push_back(
        make_primitive(ar::AdventurePrimitiveRole::RuinBody, "ruin-1", "body", false, "a", "A"));
    graph.world.push_back(
        make_banner_only(ar::AdventurePrimitiveRole::RuinBanner, "ruin-1", "banner-1", "a", "A"));
    graph.world.push_back(
        make_banner_only(ar::AdventurePrimitiveRole::RuinBanner, "ruin-1", "banner-2", "a", "A"));

    EXPECT_THROW(static_cast<void>(odetail::collect_adventure_banner_pairing_statistics(graph)),
                 std::runtime_error);
}

TEST(AdventureBannerPairing, UnresolvedTypedRuinWithNoPrimitivesPasses) {
    ar::PreparedAdventureRenderGraph graph;
    const auto stats = odetail::collect_adventure_banner_pairing_statistics(graph);
    EXPECT_EQ(stats.total_bodies, 0u);
    EXPECT_EQ(stats.total_banners, 0u);
}

TEST(AdventureBannerPairing, BannerWithPickEntryFails) {
    ar::PreparedAdventureRenderGraph graph;
    graph.world.push_back(
        make_primitive(ar::AdventurePrimitiveRole::RuinBody, "ruin-1", "body", false, "a", "A"));
    const auto banner =
        make_banner_only(ar::AdventurePrimitiveRole::RuinBanner, "ruin-1", "banner", "a", "A");
    graph.pick_entries.push_back(
        {.stable_id = banner.stable_id, .kind = ar::PickEntryKind::Stack, .object_id = "ruin-1"});
    graph.world.push_back(banner);

    EXPECT_THROW(static_cast<void>(odetail::collect_adventure_banner_pairing_statistics(graph)),
                 std::runtime_error);
}

TEST(AdventureBannerPairing, BannerWithInteractionMaskFails) {
    ar::PreparedAdventureRenderGraph graph;
    graph.world.push_back(
        make_primitive(ar::AdventurePrimitiveRole::RuinBody, "ruin-1", "body", false, "a", "A"));
    auto banner =
        make_banner_only(ar::AdventurePrimitiveRole::RuinBanner, "ruin-1", "banner", "a", "A");
    banner.interaction_mask = std::make_shared<ar::InteractionMask>();
    graph.world.push_back(banner);

    EXPECT_THROW(static_cast<void>(odetail::collect_adventure_banner_pairing_statistics(graph)),
                 std::runtime_error);
}

TEST(AdventureBannerPairing, StatisticsIgnoreDebugLabels) {
    ar::PreparedAdventureRenderGraph graph;
    add_pair(graph.ground_overlay, ar::AdventurePrimitiveRole::MapStackBody,
             ar::AdventurePrimitiveRole::MapStackBanner, "stack-1", "zzz-1", "assets", "shared");
    add_pair(graph.world, ar::AdventurePrimitiveRole::SiteBody,
             ar::AdventurePrimitiveRole::SiteBanner, "site-1", "zzz-2", "assets", "shared");
    add_pair(graph.ui_overlay, ar::AdventurePrimitiveRole::RuinBody,
             ar::AdventurePrimitiveRole::RuinBanner, "ruin-1", "zzz-3", "assets", "shared");

    const auto stats = odetail::collect_adventure_banner_pairing_statistics(graph);
    EXPECT_EQ(stats.map_stack_bodies, 1u);
    EXPECT_EQ(stats.site_bodies, 1u);
    EXPECT_EQ(stats.ruin_bodies, 1u);
    EXPECT_EQ(stats.total_bodies, 3u);
    EXPECT_EQ(stats.total_banners, 3u);
}

TEST(AdventureBannerPairing, DuplicateAssetUseCountsOnce) {
    ar::PreparedAdventureRenderGraph graph;
    add_pair(graph.world, ar::AdventurePrimitiveRole::MapStackBody,
             ar::AdventurePrimitiveRole::MapStackBanner, "stack-1", "one", "shared", "A");
    add_pair(graph.world, ar::AdventurePrimitiveRole::SiteBody,
             ar::AdventurePrimitiveRole::SiteBanner, "site-1", "two", "shared", "A");
    add_pair(graph.world, ar::AdventurePrimitiveRole::RuinBody,
             ar::AdventurePrimitiveRole::RuinBanner, "ruin-1", "three", "shared", "A");

    const auto stats = odetail::collect_adventure_banner_pairing_statistics(graph);
    EXPECT_EQ(stats.total_unique_assets, 1u);
}
