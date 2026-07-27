#include "visual_effect_resolver.hpp"

#include "unit_animation_role_set.hpp"

#include <variant>

namespace d2engine {

VisualEffectResolver::VisualEffectResolver(const BattleScene&               scene,
                                           const UnitVisualProfileRegistry* profiles)
    : scene_(scene), profiles_(profiles) {}

namespace {

const UnitAnimationRoleSet* roles_for(const BattleScene&               scene,
                                      const UnitVisualProfileRegistry* profiles,
                                      UnitInstanceId                   unit_id) {
    if (profiles == nullptr) {
        return nullptr;
    }
    const auto entity_id = scene.visual_entity_for(unit_id);
    if (!entity_id.has_value()) {
        return nullptr;
    }
    const auto* unit = scene.try_unit_by_id(*entity_id);
    if (unit == nullptr) {
        return nullptr;
    }
    return profiles->roles(unit->visual_profile_id);
}

} // namespace

std::vector<BattleVisualEvent> VisualEffectResolver::expand(const BattleVisualEvent& event) const {
    return std::visit(
        [&](const auto& value) -> std::vector<BattleVisualEvent> {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, AttackStarted>) {
                std::vector<BattleVisualEvent> result{event};
                const auto*                    roles = roles_for(scene_, profiles_, value.source);
                if (roles == nullptr) {
                    return result;
                }
                if (roles->attack_fx_override) {
                    // Per-unit explicit override: inject SourceAttackOverlayFx and/or
                    // TeamAttackOverlayFx.
                    const auto& ovr = *roles->attack_fx_override;
                    if (!ovr.source_attack_overlay.is_empty()) {
                        result.emplace_back(BattleEffectStarted{
                            .source = value.source,
                            .visual_role = BattleEffectVisualRole::SourceAttackOverlayFx,
                        });
                    }
                    if (!ovr.team_attack_overlay.is_empty()) {
                        result.emplace_back(BattleEffectStarted{
                            .source = value.source,
                            .visual_role = BattleEffectVisualRole::TeamAttackOverlayFx,
                        });
                    }
                } else if (!roles->tuch.is_empty()) {
                    // Generic fallback: TUCH as SourceCastFx.
                    result.emplace_back(BattleEffectStarted{
                        .source = value.source,
                        .role = BattleEffectRole::Tuch,
                        .visual_role = BattleEffectVisualRole::SourceCastFx,
                    });
                }
                return result;
            } else if constexpr (std::is_same_v<T, TargetDamaged>) {
                std::vector<BattleVisualEvent> result{event};
                const auto*                    roles = roles_for(scene_, profiles_, value.source);
                if (roles == nullptr) {
                    return result;
                }
                if (roles->attack_fx_override) {
                    // Per-unit override: TargetDamageFx from override clip.
                    const auto& ovr = *roles->attack_fx_override;
                    if (!ovr.target_damage_fx.is_empty()) {
                        result.emplace_back(BattleEffectStarted{
                            .source = value.source,
                            .visual_role = BattleEffectVisualRole::TargetDamageFx,
                            .target = value.target,
                        });
                    }
                } else if (!roles->heff.is_empty()) {
                    // Generic fallback: HEFF as TargetDamageFx.
                    result.emplace_back(BattleEffectStarted{
                        .source = value.source,
                        .role = BattleEffectRole::Heff,
                        .visual_role = BattleEffectVisualRole::TargetDamageFx,
                        .target = value.target,
                    });
                }
                return result;
            } else {
                return {event};
            }
        },
        event);
}

} // namespace d2engine
