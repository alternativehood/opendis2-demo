#include "tree_layout_editor.hpp"
#include "debug_tuning_types.hpp"
#include "screen_config_store.hpp"
#include "screen.hpp"
#include "screen_manager.hpp"

#include <d2log/log.hpp>

#include <algorithm>
#include <set>

namespace d2engine {

namespace {
auto kLog = d2log::get("d2.tree_editor"); // NOLINT
}

TreeLayoutEditor::TreeLayoutEditor(ScreenManager& screen_manager, ScreenConfigStore& config_store)
    : screen_manager_(screen_manager), config_store_(config_store) {}

void TreeLayoutEditor::refresh_live_sessions() {
    auto                       live = screen_manager_.live_screens();
    std::set<ScreenInstanceId> live_ids;
    for (const auto& ref : live)
        live_ids.insert(ref.id);

    // Erase sessions for destroyed screens
    for (auto it = sessions_.begin(); it != sessions_.end();) {
        if (!live_ids.contains(it->first)) {
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }

    // Clear selected_screen_id_ if the selected screen is no longer live
    if (selected_screen_id_.has_value() && !live_ids.contains(*selected_screen_id_)) {
        selected_screen_id_.reset();
    }

    // Select topmost live screen when none selected
    if (!selected_screen_id_.has_value() && !live.empty())
        selected_screen_id_ = live.back().id;
}

std::vector<LiveScreenRef> TreeLayoutEditor::targets() {
    refresh_live_sessions();
    return screen_manager_.live_screens();
}

std::optional<ScreenInstanceId> TreeLayoutEditor::selected_screen_id() {
    refresh_live_sessions();
    return selected_screen_id_;
}

bool TreeLayoutEditor::select_screen(ScreenInstanceId id) {
    refresh_live_sessions();
    auto* screen = screen_manager_.find_live_screen(id);
    if (screen == nullptr)
        return false;
    selected_screen_id_ = id;
    return true;
}

Screen* TreeLayoutEditor::selected_screen() {
    refresh_live_sessions();
    if (!selected_screen_id_.has_value()) {
        auto screens = screen_manager_.live_screens();
        if (screens.empty())
            return nullptr;
        selected_screen_id_ = screens.back().id;
    }
    auto* s = screen_manager_.find_live_screen(*selected_screen_id_);
    if (s == nullptr) {
        auto screens = screen_manager_.live_screens();
        if (screens.empty()) {
            selected_screen_id_.reset();
            return nullptr;
        }
        selected_screen_id_ = screens.back().id;
        s = screens.back().screen;
    }
    return s;
}

bool TreeLayoutEditor::select_node(std::string path) {
    refresh_live_sessions();
    auto* screen = selected_screen();
    if (screen == nullptr || path.empty())
        return false;
    if (!screen->tree_layout().has_node(path))
        return false;
    auto& session = sessions_[screen->instance_id()];
    session.selected_node_path = std::move(path);
    return true;
}

std::optional<std::string> TreeLayoutEditor::selected_node_path() {
    refresh_live_sessions();
    auto* screen = selected_screen();
    if (screen == nullptr)
        return std::nullopt;
    auto it = sessions_.find(screen->instance_id());
    if (it == sessions_.end())
        return std::nullopt;
    return it->second.selected_node_path;
}

bool TreeLayoutEditor::apply_edit(const DebugTuningEditAction& action) {
    refresh_live_sessions();
    auto* screen = selected_screen();
    if (screen == nullptr)
        return false;

    auto it = sessions_.find(screen->instance_id());
    if (it == sessions_.end())
        return false;
    auto& session = it->second;
    if (!session.selected_node_path.has_value())
        return false;

    const auto& path = *session.selected_node_path;
    auto        opt = screen->tree_layout_.node(path);
    if (!opt.has_value())
        return false;

    TreeNode    n = *opt;
    const float delta = action.step;

    switch (action.kind) {
    case DebugTuningEditKind::MoveUp:
        n.y -= delta;
        break;
    case DebugTuningEditKind::MoveDown:
        n.y += delta;
        break;
    case DebugTuningEditKind::MoveLeft:
        n.x -= delta;
        break;
    case DebugTuningEditKind::MoveRight:
        n.x += delta;
        break;
    case DebugTuningEditKind::WidthDecrease:
        n.w -= delta;
        break;
    case DebugTuningEditKind::WidthIncrease:
        n.w += delta;
        break;
    case DebugTuningEditKind::HeightIncrease:
        n.h += delta;
        break;
    case DebugTuningEditKind::HeightDecrease:
        n.h -= delta;
        break;
    case DebugTuningEditKind::ScaleBothDecrease:
        n.w -= delta;
        n.h -= delta;
        break;
    case DebugTuningEditKind::ScaleBothIncrease:
        n.w += delta;
        n.h += delta;
        break;
    case DebugTuningEditKind::LevelIncrease:
        ++n.level;
        break;
    case DebugTuningEditKind::LevelDecrease:
        --n.level;
        break;
    case DebugTuningEditKind::ParameterIncrease:
        n.alpha += delta * 0.05f;
        break;
    case DebugTuningEditKind::ParameterDecrease:
        n.alpha -= delta * 0.05f;
        break;
    }

    n.level = std::clamp(n.level, kMinDrawLevel, kMaxDrawLevel);
    n.alpha = std::clamp(n.alpha, 0.0f, 1.0f);

    // Record original on first edit
    auto& dirty = session.dirty_nodes;
    if (!dirty.contains(path)) {
        dirty[path] = {.original = *opt, .current = n};
    } else {
        dirty[path].current = n;
    }
    if (dirty[path].current == dirty[path].original) {
        dirty.erase(path);
    }

    screen->tree_layout_.set_node(path, n);
    return true;
}

bool TreeLayoutEditor::update_selected_node(const TreeNode& updated) {
    refresh_live_sessions();
    auto* screen = selected_screen();
    if (screen == nullptr)
        return false;
    auto it = sessions_.find(screen->instance_id());
    if (it == sessions_.end() || !it->second.selected_node_path.has_value())
        return false;

    const auto& path = *it->second.selected_node_path;
    if (!screen->tree_layout().has_node(path))
        return false;

    TreeNode clamped = updated;
    clamped.level = std::clamp(clamped.level, kMinDrawLevel, kMaxDrawLevel);
    clamped.alpha = std::clamp(clamped.alpha, 0.0f, 1.0f);

    auto&      dirty = it->second.dirty_nodes;
    const auto original = *screen->tree_layout().node(path);
    if (!dirty.contains(path)) {
        dirty[path] = {.original = original, .current = clamped};
    } else {
        dirty[path].current = clamped;
    }
    if (dirty[path].current == dirty[path].original) {
        dirty.erase(path);
    }
    screen->tree_layout_.set_node(path, clamped);
    return true;
}

bool TreeLayoutEditor::revert_selected() {
    refresh_live_sessions();
    auto* screen = selected_screen();
    if (screen == nullptr)
        return false;
    auto it = sessions_.find(screen->instance_id());
    if (it == sessions_.end() || !it->second.selected_node_path.has_value())
        return false;

    const auto& path = *it->second.selected_node_path;
    auto        dirty_it = it->second.dirty_nodes.find(path);
    if (dirty_it == it->second.dirty_nodes.end())
        return false;

    screen->tree_layout_.set_node(path, dirty_it->second.original);
    it->second.dirty_nodes.erase(dirty_it);
    return true;
}

void TreeLayoutEditor::revert_all() {
    refresh_live_sessions();
    auto* screen = selected_screen();
    if (screen == nullptr)
        return;
    auto it = sessions_.find(screen->instance_id());
    if (it == sessions_.end())
        return;
    for (auto& [path, entry] : it->second.dirty_nodes) {
        screen->tree_layout_.set_node(path, entry.original);
    }
    it->second.dirty_nodes.clear();
}

bool TreeLayoutEditor::is_dirty() {
    refresh_live_sessions();
    auto* screen = selected_screen();
    if (screen == nullptr)
        return false;
    auto it = sessions_.find(screen->instance_id());
    return it != sessions_.end() && !it->second.dirty_nodes.empty();
}

bool TreeLayoutEditor::is_dirty(const std::string& path) {
    refresh_live_sessions();
    auto* screen = selected_screen();
    if (screen == nullptr)
        return false;
    auto it = sessions_.find(screen->instance_id());
    return it != sessions_.end() && it->second.dirty_nodes.contains(path);
}

std::optional<Rect> TreeLayoutEditor::selected_composed_rect() {
    refresh_live_sessions();
    auto* screen = selected_screen();
    if (screen == nullptr)
        return std::nullopt;
    auto it = sessions_.find(screen->instance_id());
    if (it == sessions_.end() || !it->second.selected_node_path.has_value())
        return std::nullopt;
    const auto& path = *it->second.selected_node_path;
    if (!screen->tree_layout().has_node(path))
        return std::nullopt;
    return screen->tree_layout().compose(path);
}

bool TreeLayoutEditor::save() {
    refresh_live_sessions();
    auto* screen = selected_screen();
    if (screen == nullptr)
        return false;
    config_store_.save_render_tree(std::filesystem::path(screen->config_source()),
                                   screen->tree_layout());
    auto it = sessions_.find(screen->instance_id());
    if (it != sessions_.end())
        it->second.dirty_nodes.clear();
    return true;
}

bool TreeLayoutEditor::reload() {
    refresh_live_sessions();
    auto* screen = selected_screen();
    if (screen == nullptr)
        return false;
    auto loaded = config_store_.load_render_tree(std::filesystem::path(screen->config_source()));
    screen->tree_layout_ = std::move(loaded);
    auto it = sessions_.find(screen->instance_id());
    if (it != sessions_.end())
        it->second.dirty_nodes.clear();
    return true;
}

} // namespace d2engine
