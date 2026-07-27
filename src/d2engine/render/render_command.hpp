#pragma once

#include "color.hpp"
#include "rect.hpp"
#include "texture_provider.hpp"

#include <compare>
#include <cstddef>
#include <optional>
#include <string>

namespace d2engine {

struct RenderLayer {
    int order = 0;

    [[nodiscard]] auto operator<=>(const RenderLayer&) const = default;
};

struct RenderCommand {
    RenderLayer                layer;
    BackendTextureRef          texture;
    Rect                       destination;
    Rect                       source_rect;
    bool                       has_source_rect = false;
    float                      alpha = 1.0f;
    bool                       flip_x = false;
    bool                       flip_y = false;
    bool                       tile = false;
    bool                       allow_rotation = true;
    std::string                tree_path;
    std::optional<std::size_t> tunable_item_index;
    std::optional<Color>       fill_color;
    std::string                text;
    Color                      text_color{.r = 0, .g = 0, .b = 0, .a = 255};
    float                      font_size = 12.0f;
    bool                       center_text = true;
    bool                       game_font_text = false;
    std::string                font_face;
};

} // namespace d2engine
