#pragma once

#include "../battle_view/battle_texture_provider.hpp"

#include <functional>

namespace d2engine {

class GameTextureCache;

class SdlBattleTextureProvider final : public IBattleTextureProvider {
public:
    using TextureLookup = std::function<void*(const std::string&, const std::string&)>;

    explicit SdlBattleTextureProvider(GameTextureCache& cache);
    explicit SdlBattleTextureProvider(TextureLookup lookup) : lookup_(std::move(lookup)) {}

    [[nodiscard]] BackendTextureRef       get_texture(const std::string& container_path,
                                                      const std::string& image_name) override;
    [[nodiscard]] std::pair<float, float> texture_size(BackendTextureRef texture) const override;

private:
    TextureLookup lookup_;
};

} // namespace d2engine
