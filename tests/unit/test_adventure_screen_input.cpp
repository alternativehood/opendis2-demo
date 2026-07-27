#include <gtest/gtest.h>

#include "d2engine/app/adventure_screen_input.hpp"
#include "d2engine/input/input_event.hpp"

namespace d2engine {

TEST(AdventureScreenInput, EscapeReturnsCancel) {
    InputEvent event = KeyPressed{Key::Escape};
    auto       action = AdventureScreenInputHandler::handle(event);
    ASSERT_TRUE(action.has_value());
    EXPECT_TRUE(std::holds_alternative<AdventureCancel>(*action));
}

TEST(AdventureScreenInput, QReturnsCancel) {
    InputEvent event = KeyPressed{Key::Q};
    auto       action = AdventureScreenInputHandler::handle(event);
    ASSERT_TRUE(action.has_value());
    EXPECT_TRUE(std::holds_alternative<AdventureCancel>(*action));
}

TEST(AdventureScreenInput, BReturnsOpenDebugBattle) {
    InputEvent event = KeyPressed{Key::B};
    auto       action = AdventureScreenInputHandler::handle(event);
    ASSERT_TRUE(action.has_value());
    EXPECT_TRUE(std::holds_alternative<AdventureOpenDebugBattle>(*action));
}

TEST(AdventureScreenInput, LeftArrowReturnsPanCamera) {
    InputEvent event = KeyPressed{Key::Left};
    auto       action = AdventureScreenInputHandler::handle(event);
    ASSERT_TRUE(action.has_value());
    const auto* pan = std::get_if<AdventurePanCamera>(&*action);
    ASSERT_NE(pan, nullptr);
    EXPECT_EQ(pan->dx, -64);
    EXPECT_EQ(pan->dy, 0);
}

TEST(AdventureScreenInput, RightArrowReturnsPanCamera) {
    InputEvent event = KeyPressed{Key::Right};
    auto       action = AdventureScreenInputHandler::handle(event);
    ASSERT_TRUE(action.has_value());
    const auto* pan = std::get_if<AdventurePanCamera>(&*action);
    ASSERT_NE(pan, nullptr);
    EXPECT_EQ(pan->dx, 64);
    EXPECT_EQ(pan->dy, 0);
}

TEST(AdventureScreenInput, UpArrowReturnsPanCamera) {
    InputEvent event = KeyPressed{Key::Up};
    auto       action = AdventureScreenInputHandler::handle(event);
    ASSERT_TRUE(action.has_value());
    const auto* pan = std::get_if<AdventurePanCamera>(&*action);
    ASSERT_NE(pan, nullptr);
    EXPECT_EQ(pan->dx, 0);
    EXPECT_EQ(pan->dy, -64);
}

TEST(AdventureScreenInput, DownArrowReturnsPanCamera) {
    InputEvent event = KeyPressed{Key::Down};
    auto       action = AdventureScreenInputHandler::handle(event);
    ASSERT_TRUE(action.has_value());
    const auto* pan = std::get_if<AdventurePanCamera>(&*action);
    ASSERT_NE(pan, nullptr);
    EXPECT_EQ(pan->dx, 0);
    EXPECT_EQ(pan->dy, 64);
}

TEST(AdventureScreenInput, RightClickReturnsInspectAt) {
    InputEvent event = PointerPressed{PointerButton::Right, 150, 200};
    auto       action = AdventureScreenInputHandler::handle(event);
    ASSERT_TRUE(action.has_value());
    const auto* inspect = std::get_if<AdventureInspectAt>(&*action);
    ASSERT_NE(inspect, nullptr);
    EXPECT_EQ(inspect->x, 150);
    EXPECT_EQ(inspect->y, 200);
}

TEST(AdventureScreenInput, LeftClickReturnsSelectAt) {
    InputEvent event = PointerPressed{PointerButton::Left, 100, 100};
    auto       action = AdventureScreenInputHandler::handle(event);
    ASSERT_TRUE(action.has_value());
    const auto* select = std::get_if<AdventureSelectAt>(&*action);
    ASSERT_NE(select, nullptr);
    EXPECT_EQ(select->x, 100);
    EXPECT_EQ(select->y, 100);
}

TEST(AdventureScreenInput, LeftAndRightActionsAreDifferentTypes) {
    auto left = AdventureScreenInputHandler::handle(PointerPressed{PointerButton::Left, 0, 0});
    auto right = AdventureScreenInputHandler::handle(PointerPressed{PointerButton::Right, 0, 0});

    ASSERT_TRUE(left.has_value());
    ASSERT_TRUE(right.has_value());

    EXPECT_TRUE(std::holds_alternative<AdventureSelectAt>(*left));
    EXPECT_TRUE(std::holds_alternative<AdventureInspectAt>(*right));
    EXPECT_FALSE(std::holds_alternative<AdventureInspectAt>(*left));
    EXPECT_FALSE(std::holds_alternative<AdventureSelectAt>(*right));
}

TEST(AdventureScreenInput, PointerReleasedReturnsNone) {
    InputEvent event = PointerReleased{PointerButton::Right, 100, 100};
    auto       action = AdventureScreenInputHandler::handle(event);
    EXPECT_FALSE(action.has_value());
}

TEST(AdventureScreenInput, PointerMovedReturnsPointerAt) {
    InputEvent event = PointerMoved{200, 300};
    auto       action = AdventureScreenInputHandler::handle(event);
    ASSERT_TRUE(action.has_value());
    const auto* ptr = std::get_if<AdventurePointerAt>(&*action);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->x, 200);
    EXPECT_EQ(ptr->y, 300);
}

TEST(AdventureScreenInput, KeyReleasedReturnsNone) {
    InputEvent event = KeyReleased{Key::Escape};
    auto       action = AdventureScreenInputHandler::handle(event);
    EXPECT_FALSE(action.has_value());
}

TEST(AdventureScreenInput, UnknownKeyReturnsNone) {
    InputEvent event = KeyPressed{Key::Space};
    auto       action = AdventureScreenInputHandler::handle(event);
    EXPECT_FALSE(action.has_value());
}

TEST(AdventureScreenInput, EqualsReturnsZoomIn) {
    InputEvent event = KeyPressed{Key::Equals};
    auto       action = AdventureScreenInputHandler::handle(event);
    ASSERT_TRUE(action.has_value());
    EXPECT_TRUE(std::holds_alternative<AdventureZoomIn>(*action));
}

TEST(AdventureScreenInput, MinusReturnsZoomOut) {
    InputEvent event = KeyPressed{Key::Minus};
    auto       action = AdventureScreenInputHandler::handle(event);
    ASSERT_TRUE(action.has_value());
    EXPECT_TRUE(std::holds_alternative<AdventureZoomOut>(*action));
}

TEST(AdventureScreenInput, ArrowKeysStillPan) {
    // Zoom keys must not break existing pan bindings.
    EXPECT_TRUE(std::holds_alternative<AdventurePanCamera>(
        *AdventureScreenInputHandler::handle(KeyPressed{Key::Left})));
    EXPECT_TRUE(std::holds_alternative<AdventurePanCamera>(
        *AdventureScreenInputHandler::handle(KeyPressed{Key::Right})));
}

} // namespace d2engine
