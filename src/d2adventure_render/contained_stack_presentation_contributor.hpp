#pragma once

#include "contained_stack_presentation_builder.hpp"
#include "map_preparer.hpp"

namespace d2engine::adventure_render {

[[nodiscard]] RenderContributor make_contained_stack_presentation_contributor(
    const ContainedStackShieldAssetCatalog& shield_catalog);

} // namespace d2engine::adventure_render
