#include "text_box_renderer.hpp"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

namespace d2engine {
namespace {

#ifndef D2ENGINE_FONT_DIR
#define D2ENGINE_FONT_DIR "."
#endif

constexpr float kMinFontSize = 8.0f;
constexpr float kMaxFontSize = 128.0f;

[[nodiscard]] int rounded_font_size(float size) {
    return std::clamp(static_cast<int>(std::lround(size)), static_cast<int>(kMinFontSize),
                      static_cast<int>(kMaxFontSize));
}

[[nodiscard]] SDL_Color sdl_color(Color color) {
    return SDL_Color{.r = color.r, .g = color.g, .b = color.b, .a = color.a};
}

[[nodiscard]] float aligned_x(Rect rect, float w, TextAlign align) {
    switch (align) {
    case TextAlign::Left:
        return rect.x;
    case TextAlign::Center:
        return rect.x + ((rect.w - w) * 0.5f);
    case TextAlign::Right:
        return rect.x + rect.w - w;
    }
    return rect.x;
}

[[nodiscard]] float aligned_y(Rect rect, float h, TextVAlign align) {
    switch (align) {
    case TextVAlign::Top:
        return rect.y;
    case TextVAlign::Middle:
        return rect.y + ((rect.h - h) * 0.5f);
    case TextVAlign::Bottom:
        return rect.y + rect.h - h;
    }
    return rect.y;
}

[[nodiscard]] bool needs_clip(const Rect& text, const Rect& clip) {
    return text.x < clip.x || text.y < clip.y || text.right() > clip.right() ||
           text.bottom() > clip.bottom();
}

} // namespace

void TextBoxRenderer::TextureDeleter::operator()(SDL_Texture* texture) const {
    if (texture != nullptr) {
        SDL_DestroyTexture(texture);
    }
}

void TextBoxRenderer::FontDeleter::operator()(TTF_Font* font) const {
    if (font != nullptr) {
        TTF_CloseFont(font);
    }
}

TextBoxRenderer::TextBoxRenderer(SDL_Renderer* renderer, std::vector<FontDescriptor> fonts)
    : renderer_(renderer) {
    if (renderer_ == nullptr) {
        throw std::runtime_error("TextBoxRenderer requires SDL_Renderer");
    }
    for (auto& font : fonts) {
        fonts_.emplace(std::move(font.face), std::move(font.file));
    }
}

TextBoxRenderer::~TextBoxRenderer() {
    clear_cache();
}

std::vector<FontDescriptor> TextBoxRenderer::default_fonts() {
    return {{.face = "Charis SIL",
             .file = std::filesystem::path{D2ENGINE_FONT_DIR} / "CharisSIL-Regular.ttf"}};
}

void TextBoxRenderer::clear_cache() {
    texture_cache_.clear();
    font_cache_.clear();
}

void TextBoxRenderer::draw(const TextBox& box) {
    if (box.text.empty() || box.rect.w <= 0.0f || box.rect.h <= 0.0f) {
        return;
    }
    CachedTexture& rendered = texture_for(box);
    Rect           dst{.x = aligned_x(box.rect, rendered.w, box.style.align),
                       .y = aligned_y(box.rect, rendered.h, box.style.valign),
                       .w = rendered.w,
                       .h = rendered.h};
    SDL_FRect      sdl_dst{.x = dst.x, .y = dst.y, .w = dst.w, .h = dst.h};
    SDL_FRect      src{.x = 0.0f, .y = 0.0f, .w = rendered.w, .h = rendered.h};

    // TODO(opendis2-text-ellipsis): implement real ellipsis through SDL3_ttf text API
    // or a small measured truncation helper. Until then Ellipsis intentionally clips.
    // Do not add a custom paragraph/layout engine here.
    const bool clip = box.style.overflow == TextOverflowMode::Clip ||
                      box.style.overflow == TextOverflowMode::Ellipsis ||
                      box.style.overflow == TextOverflowMode::ShrinkToFit;
    if (clip && needs_clip(dst, box.rect)) {
        if (dst.x < box.rect.x) {
            const float delta = box.rect.x - dst.x;
            src.x += delta;
            src.w -= delta;
            sdl_dst.x = box.rect.x;
            sdl_dst.w -= delta;
        }
        if (dst.y < box.rect.y) {
            const float delta = box.rect.y - dst.y;
            src.y += delta;
            src.h -= delta;
            sdl_dst.y = box.rect.y;
            sdl_dst.h -= delta;
        }
        src.w = std::min(src.w, box.rect.right() - sdl_dst.x);
        src.h = std::min(src.h, box.rect.bottom() - sdl_dst.y);
        sdl_dst.w = src.w;
        sdl_dst.h = src.h;
    }
    if (src.w > 0.0f && src.h > 0.0f) {
        SDL_RenderTexture(renderer_, rendered.texture.get(), &src, &sdl_dst);
    }
}

TextBoxRenderer::CachedTexture& TextBoxRenderer::texture_for(const TextBox& box) {
    float font_size = std::max(kMinFontSize, box.style.font_size);
    if (box.style.overflow == TextOverflowMode::ShrinkToFit) {
        for (int size = rounded_font_size(font_size); size >= static_cast<int>(kMinFontSize);
             --size) {
            CachedTexture candidate = render_texture(box, static_cast<float>(size));
            if (candidate.w <= box.rect.w && candidate.h <= box.rect.h) {
                const std::string key = cache_key(box, static_cast<float>(size));
                auto [it, inserted] = texture_cache_.emplace(key, std::move(candidate));
                (void)inserted;
                return it->second;
            }
        }
        font_size = kMinFontSize;
    }

    const std::string key = cache_key(box, font_size);
    if (auto it = texture_cache_.find(key); it != texture_cache_.end()) {
        return it->second;
    }
    auto [it, inserted] = texture_cache_.emplace(key, render_texture(box, font_size));
    (void)inserted;
    return it->second;
}

TextBoxRenderer::CachedTexture TextBoxRenderer::render_texture(const TextBox& box,
                                                               float          font_size) {
    TTF_Font*    font = font_for(box.style.font_face, font_size);
    SDL_Surface* surface = nullptr;
    const auto   color = sdl_color(box.style.color);
    if (box.style.wrap == TextWrapMode::Word) {
        surface = TTF_RenderText_Blended_Wrapped(font, box.text.c_str(), 0, color,
                                                 static_cast<int>(std::max(1.0f, box.rect.w)));
    } else {
        surface = TTF_RenderText_Blended(font, box.text.c_str(), 0, color);
    }
    if (surface == nullptr) {
        throw std::runtime_error(std::string("TTF_RenderText_Blended failed: ") + SDL_GetError());
    }
    std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> surface_guard(surface,
                                                                              SDL_DestroySurface);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    if (texture == nullptr) {
        throw std::runtime_error(std::string("SDL_CreateTextureFromSurface failed: ") +
                                 SDL_GetError());
    }
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    return {.texture = std::unique_ptr<SDL_Texture, TextureDeleter>(texture),
            .w = static_cast<float>(surface->w),
            .h = static_cast<float>(surface->h),
            .font_size = font_size};
}

TTF_Font* TextBoxRenderer::font_for(std::string_view face, float font_size) {
    const std::string face_key(face);
    const auto        font_it = fonts_.find(face_key);
    if (font_it == fonts_.end()) {
        throw std::runtime_error("Unsupported font face: " + face_key);
    }
    const int         size = rounded_font_size(font_size);
    const std::string key = face_key + "|" + std::to_string(size);
    if (auto it = font_cache_.find(key); it != font_cache_.end()) {
        return it->second.get();
    }
    TTF_Font* font = TTF_OpenFont(font_it->second.string().c_str(), static_cast<float>(size));
    if (font == nullptr) {
        throw std::runtime_error("TTF_OpenFont failed for " + font_it->second.string() + ": " +
                                 SDL_GetError());
    }
    auto [it, inserted] = font_cache_.emplace(key, std::unique_ptr<TTF_Font, FontDeleter>(font));
    (void)inserted;
    return it->second.get();
}

std::string TextBoxRenderer::cache_key(const TextBox& box, float font_size) const {
    const int size = rounded_font_size(font_size);
    return box.style.font_face + "|" + std::to_string(size) + "|" + box.text + "|" +
           std::to_string(box.style.color.r) + "," + std::to_string(box.style.color.g) + "," +
           std::to_string(box.style.color.b) + "," + std::to_string(box.style.color.a) + "|" +
           std::to_string(static_cast<int>(box.style.wrap)) + "|" +
           std::to_string(static_cast<int>(box.style.overflow)) + "|" +
           std::to_string(static_cast<int>(box.rect.w)) + "x" +
           std::to_string(static_cast<int>(box.rect.h));
}

} // namespace d2engine
