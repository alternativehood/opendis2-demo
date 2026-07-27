#pragma once

#include <d2adventure_render/adventure_render_types.hpp>
#include <d2runtime/AdventureSite.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>

namespace d2engine::adventure_render {

struct StaticSiteVisual {
    std::string         container_path;
    std::string         logical_sprite;
    int                 canvas_foot_x = 0;
    int                 canvas_foot_y = 0;
    int                 canvas_width = 0;
    int                 canvas_height = 0;
    CanvasContentBounds content_bounds;
};

struct AnimatedSiteLayer {
    std::string            container_path;
    std::string            logical_animation;
    int                    canvas_foot_x = 0;
    int                    canvas_foot_y = 0;
    CanvasContentBounds    content_bounds;
    AdventureAnimationData animation;
};

struct AnimatedSiteVisual {
    AnimatedSiteLayer                body;
    std::optional<AnimatedSiteLayer> shadow;
};

using SiteVisual = std::variant<StaticSiteVisual, AnimatedSiteVisual>;

struct SiteAssetCatalog {
    std::array<std::optional<SiteVisual>, 4> mage_visuals;
    std::array<std::optional<SiteVisual>, 8> merchant_visuals;
    std::array<std::optional<SiteVisual>, 5> mercenary_visuals;
    std::array<std::optional<SiteVisual>, 4> trainer_visuals;

    [[nodiscard]] const SiteVisual* find(d2runtime::AdventureSiteKind kind, int image_iso) const {
        if (image_iso < 0) {
            return nullptr;
        }
        switch (kind) {
        case d2runtime::AdventureSiteKind::Mage:
            if (image_iso >= static_cast<int>(mage_visuals.size()) ||
                !mage_visuals[static_cast<std::size_t>(image_iso)].has_value()) {
                return nullptr;
            }
            return &*mage_visuals[static_cast<std::size_t>(image_iso)];
        case d2runtime::AdventureSiteKind::Merchant:
            if (image_iso >= static_cast<int>(merchant_visuals.size()) ||
                !merchant_visuals[static_cast<std::size_t>(image_iso)].has_value()) {
                return nullptr;
            }
            return &*merchant_visuals[static_cast<std::size_t>(image_iso)];
        case d2runtime::AdventureSiteKind::Mercenary:
            if (image_iso >= static_cast<int>(mercenary_visuals.size()) ||
                !mercenary_visuals[static_cast<std::size_t>(image_iso)].has_value()) {
                return nullptr;
            }
            return &*mercenary_visuals[static_cast<std::size_t>(image_iso)];
        case d2runtime::AdventureSiteKind::Trainer:
            if (image_iso >= static_cast<int>(trainer_visuals.size()) ||
                !trainer_visuals[static_cast<std::size_t>(image_iso)].has_value()) {
                return nullptr;
            }
            return &*trainer_visuals[static_cast<std::size_t>(image_iso)];
        }
        return nullptr;
    }

    [[nodiscard]] const SiteVisual& resolve(d2runtime::AdventureSiteKind kind,
                                            int                          image_iso) const {
        const auto* visual = find(kind, image_iso);
        if (visual == nullptr) {
            throw std::runtime_error("SiteAssetCatalog::resolve unsupported kind/image_iso=" +
                                     std::to_string(static_cast<int>(kind)) + "/" +
                                     std::to_string(image_iso));
        }
        return *visual;
    }
};

} // namespace d2engine::adventure_render
