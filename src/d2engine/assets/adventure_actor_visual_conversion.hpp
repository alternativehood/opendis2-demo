#pragma once

#include "iso_actor_visual_resolver.hpp"

#include <d2adventure_render/adventure_render_types.hpp>

namespace d2engine {

[[nodiscard]] adventure_render::AdventureActorVisual
to_adventure_actor_visual(const IsoActorVisual& source);

} // namespace d2engine
