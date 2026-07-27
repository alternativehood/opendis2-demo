#include "screen_manager.hpp"

#include "../render/renderer2d.hpp"

#include <concepts>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace d2engine {

void ScreenManager::switch_to(std::unique_ptr<Screen> screen) {
    revealed_screen_.reset();
    for (auto it = overlays_.rbegin(); it != overlays_.rend(); ++it) {
        (*it)->on_exit();
    }
    overlays_.clear();
    if (active_screen_) {
        active_screen_->on_exit();
    }
    active_screen_ = std::move(screen);
    if (active_screen_) {
        assign_id(*active_screen_);
        active_screen_->on_enter();
    }
}

void ScreenManager::request_switch_to(std::unique_ptr<Screen> screen) {
    pending_transitions_.push_back(SwitchRoot{std::move(screen)});
}

void ScreenManager::push_overlay(std::unique_ptr<Screen> overlay) {
    if (!overlay)
        return;
    revealed_screen_.reset();
    assign_id(*overlay);
    if (Screen* covered = top_screen()) {
        covered->on_covered();
    }
    overlay->on_enter();
    overlays_.push_back(std::move(overlay));
}

void ScreenManager::request_push_overlay(std::unique_ptr<Screen> overlay) {
    pending_transitions_.push_back(PushScreen{std::move(overlay)});
}

void ScreenManager::pop_overlay() {
    if (overlays_.empty())
        return;
    overlays_.back()->on_exit();
    overlays_.pop_back();
    if (Screen* revealed = top_screen()) {
        revealed->on_revealed();
        revealed_screen_ = revealed->instance_id();
    }
}

void ScreenManager::request_pop_overlay() {
    pending_transitions_.push_back(PopScreen{});
}

void ScreenManager::apply_pending_transition() {
    constexpr std::size_t kMaxTransitionsPerPass = 1024;
    for (std::size_t processed = 0; !pending_transitions_.empty(); ++processed) {
        if (processed == kMaxTransitionsPerPass) {
            throw std::runtime_error("ScreenManager transition loop exceeded bound");
        }
        Transition transition = std::move(pending_transitions_.front());
        pending_transitions_.pop_front();
        std::visit(
            [this](auto& value) {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::same_as<T, SwitchRoot>) {
                    switch_to(std::move(value.screen));
                } else if constexpr (std::same_as<T, PushScreen>) {
                    push_overlay(std::move(value.screen));
                } else {
                    pop_overlay();
                }
            },
            transition);
    }
}

std::optional<ScreenInstanceId> ScreenManager::consume_revealed_screen() {
    return std::exchange(revealed_screen_, std::nullopt);
}

CursorKind ScreenManager::cursor_kind() const {
    if (!overlays_.empty()) {
        return overlays_.back()->cursor_kind();
    }
    if (active_screen_) {
        return active_screen_->cursor_kind();
    }
    return CursorKind::Default;
}

bool ScreenManager::handle_input(const InputEvent& event) {
    const auto stack = stack_view();
    for (std::size_t index = stack.size(); index > 0; --index) {
        Screen*    screen = stack[index - 1];
        const bool handled = screen->handle_input(event);
        if (handled || !screen->stack_policy().input_below) {
            // A modal layer consumes the event even if it chose not to act on it:
            // lower screens must not observe blocked input.
            return true;
        }
    }
    return false;
}

void ScreenManager::update(const d2::app::ScreenUpdateContext& context) {
    const auto stack = stack_view();
    for (std::size_t index = stack.size(); index > 0; --index) {
        Screen* screen = stack[index - 1];
        screen->update(context);
        if (!screen->stack_policy().update_below) {
            break;
        }
    }
}

void ScreenManager::render(Renderer2D& renderer) {
    const auto stack = stack_view();
    if (stack.empty()) {
        return;
    }
    std::size_t first = stack.size() - 1;
    while (first > 0 && stack[first]->stack_policy().render_below) {
        --first;
    }
    for (std::size_t index = first; index < stack.size(); ++index) {
        stack[index]->render(renderer);
    }
}

std::vector<LiveScreenRef> ScreenManager::live_screens() const {
    std::vector<LiveScreenRef> screens;
    if (active_screen_) {
        screens.push_back({active_screen_->instance_id(), active_screen_.get()});
    }
    for (const auto& overlay : overlays_) {
        screens.push_back({overlay->instance_id(), overlay.get()});
    }
    return screens;
}

Screen* ScreenManager::find_live_screen(ScreenInstanceId id) const {
    if (active_screen_ && active_screen_->instance_id() == id)
        return active_screen_.get();
    for (const auto& overlay : overlays_) {
        if (overlay->instance_id() == id)
            return overlay.get();
    }
    return nullptr;
}

Screen* ScreenManager::top_screen() noexcept {
    return overlays_.empty() ? active_screen_.get() : overlays_.back().get();
}

const Screen* ScreenManager::top_screen() const noexcept {
    return overlays_.empty() ? active_screen_.get() : overlays_.back().get();
}

std::vector<Screen*> ScreenManager::stack_view() {
    std::vector<Screen*> stack;
    if (active_screen_) {
        stack.push_back(active_screen_.get());
    }
    for (auto& overlay : overlays_) {
        stack.push_back(overlay.get());
    }
    return stack;
}

std::vector<const Screen*> ScreenManager::stack_view() const {
    std::vector<const Screen*> stack;
    if (active_screen_) {
        stack.push_back(active_screen_.get());
    }
    for (const auto& overlay : overlays_) {
        stack.push_back(overlay.get());
    }
    return stack;
}

void ScreenManager::assign_id(Screen& screen) {
    screen.instance_id_ = next_screen_instance_id_++;
}

} // namespace d2engine
