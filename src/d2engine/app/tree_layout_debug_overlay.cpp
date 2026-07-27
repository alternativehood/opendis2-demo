#include "tree_layout_debug_overlay.hpp"
#include "tree_layout_editor.hpp"

#include "../render/renderer2d.hpp"

namespace d2engine {

void render_tree_layout_selection_outline(Renderer2D& renderer, TreeLayoutEditor& tree_editor) {
    auto rect = tree_editor.selected_composed_rect();
    if (!rect.has_value())
        return;
    if (rect->w <= 0.0f || rect->h <= 0.0f)
        return;

    constexpr Color outline_color{.r = 255, .g = 220, .b = 0, .a = 255};

    renderer.draw_rect(*rect, outline_color, false);

    if (rect->w > 2.0f && rect->h > 2.0f) {
        renderer.draw_rect(
            Rect{
                .x = rect->x + 1.0f, .y = rect->y + 1.0f, .w = rect->w - 2.0f, .h = rect->h - 2.0f},
            outline_color, false);
    }
}

} // namespace d2engine