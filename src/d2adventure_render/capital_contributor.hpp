#pragma once

#include <functional>
#include <string>
#include <string_view>

#include <d2runtime/AdventureWorldState.hpp>

#include <d2adventure_render/terrain/capital_asset_catalog.hpp>

namespace d2engine::adventure_render {

class PreparationContext;

struct ResolvedCapitalVisual {
    CapitalVisualState           state = CapitalVisualState::Active;
    std::string                  guardian_type_id;
    std::string                  guardian_instance_id;
    const AnimatedCapitalVisual* visual = nullptr;
};

using CapitalVisualResolveFn = std::function<ResolvedCapitalVisual(
    const d2runtime::AdventureWorldState&, const d2runtime::AdventureCapital&,
    std::string_view race_id)>;

[[nodiscard]] std::function<void(const d2runtime::AdventureWorldState&, PreparationContext&)>
make_capital_contributor(const CapitalAssetCatalog& catalog, CapitalVisualResolveFn resolve_fn);

} // namespace d2engine::adventure_render
