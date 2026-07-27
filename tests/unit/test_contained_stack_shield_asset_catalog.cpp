#include <d2engine/assets/contained_stack_shield_asset_catalog_builder.hpp>

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
using d2engine::adventure_render::ContainedStackShieldAssetCatalog;

struct SpriteMeta {
    int canvas_width = 0;
    int canvas_height = 0;
    int canvas_foot_x = 0;
    int canvas_foot_y = 0;
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

[[nodiscard]] ContainedStackShieldAssetCatalog make_catalog() {
    return d2engine::detail::build_contained_stack_shield_asset_catalog_from_metadata(
        [](std::string_view, std::string_view animation) {
            AnimationSequence sequence;
            sequence.name = std::string(animation);
            sequence.container_path = "Imgs/IsoCmon.ff";
            if (animation == "G000RR0005SHLC8") {
                sequence.native_canvas_w = 640;
                sequence.native_canvas_h = 480;
                sequence.canvas_foot_x = 321;
                sequence.canvas_foot_y = 323;
                sequence.frames.push_back({.image_name = "ZC", .index = 0, .duration_ms = 100});
                return sequence;
            }
            if (animation == "G000RR0005SHLV8") {
                sequence.native_canvas_w = 640;
                sequence.native_canvas_h = 480;
                sequence.canvas_foot_x = 321;
                sequence.canvas_foot_y = 307;
                sequence.frames.push_back({.image_name = "0C", .index = 0, .duration_ms = 100});
                return sequence;
            }
            throw std::runtime_error("unexpected shield animation request animation=" +
                                     std::string(animation));
        },
        [](std::string_view, std::string_view sprite) {
            if (sprite == "ZC") {
                return SpriteMeta{640, 480, 321, 323};
            }
            if (sprite == "0C") {
                return SpriteMeta{640, 480, 321, 307};
            }
            return SpriteMeta{128, 96, 17, 23};
        });
}

void expect_direct_mapping(const ContainedStackShieldAssetCatalog& catalog, std::string_view race,
                           d2runtime::AdventureSettlementKind kind, std::string_view expected) {
    const auto& asset = catalog.resolve(race, kind);
    EXPECT_EQ(asset.outer_logical_name, expected);
    EXPECT_EQ(asset.sprite_name, expected);
}

} // namespace

TEST(ContainedStackShieldAssetCatalog, BuildsExactlyElevenEntriesAndNormalizesLowercaseRaceIds) {
    const auto catalog = make_catalog();
    ASSERT_EQ(catalog.assets.size(), 11u);

    expect_direct_mapping(catalog, "g000rr0000", d2runtime::AdventureSettlementKind::Capital,
                          "G000RR0000SHLC8");
    expect_direct_mapping(catalog, "g000rr0000", d2runtime::AdventureSettlementKind::Village,
                          "G000RR0000SHLV8");
    expect_direct_mapping(catalog, "g000rr0001", d2runtime::AdventureSettlementKind::Capital,
                          "G000RR0001SHLC8");
    expect_direct_mapping(catalog, "g000rr0001", d2runtime::AdventureSettlementKind::Village,
                          "G000RR0001SHLV8");
    expect_direct_mapping(catalog, "g000rr0002", d2runtime::AdventureSettlementKind::Capital,
                          "G000RR0002SHLC8");
    expect_direct_mapping(catalog, "g000rr0002", d2runtime::AdventureSettlementKind::Village,
                          "G000RR0002SHLV8");
    expect_direct_mapping(catalog, "g000rr0003", d2runtime::AdventureSettlementKind::Capital,
                          "G000RR0003SHLC8");
    expect_direct_mapping(catalog, "g000rr0003", d2runtime::AdventureSettlementKind::Village,
                          "G000RR0003SHLV8");
}

TEST(ContainedStackShieldAssetCatalog, NeutralVillageResolvesAndNeutralCapitalThrows) {
    const auto  catalog = make_catalog();
    const auto& asset = catalog.resolve("G000RR0004", d2runtime::AdventureSettlementKind::Village);
    EXPECT_EQ(asset.outer_logical_name, "G000RR8888SHLV8");
    EXPECT_EQ(asset.sprite_name, "G000RR8888SHLV8");
    expect_throw_contains(
        [&] { (void)catalog.resolve("G000RR0004", d2runtime::AdventureSettlementKind::Capital); },
        "contained_stack_shield_unsupported");
}

