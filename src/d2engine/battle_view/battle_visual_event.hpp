#pragma once

#include "battle_ids.hpp"

#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace d2engine {

enum class BattleEffectRole : std::uint8_t {
    Heff,
    Tuch,
};

struct ActorSelected {
    UnitInstanceId previous;
    UnitInstanceId selected;
};

struct TargetSelected {
    UnitInstanceId previous;
    UnitInstanceId selected;
};

enum class TargetSetKind : std::uint8_t {
    Single,
    UnitList,
};

struct TargetSet {
    TargetSetKind               kind = TargetSetKind::Single;
    std::vector<UnitInstanceId> units;

    [[nodiscard]] static TargetSet single(UnitInstanceId unit) {
        return TargetSet{.kind = TargetSetKind::Single, .units = {unit}};
    }

    [[nodiscard]] static TargetSet unit_list(std::vector<UnitInstanceId> units) {
        return TargetSet{.kind = TargetSetKind::UnitList, .units = std::move(units)};
    }

    [[nodiscard]] std::optional<UnitInstanceId> single_target() const {
        if (kind == TargetSetKind::Single && units.size() == 1) {
            return units.front();
        }
        return std::nullopt;
    }
};

struct AttackStarted {
    AttackInstanceId attack_id;
    UnitInstanceId   source;
    TargetSet        targets;
};

struct AttackImpactCue {
    AttackInstanceId attack_id;
    UnitInstanceId   source;
    TargetSet        targets;
};

struct TargetDamaged {
    AttackInstanceId   attack_id;
    UnitInstanceId     source; // unit that performed the attack
    UnitInstanceId     target;
    std::optional<int> current_hp;
};

struct TargetMissed {
    AttackInstanceId attack_id;
    UnitInstanceId   target;
};

struct TargetResisted {
    AttackInstanceId attack_id;
    UnitInstanceId   target;
};

struct TargetKilled {
    AttackInstanceId attack_id;
    UnitInstanceId   target;
};

struct LifeDrained {
    AttackInstanceId   attack_id;
    UnitInstanceId     source;
    UnitInstanceId     target;
    std::optional<int> target_current_hp;
    std::optional<int> source_current_hp;
};

struct SourceHealed {
    AttackInstanceId   attack_id;
    UnitInstanceId     source;
    std::optional<int> current_hp;
};

struct UnitHitReceived {
    UnitInstanceId target;
};

struct UnitKilled {
    UnitInstanceId target;
};

struct UnitReviveStarted {
    UnitInstanceId target;
};

struct UnitRevived {
    UnitInstanceId     target;
    std::optional<int> current_hp;
};

struct CastEffectStarted {
    UnitInstanceId   caster;
    BattleEffectRole role = BattleEffectRole::Heff;
};

// Where/how an effect is placed — explicit visual intent, not animation family.
enum class BattleEffectVisualRole : std::uint8_t {
    SourceCastFx,          // on source unit; default family: TUCH
    TargetDamageFx,        // per damaged target; default family: HEFF
    TeamOverlayFx,         // one team-level overlay; explicit only
    FieldOverlayFx,        // full-battlefield effect; explicit only
    SourceAttackOverlayFx, // additional overlay on source unit from attack_fx_override
    TeamAttackOverlayFx,   // single shared AoE attack overlay from attack_fx_override
};

struct BattleEffectStarted {
    UnitInstanceId         source;
    BattleEffectRole       role = BattleEffectRole::Heff;
    BattleEffectVisualRole visual_role = BattleEffectVisualRole::TargetDamageFx;
    UnitInstanceId         target;  // for TargetDamageFx: the damaged unit
    TargetSet              targets; // for TeamOverlayFx
};

using BattleVisualEvent =
    std::variant<ActorSelected, TargetSelected, AttackStarted, AttackImpactCue, TargetDamaged,
                 TargetMissed, TargetResisted, TargetKilled, LifeDrained, SourceHealed,
                 UnitHitReceived, UnitKilled, UnitReviveStarted, UnitRevived, CastEffectStarted,
                 BattleEffectStarted>;

template <typename> inline constexpr bool unsupported_battle_visual_event = false;

[[nodiscard]] inline std::string_view battle_visual_event_name(const BattleVisualEvent& event) {
    return std::visit(
        [](const auto& value) -> std::string_view {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, ActorSelected>) {
                return "ActorSelected";
            } else if constexpr (std::is_same_v<T, TargetSelected>) {
                return "TargetSelected";
            } else if constexpr (std::is_same_v<T, AttackStarted>) {
                return "AttackStarted";
            } else if constexpr (std::is_same_v<T, AttackImpactCue>) {
                return "AttackImpactCue";
            } else if constexpr (std::is_same_v<T, TargetDamaged>) {
                return "TargetDamaged";
            } else if constexpr (std::is_same_v<T, TargetMissed>) {
                return "TargetMissed";
            } else if constexpr (std::is_same_v<T, TargetResisted>) {
                return "TargetResisted";
            } else if constexpr (std::is_same_v<T, TargetKilled>) {
                return "TargetKilled";
            } else if constexpr (std::is_same_v<T, LifeDrained>) {
                return "LifeDrained";
            } else if constexpr (std::is_same_v<T, SourceHealed>) {
                return "SourceHealed";
            } else if constexpr (std::is_same_v<T, UnitHitReceived>) {
                return "UnitHitReceived";
            } else if constexpr (std::is_same_v<T, UnitKilled>) {
                return "UnitKilled";
            } else if constexpr (std::is_same_v<T, UnitReviveStarted>) {
                return "UnitReviveStarted";
            } else if constexpr (std::is_same_v<T, UnitRevived>) {
                return "UnitRevived";
            } else if constexpr (std::is_same_v<T, CastEffectStarted>) {
                return "CastEffectStarted";
            } else if constexpr (std::is_same_v<T, BattleEffectStarted>) {
                return "BattleEffectStarted";
            } else {
                static_assert(unsupported_battle_visual_event<T>);
            }
        },
        event);
}

} // namespace d2engine
