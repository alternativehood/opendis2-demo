#include "d2engine/input/input_event.hpp"

#include <gtest/gtest.h>

namespace d2engine {
namespace {

TEST(DebugInputCapture, HiddenDebugUiForwardsPointerEvent) {
    EXPECT_TRUE(should_forward_to_screen(PointerMoved{10, 20}, false, false, false));
    EXPECT_TRUE(
        should_forward_to_screen(PointerPressed{PointerButton::Left, 10, 20}, false, false, false));
    EXPECT_TRUE(should_forward_to_screen(PointerReleased{PointerButton::Left, 10, 20}, false, false,
                                         false));
}

TEST(DebugInputCapture, HiddenDebugUiForwardsKeyboardEvent) {
    EXPECT_TRUE(should_forward_to_screen(KeyPressed{Key::Space}, false, false, false));
    EXPECT_TRUE(should_forward_to_screen(KeyReleased{Key::Space}, false, false, false));
}

TEST(DebugInputCapture, VisibleDebugUiWantsMouseBlocksPointerEvent) {
    EXPECT_FALSE(should_forward_to_screen(PointerMoved{10, 20}, true, true, false));
    EXPECT_FALSE(
        should_forward_to_screen(PointerPressed{PointerButton::Left, 10, 20}, true, true, false));
    EXPECT_FALSE(
        should_forward_to_screen(PointerReleased{PointerButton::Left, 10, 20}, true, true, false));
}

TEST(DebugInputCapture, VisibleDebugUiWantsKeyboardBlocksKeyboardEvent) {
    EXPECT_FALSE(should_forward_to_screen(KeyPressed{Key::Space}, true, false, true));
    EXPECT_FALSE(should_forward_to_screen(KeyReleased{Key::Space}, true, false, true));
}

TEST(DebugInputCapture, VisibleDebugUiWantsMouseDoesNotBlockKeyboardEvent) {
    EXPECT_TRUE(should_forward_to_screen(KeyPressed{Key::Space}, true, true, false));
}

TEST(DebugInputCapture, VisibleDebugUiWantsKeyboardDoesNotBlockPointerEvent) {
    EXPECT_TRUE(should_forward_to_screen(PointerMoved{10, 20}, true, false, true));
}

TEST(DebugInputCapture, VisibleDebugUiNoCaptureForwardsEverything) {
    EXPECT_TRUE(should_forward_to_screen(PointerMoved{10, 20}, true, false, false));
    EXPECT_TRUE(should_forward_to_screen(KeyPressed{Key::Space}, true, false, false));
}

TEST(DebugInputCapture, DebugUiHiddenIgnoresCaptureFlags) {
    EXPECT_TRUE(should_forward_to_screen(PointerMoved{10, 20}, false, true, true));
    EXPECT_TRUE(should_forward_to_screen(KeyPressed{Key::Space}, false, true, true));
}

} // namespace
} // namespace d2engine