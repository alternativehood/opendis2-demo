#pragma once

#include <d2adventure_render/adventure_render_types.hpp>
#include <d2runtime/AdventureResourceNode.hpp>

#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>

namespace d2engine::adventure_render {

struct StaticResourceNodeVisual {
    std::string container_path;
    std::string logical_sprite;

    int canvas_width = 0;
    int canvas_height = 0;
    int canvas_foot_x = 0;
    int canvas_foot_y = 0;
};

struct AnimatedResourceNodeVisual {
    std::string container_path;
    std::string logical_animation;

    int canvas_foot_x = 0;
    int canvas_foot_y = 0;

    AdventureAnimationData animation_data;
};

using ResourceNodeVisual = std::variant<StaticResourceNodeVisual, AnimatedResourceNodeVisual>;

struct ResourceNodeAssetCatalog {
    std::unordered_map<d2runtime::AdventureResourceKind, ResourceNodeVisual> visuals;

    [[nodiscard]] const ResourceNodeVisual& resolve(d2runtime::AdventureResourceKind kind) const {
        auto it = visuals.find(kind);
        if (it == visuals.end()) {
            throw std::runtime_error("ResourceNodeAssetCatalog: no visual recipe for kind=" +
                                     std::to_string(static_cast<int>(kind)));
        }
        return it->second;
    }
};

} // namespace d2engine::adventure_render
