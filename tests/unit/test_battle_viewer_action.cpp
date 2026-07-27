#include <gtest/gtest.h>

#include "d2engine/app/battle_screen_action.hpp"
#include "d2engine/app/battle_screen_input.hpp"
#include "d2engine/app/battle_tuning_controller.hpp"
#include "d2engine/battle_view/battle_render_tree_contract.hpp"
#include "d2engine/battle_view/battle_unit.hpp"
#include "d2engine/battle_view/battle_viewer_action.hpp"
#include "d2engine/input/input_event.hpp"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <utility>

namespace d2engine {

namespace {

// Helper: map an event with a given tuning context
std::optional<BattleScreenAction> map(const InputEvent& event, bool tuning_enabled) {
    return BattleScreenInputHandler::handle(event, {.tuning_enabled = tuning_enabled});
}

} // namespace

// ── Basic viewer action tests (tuning disabled) ─────────────────────────

TEST(BattleScreenInput, EscapeReturnsQuit) {
    auto action = map(KeyPressed{Key::Escape}, false);
    ASSERT_TRUE(action.has_value());
    EXPECT_EQ(*std::get_if<BattleViewerAction>(&*action), BattleViewerAction::Quit);
}

TEST(BattleScreenInput, SpaceReturnsTriggerAttack) {
    auto action = map(KeyPressed{Key::Space}, false);
    ASSERT_TRUE(action.has_value());
    EXPECT_EQ(*std::get_if<BattleViewerAction>(&*action), BattleViewerAction::TriggerAttack);
}

TEST(BattleScreenInput, UnknownKeyReturnsNone) {
    EXPECT_FALSE(map(KeyPressed{Key::Q}, false).has_value());
}

TEST(BattleScreenInput, Key1ReturnsSetRoleIdle) {
    auto action = map(KeyPressed{Key::Key1}, false);
    ASSERT_TRUE(action.has_value());
    EXPECT_EQ(*std::get_if<BattleViewerAction>(&*action), BattleViewerAction::SetRoleIdle);
}

TEST(BattleScreenInput, Key2ReturnsSetRoleHit) {
    auto action = map(KeyPressed{Key::Key2}, false);
    ASSERT_TRUE(action.has_value());
    EXPECT_EQ(*std::get_if<BattleViewerAction>(&*action), BattleViewerAction::SetRoleHit);
}

TEST(BattleScreenInput, Key3ReturnsSetRoleDeath) {
    auto action = map(KeyPressed{Key::Key3}, false);
    ASSERT_TRUE(action.has_value());
    EXPECT_EQ(*std::get_if<BattleViewerAction>(&*action), BattleViewerAction::SetRoleDeath);
}

TEST(BattleScreenInput, Key4ReturnsSetRoleAttack) {
    auto action = map(KeyPressed{Key::Key4}, false);
    ASSERT_TRUE(action.has_value());
    EXPECT_EQ(*std::get_if<BattleViewerAction>(&*action), BattleViewerAction::SetRoleAttack);
}

TEST(BattleScreenInput, Key5ReturnsSetRoleHeff) {
    auto action = map(KeyPressed{Key::Key5}, false);
    ASSERT_TRUE(action.has_value());
    EXPECT_EQ(*std::get_if<BattleViewerAction>(&*action), BattleViewerAction::SetRoleHeff);
}

TEST(BattleScreenInput, Key6ReturnsSetRoleTuch) {
    auto action = map(KeyPressed{Key::Key6}, false);
    ASSERT_TRUE(action.has_value());
    EXPECT_EQ(*std::get_if<BattleViewerAction>(&*action), BattleViewerAction::SetRoleTuch);
}

TEST(BattleScreenInput, LeftArrowReturnsStepBackward) {
    auto action = map(KeyPressed{Key::Left}, false);
    ASSERT_TRUE(action.has_value());
    EXPECT_EQ(*std::get_if<BattleViewerAction>(&*action), BattleViewerAction::StepBackward);
}

TEST(BattleScreenInput, RightArrowReturnsStepForward) {
    auto action = map(KeyPressed{Key::Right}, false);
    ASSERT_TRUE(action.has_value());
    EXPECT_EQ(*std::get_if<BattleViewerAction>(&*action), BattleViewerAction::StepForward);
}

TEST(BattleScreenInput, LReturnsToggleLayers) {
    auto action = map(KeyPressed{Key::L}, false);
    ASSERT_TRUE(action.has_value());
    EXPECT_EQ(*std::get_if<BattleViewerAction>(&*action), BattleViewerAction::ToggleLayers);
}

TEST(BattleScreenInput, IReturnsToggleTransparent) {
    auto action = map(KeyPressed{Key::I}, false);
    ASSERT_TRUE(action.has_value());
    EXPECT_EQ(*std::get_if<BattleViewerAction>(&*action), BattleViewerAction::ToggleTransparent);
}

TEST(BattleScreenInput, BReturnsCycleBackground) {
    auto action = map(KeyPressed{Key::B}, false);
    ASSERT_TRUE(action.has_value());
    EXPECT_EQ(*std::get_if<BattleViewerAction>(&*action), BattleViewerAction::CycleBackground);
}

// ── Non-key event types ─────────────────────────────────────────────────

TEST(BattleScreenInput, KeyReleasedReturnsNone) {
    EXPECT_FALSE(map(KeyReleased{Key::Escape}, false).has_value());
}

// ── Screen actions (tuning disabled) ────────────────────────────────────

TEST(BattleScreenInput, PReturnsToggleVisualPause) {
    EXPECT_TRUE(std::holds_alternative<ToggleVisualPause>(*map(KeyPressed{Key::P}, false)));
}

TEST(BattleScreenInput, RReturnsRestartScenario) {
    EXPECT_TRUE(std::holds_alternative<RestartScenario>(*map(KeyPressed{Key::R}, false)));
}

TEST(BattleScreenInput, SReturnsLogMousePosition) {
    auto action = map(KeyPressed{Key::S}, false);
    ASSERT_TRUE(action.has_value());
    EXPECT_TRUE(std::holds_alternative<LogMousePosition>(*action));
}

TEST(BattleScreenInput, SHeldReturnsLogMousePosition) {
    // When tuning disabled, S with any modifier is LogMousePosition
    auto action = map(KeyPressed{Key::S, false, KeyModifier::Shift}, false);
    ASSERT_TRUE(action.has_value());
    EXPECT_TRUE(std::holds_alternative<LogMousePosition>(*action));
}

TEST(BattleScreenInput, CtrlSReturnsLogMousePositionWhenTuningDisabled) {
    // Ctrl+S when tuning disabled → LogMousePosition (not SaveTuning)
    auto action = map(KeyPressed{Key::S, false, KeyModifier::Ctrl}, false);
    ASSERT_TRUE(action.has_value());
    EXPECT_TRUE(std::holds_alternative<LogMousePosition>(*action));
}

TEST(BattleScreenInput, PointerPressedReturnsNoActionWhenTuningDisabled) {
    EXPECT_FALSE(map(PointerPressed{PointerButton::Left, 50, 75}, false).has_value());
}

TEST(BattleScreenInput, PointerPressedReturnsNoActionWhenTuningEnabledNoItems) {
    // With empty selectable_items (default), even with tuning enabled there is no hit
    EXPECT_FALSE(map(PointerPressed{PointerButton::Left, 50, 75}, true).has_value());
}

TEST(BattleScreenInput, PointerMovedReturnsUpdateDebugPointerPosition) {
    auto action = map(PointerMoved{42, 99}, false);
    ASSERT_TRUE(action.has_value());
    const auto* ptr = std::get_if<UpdateDebugPointerPosition>(&*action);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->logical_x, 42);
    EXPECT_EQ(ptr->logical_y, 99);
}

