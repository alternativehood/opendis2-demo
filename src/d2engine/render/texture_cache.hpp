#pragma once

#include "sdl_texture.hpp"
#include "../assets/runtime_asset_context.hpp"

#include <SDL3/SDL.h>
#include <string>
#include <unordered_map>

namespace d2engine {

class TextureCache {
public:
    // Load the first available atlas from the asset context.
    // Throws std::runtime_error if no atlases are available or if loading fails.
    TextureCache(SDL_Renderer* renderer, const RuntimeAssetContext& asset_context);
    ~TextureCache() = default;

    // Disable copy/move
    TextureCache(const TextureCache&) = delete;
    TextureCache& operator=(const TextureCache&) = delete;
    TextureCache(TextureCache&&) = delete;
    TextureCache& operator=(TextureCache&&) = delete;

    // Get a texture by atlas asset ID. Returns nullptr if not found.
    [[nodiscard]] SDL_Texture* get(const std::string& atlas_asset_id) const;

    // Get all loaded atlas asset IDs.
    [[nodiscard]] std::vector<std::string> all_atlas_ids() const;

private:
    SDL_Renderer*                               renderer_ = nullptr;
    std::unordered_map<std::string, SdlTexture> cache_;

    void load_atlas(const std::string& atlas_asset_id, const std::filesystem::path& sheet_path);
};

} // namespace d2engine
