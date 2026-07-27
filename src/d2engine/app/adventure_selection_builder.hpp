#pragma once

#include "adventure_visual_resources.hpp"

#include <d2adventure_render/adventure_render_types.hpp>
#include <d2adventure_render/map_geometry.hpp>

#include <d2runtime/AdventureWorldState.hpp>

#include <optional>

namespace d2engine {

struct AdventureStackRef {
    adventure_render::StableRenderId stable_id = 0;
    std::string                      object_id;
    adventure_render::MapCell        cell;
};

[[nodiscard]] std::optional<adventure_render::MapCell>
resolve_stack_selection_cell(const d2runtime::AdventureWorldState& world,
                             const d2runtime::AdventureStack&      stack);

[[nodiscard]] adventure_render::PreparedAdventureRenderPrimitive
build_selection_primitive(const AdventureStackRef& stack, const adventure_render::MapCell& cell,
                          const SelectVisual& visual, adventure_render::StableRenderId stable_id,
                          const adventure_render::AdventureMapGeometry& geo);

} // namespace d2engine