// ── ToggleTuning (unconditional Shift+D) ────────────────────────────────

TEST(BattleScreenInput, ShiftDReturnsToggleTuning) {
    EXPECT_TRUE(std::holds_alternative<ToggleTuning>(
        *map(KeyPressed{Key::D, false, KeyModifier::Shift}, false)));
}

TEST(BattleScreenInput, ShiftDReturnsToggleTuningWhenTuningEnabled) {
    auto action = map(KeyPressed{Key::D, false, KeyModifier::Shift}, true);
    ASSERT_TRUE(action.has_value());
    EXPECT_TRUE(std::holds_alternative<ToggleTuning>(*action))
        << "Shift+D must be ToggleTuning, not a tuning edit";
}

TEST(BattleScreenInput, CtrlShiftDReturnsToggleTuningWhenTuningDisabled) {
    // Ctrl+Shift+D with tuning disabled → Shift+D caught → ToggleTuning
    auto action = map(KeyPressed{Key::D, false, KeyModifier::Ctrl | KeyModifier::Shift}, false);
    ASSERT_TRUE(action.has_value());
    EXPECT_TRUE(std::holds_alternative<ToggleTuning>(*action))
        << "Ctrl+Shift+D + tuning disabled must be ToggleTuning (Shift+D fallback)";
}

