#pragma once

#include "text_layout_types.hpp"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace d2engine {

struct FontDescriptor {
    std::string           face;
    std::filesystem::path file;
};

class TextBoxRenderer {
public:
    explicit TextBoxRenderer(SDL_Renderer*               renderer,
                             std::vector<FontDescriptor> fonts = default_fonts());
    ~TextBoxRenderer();

    TextBoxRenderer(const TextBoxRenderer&) = delete;
    TextBoxRenderer& operator=(const TextBoxRenderer&) = delete;
    TextBoxRenderer(TextBoxRenderer&&) = delete;
    TextBoxRenderer& operator=(TextBoxRenderer&&) = delete;

    void draw(const TextBox& box);
    void clear_cache();

    [[nodiscard]] static std::vector<FontDescriptor> default_fonts();

private:
    struct TextureDeleter {
        void operator()(SDL_Texture* texture) const;
    };
    struct FontDeleter {
        void operator()(TTF_Font* font) const;
    };
    struct CachedTexture {
        std::unique_ptr<SDL_Texture, TextureDeleter> texture;
        float                                        w = 0.0f;
        float                                        h = 0.0f;
        float                                        font_size = 0.0f;
    };

    [[nodiscard]] CachedTexture& texture_for(const TextBox& box);
    [[nodiscard]] CachedTexture  render_texture(const TextBox& box, float font_size);
    [[nodiscard]] TTF_Font*      font_for(std::string_view face, float font_size);
    [[nodiscard]] std::string    cache_key(const TextBox& box, float font_size) const;

    SDL_Renderer*                                                           renderer_ = nullptr;
    std::unordered_map<std::string, std::filesystem::path>                  fonts_;
    std::unordered_map<std::string, CachedTexture>                          texture_cache_;
    std::unordered_map<std::string, std::unique_ptr<TTF_Font, FontDeleter>> font_cache_;
};

} // namespace d2engine
