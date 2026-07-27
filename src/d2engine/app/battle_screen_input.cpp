#include "battle_screen_input.hpp"

#include <algorithm>
#include <cmath>

namespace d2engine {

namespace {

float compute_step(KeyModifier mod) noexcept {
    if (has_modifier(mod, KeyModifier::Ctrl) || has_modifier(mod, KeyModifier::Alt))
        return 0.25f;
    if (has_modifier(mod, KeyModifier::Shift))
        return 10.0f;
    return 1.0f;
}

std::optional<DebugTuningEditAction> key_to_edit_action(Key key, KeyModifier mod) {
    const float s = compute_step(mod);
    if (key == Key::Up)
        return DebugTuningEditAction{DebugTuningEditKind::MoveUp, s};
    if (key == Key::Down)
        return DebugTuningEditAction{DebugTuningEditKind::MoveDown, s};
    if (key == Key::Left)
        return DebugTuningEditAction{DebugTuningEditKind::MoveLeft, s};
    if (key == Key::Right)
        return DebugTuningEditAction{DebugTuningEditKind::MoveRight, s};
    if (key == Key::A)
        return DebugTuningEditAction{DebugTuningEditKind::WidthDecrease, s};
    if (key == Key::D)
        return DebugTuningEditAction{DebugTuningEditKind::WidthIncrease, s};
    if (key == Key::W)
        return DebugTuningEditAction{DebugTuningEditKind::HeightIncrease, s};
    if (key == Key::S)
        return DebugTuningEditAction{DebugTuningEditKind::HeightDecrease, s};
    if (key == Key::Q)
        return DebugTuningEditAction{DebugTuningEditKind::ScaleBothDecrease, s};
    if (key == Key::E)
        return DebugTuningEditAction{DebugTuningEditKind::ScaleBothIncrease, s};
    if (key == Key::U)
        return DebugTuningEditAction{DebugTuningEditKind::LevelIncrease, s};
    if (key == Key::J)
        return DebugTuningEditAction{DebugTuningEditKind::LevelDecrease, s};
    if (key == Key::Equals)
        return DebugTuningEditAction{DebugTuningEditKind::ParameterIncrease, s};
    if (key == Key::Minus)
        return DebugTuningEditAction{DebugTuningEditKind::ParameterDecrease, s};
    return std::nullopt;
}

std::optional<BattleScreenAction> map_to_viewer_action(const InputEvent& event) {
    const auto* key = std::get_if<KeyPressed>(&event);
    if (!key)
        return std::nullopt;

    if (key->key == Key::Escape)
        return BattleViewerAction::Quit;
    if (key->key == Key::Space)
        return BattleViewerAction::TriggerAttack;
    if (key->key == Key::Key1)
        return BattleViewerAction::SetRoleIdle;
    if (key->key == Key::Key2)
        return BattleViewerAction::SetRoleHit;
    if (key->key == Key::Key3)
        return BattleViewerAction::SetRoleDeath;
    if (key->key == Key::Key4)
        return BattleViewerAction::SetRoleAttack;
    if (key->key == Key::Key5)
        return BattleViewerAction::SetRoleHeff;
    if (key->key == Key::Key6)
        return BattleViewerAction::SetRoleTuch;
    if (key->key == Key::Left)
        return BattleViewerAction::StepBackward;
    if (key->key == Key::Right)
        return BattleViewerAction::StepForward;
    if (key->key == Key::L)
        return BattleViewerAction::ToggleLayers;
    if (key->key == Key::I)
        return BattleViewerAction::ToggleTransparent;
    if (key->key == Key::B)
        return BattleViewerAction::CycleBackground;
    return std::nullopt;
}

} // namespace

std::optional<BattleScreenAction>
BattleScreenInputHandler::handle(const InputEvent& event, const BattleScreenInputContext& context) {
    // ── A. Tuning lifecycle (only when tuning enabled) ──────────────────
    // Ctrl+D is consumed (Application-level debug UI toggle) and must not
    // fall through to section C (which would interpret D as WidthIncrease).
    if (context.tuning_enabled) {
        if (const auto* key = std::get_if<KeyPressed>(&event)) {
            if (key->key == Key::D && has_modifier(key->modifiers, KeyModifier::Ctrl)) {
                return ConsumeTuningInput{};
            }
            if (key->key == Key::S && has_modifier(key->modifiers, KeyModifier::Ctrl)) {
                return SaveTuning{};
            }
            if (key->key == Key::Z && has_modifier(key->modifiers, KeyModifier::Ctrl)) {
                return RevertAllTuning{};
            }
            if (key->key == Key::Backspace && has_modifier(key->modifiers, KeyModifier::Ctrl)) {
                return RevertAllTuning{};
            }
            if (key->key == Key::Backspace && !has_modifier(key->modifiers, KeyModifier::Ctrl)) {
                return RevertSelectedTuning{};
            }
            if (key->key == Key::L) {
                return LogSelectedTuning{};
            }
            if (key->key == Key::Tab) {
                return ConsumeTuningInput{};
            }
        }
    }

    // ── B. Shift+D: unconditional ToggleTuning ─────────────────────────
    if (const auto* key = std::get_if<KeyPressed>(&event)) {
        if (key->key == Key::D && has_modifier(key->modifiers, KeyModifier::Shift)) {
            return ToggleTuning{};
        }
    }

    // ── C. Tuning edit keys (only when tuning enabled) ───────────────────
    if (context.tuning_enabled) {
        if (const auto* key = std::get_if<KeyPressed>(&event)) {
            auto edit = key_to_edit_action(key->key, key->modifiers);
            if (edit.has_value())
                return ApplyTuningEdit{*edit};
        }
    }

    // ── D. Screen debug actions (only when tuning enabled) ──────────────
    if (context.tuning_enabled) {
        if (const auto* key = std::get_if<KeyPressed>(&event)) {
            if (key->key == Key::O && !has_modifier(key->modifiers, KeyModifier::Shift)) {
                return ToggleDebugHud{};
            }
            if (key->key == Key::O && has_modifier(key->modifiers, KeyModifier::Shift)) {
                return ToggleSoloSelected{};
            }
        }
    }

    // ── Tuning-independent screen actions ───────────────────────────────
    if (const auto* key = std::get_if<KeyPressed>(&event)) {
        if (key->key == Key::P) {
            return ToggleVisualPause{};
        }
        if (key->key == Key::R) {
            return RestartScenario{};
        }
    }

    // ── E. Debug unit movement (Gui+arrows) ─────────────────────────────
    // When tuning is enabled, arrow keys were already consumed by edit keys above.
    if (const auto* key = std::get_if<KeyPressed>(&event)) {
        if (has_modifier(key->modifiers, KeyModifier::Gui)) {
            float dx = 0.0f;
            float dy = 0.0f;
            if (key->key == Key::Up) {
                dy = -1.0f;
            } else if (key->key == Key::Down) {
                dy = 1.0f;
            } else if (key->key == Key::Left) {
                dx = -1.0f;
            } else if (key->key == Key::Right) {
                dx = 1.0f;
            } else {
                return std::nullopt;
            }
            return MoveSelectedDebugUnit{dx, dy};
        }
    }

    // ── F. S key (when tuning disabled) → LogMousePosition ──────────────
    // When tuning is enabled, S was already consumed as tuning edit above.
    if (!context.tuning_enabled) {
        if (const auto* key = std::get_if<KeyPressed>(&event)) {
            if (key->key == Key::S) {
                return LogMousePosition{};
            }
        }
    }

    // ── G. Viewer actions (last priority) ───────────────────────────────
    {
        auto viewer = map_to_viewer_action(event);
        if (viewer.has_value())
            return viewer;
    }

    // ── H. PointerMoved → UpdateDebugPointerPosition ────────────────────
    if (const auto* motion = std::get_if<PointerMoved>(&event)) {
        return UpdateDebugPointerPosition{
            .logical_x = motion->x,
            .logical_y = motion->y,
        };
    }

    // ── I. PointerPressed → hit test against selectable UI items ─────────
    // Only when tuning is enabled — DebugRenderableItem selection is a debug feature.
    if (context.tuning_enabled) {
        if (const auto* press = std::get_if<PointerPressed>(&event)) {
            return handle_pointer_click(press->x, press->y, context.selectable_items);
        }
    }

    return std::nullopt;
}

std::optional<BattleScreenAction>
BattleScreenInputHandler::handle_pointer_click(int logical_x, int logical_y,
                                               const std::span<const DebugRenderableItem>& items) {
    const float fx = static_cast<float>(logical_x);
    const float fy = static_cast<float>(logical_y);
    for (const auto& item : items) {
        if (!item.selectable || !item.visible)
            continue;
        if (fx >= item.bounds.x && fx <= item.bounds.right() && fy >= item.bounds.y &&
            fy <= item.bounds.bottom()) {
            if (item.binding.has_value()) {
                return SelectDebugItem{item.stable_id, item.tree_path};
            }
        }
    }
    return std::nullopt;
}

} // namespace d2engine
