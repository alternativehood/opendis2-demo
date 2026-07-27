#!/usr/bin/env bash
# guardrail_ui_layout_selftest.sh — verify guardrail catches regressions
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/_require_make.sh"
require_make_target lint lint-fix

TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

PASSED=0
FAILED=0

run_test() {
    local name="$1"
    local expected="$2"  # "pass" or "fail"
    shift 2

    local test_dir="$TMPDIR/$name"
    mkdir -p "$test_dir/src/d2engine/app"
    mkdir -p "$test_dir/configs/screens"

    "$@"   # execute the setup function

    if OUT=$(python3 "$SCRIPT_DIR/guardrail_ui_layout.py" --root "$test_dir" 2>&1); then
        if [[ "$expected" == "pass" ]]; then
            echo "  PASS: $name"
            PASSED=$((PASSED + 1))
        else
            echo "  FAIL: $name (expected failure, got pass)"
            FAILED=$((FAILED + 1))
        fi
    else
        if [[ "$expected" == "fail" ]]; then
            echo "  PASS: $name"
            PASSED=$((PASSED + 1))
        else
            echo "  FAIL: $name (expected pass, got: $OUT)"
            FAILED=$((FAILED + 1))
        fi
    fi
}

# ── Test A: Future Screen auto-discovered ──────────────────────────────────
setup_auto_discover() {
    cat > "$test_dir/src/d2engine/app/city_screen.hpp" <<'EOF'
#pragma once
#include <string_view>
#include <functional>
namespace d2engine {
class CityScreen : public Screen {
public:
    CityScreen(TreeLayout tl, std::string cs) : Screen(std::move(tl), std::move(cs)) {}
    std::string_view name() const override { return "CityScreen"; }
    void update(float) override {}
    void render(Renderer2D&) override {}
};
}
EOF
    cat > "$test_dir/src/d2engine/app/city_screen.cpp" <<'EOF'
#include "city_screen.hpp"
EOF
    echo '{}' > "$test_dir/configs/screens/city_screen.json"
}
run_test "auto_discover_screen" "pass" setup_auto_discover

# ── Test B: Wrong filename rejected ────────────────────────────────────────
setup_wrong_filename() {
    cat > "$test_dir/src/d2engine/app/city_view.hpp" <<'EOF'
#pragma once
class CityScreen : public Screen {
public:
    CityScreen(TreeLayout tl, std::string cs) : Screen(std::move(tl), std::move(cs)) {}
    std::string_view name() const override { return "CityScreen"; }
    void update(float) override {}
    void render(Renderer2D&) override {}
};
EOF
    echo "" > "$test_dir/src/d2engine/app/city_screen.cpp"
}
run_test "wrong_filename_rejected" "fail" setup_wrong_filename

# ── Test C: Direct Rect literal rejected ────────────────────────────────────
setup_direct_rect() {
    cat > "$test_dir/src/d2engine/app/stack_info_screen.hpp" <<'EOF'
#pragma once
class StackInfoScreen : public Screen {
public:
    StackInfoScreen(TreeLayout tl, std::string cs) : Screen(std::move(tl), std::move(cs)) {}
    std::string_view name() const override { return "StackInfoScreen"; }
    void update(float) override {}
    void render(Renderer2D&) override {}
};
EOF
    cat > "$test_dir/src/d2engine/app/stack_info_screen.cpp" <<'EOF'
#include "stack_info_screen.hpp"
void StackInfoScreen::render(Renderer2D& r) {
    Rect r{30, 40, 200, 100};
}
EOF
}
run_test "direct_rect_rejected" "fail" setup_direct_rect

