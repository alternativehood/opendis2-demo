#pragma once

#include "../color.hpp"
#include "../rect.hpp"

#include <string>

namespace d2engine {

enum class TextAlign {
    Left,
    Center,
    Right,
};

enum class TextVAlign {
    Top,
    Middle,
    Bottom,
};

enum class TextWrapMode {
    None,
    Word,
};

enum class TextOverflowMode {
    None,
    Clip,
    Ellipsis,
    ShrinkToFit,
};

struct TextBoxStyle {
    std::string      font_face = "Charis SIL";
    float            font_size = 12.0f;
    Color            color{.r = 0, .g = 0, .b = 0, .a = 255};
    TextAlign        align = TextAlign::Left;
    TextVAlign       valign = TextVAlign::Top;
    TextWrapMode     wrap = TextWrapMode::None;
    TextOverflowMode overflow = TextOverflowMode::Clip;
};

struct TextBox {
    Rect         rect;
    std::string  text;
    TextBoxStyle style;
};

} // namespace d2engine
