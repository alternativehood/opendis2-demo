#include "city_asset_catalog_builder.hpp"

#include "ff_asset_store.hpp"

#include <d2engine/animation/animation_sequence.hpp>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace d2engine {

namespace {

[[noreturn]] void throw_city_asset_error(std::string_view container, std::string_view animation,
                                         std::string_view reason) {
    throw std::runtime_error("build_city_asset_catalog: " + std::string(reason) +
                             " container=" + std::string(container) +
                             " logical_animation=" + std::string(animation));
}

adventure_render::AnimatedCityLayer build_city_layer(const FfAssetStore& store,
                                                     std::string_view    container,
                                                     std::string_view    animation_name) {
    adventure_render::AnimatedCityLayer layer;
    layer.container_path = std::string(container);
    layer.logical_animation = std::string(animation_name);

    AnimationSequence anim_meta;
    try {
        anim_meta = store.animation_metadata(container, animation_name);
    } catch (const std::exception&) {
        throw_city_asset_error(container, animation_name, "animation_metadata_unavailable");
    }

    if (anim_meta.frames.empty()) {
        throw_city_asset_error(container, animation_name, "zero_frames");
    }
    if (anim_meta.native_canvas_w <= 0 || anim_meta.native_canvas_h <= 0) {
        throw_city_asset_error(container, animation_name, "invalid_native_canvas");
    }
    if (anim_meta.canvas_foot_x < 0 || anim_meta.canvas_foot_y < 0) {
        throw_city_asset_error(container, animation_name, "negative_canvas_foot");
    }

    layer.canvas_foot_x = anim_meta.canvas_foot_x;
    layer.canvas_foot_y = anim_meta.canvas_foot_y;
    layer.animation.animation_name = std::string(animation_name);
    layer.animation.native_canvas_w = anim_meta.native_canvas_w;
    layer.animation.native_canvas_h = anim_meta.native_canvas_h;
    layer.animation.is_looping = true;
    layer.animation.frames.reserve(anim_meta.frames.size());

    for (const auto& frame : anim_meta.frames) {
        adventure_render::AdventureAnimationFrame af;
        af.record_name = frame.image_name;
        af.duration_ms = static_cast<int>(frame.duration_ms);
        const auto frame_meta = [&]() {
            try {
                return store.sprite_metadata(container, frame.image_name);
            } catch (const std::exception&) {
                throw_city_asset_error(container, animation_name, "frame_metadata_unavailable");
            }
        }();
        if (frame_meta.canvas_width <= 0 || frame_meta.canvas_height <= 0) {
            throw_city_asset_error(container, animation_name, "invalid_frame_dimensions");
        }
        af.canvas_width = frame_meta.canvas_width;
        af.canvas_height = frame_meta.canvas_height;
        layer.animation.frames.push_back(std::move(af));
    }

    return layer;
}

void validate_matching_shadow(const adventure_render::AnimatedCityLayer& body,
                              const adventure_render::AnimatedCityLayer& shadow) {
    if (body.animation.frames.size() != shadow.animation.frames.size()) {
        throw std::runtime_error(
            "build_city_asset_catalog: frame_count_mismatch container=" + body.container_path +
            " logical_animation=" + body.logical_animation + " shadow=" + shadow.logical_animation);
    }

    for (std::size_t i = 0; i < body.animation.frames.size(); ++i) {
        if (body.animation.frames[i].duration_ms != shadow.animation.frames[i].duration_ms) {
            throw std::runtime_error(
                "build_city_asset_catalog: frame_duration_mismatch container=" +
                body.container_path + " logical_animation=" + body.logical_animation +
                " shadow=" + shadow.logical_animation + " frame=" + std::to_string(i));
        }
    }
}

} // namespace

adventure_render::CityAssetCatalog build_city_asset_catalog(const FfAssetStore& store) {
    adventure_render::CityAssetCatalog catalog;

    catalog.visuals[static_cast<std::size_t>(adventure_render::CityVisualTier::Small)].body =
        build_city_layer(store, "Imgs/IsoAnim.ff", "G000FT0000NE1");

    auto medium_body = build_city_layer(store, "Imgs/IsoAnim.ff", "G000FT0000NE2");
    auto medium_shadow = build_city_layer(store, "Imgs/IsoAnim.ff", "G000FT0000NE2S");
    validate_matching_shadow(medium_body, medium_shadow);
    catalog.visuals[static_cast<std::size_t>(adventure_render::CityVisualTier::Medium)].body =
        std::move(medium_body);
    catalog.visuals[static_cast<std::size_t>(adventure_render::CityVisualTier::Medium)].shadow =
        std::move(medium_shadow);

    catalog.visuals[static_cast<std::size_t>(adventure_render::CityVisualTier::Large)].body =
        build_city_layer(store, "Imgs/IsoAnim.ff", "G000FT0000NE3");

    return catalog;
}

} // namespace d2engine
