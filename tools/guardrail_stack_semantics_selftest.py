#!/usr/bin/env python3
# require_make_target lint lint-fix
"""Real self-test for guardrail_stack_semantics.py.

Tests each check function with accepted and rejected fixtures.
Must be importable — the guardrail exposes check_*() functions.
"""

import sys
from pathlib import Path

TOOL_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOL_DIR))
import guardrail_stack_semantics as g  # noqa: E402

PASSED = 0
FAILED = 0


def ok(label: str) -> None:
    global PASSED
    print(f"  PASS: {label}")
    PASSED += 1


def fail(label: str, detail: str) -> None:
    global FAILED
    print(f"  FAIL: {label}: {detail}")
    FAILED += 1


# ── Fixtures for check_direct_uninverted_copy ────────────────────────────

accepted_inversion = """
for (std::size_t cell = 0; cell < st.positions.size(); ++cell) {
    const int mbr = st.positions[cell];
    if (mbr < 0) continue;
    if (stack.group.positions[static_cast<std::size_t>(mbr)] < 0) {
        stack.group.positions[static_cast<std::size_t>(mbr)] = static_cast<int>(cell);
    }
}
"""

rejected_direct_copy = """
for (std::size_t member_index = 0;
     member_index < st.positions.size(); ++member_index) {
    stack.group.positions[member_index] = st.positions[member_index];
}
"""

# ── Fixtures for check_sg_parser_loop_pairs_pos_with_unit ────────────────

accepted_separate_pos_pass = """
for (int i = 0; i < 6; ++i) {
    std::string key = "UNIT_" + std::to_string(i);
    std::string uid = read_string_field(rec, key);
    s.units.push_back(uid.empty() ? "G000000000" : uid);
}
for (int i = 0; i < 6; ++i) {
    std::string pos_key = "POS_" + std::to_string(i);
    s.positions.push_back(read_int_field(rec, pos_key));
}
"""

rejected_same_loop = """
for (int i = 0; i < 6; ++i) {
    std::string key = "UNIT_" + std::to_string(i);
    std::string uid = read_string_field(rec, key);
    s.units.push_back(uid.empty() ? "G000000000" : uid);
    std::string pos_key = "POS_" + std::to_string(i);
    s.positions.push_back(read_int_field(rec, pos_key));
}
"""

# ── Fixtures for check_large_formula ─────────────────────────────────────

accepted_large_formula = """
std::vector<int> derive_formation_cells(int anchor, bool is_large) {
    std::vector<int> cells;
    if (anchor < 0 || anchor > 5) return cells;
    if (is_large) {
        const int row_start = (anchor / 2) * 2;
        cells.push_back(row_start);
        cells.push_back(row_start + 1);
    } else {
        cells.push_back(anchor);
    }
    return cells;
}
"""

rejected_wrong_formula = """
std::vector<int> derive_formation_cells(int anchor, bool is_large) {
    const int row_start = (anchor / 3) * 3;
    cells.push_back(row_start);
}
"""

# ── Fixtures for check_magenta_key_on_pg ─────────────────────────────────

accepted_pg_none = """
plan.popup_background = {std::string(kInterfContainer), "_PG0500IX",
                         ImageAssetKind::ComposedSprite, ImagePostprocess::None};
"""

rejected_pg_magenta = """
plan.popup_background = {std::string(kInterfContainer), "_PG0500IX",
                         ImageAssetKind::ComposedSprite,
                         ImagePostprocess::MagentaKey};
"""

rejected_pg_magenta_multiline = """
plan.popup_background = {std::string(kInterfContainer),
                         "_PG0500IX",
                         ImageAssetKind::ComposedSprite,
                         ImagePostprocess::MagentaKey};
"""


def test_direct_uninverted_copy() -> None:
    v_acc = g.check_direct_uninverted_copy(accepted_inversion, "test")
    v_rej = g.check_direct_uninverted_copy(rejected_direct_copy, "test")

    if len(v_acc) == 0:
        ok("direct-uninverted-copy accepted fixture -> zero violations")
    else:
        fail("direct-uninverted-copy accepted fixture", f"expected 0, got {v_acc}")

    if len(v_rej) >= 1:
        ok("direct-uninverted-copy rejected fixture -> violations detected")
    else:
        fail("direct-uninverted-copy rejected fixture", "expected violations, got 0")


def test_sg_parser_pos_loop() -> None:
    v_acc = g.check_sg_parser_loop_pairs_pos_with_unit(accepted_separate_pos_pass, "test")
    v_rej = g.check_sg_parser_loop_pairs_pos_with_unit(rejected_same_loop, "test")

    if len(v_acc) == 0:
        ok("sg-parser-pos accepted fixture -> zero violations")
    else:
        fail("sg-parser-pos accepted fixture", f"expected 0, got {v_acc}")

    if len(v_rej) >= 1:
        ok("sg-parser-pos rejected fixture -> violations detected")
    else:
        fail("sg-parser-pos rejected fixture", "expected violations, got 0")


def test_large_formula() -> None:
    v_acc = g.check_large_formula(accepted_large_formula, "test")
    v_rej = g.check_large_formula(rejected_wrong_formula, "test")

    if len(v_acc) == 0:
        ok("large-formula accepted fixture -> zero violations")
    else:
        fail("large-formula accepted fixture", f"expected 0, got {v_acc}")

    if len(v_rej) >= 1:
        ok("large-formula rejected fixture -> violations detected")
    else:
        fail("large-formula rejected fixture", "expected violations, got 0")


def test_magenta_key() -> None:
    v_acc = g.check_magenta_key_on_pg(accepted_pg_none, "test")
    v_rej_single = g.check_magenta_key_on_pg(rejected_pg_magenta, "test")
    v_rej_multi = g.check_magenta_key_on_pg(rejected_pg_magenta_multiline, "test")

    if len(v_acc) == 0:
        ok("MagentaKey accepted fixture -> zero violations")
    else:
        fail("MagentaKey accepted fixture", f"expected 0, got {v_acc}")

    if len(v_rej_single) >= 1:
        ok("MagentaKey rejected fixture (single-line) -> violations detected")
    else:
        fail("MagentaKey rejected fixture", "expected violations, got 0")

    if len(v_rej_multi) >= 1:
        ok("MagentaKey rejected fixture (multi-line) -> violations detected")
    else:
        fail("MagentaKey rejected fixture (multi-line)", "expected violations, got 0")


def test_old_enum() -> None:
    v_acc = g.check_old_enum("BuildDiagnosticKind::InvalidFormationCell", "test")
    v_rej = g.check_old_enum("BuildDiagnosticKind::InvalidFormationMemberIndex", "test")

    if len(v_acc) == 0:
        ok("old-enum accepted fixture -> zero violations")
    else:
        fail("old-enum accepted fixture", f"expected 0, got {v_acc}")

    if len(v_rej) >= 1:
        ok("old-enum rejected fixture -> violations detected")
    else:
        fail("old-enum rejected fixture", "expected violations, got 0")


if __name__ == "__main__":
    test_direct_uninverted_copy()
    test_sg_parser_pos_loop()
    test_large_formula()
    test_magenta_key()
    test_old_enum()

    print()
    if FAILED == 0:
        print(f"guardrail_stack_semantics_selftest: ALL {PASSED} PASSED")
        sys.exit(0)
    else:
        print(f"guardrail_stack_semantics_selftest: {FAILED} FAILED, {PASSED} PASSED")
        sys.exit(1)
