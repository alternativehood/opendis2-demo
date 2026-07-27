#pragma once

#include "../battle_view/debug_renderable_item.hpp"
#include "../battle_view/layout_types.hpp"
#include "../battle_view/unit_attack_visual_intent.hpp"
#include "debug_tuning_types.hpp"

#include <nlohmann/json_fwd.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace d2engine {

inline constexpr const char* kBattleScreenConfigPath = "configs/screens/battle_screen.json";

struct DirtyConfigEntry {
    ConfigBinding        binding;
    VisualPlacementValue original;
    VisualPlacementValue current;
};

[[nodiscard]] inline bool is_typed_binding(const ConfigBinding& binding) {
    // PositionLevel goes through position_levels map, not placements
    switch (binding.owner_kind) {
    case BindingOwnerKind::UnitVisualProfile:
    case BindingOwnerKind::UnitVisualLayerProfile:
    case BindingOwnerKind::UnitVisualLayerDefaultProfile:
    case BindingOwnerKind::EffectProfile:
    case BindingOwnerKind::EffectDefaultProfile:
    case BindingOwnerKind::SpriteProfile:
    case BindingOwnerKind::LifecycleProfile:
    case BindingOwnerKind::SceneLayout:
        return true;
    case BindingOwnerKind::TreeLayout:
    case BindingOwnerKind::PositionLevel:
        return false;
    }
    return false;
}

struct BattleTuningState {
    float                            unit_magnitude = 0.879f;
    float                            unit_rotation = 0.0f;
    std::string                      config_file = kBattleScreenConfigPath;
    std::string                      font_face = "Charis SIL";
    LayoutMetrics                    layout_metrics;      // loaded from layout_metrics section
    DebugOverlayStyle                debug_overlay_style; // loaded from debug_overlay_style section
    std::string                      selected_debug_id = "ui:combat_frame";
    std::vector<DebugRenderableItem> current_items;
    // Typed placements keyed by binding.key()
    std::map<std::string, VisualPlacementValue> placements;
    // Canonical binding metadata per placement key — used by save
    std::map<std::string, ConfigBinding> binding_registry;
    // Per-slot depth levels keyed by "A_FRONT_0" etc
    std::map<std::string, int> position_levels;
    // Unsaved changes ONLY — absent = saved. Entries erased on revert and mark_saved.
    std::map<std::string, DirtyConfigEntry> dirty;

    UnitAttackVisualIntentMap attack_visual_intents;

    [[nodiscard]] VisualPlacementValue global_fallback(const ConfigBinding& binding) const {
        // UnitVisualProfile: no global fallback — identity
        if (binding.owner_kind == BindingOwnerKind::UnitVisualProfile) {
            return {};
        }
        // LifecycleProfile: use scene_layout.corpse as global fallback for corpse bindings
        if (binding.owner_kind == BindingOwnerKind::LifecycleProfile) {
            if (binding.role == BindingRole::Corpse) {
                const auto it = placements.find("SceneLayout:scene:Corpse");
                return it != placements.end() ? it->second : VisualPlacementValue{};
            }
            return {};
        }
        // EffectProfile: use scene_layout.global x/y as overlay offset fallback
        if (binding.owner_kind == BindingOwnerKind::EffectProfile) {
            const auto it = placements.find("SceneLayout:scene:Global");
            return it != placements.end()
                       ? VisualPlacementValue{.x = it->second.x, .y = it->second.y}
                       : VisualPlacementValue{};
        }
        return {};
    }

    [[nodiscard]] VisualPlacementValue placement(const ConfigBinding& binding) const {
        if (binding.owner_kind == BindingOwnerKind::PositionLevel) {
            const auto it = position_levels.find(binding.tree_path);
            const int  global_level = [this]() -> int {
                const auto git = placements.find("SceneLayout:scene:Global");
                return git != placements.end() ? git->second.level : 0;
            }();
            return {.level = (it != position_levels.end()) ? it->second : global_level};
        }
        if (is_typed_binding(binding)) {
            const auto it = placements.find(binding.key());
            if (it != placements.end()) {
                return it->second;
            }
            return global_fallback(binding);
        }
        return {};
    }

    [[nodiscard]] VisualPlacementValue placement(const TuningBinding& binding) const {
        const auto config = to_config_binding(binding);
        return config.has_value() ? placement(*config) : VisualPlacementValue{};
    }

    bool set_placement(const ConfigBinding& binding, VisualPlacementValue value) {
        if (!binding.writable()) {
            return false;
        }
        value.level = std::clamp(value.level, kMinDrawLevel, kMaxDrawLevel);

        if (binding.owner_kind == BindingOwnerKind::PositionLevel) {
            const VisualPlacementValue original = placement(binding);
            position_levels[binding.tree_path] = value.level;
            binding_registry[binding.key()] = binding;
            auto& entry = dirty[binding.key()];
            if (entry.binding.tree_path.empty()) {
                entry.binding = binding;
                entry.original = original;
            }
            entry.current = {.level = value.level};
            if (entry.current == entry.original) {
                dirty.erase(binding.key());
            }
            return true;
        }
        if (is_typed_binding(binding)) {
            const VisualPlacementValue original = placement(binding);
            placements[binding.key()] = value;
            binding_registry[binding.key()] = binding;
            auto& entry = dirty[binding.key()];
            if (entry.binding.tree_path.empty()) {
                entry.binding = binding;
                entry.original = original;
            }
            entry.current = value;
            if (entry.current == entry.original) {
                dirty.erase(binding.key());
            }
            return true;
        }
        return false;
    }

    bool revert(const ConfigBinding& binding) {
        if (binding.owner_kind == BindingOwnerKind::TreeLayout) {
            return false; // tree ops handled by TreeLayoutEditor
        }
        const auto it = dirty.find(binding.key());
        if (it == dirty.end())
            return false;
        const VisualPlacementValue original = it->second.original;
        dirty.erase(it);
        if (binding.owner_kind == BindingOwnerKind::PositionLevel) {
            position_levels[binding.tree_path] = original.level;
            return true;
        }
        if (is_typed_binding(binding)) {
            placements[binding.key()] = original;
            return true;
        }
        return false;
    }

    void mark_saved() { dirty.clear(); }

    [[nodiscard]] const DebugRenderableItem* selected_item() const {
        for (const auto& item : current_items) {
            if (item.stable_id == selected_debug_id) {
                return &item;
            }
        }
        return nullptr;
    }

    void set_current_items(std::vector<DebugRenderableItem> items) {
        current_items = std::move(items);
    }
};

void to_json(nlohmann::json& j, const VisualPlacementValue& v);
void from_json(const nlohmann::json& j, VisualPlacementValue& v);
void write_placement_to_json(nlohmann::json& json, const ConfigBinding& b,
                             const VisualPlacementValue& v);
void load_battle_tuning_config(BattleTuningState& state, const nlohmann::json& document,
                               const std::filesystem::path& config_path);
void write_battle_tuning_config(nlohmann::json& document, const BattleTuningState& state);

} // namespace d2engine
