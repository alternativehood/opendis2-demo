#include <gtest/gtest.h>

#include "d2engine/app/screen_manager.hpp"
#include "d2engine/input/input_event.hpp"
#include "d2engine/render/render_tree.hpp"
#include "d2engine/render/renderer2d.hpp"

#include <nlohmann/json.hpp>

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace d2engine {
namespace {

namespace {
d2engine::TreeLayout make_test_tree() {
    TreeLayout     tree;
    nlohmann::json j;
    j["/root"] = nlohmann::json::object();
    tree.load(j);
    return tree;
}
} // namespace

struct LifecycleFlags {
    bool on_enter_called = false;
    bool on_exit_called = false;
};

class TestScreen : public Screen {
public:
    explicit TestScreen(std::string id, LifecycleFlags* flags = nullptr)
        : Screen(make_test_tree(), "test://screen-layout"), id_(std::move(id)), flags_(flags) {}

    std::string_view name() const override { return id_; }

    bool                         input_handled = false;
    int                          update_count = 0;
    d2::app::ScreenUpdateContext last_update_context{};

    void on_enter() override {
        if (flags_)
            flags_->on_enter_called = true;
    }
    void on_exit() override {
        if (flags_)
            flags_->on_exit_called = true;
    }
    bool handle_input(const InputEvent&) override {
        input_handled = true;
        return true;
    }
    void update(const d2::app::ScreenUpdateContext& context) override {
        ++update_count;
        last_update_context = context;
    }
    void render(Renderer2D&) override {}

private:
    std::string     id_;
    LifecycleFlags* flags_ = nullptr;
};

TEST(ScreenManager, StartCallsOnEnter) {
    LifecycleFlags flags;
    ScreenManager  mgr;
    auto           screen = std::make_unique<TestScreen>("test", &flags);

    EXPECT_FALSE(flags.on_enter_called);
    mgr.switch_to(std::move(screen));
    EXPECT_TRUE(flags.on_enter_called);
}

TEST(ScreenManager, SwitchToCallsOnExitAndOnEnter) {
    LifecycleFlags flags_first;
    LifecycleFlags flags_second;
    ScreenManager  mgr;

    auto first = std::make_unique<TestScreen>("first", &flags_first);
    mgr.switch_to(std::move(first));

    auto second = std::make_unique<TestScreen>("second", &flags_second);
    mgr.switch_to(std::move(second));

    // flags_first survives even though the first screen is now destroyed
    EXPECT_TRUE(flags_first.on_exit_called);
    EXPECT_TRUE(flags_second.on_enter_called);
}

TEST(ScreenManager, HandleInputDelegatesToActiveScreen) {
    ScreenManager mgr;
    auto          screen = std::make_unique<TestScreen>("test");
    auto*         raw = screen.get();
    mgr.switch_to(std::move(screen));

    InputEvent event = KeyPressed{Key::Space};
    EXPECT_TRUE(mgr.handle_input(event));
    EXPECT_TRUE(raw->input_handled);
}

TEST(ScreenManager, UpdateDelegatesToActiveScreen) {
    ScreenManager mgr;
    auto          screen = std::make_unique<TestScreen>("test");
    auto*         raw = screen.get();
    mgr.switch_to(std::move(screen));

    EXPECT_EQ(raw->update_count, 0);
    mgr.update({16.0F, 16.0F});
    EXPECT_EQ(raw->update_count, 1);
    mgr.update({16.0F, 16.0F});
    EXPECT_EQ(raw->update_count, 2);
}

TEST(ScreenManager, UpdateForwardsBothTimeDomainsUnchanged) {
    ScreenManager mgr;
    auto          screen = std::make_unique<TestScreen>("test");
    auto*         raw = screen.get();
    mgr.switch_to(std::move(screen));
    const d2::app::ScreenUpdateContext context{.real_delta_ms = 20.0F, .animation_delta_ms = 40.0F};
    mgr.update(context);
    EXPECT_FLOAT_EQ(raw->last_update_context.real_delta_ms, 20.0F);
    EXPECT_FLOAT_EQ(raw->last_update_context.animation_delta_ms, 40.0F);
}

TEST(ScreenManager, RequestQuitSetsFlag) {
    ScreenManager mgr;
    mgr.request_quit();
    EXPECT_TRUE(mgr.quit_requested());
}

TEST(ScreenManager, SecondSwitchToCallsExitOnPreviousScreen) {
    LifecycleFlags flags_first;
    LifecycleFlags flags_second;
    LifecycleFlags flags_third;
    ScreenManager  mgr;

    auto first = std::make_unique<TestScreen>("first", &flags_first);
    mgr.switch_to(std::move(first));

    auto second = std::make_unique<TestScreen>("second", &flags_second);
    mgr.switch_to(std::move(second));

    EXPECT_TRUE(flags_first.on_exit_called);
    EXPECT_TRUE(flags_second.on_enter_called);

    auto third = std::make_unique<TestScreen>("third", &flags_third);
    mgr.switch_to(std::move(third));

    EXPECT_TRUE(flags_second.on_exit_called);
    EXPECT_TRUE(flags_third.on_enter_called);
}

TEST(ScreenManager, ScreenCanTriggerQuitViaExternalCallback) {
    // Verify that a screen can trigger quit by calling
    // request_quit on the ScreenManager through a callback.
    ScreenManager mgr;

    class QuitTestScreen : public Screen {
    public:
        QuitTestScreen() : Screen(make_test_tree(), "test://screen-layout") {}
        std::function<void()> on_quit;
        bool                  handled = false;
        std::string_view      name() const override { return "quit_test"; }
        bool                  handle_input(const InputEvent&) override {
            if (on_quit)
                on_quit();
            handled = true;
            return true;
        }
        void update(const d2::app::ScreenUpdateContext&) override {}
        void render(Renderer2D&) override {}
    };

    auto screen = std::make_unique<QuitTestScreen>();
    screen->on_quit = [&]() { mgr.request_quit(); };
    mgr.switch_to(std::move(screen));

    EXPECT_FALSE(mgr.quit_requested());
    InputEvent event = KeyPressed{Key::Escape};
    mgr.handle_input(event);
    EXPECT_TRUE(mgr.quit_requested());
}

TEST(ScreenManager, DeferredSwitchDoesNotDestroyActiveScreenDuringHandleInput) {
    struct DeferredSwitchState {
        bool a_handle_returned = false;
        bool a_destroyed = false;
        bool a_destroyed_before_handle_return = false;
        bool b_entered = false;
    };

    class TargetScreen final : public Screen {
    public:
        explicit TargetScreen(DeferredSwitchState& state)
            : Screen(make_test_tree(), "test://screen-layout"), state_(state) {}
        std::string_view name() const override { return "target"; }
        void             on_enter() override { state_.b_entered = true; }
        void             update(const d2::app::ScreenUpdateContext&) override {}
        void             render(Renderer2D&) override {}

    private:
        DeferredSwitchState& state_;
    };

    class SwitchingScreen final : public Screen {
    public:
        SwitchingScreen(ScreenManager& manager, DeferredSwitchState& state)
            : Screen(make_test_tree(), "test://screen-layout"), manager_(manager), state_(state) {}
        SwitchingScreen(const SwitchingScreen&) = delete;
        SwitchingScreen& operator=(const SwitchingScreen&) = delete;
        SwitchingScreen(SwitchingScreen&&) = delete;
        SwitchingScreen& operator=(SwitchingScreen&&) = delete;
        ~SwitchingScreen() override {
            state_.a_destroyed = true;
            state_.a_destroyed_before_handle_return = !state_.a_handle_returned;
        }

        std::string_view name() const override { return "switching"; }
        bool             handle_input(const InputEvent&) override {
            manager_.request_switch_to(std::make_unique<TargetScreen>(state_));
            state_.a_handle_returned = true;
            return true;
        }
        void update(const d2::app::ScreenUpdateContext&) override {}
        void render(Renderer2D&) override {}

    private:
        ScreenManager&       manager_;
        DeferredSwitchState& state_;
    };

    DeferredSwitchState state;
    ScreenManager       mgr;
    mgr.switch_to(std::make_unique<SwitchingScreen>(mgr, state));

    InputEvent event = KeyPressed{Key::Space};
    EXPECT_TRUE(mgr.handle_input(event));
    EXPECT_TRUE(state.a_handle_returned);
    EXPECT_FALSE(state.a_destroyed);
    EXPECT_TRUE(mgr.has_pending_transition());
    EXPECT_FALSE(state.b_entered);

    mgr.apply_pending_transition();
    EXPECT_TRUE(state.a_destroyed);
    EXPECT_FALSE(state.a_destroyed_before_handle_return);
    EXPECT_TRUE(state.b_entered);
}

// ── Modal overlay tests ─────────────────────────────────────────────────
//
// Helper: a screen that tracks whether it rendered
class RenderTrackedScreen : public Screen {
public:
    explicit RenderTrackedScreen(std::string id, bool* rendered_flag = nullptr,
                                 ScreenStackPolicy policy = {})
        : Screen(make_test_tree(), "test://screen-layout"), id_(std::move(id)),
          rendered_(rendered_flag), policy_(policy) {}

    std::string_view  name() const override { return id_; }
    ScreenStackPolicy stack_policy() const noexcept override { return policy_; }
    bool              input_handled = false;
    bool              input_claimed = true;
    int               update_count = 0;

    bool handle_input(const InputEvent&) override {
        input_handled = true;
        return input_claimed;
    }
    void update(const d2::app::ScreenUpdateContext&) override { ++update_count; }
    void render(Renderer2D&) override {
        if (rendered_)
            *rendered_ = true;
    }

private:
    std::string       id_;
    bool*             rendered_ = nullptr;
    ScreenStackPolicy policy_;
};

struct PolicyScreenTrace {
    std::vector<std::string>* render_order = nullptr;
    int                       update_count = 0;
    int                       input_count = 0;
    int                       covered_count = 0;
    int                       revealed_count = 0;
    int                       exit_count = 0;
    bool                      input_result = false;
};

class PolicyScreen final : public Screen {
public:
    PolicyScreen(std::string id, ScreenStackPolicy policy, PolicyScreenTrace& trace)
        : Screen(make_test_tree(), "test://screen-layout"), id_(std::move(id)), policy_(policy),
          trace_(trace) {}

    std::string_view  name() const override { return id_; }
    ScreenStackPolicy stack_policy() const noexcept override { return policy_; }
    void              on_covered() override { ++trace_.covered_count; }
    void              on_revealed() override { ++trace_.revealed_count; }
    void              on_exit() override { ++trace_.exit_count; }
    bool              handle_input(const InputEvent&) override {
        ++trace_.input_count;
        return trace_.input_result;
    }
    void update(const d2::app::ScreenUpdateContext&) override { ++trace_.update_count; }
    void render(Renderer2D&) override {
        if (trace_.render_order != nullptr) {
            trace_.render_order->push_back(id_);
        }
    }

private:
    std::string        id_;
    ScreenStackPolicy  policy_;
    PolicyScreenTrace& trace_;
};

TEST(ScreenManager, BaseScreenSurvivesOverlayPushPop) {
    LifecycleFlags base_flags;
    LifecycleFlags overlay_flags;
    ScreenManager  mgr;

    auto base = std::make_unique<TestScreen>("base", &base_flags);
    mgr.switch_to(std::move(base));
    EXPECT_TRUE(base_flags.on_enter_called);

    // Push overlay
    auto overlay = std::make_unique<TestScreen>("overlay", &overlay_flags);
    mgr.push_overlay(std::move(overlay));
    EXPECT_TRUE(overlay_flags.on_enter_called);

    // Pop overlay
    mgr.pop_overlay();
    EXPECT_TRUE(overlay_flags.on_exit_called);

    // Base screen still active
    EXPECT_FALSE(base_flags.on_exit_called);
}

TEST(ScreenManager, OverlayReceivesInputInsteadOfBase) {
    ScreenManager mgr;

    auto  base = std::make_unique<RenderTrackedScreen>("base");
    auto* raw_base = base.get();
    mgr.switch_to(std::move(base));

    auto  overlay = std::make_unique<RenderTrackedScreen>("overlay");
    auto* raw_overlay = overlay.get();
    overlay->input_claimed = false; // overlay does not claim the event
    mgr.push_overlay(std::move(overlay));

    InputEvent event = KeyPressed{Key::Escape};
    const bool consumed = mgr.handle_input(event);

    // Modal: ScreenManager must return true when any overlay exists,
    // regardless of whether the overlay itself claimed the event.
    EXPECT_TRUE(consumed);
    // Only overlay receives input (not base)
    EXPECT_TRUE(raw_overlay->input_handled);
    EXPECT_FALSE(raw_base->input_handled);
}

TEST(ScreenManager, BaseRendersBeforeOverlay) {
    ScreenManager            mgr;
    std::vector<const char*> render_order;

    class OrderTrackedScreen final : public Screen {
    public:
        OrderTrackedScreen(const char* id, std::vector<const char*>& order,
                           ScreenStackPolicy policy = {})
            : Screen(make_test_tree(), "test://screen-layout"), id_(id), order_(order),
              policy_(policy) {}
        std::string_view  name() const override { return id_; }
        ScreenStackPolicy stack_policy() const noexcept override { return policy_; }
        bool              handle_input(const InputEvent&) override { return false; }
        void              update(const d2::app::ScreenUpdateContext&) override {}
        void              render(Renderer2D&) override { order_.push_back(id_); }

    private:
        const char*               id_;
        std::vector<const char*>& order_;
        ScreenStackPolicy         policy_;
    };

    auto base = std::make_unique<OrderTrackedScreen>("base", render_order);
    mgr.switch_to(std::move(base));

    auto overlay = std::make_unique<OrderTrackedScreen>("overlay", render_order,
                                                        ScreenStackPolicy{.render_below = true});
    mgr.push_overlay(std::move(overlay));

    Renderer2D r{nullptr}; // render() does not touch the SDL renderer for these tests
    mgr.render(r);

    ASSERT_EQ(render_order.size(), 2u);
    EXPECT_STREQ(render_order[0], "base");
    EXPECT_STREQ(render_order[1], "overlay");
}

TEST(ScreenManager, DeferredOverlayPushIsNotAppliedDuringHandleInput) {
    ScreenManager mgr;
    mgr.switch_to(std::make_unique<RenderTrackedScreen>("base"));

    // Request push during handle_input
    struct Pusher final : public Screen {
        explicit Pusher(ScreenManager& mgr)
            : Screen(make_test_tree(), "test://screen-layout"), mgr_(mgr) {}
        std::string_view name() const override { return "pusher"; }
        bool             handle_input(const InputEvent&) override {
            mgr_.request_push_overlay(std::make_unique<RenderTrackedScreen>("overlay"));
            return true;
        }
        void           update(const d2::app::ScreenUpdateContext&) override {}
        void           render(Renderer2D&) override {}
        ScreenManager& mgr_;
    };
    mgr.switch_to(std::make_unique<Pusher>(mgr));

    InputEvent event = KeyPressed{Key::Escape};
    mgr.handle_input(event);

    // Overlay should NOT have been applied yet (deferred)
    EXPECT_TRUE(mgr.has_pending_transition());

    // After applying, transition is consumed
    mgr.apply_pending_transition();
    EXPECT_FALSE(mgr.has_pending_transition());
}

TEST(ScreenManager, DeferredOverlayPopDoesNotDestroyScreenDuringHandleInput) {
    struct PopState {
        bool overlay_exited = false;
    };

    class PopOverlayScreen final : public Screen {
    public:
        PopOverlayScreen(ScreenManager& mgr, PopState& state, bool& destroyed)
            : Screen(make_test_tree(), "test://screen-layout"), mgr_(mgr), state_(state),
              destroyed_(destroyed) {}
        ~PopOverlayScreen() override { destroyed_ = true; }
        PopOverlayScreen(const PopOverlayScreen&) = delete;
        PopOverlayScreen& operator=(const PopOverlayScreen&) = delete;
        PopOverlayScreen(PopOverlayScreen&&) = delete;
        PopOverlayScreen& operator=(PopOverlayScreen&&) = delete;

        std::string_view name() const override { return "pop_overlay"; }
        bool             handle_input(const InputEvent&) override {
            mgr_.request_pop_overlay();
            EXPECT_FALSE(destroyed_);
            state_.overlay_exited = true;
            return true;
        }
        void on_exit() override { state_.overlay_exited = true; }
        void update(const d2::app::ScreenUpdateContext&) override {}
        void render(Renderer2D&) override {}

    private:
        ScreenManager& mgr_;
        PopState&      state_;
        bool&          destroyed_;
    };

    PopState      state;
    bool          overlay_destroyed = false;
    ScreenManager mgr;

    auto base = std::make_unique<TestScreen>("base");
    mgr.switch_to(std::move(base));

    auto overlay = std::make_unique<PopOverlayScreen>(mgr, state, overlay_destroyed);
    mgr.push_overlay(std::move(overlay));

    InputEvent event = KeyPressed{Key::Escape};
    EXPECT_TRUE(mgr.handle_input(event));

    // At this point the overlay should NOT have been destroyed
    // (deferred pop, screen is not destroyed inside its own handler)
    EXPECT_FALSE(overlay_destroyed);

    mgr.apply_pending_transition();

    // After apply_pending_transition: overlay on_exit called and destroyed
    EXPECT_TRUE(state.overlay_exited);
    EXPECT_TRUE(overlay_destroyed);

    // Base should now receive input again
    {
        auto  base_screen = std::make_unique<RenderTrackedScreen>("base2");
        auto* raw_base = base_screen.get();
        mgr.switch_to(std::move(base_screen));
        EXPECT_FALSE(raw_base->input_handled);
        InputEvent ev = KeyPressed{Key::Space};
        mgr.handle_input(ev);
        EXPECT_TRUE(raw_base->input_handled);
    }
}

TEST(ScreenManager, SwitchToBaseCallsOnExitOnOverlays) {
    LifecycleFlags overlay_flags;
    ScreenManager  mgr;

    auto base = std::make_unique<TestScreen>("base");
    mgr.switch_to(std::move(base));

    auto overlay = std::make_unique<TestScreen>("overlay", &overlay_flags);
    mgr.push_overlay(std::move(overlay));
    EXPECT_TRUE(overlay_flags.on_enter_called);
    EXPECT_FALSE(overlay_flags.on_exit_called);

    // Switch to new base screen
    LifecycleFlags new_base_flags;
    auto           new_base = std::make_unique<TestScreen>("new_base", &new_base_flags);
    mgr.switch_to(std::move(new_base));

    // Overlay should have received on_exit
    EXPECT_TRUE(overlay_flags.on_exit_called);
    EXPECT_TRUE(new_base_flags.on_enter_called);

    // Input should go to new base (no overlays remain)
    InputEvent event = KeyPressed{Key::Escape};
    EXPECT_TRUE(mgr.handle_input(event));
}

TEST(ScreenManager, UpdateRoutesToOverlayWhenPresent) {
    ScreenManager mgr;

    auto  base = std::make_unique<RenderTrackedScreen>("base");
    auto* raw_base = base.get();
    mgr.switch_to(std::move(base));

    auto  overlay = std::make_unique<RenderTrackedScreen>("overlay");
    auto* raw_overlay = overlay.get();
    mgr.push_overlay(std::move(overlay));

    ASSERT_EQ(raw_base->update_count, 0);
    ASSERT_EQ(raw_overlay->update_count, 0);

    mgr.update({16.0F, 16.0F});

    // Only overlay should be updated
    EXPECT_EQ(raw_base->update_count, 0);
    EXPECT_EQ(raw_overlay->update_count, 1);
}

TEST(ScreenManager, UpdateRoutesToBaseWhenNoOverlay) {
    ScreenManager mgr;

    auto  base = std::make_unique<RenderTrackedScreen>("base");
    auto* raw_base = base.get();
    mgr.switch_to(std::move(base));

    ASSERT_EQ(raw_base->update_count, 0);

    mgr.update({16.0F, 16.0F});

    EXPECT_EQ(raw_base->update_count, 1);
}

TEST(ScreenManager, UpdateReturnsToBaseAfterOverlayPop) {
    ScreenManager mgr;

    auto  base = std::make_unique<RenderTrackedScreen>("base");
    auto* raw_base = base.get();
    mgr.switch_to(std::move(base));

    auto  overlay = std::make_unique<RenderTrackedScreen>("overlay");
    auto* raw_overlay = overlay.get();
    mgr.push_overlay(std::move(overlay));

    mgr.update({16.0F, 16.0F});
    EXPECT_EQ(raw_overlay->update_count, 1);
    EXPECT_EQ(raw_base->update_count, 0);

    mgr.pop_overlay();

    mgr.update({16.0F, 16.0F});
    EXPECT_EQ(raw_base->update_count, 1);
}

TEST(ScreenManager, OpaqueTopRendersWithoutRoot) {
    ScreenManager            mgr;
    std::vector<std::string> order;
    PolicyScreenTrace        root{.render_order = &order};
    PolicyScreenTrace        opaque{.render_order = &order};

    mgr.switch_to(std::make_unique<PolicyScreen>("root", ScreenStackPolicy{}, root));
    mgr.push_overlay(std::make_unique<PolicyScreen>("opaque", ScreenStackPolicy{}, opaque));

    Renderer2D renderer{nullptr};
    mgr.render(renderer);

    ASSERT_EQ(order.size(), 1U);
    EXPECT_EQ(order[0], "opaque");
}

TEST(ScreenManager, TransparentTopRendersAboveOpaqueLayer) {
    ScreenManager            mgr;
    std::vector<std::string> order;
    PolicyScreenTrace        root{.render_order = &order};
    PolicyScreenTrace        opaque{.render_order = &order};
    PolicyScreenTrace        transparent{.render_order = &order};

    mgr.switch_to(std::make_unique<PolicyScreen>("root", ScreenStackPolicy{}, root));
    mgr.push_overlay(std::make_unique<PolicyScreen>("opaque", ScreenStackPolicy{}, opaque));
    mgr.push_overlay(std::make_unique<PolicyScreen>(
        "transparent", ScreenStackPolicy{.render_below = true}, transparent));

    Renderer2D renderer{nullptr};
    mgr.render(renderer);

    ASSERT_EQ(order.size(), 2U);
    EXPECT_EQ(order[0], "opaque");
    EXPECT_EQ(order[1], "transparent");
}

TEST(ScreenManager, NestedCoverAndRevealFollowTopOfStack) {
    ScreenManager     mgr;
    PolicyScreenTrace adventure;
    PolicyScreenTrace battle;
    PolicyScreenTrace stack_info;

    mgr.switch_to(std::make_unique<PolicyScreen>("adventure", ScreenStackPolicy{}, adventure));
    mgr.push_overlay(std::make_unique<PolicyScreen>("battle", ScreenStackPolicy{}, battle));
    EXPECT_EQ(adventure.covered_count, 1);
    EXPECT_EQ(battle.covered_count, 0);

    mgr.push_overlay(std::make_unique<PolicyScreen>(
        "stack_info", ScreenStackPolicy{.render_below = true}, stack_info));
    EXPECT_EQ(battle.covered_count, 1);
    EXPECT_EQ(adventure.revealed_count, 0);

    mgr.pop_overlay();
    EXPECT_EQ(stack_info.exit_count, 1);
    EXPECT_EQ(battle.revealed_count, 1);
    EXPECT_EQ(adventure.revealed_count, 0);

    mgr.pop_overlay();
    EXPECT_EQ(battle.exit_count, 1);
    EXPECT_EQ(adventure.revealed_count, 1);
}

TEST(ScreenManager, NestedPopReportsActualRevealedScreen) {
    ScreenManager     mgr;
    PolicyScreenTrace adventure;
    PolicyScreenTrace battle;
    PolicyScreenTrace stack_info;

    auto adventure_screen =
        std::make_unique<PolicyScreen>("adventure", ScreenStackPolicy{}, adventure);
    mgr.switch_to(std::move(adventure_screen));
    auto battle_screen = std::make_unique<PolicyScreen>("battle", ScreenStackPolicy{}, battle);
    PolicyScreen* battle_raw = battle_screen.get();
    mgr.push_overlay(std::move(battle_screen));
    mgr.push_overlay(std::make_unique<PolicyScreen>(
        "stack_info", ScreenStackPolicy{.render_below = true}, stack_info));

    mgr.pop_overlay();

    const auto revealed = mgr.consume_revealed_screen();
    ASSERT_TRUE(revealed.has_value());
    EXPECT_EQ(mgr.find_live_screen(*revealed), battle_raw);
}

TEST(ScreenManager, PopThenPushDoesNotReportCoveredScreenAsRevealed) {
    ScreenManager     mgr;
    PolicyScreenTrace root;
    PolicyScreenTrace overlay;
    PolicyScreenTrace replacement;

    mgr.switch_to(std::make_unique<PolicyScreen>("root", ScreenStackPolicy{}, root));
    mgr.push_overlay(std::make_unique<PolicyScreen>("overlay", ScreenStackPolicy{}, overlay));
    mgr.request_pop_overlay();
    mgr.request_push_overlay(
        std::make_unique<PolicyScreen>("replacement", ScreenStackPolicy{}, replacement));
    mgr.apply_pending_transition();

    EXPECT_FALSE(mgr.consume_revealed_screen().has_value());
}

TEST(ScreenManager, PopThenSwitchDoesNotReportReplacedScreenAsRevealed) {
    ScreenManager     mgr;
    PolicyScreenTrace root;
    PolicyScreenTrace overlay;
    PolicyScreenTrace replacement;

    mgr.switch_to(std::make_unique<PolicyScreen>("root", ScreenStackPolicy{}, root));
    mgr.push_overlay(std::make_unique<PolicyScreen>("overlay", ScreenStackPolicy{}, overlay));
    mgr.request_pop_overlay();
    mgr.request_switch_to(
        std::make_unique<PolicyScreen>("replacement", ScreenStackPolicy{}, replacement));
    mgr.apply_pending_transition();

    EXPECT_FALSE(mgr.consume_revealed_screen().has_value());
}

TEST(ScreenManager, FifoTransitionsPreservePushThenPop) {
    ScreenManager     mgr;
    PolicyScreenTrace root;
    PolicyScreenTrace overlay;

    mgr.switch_to(std::make_unique<PolicyScreen>("root", ScreenStackPolicy{}, root));
    mgr.request_push_overlay(
        std::make_unique<PolicyScreen>("overlay", ScreenStackPolicy{}, overlay));
    mgr.request_pop_overlay();
    mgr.apply_pending_transition();

    EXPECT_EQ(root.covered_count, 1);
    EXPECT_EQ(root.revealed_count, 1);
    EXPECT_EQ(overlay.exit_count, 1);
}

TEST(ScreenManager, FifoTransitionsApplyMultipleQueuedPops) {
    ScreenManager     mgr;
    PolicyScreenTrace root;
    PolicyScreenTrace first;
    PolicyScreenTrace second;

    mgr.switch_to(std::make_unique<PolicyScreen>("root", ScreenStackPolicy{}, root));
    mgr.push_overlay(std::make_unique<PolicyScreen>("first", ScreenStackPolicy{}, first));
    mgr.push_overlay(std::make_unique<PolicyScreen>("second", ScreenStackPolicy{}, second));
    mgr.request_pop_overlay();
    mgr.request_pop_overlay();
    mgr.apply_pending_transition();

    EXPECT_EQ(second.exit_count, 1);
    EXPECT_EQ(first.exit_count, 1);
    EXPECT_EQ(root.revealed_count, 1);
}

TEST(ScreenManager, SwitchingRootExitsFullStackFromTopDown) {
    ScreenManager     mgr;
    PolicyScreenTrace root;
    PolicyScreenTrace first;
    PolicyScreenTrace second;
    PolicyScreenTrace replacement;

    mgr.switch_to(std::make_unique<PolicyScreen>("root", ScreenStackPolicy{}, root));
    mgr.push_overlay(std::make_unique<PolicyScreen>("first", ScreenStackPolicy{}, first));
    mgr.push_overlay(std::make_unique<PolicyScreen>("second", ScreenStackPolicy{}, second));
    mgr.request_switch_to(
        std::make_unique<PolicyScreen>("replacement", ScreenStackPolicy{}, replacement));
    mgr.apply_pending_transition();

    EXPECT_EQ(second.exit_count, 1);
    EXPECT_EQ(first.exit_count, 1);
    EXPECT_EQ(root.exit_count, 1);
}

TEST(ScreenManager, StackPoliciesBlockLowerUpdateAndInput) {
    ScreenManager     mgr;
    PolicyScreenTrace root;
    PolicyScreenTrace battle;
    PolicyScreenTrace stack_info;

    mgr.switch_to(std::make_unique<PolicyScreen>("root", ScreenStackPolicy{}, root));
    mgr.push_overlay(std::make_unique<PolicyScreen>("battle", ScreenStackPolicy{}, battle));
    mgr.update({16.0F, 16.0F});
    InputEvent event = KeyPressed{Key::Space};
    EXPECT_TRUE(mgr.handle_input(event));
    EXPECT_EQ(root.update_count, 0);
    EXPECT_EQ(root.input_count, 0);
    EXPECT_EQ(battle.update_count, 1);
    EXPECT_EQ(battle.input_count, 1);

    mgr.push_overlay(std::make_unique<PolicyScreen>(
        "stack_info", ScreenStackPolicy{.render_below = true}, stack_info));
    mgr.update({16.0F, 16.0F});
    EXPECT_TRUE(mgr.handle_input(event));
    EXPECT_EQ(battle.update_count, 1);
    EXPECT_EQ(battle.input_count, 1);
    EXPECT_EQ(stack_info.update_count, 1);
    EXPECT_EQ(stack_info.input_count, 1);
}

} // namespace
} // namespace d2engine
