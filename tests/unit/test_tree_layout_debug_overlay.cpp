#include <gtest/gtest.h>

#include "d2engine/app/tree_layout_debug_overlay.hpp"
#include "d2engine/app/tree_layout_editor.hpp"
#include "d2engine/app/screen_manager.hpp"
#include "d2engine/app/screen_config_store.hpp"
#include "d2engine/render/renderer2d.hpp"
#include "d2engine/render/color.hpp"
#include "d2engine/render/rect.hpp"

#include <cstdint>
#include <vector>

namespace d2engine {
namespace {

struct DrawRectCall {
    Rect  rect;
    Color color;
    bool  filled;
    bool  operator==(const DrawRectCall& o) const {
        return rect.x == o.rect.x && rect.y == o.rect.y && rect.w == o.rect.w &&
               rect.h == o.rect.h && color.r == o.color.r && color.g == o.color.g &&
               color.b == o.color.b && color.a == o.color.a && filled == o.filled;
    }
};

class MockOverlayRenderer2D final : public Renderer2D {
public:
    MockOverlayRenderer2D() : Renderer2D(nullptr) {}

    std::vector<DrawRectCall> rect_calls;

    void draw_rect(Rect rect, Color c, bool filled) override {
        rect_calls.push_back({rect, c, filled});
    }

    // Unused overrides
    void draw_texture(SDL_Texture*, Rect, float, bool, bool) override {}
    void draw_texture(SDL_Texture*, Rect, Rect, float, bool, bool) override {}
    void draw_texture_rotated(SDL_Texture*, Rect, float, bool, bool, double, float,
                              float) override {}
};

struct TestScreen : public Screen {
    explicit TestScreen(TreeLayout tree) : Screen(std::move(tree), "test://overlay-screen") {}
    std::string_view name() const override { return "TestScreen"; }
    void             update(const d2::app::ScreenUpdateContext&) override {}
    void             render(Renderer2D&) override {}
};

struct OverlayFixture {
    ScreenManager     manager;
    ScreenConfigStore config_store{"."};
    TreeLayoutEditor  editor{manager, config_store};

    TreeLayout make_tree() {
        TreeLayout t;
        t.set_node("/root", TreeNode{.kind = "container", .x = 100, .y = 50, .w = 400, .h = 300});
        t.set_node("/root/child", TreeNode{.kind = "widget", .x = 20, .y = 30, .w = 80, .h = 40});
        return t;
    }

    void install() { manager.switch_to(std::make_unique<TestScreen>(make_tree())); }
};

TEST(TreeLayoutDebugOverlay, NoSelectedNodeDrawsNothing) {
    OverlayFixture fx;
    fx.install();
    MockOverlayRenderer2D r;
    render_tree_layout_selection_outline(r, fx.editor);
    EXPECT_TRUE(r.rect_calls.empty());
}

TEST(TreeLayoutDebugOverlay, ValidNodeDrawsOuterAndInnerOutline) {
    OverlayFixture fx;
    fx.install();
    MockOverlayRenderer2D r;
    ASSERT_TRUE(fx.editor.select_node("/root/child"));
    render_tree_layout_selection_outline(r, fx.editor);

    ASSERT_EQ(r.rect_calls.size(), 2u);

    // Outer rect at composed position: parent(100,50) + child(20,30) = (120, 80), w=80, h=40
    EXPECT_FLOAT_EQ(r.rect_calls[0].rect.x, 120.0f);
    EXPECT_FLOAT_EQ(r.rect_calls[0].rect.y, 80.0f);
    EXPECT_FLOAT_EQ(r.rect_calls[0].rect.w, 80.0f);
    EXPECT_FLOAT_EQ(r.rect_calls[0].rect.h, 40.0f);
    EXPECT_EQ(r.rect_calls[0].color.r, 255);
    EXPECT_EQ(r.rect_calls[0].color.g, 220);
    EXPECT_EQ(r.rect_calls[0].color.b, 0);
    EXPECT_EQ(r.rect_calls[0].color.a, 255);
    EXPECT_FALSE(r.rect_calls[0].filled);

    // Inner rect at (121, 81, 78, 38)
    EXPECT_FLOAT_EQ(r.rect_calls[1].rect.x, 121.0f);
    EXPECT_FLOAT_EQ(r.rect_calls[1].rect.y, 81.0f);
    EXPECT_FLOAT_EQ(r.rect_calls[1].rect.w, 78.0f);
    EXPECT_FLOAT_EQ(r.rect_calls[1].rect.h, 38.0f);
    EXPECT_EQ(r.rect_calls[1].color.r, 255);
    EXPECT_EQ(r.rect_calls[1].color.g, 220);
    EXPECT_EQ(r.rect_calls[1].color.b, 0);
    EXPECT_EQ(r.rect_calls[1].color.a, 255);
    EXPECT_FALSE(r.rect_calls[1].filled);
}

TEST(TreeLayoutDebugOverlay, ZeroSizeRectDrawsNothing) {
    OverlayFixture fx;
    fx.install();
    MockOverlayRenderer2D r;
    // Select node and shrink to zero
    ASSERT_TRUE(fx.editor.select_node("/root/child"));
    TreeNode u = TreeNode{.kind = "widget", .x = 0, .y = 0, .w = 0, .h = 0};
    ASSERT_TRUE(fx.editor.update_selected_node(u));
    render_tree_layout_selection_outline(r, fx.editor);
    EXPECT_TRUE(r.rect_calls.empty());
}

TEST(TreeLayoutDebugOverlay, TinyRectDrawsOnlyOuterOutline) {
    OverlayFixture fx;
    fx.install();
    MockOverlayRenderer2D r;
    ASSERT_TRUE(fx.editor.select_node("/root/child"));
    // Make node 1x1 pixel — too small for inner outline
    TreeNode u = TreeNode{.kind = "widget", .x = 20, .y = 30, .w = 1, .h = 1};
    ASSERT_TRUE(fx.editor.update_selected_node(u));
    render_tree_layout_selection_outline(r, fx.editor);
    ASSERT_EQ(r.rect_calls.size(), 1u);
    EXPECT_FLOAT_EQ(r.rect_calls[0].rect.w, 1.0f);
    EXPECT_FLOAT_EQ(r.rect_calls[0].rect.h, 1.0f);
    EXPECT_FALSE(r.rect_calls[0].filled);
}

} // namespace
} // namespace d2engine
