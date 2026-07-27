#include "stack_banner_asset_catalog_builder.hpp"

#include "ff_asset_store.hpp"

#include <d2adventure_render/stack_banner_asset_catalog.hpp>

#include <cstddef>
#include <stdexcept>
#include <string>

namespace d2engine {

namespace {

[[nodiscard]] std::runtime_error build_error(const std::string& reason) {
    return std::runtime_error("build_stack_banner_asset_catalog: " + reason);
}

} // namespace

namespace detail {

::d2engine::adventure_render::StackBannerAssetCatalog
build_stack_banner_asset_catalog_from_sequence(const ::d2engine::AnimationSequence& sequence) {
    if (sequence.frames.empty()) {
        throw build_error("stack_banner_empty_sequence animation=" + sequence.name);
    }
    if (sequence.native_canvas_w <= 0 || sequence.native_canvas_h <= 0) {
        throw build_error("stack_banner_invalid_native_canvas animation=" + sequence.name +
                          " canvas=(" + std::to_string(sequence.native_canvas_w) + "x" +
                          std::to_string(sequence.native_canvas_h) + ")");
    }
    if (sequence.canvas_foot_x < 0 || sequence.canvas_foot_y < 0) {
        throw build_error("stack_banner_invalid_canvas_foot animation=" + sequence.name +
                          " foot=(" + std::to_string(sequence.canvas_foot_x) + "," +
                          std::to_string(sequence.canvas_foot_y) + ")");
    }

    auto animation_metadata = [&](std::string_view, std::string_view) { return sequence; };
    auto sprite_metadata = [&](std::string_view, std::string_view) {
        struct SpriteMeta {
            int                                             canvas_width;
            int                                             canvas_height;
            int                                             canvas_foot_x;
            int                                             canvas_foot_y;
            d2engine::adventure_render::CanvasContentBounds content_bounds;
        };
        return SpriteMeta{sequence.native_canvas_w, sequence.native_canvas_h,
                          sequence.canvas_foot_x, sequence.canvas_foot_y,
                          sequence.frames.front().content_bounds};
    };

    return build_stack_banner_asset_catalog_from_metadata(animation_metadata, sprite_metadata);
}

} // namespace detail

::d2engine::adventure_render::StackBannerAssetCatalog
build_stack_banner_asset_catalog(const FfAssetStore& store) {
    auto animation_metadata = [&store](std::string_view container, std::string_view animation) {
        return store.animation_metadata(container, animation);
    };
    auto sprite_metadata = [&store](std::string_view container, std::string_view sprite) {
        return store.sprite_metadata(container, sprite);
    };
    return detail::build_stack_banner_asset_catalog_from_metadata(animation_metadata,
                                                                  sprite_metadata);
}

} // namespace d2engine
