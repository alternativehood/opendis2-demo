#include "raw_animation_role_resolver_factory.hpp"

#include "../assets/ff_asset_store.hpp"

namespace d2engine {

AnimationRoleResolver make_animation_role_resolver_from_raw_ff(const FfAssetStore& store,
                                                               const std::string&  container) {
    const auto names = store.animations_in(container);
    return AnimationRoleResolver{{names.begin(), names.end()}};
}

} // namespace d2engine