# ── Test D: Manual UI arithmetic rejected ───────────────────────────────────
setup_manual_arithmetic() {
    cat > "$test_dir/src/d2engine/app/stack_info_screen.hpp" <<'EOF'
#pragma once
class StackInfoScreen : public Screen {
public:
    StackInfoScreen(TreeLayout tl, std::string cs) : Screen(std::move(tl), std::move(cs)) {}
    std::string_view name() const override { return "StackInfoScreen"; }
    void update(float) override {}
    void render(Renderer2D&) override {}
};
EOF
    cat > "$test_dir/src/d2engine/app/stack_info_screen.cpp" <<'EOF'
#include "stack_info_screen.hpp"
void StackInfoScreen::render(Renderer2D& r) {
    int x = origin_x + col * 90;
}
EOF
}
run_test "manual_arithmetic_rejected" "fail" setup_manual_arithmetic

# ── Test E: TreeLayout path usage accepted ──────────────────────────────────
setup_treelayout_path() {
    cat > "$test_dir/src/d2engine/app/stack_info_screen.hpp" <<'EOF'
#pragma once
class StackInfoScreen : public Screen {
public:
    StackInfoScreen(TreeLayout tl, std::string cs) : Screen(std::move(tl), std::move(cs)) {}
    std::string_view name() const override { return "StackInfoScreen"; }
    void update(float) override {}
    void render(Renderer2D&) override {}
};
EOF
    cat > "$test_dir/src/d2engine/app/stack_info_screen.cpp" <<'EOF'
#include "stack_info_screen.hpp"
void StackInfoScreen::render(Renderer2D& r) {
    const auto rect = layout_rect("/city/button");
}
EOF
    echo '{}' > "$test_dir/configs/screens/stack_info_screen.json"
}
run_test "treelayout_path_accepted" "pass" setup_treelayout_path

# ── Test F: Adventure world-space exception accepted ────────────────────────
setup_world_space() {
    cat > "$test_dir/src/d2engine/app/adventure_screen.hpp" <<'EOF'
#pragma once
class AdventureScreen : public Screen {
public:
    AdventureScreen(TreeLayout tl, std::string cs) : Screen(std::move(tl), std::move(cs)) {}
    std::string_view name() const override { return "AdventureScreen"; }
    void update(float) override {}
    void render(Renderer2D&) override {}
};
EOF
    cat > "$test_dir/src/d2engine/app/adventure_screen.cpp" <<'EOF'
#include "adventure_screen.hpp"
void AdventureScreen::render(Renderer2D& r) {
    int iso_x = tile_col * 32;
    int iso_y = tile_row * 16;
}
EOF
    echo '{}' > "$test_dir/configs/screens/adventure_screen.json"
}
run_test "worldspace_accepted" "pass" setup_world_space

# ── Test G: AdventureScreen manual UI Rect rejected (no whole-file exemption) ─
setup_adventure_ui_rect_bypass() {
    cat > "$test_dir/src/d2engine/app/adventure_screen.hpp" <<'EOF'
#pragma once
class AdventureScreen : public Screen {
public:
    AdventureScreen(TreeLayout tl, std::string cs) : Screen(std::move(tl), std::move(cs)) {}
    std::string_view name() const override { return "AdventureScreen"; }
    void update(float) override {}
    void render(Renderer2D&) override {}
};
EOF
    cat > "$test_dir/src/d2engine/app/adventure_screen.cpp" <<'EOF'
#include "adventure_screen.hpp"
void AdventureScreen::render(Renderer2D& r) {
    Rect ui_panel{30, 40, 200, 100};
}
EOF
}
run_test "adventure_ui_rect_rejected" "fail" setup_adventure_ui_rect_bypass

# ── Test H: Local-variable destination Rect bypass rejected ──────────────────
setup_local_var_rect_bypass() {
    cat > "$test_dir/src/d2engine/app/city_screen.hpp" <<'EOF'
#pragma once
class CityScreen : public Screen {
public:
    CityScreen(TreeLayout tl, std::string cs) : Screen(std::move(tl), std::move(cs)) {}
    std::string_view name() const override { return "CityScreen"; }
    void update(float) override {}
    void render(Renderer2D&) override {}
};
EOF
    cat > "$test_dir/src/d2engine/app/city_screen.cpp" <<'EOF'
#include "city_screen.hpp"
void CityScreen::render(Renderer2D& r) {
    int x = 30;
    int y = 40;
    int w = 200;
    int h = 100;
    renderer.draw_texture(nullptr, Rect{x, y, w, h});
}
EOF
}
run_test "local_var_rect_rejected" "fail" setup_local_var_rect_bypass

