#pragma once

#include <d2engine/animation/animation_sequence.hpp>
#include <d2adventure_render/stack_banner_asset_catalog.hpp>

#include <stdexcept>
#include <string>
#include <string_view>

namespace d2engine {

class FfAssetStore;

namespace detail {

template <typename AnimationMetadataFn, typename SpriteMetadataFn>
[[nodiscard]] ::d2engine::adventure_render::StackBannerAssetCatalog
build_stack_banner_asset_catalog_from_metadata(AnimationMetadataFn&& animation_metadata,
                                               SpriteMetadataFn&&    sprite_metadata) {
    constexpr std::string_view kContainerPath = "Imgs/IsoCmon.ff";

    auto make_logical_name = [](int banner) {
        const int   value = banner * 100;
        std::string digits = std::to_string(value);
        if (digits.size() < 4) {
            digits.insert(digits.begin(), 4u - digits.size(), '0');
        }
        return std::string("STACK_BANNER_") + digits;
    };

    ::d2engine::adventure_render::StackBannerAssetCatalog catalog;
    catalog.frames.reserve(15);

    for (int banner = 0; banner <= 14; ++banner) {
        ::d2engine::adventure_render::StackBannerAsset frame;
        frame.container_path = std::string(kContainerPath);

        if (banner < 14) {
            const auto logical_sprite = make_logical_name(banner);
            const auto sprite = sprite_metadata(kContainerPath, logical_sprite);
            if (!sprite.content_bounds.valid()) {
                throw std::runtime_error("stack_banner_invalid_content_bounds sprite=" +
                                         logical_sprite);
            }
            frame.record_name = logical_sprite;
            frame.canvas_width = sprite.canvas_width;
            frame.canvas_height = sprite.canvas_height;
            frame.canvas_foot_x = sprite.canvas_foot_x;
            frame.canvas_foot_y = sprite.canvas_foot_y;
            frame.content_bounds = sprite.content_bounds;
        } else {
            const auto sequence = animation_metadata(kContainerPath, "STACK_BANNER_1400");
            if (sequence.frames.size() != 1u) {
                throw std::runtime_error("stack_banner_1400_invalid_frame_count frame_count=" +
                                         std::to_string(sequence.frames.size()));
            }
            const auto& anim_frame = sequence.frames.front();
            if (anim_frame.image_name != "TI") {
                throw std::runtime_error("stack_banner_1400_invalid_frame_name frame_name=" +
                                         anim_frame.image_name);
            }
            const auto sprite = sprite_metadata(kContainerPath, anim_frame.image_name);
            if (!sprite.content_bounds.valid()) {
                throw std::runtime_error("stack_banner_invalid_content_bounds sprite=" +
                                         anim_frame.image_name);
            }
            frame.record_name = anim_frame.image_name;
            frame.canvas_width = sprite.canvas_width;
            frame.canvas_height = sprite.canvas_height;
            frame.canvas_foot_x = sprite.canvas_foot_x;
            frame.canvas_foot_y = sprite.canvas_foot_y;
            frame.content_bounds = sprite.content_bounds;
        }

        catalog.frames.push_back(std::move(frame));
    }

    return catalog;
}

[[nodiscard]] ::d2engine::adventure_render::StackBannerAssetCatalog
build_stack_banner_asset_catalog_from_sequence(const ::d2engine::AnimationSequence& sequence);

} // namespace detail

[[nodiscard]] ::d2engine::adventure_render::StackBannerAssetCatalog
build_stack_banner_asset_catalog(const FfAssetStore& store);

} // namespace d2engine
