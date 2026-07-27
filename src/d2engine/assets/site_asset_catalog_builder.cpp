#include "site_asset_catalog_builder.hpp"

#include "ff_asset_store.hpp"

#include <d2engine/animation/animation_sequence.hpp>

#include <algorithm>
#include <array>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace d2engine {

namespace {

[[noreturn]] void throw_site_asset_error(std::string_view container, std::string_view logical,
                                         std::string_view reason) {
    throw std::runtime_error("build_site_asset_catalog: " + std::string(reason) + " container=" +
                             std::string(container) + " logical_asset=" + std::string(logical));
}

adventure_render::StaticSiteVisual build_static_site_visual(const FfAssetStore& store,
                                                            std::string_view    container,
                                                            std::string_view    logical_sprite) {
    adventure_render::StaticSiteVisual visual;
    visual.container_path = std::string(container);
    visual.logical_sprite = std::string(logical_sprite);

    FfAssetStore::SpriteMetadata meta{};
    try {
        meta = store.sprite_metadata(container, logical_sprite);
    } catch (const std::exception& e) {
        throw std::runtime_error(
            "build_site_asset_catalog: failed to resolve container=" + std::string(container) +
            " logical_asset=" + std::string(logical_sprite) + " reason=" + e.what());
    }

    if (meta.canvas_width <= 0 || meta.canvas_height <= 0 || meta.canvas_foot_x < 0 ||
        meta.canvas_foot_y < 0) {
        throw_site_asset_error(container, logical_sprite, "invalid_static_metadata");
    }
    if (!meta.content_bounds.valid()) {
        throw_site_asset_error(container, logical_sprite, "invalid_static_content_bounds");
    }

    visual.canvas_foot_x = meta.canvas_foot_x;
    visual.canvas_foot_y = meta.canvas_foot_y;
    visual.canvas_width = meta.canvas_width;
    visual.canvas_height = meta.canvas_height;
    visual.content_bounds = meta.content_bounds;
    return visual;
}

adventure_render::AnimatedSiteLayer build_animated_site_layer(const FfAssetStore& store,
                                                              std::string_view    container,
                                                              std::string_view logical_animation) {
    adventure_render::AnimatedSiteLayer layer;
    layer.container_path = std::string(container);
    layer.logical_animation = std::string(logical_animation);

    AnimationSequence anim_meta;
    try {
        anim_meta = store.animation_metadata(container, logical_animation);
    } catch (const std::exception& e) {
        throw std::runtime_error(
            "build_site_asset_catalog: failed to resolve container=" + std::string(container) +
            " logical_asset=" + std::string(logical_animation) + " reason=" + e.what());
    }

    if (anim_meta.frames.empty()) {
        throw_site_asset_error(container, logical_animation, "zero_frames");
    }
    if (anim_meta.native_canvas_w <= 0 || anim_meta.native_canvas_h <= 0) {
        throw_site_asset_error(container, logical_animation, "invalid_native_canvas");
    }
    if (anim_meta.canvas_foot_x < 0 || anim_meta.canvas_foot_y < 0) {
        throw_site_asset_error(container, logical_animation, "negative_canvas_foot");
    }

    layer.canvas_foot_x = anim_meta.canvas_foot_x;
    layer.canvas_foot_y = anim_meta.canvas_foot_y;
    layer.content_bounds = {};
    layer.animation.animation_name = std::string(logical_animation);
    layer.animation.native_canvas_w = anim_meta.native_canvas_w;
    layer.animation.native_canvas_h = anim_meta.native_canvas_h;
    layer.animation.is_looping = true;
    layer.animation.timing_source =
        adventure_render::AdventureAnimationTimingSource::ProvisionalFallback;
    layer.animation.frames.reserve(anim_meta.frames.size());

    for (const auto& frame : anim_meta.frames) {
        adventure_render::AdventureAnimationFrame out;
        out.record_name = frame.image_name;
        out.duration_ms = static_cast<int>(frame.duration_ms);

        const auto meta = [&]() {
            try {
                return store.sprite_metadata(container, frame.image_name);
            } catch (const std::exception& e) {
                throw std::runtime_error("build_site_asset_catalog: failed to resolve container=" +
                                         std::string(container) + " logical_asset=" +
                                         std::string(frame.image_name) + " reason=" + e.what());
            }
        }();

        if (meta.canvas_width <= 0 || meta.canvas_height <= 0) {
            throw_site_asset_error(container, frame.image_name, "invalid_frame_dimensions");
        }
        if (!meta.content_bounds.valid()) {
            throw_site_asset_error(container, frame.image_name, "invalid_frame_content_bounds");
        }

        out.canvas_width = meta.canvas_width;
        out.canvas_height = meta.canvas_height;
        out.content_bounds = meta.content_bounds;
        layer.content_bounds =
            adventure_render::union_canvas_content_bounds(layer.content_bounds, out.content_bounds);
        layer.animation.frames.push_back(std::move(out));
    }

    if (!layer.content_bounds.valid()) {
        throw_site_asset_error(container, logical_animation, "invalid_layer_content_bounds");
    }

    return layer;
}

adventure_render::AnimatedSiteVisual build_merchant_animated_visual(const FfAssetStore& store,
                                                                    bool has_shadow) {
    adventure_render::AnimatedSiteVisual visual;
    visual.body = build_animated_site_layer(store, "Imgs/IsoAnim.ff", "G000SI0000MERH00");
    if (has_shadow) {
        visual.shadow = build_animated_site_layer(store, "Imgs/IsoAnim.ff", "G000SI0000MERH00S");
        if (visual.body.animation.frames.size() != visual.shadow->animation.frames.size()) {
            throw std::runtime_error(
                "build_site_asset_catalog: frame_count_mismatch container=Imgs/IsoAnim.ff "
                "logical_asset=G000SI0000MERH00 shadow=G000SI0000MERH00S");
        }
        for (std::size_t i = 0; i < visual.body.animation.frames.size(); ++i) {
            if (visual.body.animation.frames[i].duration_ms !=
                visual.shadow->animation.frames[i].duration_ms) {
                throw std::runtime_error(
                    "build_site_asset_catalog: frame_duration_mismatch container=Imgs/IsoAnim.ff "
                    "logical_asset=G000SI0000MERH00 shadow=G000SI0000MERH00S frame=" +
                    std::to_string(i));
            }
        }
    }
    return visual;
}

} // namespace

