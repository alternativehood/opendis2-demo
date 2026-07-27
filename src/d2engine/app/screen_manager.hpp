#pragma once

#include "screen.hpp"

#include <memory>
#include <optional>
#include <deque>
#include <variant>
#include <vector>

namespace d2engine {

class Renderer2D;

struct LiveScreenRef {
    ScreenInstanceId id;
    Screen*          screen;
};

class ScreenManager {
public:
    void switch_to(std::unique_ptr<Screen> screen);
    void request_switch_to(std::unique_ptr<Screen> screen);

    // Modal overlay support
    void push_overlay(std::unique_ptr<Screen> overlay);
    void request_push_overlay(std::unique_ptr<Screen> overlay);
    void pop_overlay();
    void request_pop_overlay();

    void apply_pending_transition();

    [[nodiscard]] bool quit_requested() const { return quit_requested_; }
    void               request_quit() { quit_requested_ = true; }
    [[nodiscard]] bool has_pending_transition() const { return !pending_transitions_.empty(); }

    [[nodiscard]] CursorKind cursor_kind() const;

    [[nodiscard]] std::optional<ScreenInstanceId> consume_revealed_screen();

    bool handle_input(const InputEvent& event);
    void update(const d2::app::ScreenUpdateContext& context);
    void render(Renderer2D& renderer);

    [[nodiscard]] std::vector<LiveScreenRef> live_screens() const;
    [[nodiscard]] Screen*                    find_live_screen(ScreenInstanceId id) const;

private:
    void                                     assign_id(Screen& screen);
    [[nodiscard]] Screen*                    top_screen() noexcept;
    [[nodiscard]] const Screen*              top_screen() const noexcept;
    [[nodiscard]] std::vector<Screen*>       stack_view();
    [[nodiscard]] std::vector<const Screen*> stack_view() const;

    struct SwitchRoot {
        std::unique_ptr<Screen> screen;
    };
    struct PushScreen {
        std::unique_ptr<Screen> screen;
    };
    struct PopScreen {};
    using Transition = std::variant<SwitchRoot, PushScreen, PopScreen>;

    std::unique_ptr<Screen>              active_screen_;
    std::vector<std::unique_ptr<Screen>> overlays_;
    std::deque<Transition>               pending_transitions_;
    std::optional<ScreenInstanceId>      revealed_screen_;
    bool                                 quit_requested_ = false;
    ScreenInstanceId                     next_screen_instance_id_ = 1;
};

} // namespace d2engine
