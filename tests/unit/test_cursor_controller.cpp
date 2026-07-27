#include <gtest/gtest.h>

#include "d2engine/app/cursor_controller.hpp"

#include <SDL3/SDL.h>

#include <string>

namespace d2engine {
namespace {

class CursorControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            GTEST_SKIP() << "SDL_Init failed: " << SDL_GetError();
        }
        default_cursor_ = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
        select_unit_cursor_ = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR);
        if (!default_cursor_ || !select_unit_cursor_) {
            GTEST_SKIP() << "SDL_CreateSystemCursor failed";
        }
    }

    void TearDown() override {
        controller_.deactivate();
        if (select_unit_cursor_) {
            SDL_DestroyCursor(select_unit_cursor_);
            select_unit_cursor_ = nullptr;
        }
        if (default_cursor_) {
            SDL_DestroyCursor(default_cursor_);
            default_cursor_ = nullptr;
        }
        SDL_Quit();
    }

    CursorController controller_;
    SDL_Cursor*      default_cursor_ = nullptr;
    SDL_Cursor*      select_unit_cursor_ = nullptr;
};

TEST_F(CursorControllerTest, ActivationFailsWithoutCursors) {
    CursorController ctl;
    EXPECT_FALSE(ctl.activate());
    EXPECT_FALSE(ctl.current_kind().has_value());
}

TEST_F(CursorControllerTest, ActivationSucceedsWithCursors) {
    controller_.set_cursors(default_cursor_, select_unit_cursor_);
    EXPECT_TRUE(controller_.activate());
}

TEST_F(CursorControllerTest, ActivationSetsDefaultKind) {
    controller_.set_cursors(default_cursor_, select_unit_cursor_);
    ASSERT_TRUE(controller_.activate());
    ASSERT_TRUE(controller_.current_kind().has_value());
    EXPECT_EQ(*controller_.current_kind(), CursorKind::Default);
}

TEST_F(CursorControllerTest, ActivationIsIdempotent) {
    controller_.set_cursors(default_cursor_, select_unit_cursor_);
    EXPECT_TRUE(controller_.activate());
    EXPECT_TRUE(controller_.activate());
}

TEST_F(CursorControllerTest, SetKindSelectUnitChangesState) {
    controller_.set_cursors(default_cursor_, select_unit_cursor_);
    ASSERT_TRUE(controller_.activate());
    ASSERT_EQ(*controller_.current_kind(), CursorKind::Default);

    controller_.set_kind(CursorKind::SelectUnit);
    ASSERT_TRUE(controller_.current_kind().has_value());
    EXPECT_EQ(*controller_.current_kind(), CursorKind::SelectUnit);
}

TEST_F(CursorControllerTest, SetKindDefaultChangesBack) {
    controller_.set_cursors(default_cursor_, select_unit_cursor_);
    ASSERT_TRUE(controller_.activate());
    controller_.set_kind(CursorKind::SelectUnit);
    ASSERT_EQ(*controller_.current_kind(), CursorKind::SelectUnit);

    controller_.set_kind(CursorKind::Default);
    ASSERT_TRUE(controller_.current_kind().has_value());
    EXPECT_EQ(*controller_.current_kind(), CursorKind::Default);
}

TEST_F(CursorControllerTest, SetKindSameKindIsNoOp) {
    controller_.set_cursors(default_cursor_, select_unit_cursor_);
    ASSERT_TRUE(controller_.activate());

    controller_.set_kind(CursorKind::SelectUnit);
    ASSERT_EQ(*controller_.current_kind(), CursorKind::SelectUnit);

    controller_.set_kind(CursorKind::SelectUnit);
    EXPECT_EQ(*controller_.current_kind(), CursorKind::SelectUnit);

    controller_.set_kind(CursorKind::Default);
    ASSERT_EQ(*controller_.current_kind(), CursorKind::Default);

    controller_.set_kind(CursorKind::Default);
    EXPECT_EQ(*controller_.current_kind(), CursorKind::Default);
}

TEST_F(CursorControllerTest, SetKindNotActiveNoOp) {
    controller_.set_cursors(default_cursor_, select_unit_cursor_);
    EXPECT_FALSE(controller_.current_kind().has_value());

    controller_.set_kind(CursorKind::SelectUnit);
    EXPECT_FALSE(controller_.current_kind().has_value());
}

TEST_F(CursorControllerTest, DeactivateResetsState) {
    controller_.set_cursors(default_cursor_, select_unit_cursor_);
    ASSERT_TRUE(controller_.activate());
    controller_.set_kind(CursorKind::SelectUnit);
    ASSERT_EQ(*controller_.current_kind(), CursorKind::SelectUnit);

    controller_.deactivate();
    EXPECT_FALSE(controller_.current_kind().has_value());
}

TEST_F(CursorControllerTest, ReActivationForceAppliesDefault) {
    controller_.set_cursors(default_cursor_, select_unit_cursor_);
    ASSERT_TRUE(controller_.activate());
    controller_.set_kind(CursorKind::SelectUnit);
    ASSERT_EQ(*controller_.current_kind(), CursorKind::SelectUnit);

    controller_.deactivate();
    ASSERT_FALSE(controller_.current_kind().has_value());

    ASSERT_TRUE(controller_.activate());
    ASSERT_TRUE(controller_.current_kind().has_value());
    EXPECT_EQ(*controller_.current_kind(), CursorKind::Default);
}

} // namespace
} // namespace d2engine
