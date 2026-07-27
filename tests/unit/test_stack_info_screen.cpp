#include <gtest/gtest.h>

#include "d2engine/app/stack_info_screen_input.hpp"

using namespace d2engine;

TEST(StackInfoScreenInput, EscapeProducesCancel) {
    InputEvent event = KeyPressed{Key::Escape};
    auto       action = StackInfoScreenInputHandler::handle(event);
    ASSERT_TRUE(action.has_value());
    EXPECT_TRUE(std::get_if<StackInfoCancel>(&*action));
}

TEST(StackInfoScreenInput, RightClickDoesNotCancel) {
    InputEvent event = PointerPressed{PointerButton::Right, 100, 200};
    auto       action = StackInfoScreenInputHandler::handle(event);
    EXPECT_FALSE(action.has_value());
}

TEST(StackInfoScreenInput, LeftClickDoesNotCancel) {
    InputEvent event = PointerPressed{PointerButton::Left, 50, 75};
    auto       action = StackInfoScreenInputHandler::handle(event);
    EXPECT_FALSE(action.has_value());
}

TEST(StackInfoScreenInput, SpaceDoesNotCancel) {
    InputEvent event = KeyPressed{Key::Space};
    auto       action = StackInfoScreenInputHandler::handle(event);
    EXPECT_FALSE(action.has_value());
}

TEST(StackInfoScreenInput, PointerMovedDoesNotCancel) {
    InputEvent event = PointerMoved{10, 20};
    auto       action = StackInfoScreenInputHandler::handle(event);
    EXPECT_FALSE(action.has_value());
}
