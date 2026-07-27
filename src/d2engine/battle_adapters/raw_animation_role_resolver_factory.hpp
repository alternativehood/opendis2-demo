#pragma once

#include "../battle_view/animation_role_resolver.hpp"

#include <string>

namespace d2engine {

class FfAssetStore;

[[nodiscard]] AnimationRoleResolver
make_animation_role_resolver_from_raw_ff(const FfAssetStore& store, const std::string& container);

} // namespace d2engine
