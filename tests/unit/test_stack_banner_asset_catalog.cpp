#include <d2engine/assets/stack_banner_asset_catalog_builder.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace {

using d2engine::AnimationSequence;
using d2engine::adventure_render::StackBannerAssetCatalog;

struct SpriteMeta {
    int                                             canvas_width = 0;
    int                                             canvas_height = 0;
    int                                             canvas_foot_x = 0;
    int                                             canvas_foot_y = 0;
    d2engine::adventure_render::CanvasContentBounds content_bounds;
};

[[nodiscard]] std::string read_source_file(const std::string& rel_path) {
    const auto    path = std::filesystem::path(OPENDIS2_SOURCE_DIR) / rel_path;
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("cannot open source file: " + path.string());
    }
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

template <typename Fn> void expect_throw_contains(Fn&& fn, const std::string& needle) {
    try {
        fn();
        FAIL() << "expected exception containing: " << needle;
    } catch (const std::exception& e) {
        EXPECT_NE(std::string(e.what()).find(needle), std::string::npos) << e.what();
    }
}

} // namespace

TEST(StackBannerAssetCatalog, BuildsExactFifteenEntriesAndResolvesStackBannerSprites) {
    int                      animation_calls = 0;
    std::vector<std::string> sprite_requests;

    const auto catalog = d2engine::detail::build_stack_banner_asset_catalog_from_metadata(
        [&](std::string_view container, std::string_view animation) {
            ++animation_calls;
            EXPECT_EQ(container, "Imgs/IsoCmon.ff");
            EXPECT_EQ(animation, "STACK_BANNER_1400");

            AnimationSequence sequence;
            sequence.name = std::string(animation);
            sequence.container_path = std::string(container);
            sequence.native_canvas_w = 640;
            sequence.native_canvas_h = 480;
            sequence.canvas_foot_x = 111;
            sequence.canvas_foot_y = 222;
            sequence.frames.push_back({.image_name = "TI", .index = 0, .duration_ms = 100});
            return sequence;
        },
        [&](std::string_view container, std::string_view sprite) {
            EXPECT_EQ(container, "Imgs/IsoCmon.ff");
            sprite_requests.emplace_back(sprite);
            return SpriteMeta{640, 480, 111, 222, {10, 20, 30, 40}};
        });

    ASSERT_EQ(catalog.frames.size(), 15u);
    EXPECT_EQ(catalog.resolve_banner(0).record_name, "STACK_BANNER_0000");
    EXPECT_EQ(catalog.resolve_banner(4).record_name, "STACK_BANNER_0400");
    EXPECT_EQ(catalog.resolve_banner(9).record_name, "STACK_BANNER_0900");
    EXPECT_EQ(catalog.resolve_banner(13).record_name, "STACK_BANNER_1300");
    EXPECT_EQ(catalog.resolve_banner(14).record_name, "TI");
    EXPECT_EQ(catalog.resolve_banner(14).canvas_width, 640);
    EXPECT_EQ(catalog.resolve_banner(14).canvas_height, 480);
    EXPECT_EQ(catalog.resolve_banner(14).canvas_foot_x, 111);
    EXPECT_EQ(catalog.resolve_banner(14).canvas_foot_y, 222);

    EXPECT_EQ(animation_calls, 1);
    EXPECT_EQ(sprite_requests.size(), 15u);
    EXPECT_EQ(sprite_requests.front(), "STACK_BANNER_0000");
    EXPECT_EQ(sprite_requests[4], "STACK_BANNER_0400");
    EXPECT_EQ(sprite_requests[9], "STACK_BANNER_0900");
    EXPECT_EQ(sprite_requests[13], "STACK_BANNER_1300");
    EXPECT_EQ(sprite_requests.back(), "TI");
    EXPECT_EQ(std::count(sprite_requests.begin(), sprite_requests.end(), "STACK_BANNER_1400"), 0);
}

TEST(StackBannerAssetCatalog, Banner14RequiresExactlyOneAnimationFrame) {
    expect_throw_contains(
        [&] {
            (void)d2engine::detail::build_stack_banner_asset_catalog_from_metadata(
                [](std::string_view, std::string_view) {
                    AnimationSequence sequence;
                    sequence.frames.push_back({.image_name = "TI", .index = 0, .duration_ms = 100});
                    sequence.frames.push_back(
                        {.image_name = "EXTRA", .index = 1, .duration_ms = 100});
                    return sequence;
                },
                [](std::string_view, std::string_view) {
                    return SpriteMeta{640, 480, 111, 222, {10, 20, 30, 40}};
                });
        },
        "stack_banner_1400_invalid_frame_count");
}

TEST(StackBannerAssetCatalog, Banner14RequiresFrameNameTi) {
    expect_throw_contains(
        [&] {
            (void)d2engine::detail::build_stack_banner_asset_catalog_from_metadata(
                [](std::string_view, std::string_view) {
                    AnimationSequence sequence;
                    sequence.frames.push_back(
                        {.image_name = "NOT_TI", .index = 0, .duration_ms = 100});
                    return sequence;
                },
                [](std::string_view, std::string_view) {
                    return SpriteMeta{640, 480, 111, 222, {10, 20, 30, 40}};
                });
        },
        "stack_banner_1400_invalid_frame_name");
}

TEST(StackBannerAssetCatalog, RejectsNegativeAndOutOfRangeBannerIndices) {
    const auto catalog = d2engine::detail::build_stack_banner_asset_catalog_from_metadata(
        [](std::string_view, std::string_view) {
            AnimationSequence sequence;
            sequence.frames.push_back({.image_name = "TI", .index = 0, .duration_ms = 100});
            return sequence;
        },
        [](std::string_view, std::string_view) {
            return SpriteMeta{640, 480, 111, 222, {10, 20, 30, 40}};
        });

    expect_throw_contains([&] { (void)catalog.resolve_banner(-1); },
                          "stack_banner_index_out_of_range");
    expect_throw_contains([&] { (void)catalog.resolve_banner(15); },
                          "stack_banner_index_out_of_range");
}

TEST(StackBannerAssetCatalog, ProductionCodeDoesNotReferenceShieldsLogicalAsset) {
    const auto builder_source =
        read_source_file("src/d2engine/assets/stack_banner_asset_catalog_builder.cpp");
    const auto startup_source = read_source_file("src/opendis2/adventure_startup_screen.cpp");

    EXPECT_EQ(builder_source.find("animation_metadata(\"Imgs/IsoCmon.ff\", \"SHIELDS\")"),
              std::string::npos);
    EXPECT_EQ(builder_source.find("SHIELDS.PNG"), std::string::npos);
    EXPECT_EQ(builder_source.find("SHIELDS"), std::string::npos);
    EXPECT_EQ(startup_source.find("SHIELDS frame"), std::string::npos);
    EXPECT_EQ(startup_source.find("SHIELDS.PNG"), std::string::npos);
}
