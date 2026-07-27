#pragma once

#include "color.hpp"
#include "rect.hpp"
#include "text/text_layout_types.hpp"

#include <SDL3/SDL.h>

namespace d2engine {

class TextBoxRenderer;

// Thin non-owning wrapper over SDL_Renderer*. Provides named 2D draw operations
// so callers never touch the SDL render API directly.
class Renderer2D {
public:
    explicit Renderer2D(SDL_Renderer* renderer) : renderer_(renderer) {}
    virtual ~Renderer2D() = default;

    // Disable copy/move — non-owning, but copying the wrapper is confusing
    Renderer2D(const Renderer2D&) = delete;
    Renderer2D& operator=(const Renderer2D&) = delete;
    Renderer2D(Renderer2D&&) = delete;
    Renderer2D& operator=(Renderer2D&&) = delete;

    virtual void clear(Color c);

    // Draw texture at screen-space destination rect. alpha=1.0 is fully opaque.
    virtual void draw_texture(SDL_Texture* tex, Rect dst, float alpha = 1.0f, bool flip_h = false,
                              bool flip_v = false);

    // Draw texture with rotation (degrees, clockwise) around a center point relative to dst.
    virtual void draw_texture_rotated(SDL_Texture* tex, Rect dst, float alpha, bool flip_h,
                                      bool flip_v, double angle_deg, float center_x,
                                      float center_y);

    // Draw texture with an explicit source crop rect.
    virtual void draw_texture(SDL_Texture* tex, Rect src, Rect dst, float alpha = 1.0f,
                              bool flip_h = false, bool flip_v = false);

    // Draw a filled or outlined rectangle with the given color.
    virtual void draw_rect(Rect rect, Color c, bool filled = true);

    // Draw a line segment between two screen-space points.
    virtual void draw_line(float x1, float y1, float x2, float y2, Color c);

    // Draw debug text using SDL3 built-in font (8×8 px per char, white).
    virtual void draw_debug_text(float x, float y, const char* text);
    virtual void draw_debug_text_scaled(float x, float y, const char* text, float scale);
    virtual void draw_debug_text_colored_scaled(float x, float y, const char* text, Color c,
                                                float scale);
    virtual void draw_text_box(const TextBox& box);

    // cppcheck-suppress unusedFunction
    void set_text_box_renderer(TextBoxRenderer* renderer) { text_box_renderer_ = renderer; }

private:
    SDL_Renderer*    renderer_;
    TextBoxRenderer* text_box_renderer_ = nullptr;
};

} // namespace d2engine
