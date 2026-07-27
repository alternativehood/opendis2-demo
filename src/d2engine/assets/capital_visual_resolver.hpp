#pragma once

#include <d2adventure_render/capital_contributor.hpp>

#include <string_view>

namespace d2engine {

class GameDataRegistry;

class CapitalVisualResolver {
public:
    CapitalVisualResolver(const d2engine::adventure_render::CapitalAssetCatalog& catalog,
                          const GameDataRegistry&                                game_data);

    [[nodiscard]] d2engine::adventure_render::ResolvedCapitalVisual
    resolve(const d2runtime::AdventureWorldState& world, const d2runtime::AdventureCapital& capital,
            std::string_view race_id) const;

private:
    const d2engine::adventure_render::CapitalAssetCatalog& catalog_;
    const GameDataRegistry&                                game_data_;
};

} // namespace d2engine
