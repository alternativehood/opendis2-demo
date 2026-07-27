#include "contained_stack_shield_asset_catalog_builder.hpp"

#include "ff_asset_store.hpp"

namespace d2engine {

::d2engine::adventure_render::ContainedStackShieldAssetCatalog
build_contained_stack_shield_asset_catalog(const FfAssetStore& store) {
    auto animation_metadata = [&store](std::string_view container, std::string_view animation) {
        return store.animation_metadata(container, animation);
    };
    auto sprite_metadata = [&store](std::string_view container, std::string_view sprite) {
        return store.sprite_metadata(container, sprite);
    };
    return detail::build_contained_stack_shield_asset_catalog_from_metadata(animation_metadata,
                                                                            sprite_metadata);
}

} // namespace d2engine
