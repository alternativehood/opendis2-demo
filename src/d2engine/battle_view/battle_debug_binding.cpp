#include "battle_debug_binding.hpp"

#include <magic_enum.hpp>

namespace d2engine {

const char* to_string(BindingOwnerKind k) noexcept {
    const auto name = magic_enum::enum_name(k);
    return name.empty() ? "" : name.data();
}

const char* to_string(BindingRole r) noexcept {
    const auto name = magic_enum::enum_name(r);
    return name.empty() ? "" : name.data();
}

const char* role_json_key(BindingRole r) noexcept {
    switch (r) {
    case BindingRole::UnitIdle:
        return "idle";
    case BindingRole::UnitAttack:
        return "attack";
    case BindingRole::UnitHit:
        return "hit";
    case BindingRole::UnitBase:
        return "base";
    case BindingRole::Source:
        return "source";
    case BindingRole::Target:
        return "target";
    case BindingRole::TargetTeam:
        return "target_team";
    case BindingRole::Global:
        return "global";
    case BindingRole::Corpse:
        return "corpse";
    case BindingRole::DeathFx:
        return "death_fx";
    case BindingRole::ReviveSmall:
        return "revive_small";
    case BindingRole::ReviveLarge:
        return "revive_large";
    case BindingRole::SelectionMarker:
        return "selection_marker";
    case BindingRole::TargetMarker:
        return "target_marker";
    case BindingRole::Background:
        return "background";
    case BindingRole::CombatFrame:
        return "combat_frame";
    case BindingRole::Unit:
        return "unit";
    }
    return "";
}

std::optional<BindingOwnerKind> binding_owner_kind_from_string(std::string_view s) {
    return magic_enum::enum_cast<BindingOwnerKind>(s);
}

std::optional<BindingRole> binding_role_from_string(std::string_view s) {
    return magic_enum::enum_cast<BindingRole>(s);
}

bool operator==(std::string_view lhs, BindingOwnerKind rhs) noexcept {
    return lhs == to_string(rhs);
}

bool operator==(BindingOwnerKind lhs, std::string_view rhs) noexcept {
    return rhs == lhs;
}

bool operator!=(std::string_view lhs, BindingOwnerKind rhs) noexcept {
    return !(lhs == rhs);
}

bool operator!=(BindingOwnerKind lhs, std::string_view rhs) noexcept {
    return !(lhs == rhs);
}

bool operator==(std::string_view lhs, BindingRole rhs) noexcept {
    return lhs == to_string(rhs);
}

bool operator==(BindingRole lhs, std::string_view rhs) noexcept {
    return rhs == lhs;
}

bool operator!=(std::string_view lhs, BindingRole rhs) noexcept {
    return !(lhs == rhs);
}

bool operator!=(BindingRole lhs, std::string_view rhs) noexcept {
    return !(lhs == rhs);
}

ConfigBinding::operator TuningBinding() const {
    return to_tuning_binding(*this);
}

TuningBinding to_tuning_binding(const ConfigBinding& binding) {
    return {.domain = "battle",
            .owner_kind = to_string(binding.owner_kind),
            .owner_id = binding.target_id.empty() ? binding.tree_path : binding.target_id,
            .role = to_string(binding.role),
            .property = "placement",
            .config_path = binding.config_file,
            .profile_id = binding.tree_path,
            .tree_path = binding.tree_path,
            .side = binding.side,
            .display_path = binding.display_path};
}

std::optional<ConfigBinding> to_config_binding(const TuningBinding& binding) {
    if (binding.domain != "battle") {
        return std::nullopt;
    }
    auto owner_kind = binding_owner_kind_from_string(binding.owner_kind);
    auto role = binding_role_from_string(binding.role);
    if (!owner_kind.has_value() || !role.has_value()) {
        return std::nullopt;
    }
    return ConfigBinding{.config_file = binding.config_path,
                         .owner_kind = *owner_kind,
                         .tree_path =
                             binding.profile_id.empty() ? binding.owner_id : binding.profile_id,
                         .target_id = binding.profile_id.empty() ? std::string{} : binding.owner_id,
                         .role = *role,
                         .side = binding.side,
                         .display_path = binding.display_path};
}

} // namespace d2engine