TEST(ContainedStackShieldAssetCatalog, UnsupportedRaceThrows) {
    const auto catalog = make_catalog();
    expect_throw_contains(
        [&] { (void)catalog.resolve("G000RR9999", d2runtime::AdventureSettlementKind::Village); },
        "contained_stack_shield_unsupported");
}

TEST(ContainedStackShieldAssetCatalog, ElfCapitalAndVillageUseAuthoredFramesAndCanvas) {
    const auto catalog = make_catalog();

    const auto& capital =
        catalog.resolve("G000RR0005", d2runtime::AdventureSettlementKind::Capital);
    EXPECT_EQ(capital.outer_logical_name, "G000RR0005SHLC8");
    EXPECT_EQ(capital.sprite_name, "ZC");
    EXPECT_EQ(capital.canvas_width, 640);
    EXPECT_EQ(capital.canvas_height, 480);
    EXPECT_EQ(capital.canvas_foot_x, 321);
    EXPECT_EQ(capital.canvas_foot_y, 323);

    const auto& village =
        catalog.resolve("G000RR0005", d2runtime::AdventureSettlementKind::Village);
    EXPECT_EQ(village.outer_logical_name, "G000RR0005SHLV8");
    EXPECT_EQ(village.sprite_name, "0C");
    EXPECT_EQ(village.canvas_width, 640);
    EXPECT_EQ(village.canvas_height, 480);
    EXPECT_EQ(village.canvas_foot_x, 321);
    EXPECT_EQ(village.canvas_foot_y, 307);
}

TEST(ContainedStackShieldAssetCatalog, ElfAnimationAndCanvasContractsAreEnforced) {
    expect_throw_contains(
        [&] {
            (void)d2engine::detail::build_contained_stack_shield_asset_catalog_from_metadata(
                [](std::string_view, std::string_view animation) {
                    AnimationSequence sequence;
                    sequence.name = std::string(animation);
                    sequence.container_path = "Imgs/IsoCmon.ff";
                    sequence.frames.push_back({.image_name = "ZC", .index = 0, .duration_ms = 100});
                    sequence.frames.push_back(
                        {.image_name = "EXTRA", .index = 1, .duration_ms = 100});
                    return sequence;
                },
                [](std::string_view, std::string_view) { return SpriteMeta{640, 480, 321, 323}; });
        },
        "contained_stack_shield_elf_frame_contract_failed");

    expect_throw_contains(
        [&] {
            (void)d2engine::detail::build_contained_stack_shield_asset_catalog_from_metadata(
                [](std::string_view, std::string_view animation) {
                    AnimationSequence sequence;
                    sequence.name = std::string(animation);
                    sequence.container_path = "Imgs/IsoCmon.ff";
                    sequence.frames.push_back(
                        {.image_name = "NOT_ZC", .index = 0, .duration_ms = 100});
                    return sequence;
                },
                [](std::string_view, std::string_view) { return SpriteMeta{640, 480, 321, 323}; });
        },
        "contained_stack_shield_elf_frame_contract_failed");

    expect_throw_contains(
        [&] {
            (void)d2engine::detail::build_contained_stack_shield_asset_catalog_from_metadata(
                [](std::string_view, std::string_view animation) {
                    AnimationSequence sequence;
                    sequence.name = std::string(animation);
                    sequence.container_path = "Imgs/IsoCmon.ff";
                    sequence.native_canvas_w = 640;
                    sequence.native_canvas_h = 480;
                    sequence.canvas_foot_x = 321;
                    sequence.canvas_foot_y = 323;
                    sequence.frames.push_back({.image_name = "ZC", .index = 0, .duration_ms = 100});
                    return sequence;
                },
                [](std::string_view, std::string_view) { return SpriteMeta{639, 480, 321, 323}; });
        },
        "contained_stack_shield_elf_frame_contract_failed");
}

TEST(ContainedStackShieldAssetCatalog, ShieldSelectionNeverReadsBannerIndex) {
    const auto source =
        read_source_file("src/d2engine/assets/contained_stack_shield_asset_catalog_builder.cpp");
    EXPECT_EQ(source.find("banner"), std::string::npos);
}
