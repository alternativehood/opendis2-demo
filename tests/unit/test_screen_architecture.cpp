#include <gtest/gtest.h>

#include "d2engine/app/screen.hpp"
#include "d2engine/render/render_tree.hpp"

#include <nlohmann/json.hpp>

#include <cmath>
#include <fstream>

namespace d2engine {

TEST(ScreenArchitecture, ConstructorRequiresTreeLayout) {
    nlohmann::json j;
    j["/root"] = nlohmann::json::object();
    TreeLayout tree;
    tree.load(j);

    // For cppcheck: validate that tree_layout() and config_source() accessors work
    class TestUiScreen : public Screen {
    public:
        TestUiScreen(TreeLayout tl, std::string cs) : Screen(std::move(tl), std::move(cs)) {}
        std::string_view name() const override { return "test_ui"; }
        void             update(const d2::app::ScreenUpdateContext&) override {}
        void             render(Renderer2D&) override {}
    };

    TestUiScreen screen(std::move(tree), "configs/screens/test.json");
    EXPECT_FALSE(screen.tree_layout().empty());
    EXPECT_EQ(screen.config_source(), "configs/screens/test.json");
}

TEST(ScreenArchitecture, LayoutRectUsesCompose) {
    nlohmann::json j;
    j["/screen"] = {{"x", 10}, {"y", 20}, {"w", 100}, {"h", 50}};
    TreeLayout tree;
    tree.load(j);

    class TestUiScreen : public Screen {
    public:
        TestUiScreen(TreeLayout tl) : Screen(std::move(tl), "test://screen-layout") {}
        std::string_view name() const override { return "test"; }
        void             update(const d2::app::ScreenUpdateContext&) override {}
        void             render(Renderer2D&) override {}
    };

    TestUiScreen screen(std::move(tree));
    auto         rect = screen.layout_rect("/screen");
    EXPECT_FLOAT_EQ(rect.x, 10.0f);
    EXPECT_FLOAT_EQ(rect.y, 20.0f);
    EXPECT_FLOAT_EQ(rect.w, 100.0f);
    EXPECT_FLOAT_EQ(rect.h, 50.0f);
}

TEST(ScreenArchitecture, ParentCompositionMovesChildren) {
    nlohmann::json j;
    j["/panel"] = {{"x", 5}, {"y", 10}, {"w", 200}, {"h", 300}};
    j["/panel/child"] = {{"x", 3}, {"y", 4}, {"w", 50}, {"h", 40}};
    TreeLayout tree;
    tree.load(j);

    class TestUiScreen : public Screen {
    public:
        TestUiScreen(TreeLayout tl) : Screen(std::move(tl), "test://screen-layout") {}
        std::string_view name() const override { return "test"; }
        void             update(const d2::app::ScreenUpdateContext&) override {}
        void             render(Renderer2D&) override {}
    };

    TestUiScreen screen(std::move(tree));
    auto         rect = screen.layout_rect("/panel/child");
    EXPECT_FLOAT_EQ(rect.x, 8.0f);  // 5 + 3
    EXPECT_FLOAT_EQ(rect.y, 14.0f); // 10 + 4
    EXPECT_FLOAT_EQ(rect.w, 50.0f); // deepest w
    EXPECT_FLOAT_EQ(rect.h, 40.0f); // deepest h
}

TEST(ScreenArchitecture, StackInfoConfigHasAllLargeRowNodes) {
    std::ifstream file(std::string(OPENDIS2_SOURCE_DIR) +
                       "/configs/screens/stack_info_screen.json");
    ASSERT_TRUE(file.is_open());
    nlohmann::json j;
    file >> j;
    ASSERT_TRUE(j.contains("render_tree")) << "Config must have 'render_tree' key";
    TreeLayout tree;
    tree.load(j["render_tree"]);
    EXPECT_TRUE(tree.has_node("/stack_info/formation/large_row_0"));
    EXPECT_TRUE(tree.has_node("/stack_info/formation/large_row_0/portrait"));
    EXPECT_TRUE(tree.has_node("/stack_info/formation/large_row_0/name"));
    EXPECT_TRUE(tree.has_node("/stack_info/formation/large_row_1"));
    EXPECT_TRUE(tree.has_node("/stack_info/formation/large_row_1/portrait"));
    EXPECT_TRUE(tree.has_node("/stack_info/formation/large_row_1/name"));
    EXPECT_TRUE(tree.has_node("/stack_info/formation/large_row_2"));
    EXPECT_TRUE(tree.has_node("/stack_info/formation/large_row_2/portrait"));
    EXPECT_TRUE(tree.has_node("/stack_info/formation/large_row_2/name"));
}

TEST(ScreenArchitecture, StackInfoSlotsHorizontallySwappedColumns) {
    std::ifstream file(std::string(OPENDIS2_SOURCE_DIR) +
                       "/configs/screens/stack_info_screen.json");
    ASSERT_TRUE(file.is_open());
    nlohmann::json j;
    file >> j;
    ASSERT_TRUE(j.contains("render_tree"));
    TreeLayout tree;
    tree.load(j["render_tree"]);

    auto r0 = tree.compose("/stack_info/formation/slot_0");
    auto r1 = tree.compose("/stack_info/formation/slot_1");
    auto r2 = tree.compose("/stack_info/formation/slot_2");
    auto r3 = tree.compose("/stack_info/formation/slot_3");
    auto r4 = tree.compose("/stack_info/formation/slot_4");
    auto r5 = tree.compose("/stack_info/formation/slot_5");

    // Cell 0 is the right column, cell 1 is the left column
    EXPECT_GT(r0.x, r1.x) << "slot_0 must be right of slot_1";
    EXPECT_GT(r2.x, r3.x) << "slot_2 must be right of slot_3";
    EXPECT_GT(r4.x, r5.x) << "slot_4 must be right of slot_5";

    // Same row pairs are at (very nearly) the same y
    const auto y_row0 = (r0.y + r1.y) / 2.0f;
    const auto y_row1 = (r2.y + r3.y) / 2.0f;
    const auto y_row2 = (r4.y + r5.y) / 2.0f;
    EXPECT_LT(std::abs(r0.y - r1.y), 2) << "slot_0 and slot_1 must share y for top row";
    EXPECT_LT(std::abs(r2.y - r3.y), 2) << "slot_2 and slot_3 must share y for middle row";
    EXPECT_LT(std::abs(r4.y - r5.y), 2) << "slot_4 and slot_5 must share y for bottom row";

    // Rows are vertically separated
    EXPECT_LT(y_row0, y_row1) << "top row must be above middle row";
    EXPECT_LT(y_row1, y_row2) << "middle row must be above bottom row";
}

TEST(ScreenArchitecture, StackInfoLargeRowsSpanBothColumns) {
    std::ifstream file(std::string(OPENDIS2_SOURCE_DIR) +
                       "/configs/screens/stack_info_screen.json");
    ASSERT_TRUE(file.is_open());
    nlohmann::json j;
    file >> j;
    ASSERT_TRUE(j.contains("render_tree"));
    TreeLayout tree;
    tree.load(j["render_tree"]);

    auto s0 = tree.compose("/stack_info/formation/slot_0");
    auto s1 = tree.compose("/stack_info/formation/slot_1");
    auto s4 = tree.compose("/stack_info/formation/slot_4");
    auto s5 = tree.compose("/stack_info/formation/slot_5");

    auto lr0 = tree.compose("/stack_info/formation/large_row_0");
    auto lr1 = tree.compose("/stack_info/formation/large_row_1");
    auto lr2 = tree.compose("/stack_info/formation/large_row_2");

    // Large rows use verified 164x99 frame, centered between the two columns
    const auto col_left = std::min(s1.x, s5.x);
    const auto col_right = std::max(s0.x + s0.w, s4.x + s4.w);
    const auto two_col_center = (col_left + col_right) / 2;
    const auto lr_center = lr0.x + lr0.w / 2;

    EXPECT_EQ(lr0.w, 164);
    EXPECT_EQ(lr0.h, 99);
    EXPECT_LT(std::abs(lr_center - two_col_center), 2) << "large row must be centered";
    EXPECT_EQ(lr1.w, 164);
    EXPECT_EQ(lr2.w, 164);
}

TEST(ScreenArchitecture, StackInfoSlotPortraitInsetInsideFrame) {
    std::ifstream file(std::string(OPENDIS2_SOURCE_DIR) +
                       "/configs/screens/stack_info_screen.json");
    ASSERT_TRUE(file.is_open());
    nlohmann::json j;
    file >> j;
    ASSERT_TRUE(j.contains("render_tree"));
    TreeLayout tree;
    tree.load(j["render_tree"]);

    auto f0 = tree.compose("/stack_info/formation/slot_0/frame");
    auto p0 = tree.compose("/stack_info/formation/slot_0/portrait");

    // Frame is 84x99
    EXPECT_EQ(f0.w, 84);
    EXPECT_EQ(f0.h, 99);

    // Portrait is 70x85, inset 7px
    EXPECT_EQ(p0.x - f0.x, 7);
    EXPECT_EQ(p0.y - f0.y, 7);
    EXPECT_EQ(p0.w, 70);
    EXPECT_EQ(p0.h, 85);
}

TEST(ScreenArchitecture, StackInfoLargeRowPortraitInsetInsideFrame) {
    std::ifstream file(std::string(OPENDIS2_SOURCE_DIR) +
                       "/configs/screens/stack_info_screen.json");
    ASSERT_TRUE(file.is_open());
    nlohmann::json j;
    file >> j;
    ASSERT_TRUE(j.contains("render_tree"));
    TreeLayout tree;
    tree.load(j["render_tree"]);

    auto f0 = tree.compose("/stack_info/formation/large_row_0/frame");
    auto p0 = tree.compose("/stack_info/formation/large_row_0/portrait");

    // Frame is 164x99
    EXPECT_EQ(f0.w, 164);
    EXPECT_EQ(f0.h, 99);

    // Portrait is 150x85, inset 7px
    EXPECT_EQ(p0.x - f0.x, 7);
    EXPECT_EQ(p0.y - f0.y, 7);
    EXPECT_EQ(p0.w, 150);
    EXPECT_EQ(p0.h, 85);
}

TEST(ScreenArchitecture, StackInfoLeaderTextNodesExist) {
    std::ifstream file(std::string(OPENDIS2_SOURCE_DIR) +
                       "/configs/screens/stack_info_screen.json");
    ASSERT_TRUE(file.is_open());
    nlohmann::json j;
    file >> j;
    ASSERT_TRUE(j.contains("render_tree"));
    TreeLayout tree;
    tree.load(j["render_tree"]);

    EXPECT_TRUE(tree.has_node("/stack_info/leader/name"));
    EXPECT_TRUE(tree.has_node("/stack_info/leader/race"));
    EXPECT_TRUE(tree.has_node("/stack_info/leader/battles_won"));
    EXPECT_TRUE(tree.has_node("/stack_info/leader/portrait"));

    auto portrait = tree.compose("/stack_info/leader/portrait");
    auto name = tree.compose("/stack_info/leader/name");
    auto race = tree.compose("/stack_info/leader/race");
    auto battles = tree.compose("/stack_info/leader/battles_won");

    // Text regions must be near the bottom of the leader portrait area
    EXPECT_GT(name.y, portrait.y + portrait.h - 20) << "name must be near/below portrait";
    EXPECT_GT(race.y, name.y + name.h - 4) << "race must be near/below name";
    EXPECT_GT(battles.y, race.y + race.h - 4) << "battles must be near/below race";
}

} // namespace d2engine
