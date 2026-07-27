#pragma once

#include <d2adventure_render/contained_stack_shield_asset_catalog.hpp>
#include <d2engine/animation/animation_sequence.hpp>

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <string_view>

namespace d2engine {

class FfAssetStore;

namespace detail {

template <typename AnimationMetadataFn, typename SpriteMetadataFn>
[[nodiscard]] ::d2engine::adventure_render::ContainedStackShieldAssetCatalog
build_contained_stack_shield_asset_catalog_from_metadata(AnimationMetadataFn&& animation_metadata,
                                                         SpriteMetadataFn&&    sprite_metadata);

} // namespace detail

[[nodiscard]] ::d2engine::adventure_render::ContainedStackShieldAssetCatalog
build_contained_stack_shield_asset_catalog(const FfAssetStore& store);

namespace detail {

namespace internal {

[[nodiscard]] inline std::string upper_ascii(std::string_view value) {
    std::string result(value);
    std::ranges::transform(result, result.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    });
    return result;
}

[[nodiscard]] inline std::string settlement_name(d2runtime::AdventureSettlementKind kind) {
    return kind == d2runtime::AdventureSettlementKind::Village ? "Village" : "Capital";
}

template <typename SpriteMetadataFn>
[[nodiscard]] inline ::d2engine::adventure_render::ContainedStackShieldAsset
make_direct_asset(std::string_view logical_name, SpriteMetadataFn&& sprite_metadata) {
    const auto meta = sprite_metadata("Imgs/IsoCmon.ff", logical_name);
    return {.container_path = "Imgs/IsoCmon.ff",
            .outer_logical_name = std::string(logical_name),
            .sprite_name = std::string(logical_name),
            .canvas_width = meta.canvas_width,
            .canvas_height = meta.canvas_height,
            .canvas_foot_x = meta.canvas_foot_x,
            .canvas_foot_y = meta.canvas_foot_y};
}

template <typename AnimationMetadataFn, typename SpriteMetadataFn>
[[nodiscard]] inline ::d2engine::adventure_render::ContainedStackShieldAsset
make_elf_asset(std::string_view logical_name, std::string_view expected_frame,
               AnimationMetadataFn&& animation_metadata, SpriteMetadataFn&& sprite_metadata,
               int expected_foot_x, int expected_foot_y) {
    const auto sequence = animation_metadata("Imgs/IsoCmon.ff", logical_name);
    if (sequence.frames.size() != 1u) {
        throw std::runtime_error("contained_stack_shield_elf_frame_contract_failed logical=" +
                                 std::string(logical_name));
    }
    const auto& frame = sequence.frames.front();
    if (frame.image_name != expected_frame) {
        throw std::runtime_error("contained_stack_shield_elf_frame_contract_failed logical=" +
                                 std::string(logical_name));
    }
    const auto sprite = sprite_metadata("Imgs/IsoCmon.ff", expected_frame);
    if (sequence.native_canvas_w != sprite.canvas_width ||
        sequence.native_canvas_h != sprite.canvas_height ||
        sequence.canvas_foot_x != sprite.canvas_foot_x ||
        sequence.canvas_foot_y != sprite.canvas_foot_y) {
        throw std::runtime_error("contained_stack_shield_elf_frame_contract_failed logical=" +
                                 std::string(logical_name));
    }
    if (sprite.canvas_foot_x != expected_foot_x || sprite.canvas_foot_y != expected_foot_y) {
        throw std::runtime_error("contained_stack_shield_elf_frame_contract_failed logical=" +
                                 std::string(logical_name));
    }
    return {.container_path = "Imgs/IsoCmon.ff",
            .outer_logical_name = std::string(logical_name),
            .sprite_name = std::string(expected_frame),
            .canvas_width = sprite.canvas_width,
            .canvas_height = sprite.canvas_height,
            .canvas_foot_x = sprite.canvas_foot_x,
            .canvas_foot_y = sprite.canvas_foot_y};
}

} // namespace internal

template <typename AnimationMetadataFn, typename SpriteMetadataFn>
[[nodiscard]] inline ::d2engine::adventure_render::ContainedStackShieldAssetCatalog
build_contained_stack_shield_asset_catalog_from_metadata(AnimationMetadataFn&& animation_metadata,
                                                         SpriteMetadataFn&&    sprite_metadata) {
    ::d2engine::adventure_render::ContainedStackShieldAssetCatalog catalog;
    catalog.assets.reserve(11);

    catalog.assets.push_back(internal::make_direct_asset("G000RR0000SHLC8", sprite_metadata));
    catalog.assets.push_back(internal::make_direct_asset("G000RR0000SHLV8", sprite_metadata));
    catalog.assets.push_back(internal::make_direct_asset("G000RR0001SHLC8", sprite_metadata));
    catalog.assets.push_back(internal::make_direct_asset("G000RR0001SHLV8", sprite_metadata));
    catalog.assets.push_back(internal::make_direct_asset("G000RR0002SHLC8", sprite_metadata));
    catalog.assets.push_back(internal::make_direct_asset("G000RR0002SHLV8", sprite_metadata));
    catalog.assets.push_back(internal::make_direct_asset("G000RR0003SHLC8", sprite_metadata));
    catalog.assets.push_back(internal::make_direct_asset("G000RR0003SHLV8", sprite_metadata));
    catalog.assets.push_back(internal::make_direct_asset("G000RR8888SHLV8", sprite_metadata));
    catalog.assets.push_back(internal::make_elf_asset("G000RR0005SHLC8", "ZC", animation_metadata,
                                                      sprite_metadata, 321, 323));
    catalog.assets.push_back(internal::make_elf_asset("G000RR0005SHLV8", "0C", animation_metadata,
                                                      sprite_metadata, 321, 307));

    return catalog;
}

} // namespace detail

} // namespace d2engine
