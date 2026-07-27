#pragma once

#include "../render/render_tuning.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace d2engine {

using VisualPlacementValue = TunablePropertyValue;
using DebugRenderableItem = TunableRenderItem;

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#endif
enum class BindingOwnerKind : std::uint8_t {
    UnitVisualProfile,
    UnitVisualLayerProfile,
    UnitVisualLayerDefaultProfile,
    EffectProfile,
    EffectDefaultProfile,
    SpriteProfile,
    LifecycleProfile,
    SceneLayout,
    TreeLayout,
    PositionLevel,
};
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

enum class BindingRole : std::uint8_t {
    UnitIdle,
    UnitAttack,
    UnitHit,
    UnitBase,
    Source,
    Target,
    TargetTeam,
    Global,
    Corpse,
    DeathFx,
    ReviveSmall,
    ReviveLarge,
    SelectionMarker,
    TargetMarker,
    Background,
    CombatFrame,
    Unit,
};

[[nodiscard]] const char*                     to_string(BindingOwnerKind k) noexcept;
[[nodiscard]] const char*                     to_string(BindingRole r) noexcept;
[[nodiscard]] const char*                     role_json_key(BindingRole r) noexcept;
[[nodiscard]] std::optional<BindingOwnerKind> binding_owner_kind_from_string(std::string_view s);
[[nodiscard]] std::optional<BindingRole>      binding_role_from_string(std::string_view s);
[[nodiscard]] bool operator==(std::string_view lhs, BindingOwnerKind rhs) noexcept;
[[nodiscard]] bool operator==(BindingOwnerKind lhs, std::string_view rhs) noexcept;
[[nodiscard]] bool operator!=(std::string_view lhs, BindingOwnerKind rhs) noexcept;
[[nodiscard]] bool operator!=(BindingOwnerKind lhs, std::string_view rhs) noexcept;
[[nodiscard]] bool operator==(std::string_view lhs, BindingRole rhs) noexcept;
[[nodiscard]] bool operator==(BindingRole lhs, std::string_view rhs) noexcept;
[[nodiscard]] bool operator!=(std::string_view lhs, BindingRole rhs) noexcept;
[[nodiscard]] bool operator!=(BindingRole lhs, std::string_view rhs) noexcept;

struct ConfigBinding {
    std::string      config_file;
    BindingOwnerKind owner_kind = BindingOwnerKind::EffectProfile;
    std::string      tree_path;
    std::string      target_id;
    BindingRole      role = BindingRole::Global;
    std::string      side;
    std::string      display_path;

    [[nodiscard]] std::string key() const {
        const std::string& id = target_id.empty() ? tree_path : target_id;
        std::string        k =
            std::string(to_string(owner_kind)) + ":" + id + ":" + std::string(to_string(role));
        if (!side.empty()) {
            k += ':';
            k += side;
        }
        return k;
    }

    [[nodiscard]] bool writable() const {
        return !config_file.empty() && (!tree_path.empty() || !target_id.empty());
    }

    [[nodiscard]] std::string parent_path() const {
        const auto pos = tree_path.rfind('/');
        return pos != std::string::npos ? tree_path.substr(0, pos) : "";
    }

    [[nodiscard]] operator TuningBinding() const;
};

[[nodiscard]] TuningBinding                to_tuning_binding(const ConfigBinding& binding);
[[nodiscard]] std::optional<ConfigBinding> to_config_binding(const TuningBinding& binding);

class BattleTuningBindingResolver : public ITuningBindingResolver {
public:
    explicit BattleTuningBindingResolver(struct BattleTuningState& state) : state_(state) {}

    [[nodiscard]] bool can_resolve(const TuningBinding& binding) const override;
    [[nodiscard]] std::optional<TunablePropertyValue>
         read_property(const TuningBinding& binding) const override;
    bool write_property(const TuningBinding& binding, const TunablePropertyValue& value) override;

private:
    struct BattleTuningState& state_;
};

} // namespace d2engine