// ── Tuning lifecycle (tuning enabled) ───────────────────────────────────

TEST(BattleScreenInput, CtrlSReturnsSaveTuning) {
    EXPECT_TRUE(std::holds_alternative<SaveTuning>(
        *map(KeyPressed{Key::S, false, KeyModifier::Ctrl}, true)));
}

TEST(BattleScreenInput, CtrlZReturnsRevertAllTuning) {
    EXPECT_TRUE(std::holds_alternative<RevertAllTuning>(
        *map(KeyPressed{Key::Z, false, KeyModifier::Ctrl}, true)));
}

TEST(BattleScreenInput, BackspaceReturnsRevertSelectedTuning) {
    EXPECT_TRUE(
        std::holds_alternative<RevertSelectedTuning>(*map(KeyPressed{Key::Backspace}, true)));
}

TEST(BattleScreenInput, CtrlBackspaceReturnsRevertAllTuning) {
    EXPECT_TRUE(std::holds_alternative<RevertAllTuning>(
        *map(KeyPressed{Key::Backspace, false, KeyModifier::Ctrl}, true)));
}

TEST(BattleScreenInput, LReturnsLogSelectedTuning) {
    auto action = map(KeyPressed{Key::L}, true);
    ASSERT_TRUE(action.has_value());
    EXPECT_TRUE(std::holds_alternative<LogSelectedTuning>(*action))
        << "L + tuning enabled must be LogSelectedTuning, not ToggleLayers";
}

TEST(BattleScreenInput, CtrlDReturnsConsumeTuningInput) {
    auto action = map(KeyPressed{Key::D, false, KeyModifier::Ctrl}, true);
    ASSERT_TRUE(action.has_value());
    EXPECT_TRUE(std::holds_alternative<ConsumeTuningInput>(*action))
        << "Ctrl+D + tuning enabled must be ConsumeTuningInput, not ApplyTuningEdit";
}

TEST(BattleScreenInput, TabReturnsConsumeTuningInput) {
    EXPECT_TRUE(std::holds_alternative<ConsumeTuningInput>(*map(KeyPressed{Key::Tab}, true)));
}

TEST(BattleScreenInput, TabReturnsNoActionWhenTuningDisabled) {
    EXPECT_FALSE(map(KeyPressed{Key::Tab}, false).has_value());
}

// ── Tuning edit keys (tuning enabled) ───────────────────────────────────

TEST(BattleScreenInput, DReturnsApplyTuningEditWidthIncrease) {
    auto action = map(KeyPressed{Key::D}, true);
    ASSERT_TRUE(action.has_value());
    const auto* edit = std::get_if<ApplyTuningEdit>(&*action);
    ASSERT_NE(edit, nullptr);
    EXPECT_EQ(edit->edit.kind, DebugTuningEditKind::WidthIncrease);
    EXPECT_FLOAT_EQ(edit->edit.step, 1.0f);
}

TEST(BattleScreenInput, SReturnsApplyTuningEditHeightDecrease) {
    auto action = map(KeyPressed{Key::S}, true);
    ASSERT_TRUE(action.has_value());
    const auto* edit = std::get_if<ApplyTuningEdit>(&*action);
    ASSERT_NE(edit, nullptr);
    EXPECT_EQ(edit->edit.kind, DebugTuningEditKind::HeightDecrease);
    EXPECT_FLOAT_EQ(edit->edit.step, 1.0f);
}

TEST(BattleScreenInput, CtrlDNotApplyTuningEdit) {
    auto action = map(KeyPressed{Key::D, false, KeyModifier::Ctrl}, true);
    ASSERT_TRUE(action.has_value());
    EXPECT_FALSE(std::holds_alternative<ApplyTuningEdit>(*action));
    EXPECT_TRUE(std::holds_alternative<ConsumeTuningInput>(*action));
}

TEST(BattleScreenInput, ShiftDNotApplyTuningEdit) {
    // Shift+D is ToggleTuning, NOT WidthIncrease with step=10
    auto action = map(KeyPressed{Key::D, false, KeyModifier::Shift}, true);
    ASSERT_TRUE(action.has_value());
    EXPECT_FALSE(std::holds_alternative<ApplyTuningEdit>(*action));
    EXPECT_TRUE(std::holds_alternative<ToggleTuning>(*action));
}