adventure_render::SiteAssetCatalog build_site_asset_catalog(const FfAssetStore& store) {
    adventure_render::SiteAssetCatalog catalog;

    catalog.mage_visuals[0] = adventure_render::SiteVisual{
        build_static_site_visual(store, "Imgs/IsoCmon.ff", "G000SI0000MAGE00")};
    catalog.mage_visuals[1] = adventure_render::SiteVisual{
        build_static_site_visual(store, "Imgs/IsoCmon.ff", "G000SI0000MAGE01")};
    catalog.mage_visuals[2] = adventure_render::SiteVisual{
        build_static_site_visual(store, "Imgs/IsoCmon.ff", "G000SI0000MAGE02")};
    catalog.mage_visuals[3] = adventure_render::SiteVisual{
        build_static_site_visual(store, "Imgs/IsoCmon.ff", "G000SI0000MAGE03")};

    catalog.merchant_visuals[0] =
        adventure_render::SiteVisual{build_merchant_animated_visual(store, true)};
    catalog.merchant_visuals[1] = adventure_render::SiteVisual{
        build_static_site_visual(store, "Imgs/IsoCmon.ff", "G000SI0000MERH01")};
    catalog.merchant_visuals[2] = adventure_render::SiteVisual{
        build_static_site_visual(store, "Imgs/IsoCmon.ff", "G000SI0000MERH02")};
    catalog.merchant_visuals[3] = adventure_render::SiteVisual{
        build_static_site_visual(store, "Imgs/IsoCmon.ff", "G000SI0000MERH03")};
    catalog.merchant_visuals[4] = adventure_render::SiteVisual{
        build_static_site_visual(store, "Imgs/IsoCmon.ff", "G000SI0000MERH04")};
    catalog.merchant_visuals[5] = adventure_render::SiteVisual{
        build_static_site_visual(store, "Imgs/IsoCmon.ff", "G000SI0000MERH05")};
    catalog.merchant_visuals[6] = adventure_render::SiteVisual{
        build_static_site_visual(store, "Imgs/IsoCmon.ff", "G000SI0000MERH06")};
    catalog.merchant_visuals[7] =
        adventure_render::SiteVisual{build_merchant_animated_visual(store, false)};

    catalog.mercenary_visuals[0] = adventure_render::SiteVisual{
        build_static_site_visual(store, "Imgs/IsoCmon.ff", "G000SI0000MERC00")};
    catalog.mercenary_visuals[1] = adventure_render::SiteVisual{
        build_static_site_visual(store, "Imgs/IsoCmon.ff", "G000SI0000MERC01")};
    catalog.mercenary_visuals[2] = adventure_render::SiteVisual{
        build_static_site_visual(store, "Imgs/IsoCmon.ff", "G000SI0000MERC02")};
    catalog.mercenary_visuals[3] = adventure_render::SiteVisual{
        build_static_site_visual(store, "Imgs/IsoCmon.ff", "G000SI0000MERC03")};
    catalog.mercenary_visuals[4] = adventure_render::SiteVisual{
        build_static_site_visual(store, "Imgs/IsoCmon.ff", "G000SI0000MERC04")};

    catalog.trainer_visuals[0] = adventure_render::SiteVisual{
        build_static_site_visual(store, "Imgs/IsoCmon.ff", "G000SI0000TRAI00")};
    catalog.trainer_visuals[1] = adventure_render::SiteVisual{
        build_static_site_visual(store, "Imgs/IsoCmon.ff", "G000SI0000TRAI01")};
    catalog.trainer_visuals[2] = adventure_render::SiteVisual{
        build_static_site_visual(store, "Imgs/IsoCmon.ff", "G000SI0000TRAI02")};
    catalog.trainer_visuals[3] = adventure_render::SiteVisual{
        build_static_site_visual(store, "Imgs/IsoCmon.ff", "G000SI0000TRAI03")};

    return catalog;
}

} // namespace d2engine
