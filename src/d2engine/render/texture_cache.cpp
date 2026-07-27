#include "texture_cache.hpp"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <filesystem>
#include <sstream>

namespace d2engine {

TextureCache::TextureCache(SDL_Renderer* renderer, const RuntimeAssetContext& asset_context)
    : renderer_(renderer) {
    const auto& sheets = asset_context.database().atlas_sheets();
    if (sheets.empty()) {
        throw std::runtime_error("TextureCache: no atlas sheets available");
    }

    const auto& asset_root = asset_context.database().asset_root();

    for (const auto& sheet : sheets) {
        const auto        full_path = asset_root / sheet.path;
        const std::string sheet_id = sheet.atlas_asset_id + "/sheet" + std::to_string(sheet.index);
        load_atlas(sheet_id, full_path);
    }
}

SDL_Texture* TextureCache::get(const std::string& atlas_asset_id) const {
    const auto it = cache_.find(atlas_asset_id);
    if (it == cache_.end()) {
        return nullptr;
    }
    return it->second.get();
}

std::vector<std::string> TextureCache::all_atlas_ids() const {
    std::vector<std::string> result;
    result.reserve(cache_.size());
    for (const auto& pair : cache_) {
        result.push_back(pair.first);
    }
    return result;
}

void TextureCache::load_atlas(const std::string&           atlas_asset_id,
                              const std::filesystem::path& sheet_path) {
    SDL_Surface* surface = IMG_Load(sheet_path.string().c_str());
    if (surface == nullptr) {
        std::ostringstream msg;
        msg << "Failed to load atlas '" << atlas_asset_id << "' from '" << sheet_path.string()
            << "': " << SDL_GetError();
        throw std::runtime_error(msg.str());
    }
    std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> surface_guard(surface,
                                                                              SDL_DestroySurface);

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    if (texture == nullptr) {
        std::ostringstream msg;
        msg << "Failed to create SDL texture for atlas '" << atlas_asset_id
            << "': " << SDL_GetError();
        throw std::runtime_error(msg.str());
    }

    cache_[atlas_asset_id] = SdlTexture(texture);
}

} // namespace d2engine