# ── Test I: Missing screen config rejected ───────────────────────────────────
setup_missing_config() {
    cat > "$test_dir/src/d2engine/app/city_screen.hpp" <<'EOF'
#pragma once
class CityScreen : public Screen {
public:
    CityScreen(TreeLayout tl, std::string cs) : Screen(std::move(tl), std::move(cs)) {}
    std::string_view name() const override { return "CityScreen"; }
    void update(float) override {}
    void render(Renderer2D&) override {}
};
EOF
    cat > "$test_dir/src/d2engine/app/city_screen.cpp" <<'EOF'
#include "city_screen.hpp"
void CityScreen::render(Renderer2D& r) {
    const auto rect = layout_rect("/city/button");
}
EOF
    # Do NOT create configs/screens/city_screen.json — should fail
}
run_test "missing_config_rejected" "fail" setup_missing_config

# ── Test J: Screen in .cpp file discovered and rejected ──────────────────────
setup_cpp_declaration() {
    cat > "$test_dir/src/d2engine/app/city_screen.hpp" <<'EOF'
#pragma once
EOF
    cat > "$test_dir/src/d2engine/app/city_screen.cpp" <<'EOF'
#include "city_screen.hpp"
class CityScreen : public Screen {
public:
    CityScreen(TreeLayout tl, std::string cs) : Screen(std::move(tl), std::move(cs)) {}
    std::string_view name() const override { return "CityScreen"; }
    void update(float) override {}
    void render(Renderer2D&) override {}
};
EOF
}
run_test "cpp_declaration_rejected" "fail" setup_cpp_declaration

# ── Test K: BattleScreen uses normal screen config convention ────────────────
setup_battle_unified() {
    cat > "$test_dir/src/d2engine/app/battle_screen.hpp" <<'EOF'
#pragma once
class BattleScreen : public Screen {
public:
    BattleScreen(TreeLayout tl, std::string cs) : Screen(std::move(tl), std::move(cs)) {}
    std::string_view name() const override { return "BattleScreen"; }
    void update(float) override {}
    void render(Renderer2D&) override {}
};
EOF
    cat > "$test_dir/src/d2engine/app/battle_screen.cpp" <<'EOF'
#include "battle_screen.hpp"
void BattleScreen::render(Renderer2D& r) {
    const auto rect = layout_rect("/battle/panel");
}
EOF
    echo '{"render_tree":{}}' > "$test_dir/configs/screens/battle_screen.json"
}
run_test "battle_unified_config" "pass" setup_battle_unified

# ── Test L: TreeLayout draw_texture pass-through accepted ────────────────────
setup_draw_texture_layout() {
    cat > "$test_dir/src/d2engine/app/city_screen.hpp" <<'EOF'
#pragma once
class CityScreen : public Screen {
public:
    CityScreen(TreeLayout tl, std::string cs) : Screen(std::move(tl), std::move(cs)) {}
    std::string_view name() const override { return "CityScreen"; }
    void update(float) override {}
    void render(Renderer2D&) override {}
};
EOF
    cat > "$test_dir/src/d2engine/app/city_screen.cpp" <<'EOF'
#include "city_screen.hpp"
void CityScreen::render(Renderer2D& r) {
    const auto dst = layout_rect("/city/panel");
    renderer.draw_texture(nullptr, dst);
}
EOF
    cat > "$test_dir/configs/screens/city_screen.json" <<'EOF'
{"/city/panel":{"x":0,"y":0,"w":200,"h":100}}
EOF
}
run_test "draw_texture_layout_accepted" "pass" setup_draw_texture_layout

echo ""
echo "guardrail_ui_layout_selftest: $PASSED passed, $FAILED failed"
if [[ $FAILED -gt 0 ]]; then
    exit 1
fi
exit 0
