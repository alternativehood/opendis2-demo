#include "renderer2d.hpp"
#include "text/text_box_renderer.hpp"

#include <SDL3/SDL.h>

#include <stdexcept>

namespace d2engine {
namespace {

SDL_FlipMode flip_mode(bool flip_h, bool flip_v) {
    int flags = SDL_FLIP_NONE;
    if (flip_h) {
        flags |= SDL_FLIP_HORIZONTAL;
    }
    if (flip_v) {
        flags |= SDL_FLIP_VERTICAL;
    }
    return static_cast<SDL_FlipMode>(flags);
}

} // namespace

void Renderer2D::clear(Color c) {
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
    SDL_RenderClear(renderer_);
}

void Renderer2D::draw_texture(SDL_Texture* tex, Rect dst, float alpha, bool flip_h, bool flip_v) {
    SDL_SetTextureAlphaModFloat(tex, alpha);
    SDL_FRect const d{.x = dst.x, .y = dst.y, .w = dst.w, .h = dst.h};
    if (flip_h || flip_v) {
        SDL_RenderTextureRotated(renderer_, tex, nullptr, &d, 0.0, nullptr,
                                 flip_mode(flip_h, flip_v));
    } else {
        SDL_RenderTexture(renderer_, tex, nullptr, &d);
    }
    SDL_SetTextureAlphaModFloat(tex, 1.0f);
}

void Renderer2D::draw_texture(SDL_Texture* tex, Rect src, Rect dst, float alpha, bool flip_h,
                              bool flip_v) {
    SDL_SetTextureAlphaModFloat(tex, alpha);
    SDL_FRect const s{.x = src.x, .y = src.y, .w = src.w, .h = src.h};
    SDL_FRect const d{.x = dst.x, .y = dst.y, .w = dst.w, .h = dst.h};
    if (flip_h || flip_v) {
        SDL_RenderTextureRotated(renderer_, tex, &s, &d, 0.0, nullptr, flip_mode(flip_h, flip_v));
    } else {
        SDL_RenderTexture(renderer_, tex, &s, &d);
    }
    SDL_SetTextureAlphaModFloat(tex, 1.0f);
}

void Renderer2D::draw_texture_rotated(SDL_Texture* tex, Rect dst, float alpha, bool flip_h,
                                      bool flip_v, double angle_deg, float center_x,
                                      float center_y) {
    SDL_SetTextureAlphaModFloat(tex, alpha);
    SDL_FRect const  d{.x = dst.x, .y = dst.y, .w = dst.w, .h = dst.h};
    SDL_FPoint const c{.x = center_x, .y = center_y};
    SDL_RenderTextureRotated(renderer_, tex, nullptr, &d, angle_deg, &c, flip_mode(flip_h, flip_v));
    SDL_SetTextureAlphaModFloat(tex, 1.0f);
}

void Renderer2D::draw_rect(Rect rect, Color c, bool filled) {
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
    SDL_FRect const r{.x = rect.x, .y = rect.y, .w = rect.w, .h = rect.h};
    if (filled) {
        SDL_RenderFillRect(renderer_, &r);
    } else {
        SDL_RenderRect(renderer_, &r);
    }
}

void Renderer2D::draw_line(float x1, float y1, float x2, float y2, Color c) {
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
    SDL_RenderLine(renderer_, x1, y1, x2, y2);
}

void Renderer2D::draw_debug_text(float x, float y, const char* text) {
    SDL_RenderDebugText(renderer_, x, y, text);
}

void Renderer2D::draw_debug_text_colored_scaled(float x, float y, const char* text, Color c,
                                                float scale) {
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
    draw_debug_text_scaled(x, y, text, scale);
}

void Renderer2D::draw_debug_text_scaled(float x, float y, const char* text, float scale) {
    if (scale <= 1.0f) {
        draw_debug_text(x, y, text);
        return;
    }
    float old_x = 1.0f;
    float old_y = 1.0f;
    SDL_GetRenderScale(renderer_, &old_x, &old_y);
    SDL_SetRenderScale(renderer_, scale, scale);
    SDL_RenderDebugText(renderer_, x / scale, y / scale, text);
    SDL_SetRenderScale(renderer_, old_x, old_y);
}

void Renderer2D::draw_text_box(const TextBox& box) {
    if (text_box_renderer_ == nullptr) {
        throw std::runtime_error("Renderer2D text box renderer is not initialized");
    }
    text_box_renderer_->draw(box);
}

} // namespace d2engine
