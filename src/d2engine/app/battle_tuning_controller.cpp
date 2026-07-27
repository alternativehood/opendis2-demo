#include "battle_tuning_controller.hpp"
#include "battle_tuning_state.hpp"

#include <d2log/log.hpp>

#include <algorithm>

namespace d2engine {

namespace {
auto kLog = d2log::get("d2.tuning"); // NOLINT(cert-err58-cpp)
} // namespace

void BattleTuningController::toggle_enabled() {
    enabled_ = !enabled_;
    kLog->info("debug_tuning state={}", enabled_ ? "on" : "off");
}

void BattleTuningController::save() {
    if (config_path_.empty()) {
        kLog->warn("debug_tuning_save no_config_path");
        return;
    }
    auto document = config_store_.load_document(config_path_);
    write_battle_tuning_config(document, state_);
    config_store_.save_document_atomic(config_path_, document);
    state_.mark_saved();
    kLog->info("debug_tuning_saved path={}", config_path_.string());
}

void BattleTuningController::revert_selected() {
    const DebugRenderableItem* item = state_.selected_item();
    if (item == nullptr || !item->binding.has_value()) {
        kLog->debug("debug_tuning_revert no_writable_selection");
        return;
    }
    const auto binding = to_config_binding(*item->binding);
    if (binding.has_value() && state_.revert(*binding)) {
        kLog->debug("debug_tuning_reverted key={}", binding->key());
    }
}

void BattleTuningController::update_placement(const ConfigBinding&        binding,
                                              const VisualPlacementValue& updated) {
    if (!state_.set_placement(binding, updated)) {
        kLog->debug("debug_tuning update_placement read_only key={}", binding.key());
        return;
    }
    const auto actual = state_.placement(binding);
    kLog->debug("debug_tuning id={} x={:.2f} y={:.2f} sx={:.3f} sy={:.3f} level={} delay={}",
                binding.display_path, static_cast<double>(actual.x), static_cast<double>(actual.y),
                static_cast<double>(actual.scale_x), static_cast<double>(actual.scale_y),
                actual.level, actual.frame_delay);
}

void BattleTuningController::revert_all() {
    std::vector<ConfigBinding> bindings;
    bindings.reserve(state_.dirty.size());
    for (const auto& [_, entry] : state_.dirty) {
        bindings.push_back(entry.binding);
    }
    for (const auto& binding : bindings) {
        static_cast<void>(state_.revert(binding));
    }
    kLog->info("debug_tuning_reverted_all");
}

bool BattleTuningController::apply_edit(const DebugTuningEditAction& action) {
    if (!enabled_) {
        return false;
    }

    const DebugRenderableItem* item = state_.selected_item();
    const bool                 has_selection = (item != nullptr && item->binding.has_value());

    if (!has_selection) {
        kLog->debug("debug_tuning selected_item_is_read_only");
        return true;
    }

    const auto binding = to_config_binding(*item->binding);
    if (!binding.has_value()) {
        kLog->debug("debug_tuning selected_item_is_read_only");
        return true;
    }

    const float delta = action.step;

    if (binding->owner_kind == BindingOwnerKind::TreeLayout) {
        kLog->debug("debug_tuning tree_layout edit not routed through controller");
        return true;
    }

    // VisualPlacement editing
    VisualPlacementValue value = state_.placement(*binding);

    switch (action.kind) {
    case DebugTuningEditKind::MoveUp:
        value.y -= delta;
        break;
    case DebugTuningEditKind::MoveDown:
        value.y += delta;
        break;
    case DebugTuningEditKind::MoveLeft:
        value.x -= delta;
        break;
    case DebugTuningEditKind::MoveRight:
        value.x += delta;
        break;
    case DebugTuningEditKind::WidthDecrease:
        value.scale_x -= 0.005f * delta;
        break;
    case DebugTuningEditKind::WidthIncrease:
        value.scale_x += 0.005f * delta;
        break;
    case DebugTuningEditKind::HeightIncrease:
        value.scale_y += 0.005f * delta;
        break;
    case DebugTuningEditKind::HeightDecrease:
        value.scale_y -= 0.005f * delta;
        break;
    case DebugTuningEditKind::ScaleBothDecrease:
        value.scale_x -= 0.005f * delta;
        value.scale_y -= 0.005f * delta;
        break;
    case DebugTuningEditKind::ScaleBothIncrease:
        value.scale_x += 0.005f * delta;
        value.scale_y += 0.005f * delta;
        break;
    case DebugTuningEditKind::LevelIncrease:
        ++value.level;
        break;
    case DebugTuningEditKind::LevelDecrease:
        --value.level;
        break;
    case DebugTuningEditKind::ParameterIncrease:
        ++value.frame_delay;
        break;
    case DebugTuningEditKind::ParameterDecrease:
        --value.frame_delay;
        break;
    }

    if (!state_.set_placement(*binding, value)) {
        kLog->debug("debug_tuning selected_item_is_read_only");
        return true;
    }

    const VisualPlacementValue actual = state_.placement(*binding);
    kLog->debug(
        "debug_tuning id={} path={} x={:.2f} y={:.2f} sx={:.3f} sy={:.3f} level={} delay={}",
        item->stable_id, binding->display_path, static_cast<double>(actual.x),
        static_cast<double>(actual.y), static_cast<double>(actual.scale_x),
        static_cast<double>(actual.scale_y), actual.level, actual.frame_delay);
    return true;
}

} // namespace d2engine
