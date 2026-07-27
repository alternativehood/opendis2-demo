#pragma once

#include "contained_stack_shield_asset_catalog.hpp"

#include <d2adventure_render/map_geometry.hpp>
#include <d2adventure_render/preparation_context.hpp>

#include <optional>
#include <string>

namespace d2engine::adventure_render {

struct ContainedStackPresentation {
    PreparedAdventureRenderPrimitive shield;
    PickEntry                        pick_entry;
    std::string                      shield_outer_logical_name;
    std::string                      shield_sprite_name;
};

class ContainedStackPresentationBuilder {
public:
    ContainedStackPresentationBuilder(const ContainedStackShieldAssetCatalog& shield_catalog,
                                      const AdventureMapGeometry&             geometry);

    [[nodiscard]] ContainedStackPresentation
    build(const d2runtime::AdventureWorldState& world, const d2runtime::AdventureStack& stack,
          const d2runtime::AdventureContainedStackLocation& location,
          const d2runtime::AdventureSubraceRef&             subrace) const;

private:
    const ContainedStackShieldAssetCatalog& shield_catalog_;
    const AdventureMapGeometry&             geometry_;
};

} // namespace d2engine::adventure_render
