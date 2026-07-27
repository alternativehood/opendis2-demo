#pragma once

#include "../render/render_tree.hpp"

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <cstdint>

namespace d2engine {

class ScreenManager;
class ScreenConfigStore;
class Screen;

using ScreenInstanceId = std::uint64_t;

class TreeLayoutEditor {
public:
    TreeLayoutEditor(ScreenManager& screen_manager, ScreenConfigStore& config_store);

    TreeLayoutEditor(const TreeLayoutEditor&) = delete;
    TreeLayoutEditor& operator=(const TreeLayoutEditor&) = delete;
    TreeLayoutEditor(TreeLayoutEditor&&) = default;

    [[nodiscard]] std::vector<struct LiveScreenRef> targets();
    [[nodiscard]] std::optional<ScreenInstanceId>   selected_screen_id();
    bool                                            select_screen(ScreenInstanceId id);
    Screen*                                         selected_screen();

    bool                                     select_node(std::string path);
    [[nodiscard]] std::optional<std::string> selected_node_path();

    bool apply_edit(const struct DebugTuningEditAction& action);
    bool update_selected_node(const TreeNode& updated);
    bool revert_selected();
    void revert_all();

    [[nodiscard]] bool is_dirty();
    [[nodiscard]] bool is_dirty(const std::string& path);

    bool save();
    bool reload();

    [[nodiscard]] std::optional<Rect> selected_composed_rect();

private:
    void refresh_live_sessions();
    struct DirtyNodeEntry {
        TreeNode original;
        TreeNode current;
    };
    struct ScreenEditSession {
        std::optional<std::string>            selected_node_path;
        std::map<std::string, DirtyNodeEntry> dirty_nodes;
    };

    ScreenManager&                                screen_manager_;
    ScreenConfigStore&                            config_store_;
    std::optional<ScreenInstanceId>               selected_screen_id_;
    std::map<ScreenInstanceId, ScreenEditSession> sessions_;
};

} // namespace d2engine