TEST(BattleScreenInput, CtrlSNotApplyTuningEdit) {
    // Ctrl+S is SaveTuning, NOT HeightDecrease with step=0.25
    auto action = map(KeyPressed{Key::S, false, KeyModifier::Ctrl}, true);
    ASSERT_TRUE(action.has_value());
    EXPECT_FALSE(std::holds_alternative<ApplyTuningEdit>(*action));
    EXPECT_TRUE(std::holds_alternative<SaveTuning>(*action));
}

TEST(BattleScreenInput, DNoModReturnsApplyTuningEditWhenTuningEnabled) {
    auto action = map(KeyPressed{Key::D}, true);
    ASSERT_TRUE(action.has_value());
    EXPECT_TRUE(std::holds_alternative<ApplyTuningEdit>(*action));
}

TEST(BattleScreenInput, DNoModReturnsNoActionWhenTuningDisabled) {
    EXPECT_FALSE(map(KeyPressed{Key::D}, false).has_value());
}

TEST(BattleScreenInput, LeftArrowReturnsTuningEditWhenTuningEnabled) {
    auto action = map(KeyPressed{Key::Left}, true);
    ASSERT_TRUE(action.has_value());
    EXPECT_TRUE(std::holds_alternative<ApplyTuningEdit>(*action))
        << "Left + tuning enabled must be tuning edit, not StepBackward";
}

TEST(BattleScreenInput, GuiLeftReturnsMoveDebugWhenTuningDisabled) {
    auto action = map(KeyPressed{Key::Left, false, KeyModifier::Gui}, false);
    ASSERT_TRUE(action.has_value());
    EXPECT_TRUE(std::holds_alternative<MoveSelectedDebugUnit>(*action));
}

TEST(BattleScreenInput, GuiLeftReturnsTuningEditWhenTuningEnabled) {
    // Tuning edit arrows take precedence over Gui+arrows when tuning is enabled
    auto action = map(KeyPressed{Key::Left, false, KeyModifier::Gui}, true);
    ASSERT_TRUE(action.has_value());
    EXPECT_TRUE(std::holds_alternative<ApplyTuningEdit>(*action))
        << "Gui+Left + tuning enabled must be tuning edit (edit arrows take precedence)";
}

// ── L state-dependent behavior ──────────────────────────────────────────

TEST(BattleScreenInput, LWithTuningReturnsLogSelectedTuning) {
    auto action = map(KeyPressed{Key::L}, true);
    ASSERT_TRUE(action.has_value());
    EXPECT_TRUE(std::holds_alternative<LogSelectedTuning>(*action));
}

TEST(BattleScreenInput, LWithoutTuningReturnsToggleLayers) {
    auto action = map(KeyPressed{Key::L}, false);
    ASSERT_TRUE(action.has_value());
    EXPECT_EQ(*std::get_if<BattleViewerAction>(&*action), BattleViewerAction::ToggleLayers);
}

// ── Screen debug actions (only when tuning enabled) ─────────────────────

TEST(BattleScreenInput, OReturnsToggleDebugHudWhenTuningEnabled) {
    EXPECT_TRUE(std::holds_alternative<ToggleDebugHud>(*map(KeyPressed{Key::O}, true)));
}

TEST(BattleScreenInput, OReturnsNoActionWhenTuningDisabled) {
    EXPECT_FALSE(map(KeyPressed{Key::O}, false).has_value());
}

TEST(BattleScreenInput, ShiftOReturnsToggleSoloSelectedWhenTuningEnabled) {
    EXPECT_TRUE(std::holds_alternative<ToggleSoloSelected>(
        *map(KeyPressed{Key::O, false, KeyModifier::Shift}, true)));
}

TEST(BattleScreenInput, ShiftOReturnsNoActionWhenTuningDisabled) {
    EXPECT_FALSE(map(KeyPressed{Key::O, false, KeyModifier::Shift}, false).has_value());
}

// ── Tuning lifecycle-only when tuning enabled ───────────────────────────

TEST(BattleScreenInput, CtrlSReturnsNoActionWhenTuningDisabled) {
    // Ctrl+S when tuning disabled → LogMousePosition, not SaveTuning
    auto action = map(KeyPressed{Key::S, false, KeyModifier::Ctrl}, false);
    ASSERT_TRUE(action.has_value());
    EXPECT_TRUE(std::holds_alternative<LogMousePosition>(*action));
}

} // namespace d2engine
