#include "sdl_battle_texture_provider.hpp"

#include "../render/game_texture_cache.hpp"

#include <SDL3/SDL.h>

namespace d2engine {

SdlBattleTextureProvider::SdlBattleTextureProvider(GameTextureCache& cache)
    : lookup_([&cache](const std::string& container_path, const std::string& image_name) {
          return static_cast<void*>(cache.get(container_path, image_name));
      }) {}

BackendTextureRef SdlBattleTextureProvider::get_texture(const std::string& container_path,
                                                        const std::string& image_name) {
    try {
        return BackendTextureRef{.native = lookup_(container_path, image_name)};
    } catch (...) {
        return {};
    }
}

std::pair<float, float> SdlBattleTextureProvider::texture_size(BackendTextureRef texture) const {
    float width = 0.0f;
    float height = 0.0f;
    if (texture.present()) {
        SDL_GetTextureSize(static_cast<SDL_Texture*>(texture.native), &width, &height);
    }
    return {width, height};
}

} // namespace d2engine
