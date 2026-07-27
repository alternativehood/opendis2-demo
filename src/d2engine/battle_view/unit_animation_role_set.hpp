#pragma once

#include "battle_effect_clip.hpp"
#include "unit_state_clip.hpp"

#include <optional>

namespace d2engine {

// Per-unit explicit attack FX override. When present, the resolver uses these
// clips instead of the generic TUCH/HEFF fallback logic.
struct UnitAttackFxOverride {
    BattleEffectClip source_attack_overlay; // SourceAttackOverlayFx; e.g. HEFFA/1991 for Nightmare
    BattleEffectClip team_attack_overlay;   // TeamAttackOverlayFx; e.g. HEFFD/1992 for Nightmare
    BattleEffectClip target_damage_fx;      // TargetDamageFx; e.g. TUCHB/2011 for Nightmare
};

// Per-unit animation role set. Includes STILA (unit death pose) as ::death.
// Faction death fx and corpse sprites live in UnitLifecycleVisualProfile.
struct UnitAnimationRoleSet {
    UnitStateClip                       idle;
    UnitStateClip                       hit;
    UnitStateClip                       attack;
    UnitStateClip                       death; // unit death pose (STILA), not faction death effect
    BattleEffectClip                    heff;
    BattleEffectClip                    tuch;
    std::optional<UnitAttackFxOverride> attack_fx_override;
};

} // namespace d2engine
