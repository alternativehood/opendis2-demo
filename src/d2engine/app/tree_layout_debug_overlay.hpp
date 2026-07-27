#pragma once

#include "../render/color.hpp"
#include "../render/rect.hpp"

namespace d2engine {

class Renderer2D;
class TreeLayoutEditor;

void render_tree_layout_selection_outline(Renderer2D& renderer, TreeLayoutEditor& tree_editor);

} // namespace d2engine