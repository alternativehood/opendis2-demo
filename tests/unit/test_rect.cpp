#include <gtest/gtest.h>

#include "d2engine/render/rect.hpp"

namespace d2engine {

TEST(Rect, RightEdge) {
    const Rect r{.x = 10.0F, .y = 0.0F, .w = 100.0F, .h = 0.0F};
    EXPECT_FLOAT_EQ(r.right(), 110.0F);
}

TEST(Rect, BottomEdge) {
    const Rect r{.x = 0.0F, .y = 20.0F, .w = 0.0F, .h = 50.0F};
    EXPECT_FLOAT_EQ(r.bottom(), 70.0F);
}

TEST(Rect, ZeroSize) {
    const Rect r{.x = 5.0F, .y = 7.0F, .w = 0.0F, .h = 0.0F};
    EXPECT_FLOAT_EQ(r.right(), 5.0F);
    EXPECT_FLOAT_EQ(r.bottom(), 7.0F);
}

} // namespace d2engine
