#pragma once

#include "../battle_view/animation_catalog.hpp"
#include "../battle_view/animation_role_resolver.hpp"

#include <memory>
#include <string>

namespace d2engine {

class AssetRuntime;

// Concrete implementation of IAnimationCatalog that wraps RawResourceLoader
// and AnimationRoleResolver to resolve clips from raw .ff containers.
// This is the WIP adapter used while the engine still loads directly from .ff files.
class RawFfAnimationCatalog : public IAnimationCatalog {
public:
    RawFfAnimationCatalog(AssetRuntime& assets, std::string unit_container,
                          std::string effect_container);
    ~RawFfAnimationCatalog() override = default;

    RawFfAnimationCatalog(const RawFfAnimationCatalog&) = delete;
    RawFfAnimationCatalog& operator=(const RawFfAnimationCatalog&) = delete;
    RawFfAnimationCatalog(RawFfAnimationCatalog&&) = delete;
    RawFfAnimationCatalog& operator=(RawFfAnimationCatalog&&) = delete;

    [[nodiscard]] AnimationSequence unit_clip(std::string_view unit_type, std::string_view role,
                                              char direction) const override;
    [[nodiscard]] AnimationSequence battle_effect(std::string_view role) const override;
    [[nodiscard]] AnimationSequence image_sequence(std::string_view base_name) const override;

    [[nodiscard]] UnitStateClip unit_state_bundle(std::string_view unit_type,
                                                  std::string_view action,
                                                  char             direction) const override;

    [[nodiscard]] BattleEffectClip battle_effect_bundle(std::string_view unit_type,
                                                        std::string_view effect_family,
                                                        char             direction) const override;

private:
    std::string                            unit_container_;
    std::string                            effect_container_;
    AssetRuntime*                          assets_; // non-owning reference
    std::unique_ptr<AnimationRoleResolver> resolver_;
};

} // namespace d2engine
