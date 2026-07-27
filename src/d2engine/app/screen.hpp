#pragma once

#include "../input/input_event.hpp"
#include "../render/render_tree.hpp"
#include "screen_update_context.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace d2engine {

class Renderer2D;
class ScreenManager;
class TreeLayoutEditor;

using ScreenInstanceId = std::uint64_t;

enum class CursorKind : uint8_t {
    Default,
    SelectUnit,
};

struct ScreenStackPolicy {
    bool render_below = false;
    bool update_below = false;
    bool input_below = false;
};

class Screen {
public:
    explicit Screen(TreeLayout tree_layout, std::string config_source)
        : tree_layout_(std::move(tree_layout)), config_source_(std::move(config_source)) {
        if (config_source_.empty()) {
            throw std::logic_error("Screen config_source must not be empty");
        }
    }
    virtual ~Screen() = default;

    Screen(const Screen&) = delete;
    Screen& operator=(const Screen&) = delete;
    Screen(Screen&&) = delete;
    Screen& operator=(Screen&&) = delete;

    virtual std::string_view name() const = 0;

    virtual void on_enter() {}
    virtual void on_exit() {}

    // Called when screen is covered by a pushed overlay (base screen is still active
    // but no longer receives input/update/render directly).
    virtual void on_covered() {}

    // Called when all overlays are popped and this screen becomes visible again.
    // The screen should refresh its pointer-based state from the global pointer
    // state managed by CursorController / Application.
    virtual void on_revealed() {}

    virtual CursorKind                      cursor_kind() const { return CursorKind::Default; }
    [[nodiscard]] virtual ScreenStackPolicy stack_policy() const noexcept { return {}; }

    [[nodiscard]] const TreeLayout& tree_layout() const { return tree_layout_; }
    [[nodiscard]] Rect              layout_rect(std::string_view path) const {
        return tree_layout_.compose(std::string{path});
    }
    [[nodiscard]] const std::string& config_source() const { return config_source_; }
    [[nodiscard]] ScreenInstanceId   instance_id() const noexcept { return instance_id_; }

    virtual bool handle_input(const InputEvent& /*event*/) { return false; }
    virtual void update(const d2::app::ScreenUpdateContext& context) = 0;
    virtual void render(Renderer2D& renderer) = 0;

private:
    friend class ScreenManager;
    friend class TreeLayoutEditor;

    TreeLayout       tree_layout_;
    std::string      config_source_;
    ScreenInstanceId instance_id_ = 0;
};

} // namespace d2engine
