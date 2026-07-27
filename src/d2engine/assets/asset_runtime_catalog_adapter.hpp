#pragma once

#include "asset_runtime.hpp"
#include "ff_asset_store.hpp"
#include "sprite_animation_catalog.hpp"

#include <algorithm>

namespace d2engine {

class AssetRuntimeCatalogAdapter final : public ISpriteAnimationCatalog {
public:
    explicit AssetRuntimeCatalogAdapter(const AssetRuntime& assets) : assets_(&assets) {}

    [[nodiscard]] std::vector<std::string>
    animations_in(std::string_view container) const override {
        return assets_->store().animations_in(container);
    }

    [[nodiscard]] AnimationSequence animation_sequence(std::string_view container,
                                                       std::string_view anim_name) const override {
        return assets_->animation_sequence(container, anim_name);
    }

    [[nodiscard]] AnimationSpriteMeta sprite_metadata(std::string_view container,
                                                      std::string_view sprite_name) const override {
        const auto resolved = assets_->store().resolve_sprite_fast(container, sprite_name);
        if (!resolved.has_value()) {
            throw std::runtime_error("sprite_metadata: unresolved logical sprite " +
                                     std::string(sprite_name) + " in " + std::string(container));
        }
        if (resolved->is_raw_png) {
            throw std::runtime_error(
                "sprite_metadata: raw PNG where OPT frame metadata required for " +
                std::string(sprite_name) + " in " + std::string(container));
        }
        if (resolved->frame == nullptr) {
            throw std::runtime_error("sprite_metadata: missing OPT ImageFrame for " +
                                     std::string(sprite_name) + " in " + std::string(container));
        }
        if (resolved->frame->output_width <= 0 || resolved->frame->output_height <= 0) {
            throw std::runtime_error("sprite_metadata: non-positive output dimensions for " +
                                     std::string(sprite_name) + " in " + std::string(container));
        }
        d2engine::adventure_render::CanvasContentBounds content_bounds;
        if (!resolved->frame->pieces.empty()) {
            content_bounds.min_x = resolved->frame->pieces.front().output_x;
            content_bounds.max_x =
                resolved->frame->pieces.front().output_x + resolved->frame->pieces.front().width;
            content_bounds.min_y = resolved->frame->pieces.front().output_y;
            content_bounds.max_y =
                resolved->frame->pieces.front().output_y + resolved->frame->pieces.front().height;
            for (std::size_t i = 1; i < resolved->frame->pieces.size(); ++i) {
                const auto& piece = resolved->frame->pieces[i];
                content_bounds.min_x = std::min(content_bounds.min_x, piece.output_x);
                content_bounds.min_y = std::min(content_bounds.min_y, piece.output_y);
                content_bounds.max_x = std::max(content_bounds.max_x, piece.output_x + piece.width);
                content_bounds.max_y =
                    std::max(content_bounds.max_y, piece.output_y + piece.height);
            }
        }
        return {
            .canvas_width = resolved->frame->output_width,
            .canvas_height = resolved->frame->output_height,
            .has_visible_pieces = !resolved->frame->pieces.empty(),
            .content_bounds = content_bounds,
        };
    }

private:
    const AssetRuntime* assets_;
};

} // namespace d2engine
