#include <gtest/gtest.h>

#include "d2engine/render/camera2d.hpp"

namespace d2engine {

TEST(Camera2D, IdentityScale) {
    const Camera2D cam{1.0F};
    const Rect     logical{.x = 10.0F, .y = 20.0F, .w = 100.0F, .h = 50.0F};
    const Rect     screen = cam.to_screen(logical);
    EXPECT_FLOAT_EQ(screen.x, 10.0F);
    EXPECT_FLOAT_EQ(screen.y, 20.0F);
    EXPECT_FLOAT_EQ(screen.w, 100.0F);
    EXPECT_FLOAT_EQ(screen.h, 50.0F);
}

TEST(Camera2D, DoubleScale) {
    const Camera2D cam{2.0F};
    const Rect     logical{.x = 10.0F, .y = 20.0F, .w = 100.0F, .h = 50.0F};
    const Rect     screen = cam.to_screen(logical);
    EXPECT_FLOAT_EQ(screen.x, 20.0F);
    EXPECT_FLOAT_EQ(screen.y, 40.0F);
    EXPECT_FLOAT_EQ(screen.w, 200.0F);
    EXPECT_FLOAT_EQ(screen.h, 100.0F);
}

} // namespace d2engine
