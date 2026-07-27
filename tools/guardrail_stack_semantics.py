#!/usr/bin/env python3
# require_make_target lint lint-fix
"""Guardrail: enforce stack-position semantics, large-unit formula, and MagentaKey policy.

SG POS_n fields are cell-indexed: POS[formation_cell] = member_index.
The runtime must invert this to derive member→cell convenience views.
This guardrail detects violations of these semantics.

Exit 1 on any violation, 0 on pass.
"""

import re
import sys
from pathlib import Path
from typing import Iterable

REPO_ROOT = Path(__file__).resolve().parent.parent
SRC_DIR = REPO_ROOT / "src"

# ---------------------------------------------------------------------------
# Checks that can be called with supplied text (used by selftest)
# ---------------------------------------------------------------------------


def check_direct_uninverted_copy(text: str, path: str = "") -> list[str]:
    """Detect direct non-inverted copy from cell-indexed SG positions
    to runtime member-indexed positions without inversion.

    The bad pattern:
      stack.group.positions[member_index] = st.positions[member_index];
    or
      positions[idx] = st.positions[idx];
    """
    violations = []
    lines = text.splitlines()
    for i, line in enumerate(lines, 1):
        stripped = line.strip()
        # Match patterns like positions[X] = st.positions[X] where X is any variable
        m = re.search(
            r"\.positions\[(\w+)\]\s*=\s*st\.positions\[(\w+)\]", stripped
        )
        if m and m.group(1) == m.group(2):
            violations.append(
                f"{path}:{i}: Direct uninverted copy: positions[{m.group(1)}] = "
                f"st.positions[{m.group(2)}]. "
                f"POS_n is cell-indexed; must invert to derive member->cell mapping."
            )
    return violations


def check_sg_parser_loop_pairs_pos_with_unit(text: str, path: str = "") -> list[str]:
    """Detect the old pattern where POS_n is pushed to a positions vector
    in the same loop iteration as UNIT_n.

    POS_n must be read as a separate cell-indexed pass.
    Only checks for positions.push_back patterns (not per-unit position fields in templates).
    """
    violations = []
    lines = text.splitlines()

    for i, line in enumerate(lines):
        stripped = line.strip()
        if re.search(r"for\s*\(.*\bi\s*=.*\bi\s*<\s*6", stripped):
            # Find the matching close brace for this for-loop
            brace_count = 0
            found_open = False
            loop_body_lines = []
            for j in range(i, len(lines)):
                s = lines[j]
                brace_count += s.count("{") - s.count("}")
                if "{" in s:
                    found_open = True
                loop_body_lines.append(j)
                if found_open and brace_count <= 0:
                    break
            # Check if this loop body contains UNIT_, POS_, and positions.push_back
            has_unit = any("UNIT_" in lines[l] for l in loop_body_lines)
            has_pos = any("POS_" in lines[l] for l in loop_body_lines)
            has_push = any("positions.push_back" in lines[l] for l in loop_body_lines)
            if has_unit and has_pos and has_push:
                violations.append(
                    f"{path}:{i + 1}: POS_n pushed to positions vector in same loop block "
                    f"as UNIT_n. POS_n is cell-indexed; read it in a separate pass after all "
                    f"UNIT_n entries."
                )
    return violations


def check_large_formula(text: str, path: str = "") -> list[str]:
    """Reject (anchor / 3) * 3 in derive_formation_cells."""
    violations = []
    lines = text.splitlines()
    for i, line in enumerate(lines, 1):
        if "/ 3) * 3" in line and "anchor" in line.lower():
            violations.append(
                f"{path}:{i}: Wrong large-unit formula: (anchor / 3) * 3. "
                f"Must use (anchor / 2) * 2."
            )
    return violations


def check_magenta_key_on_pg(text: str, path: str = "") -> list[str]:
    """Detect MagentaKey on _PG0500IX, including multi-line initializers."""
    violations = []
    lines = text.splitlines()
    for li, line in enumerate(lines):
        if "_PG0500IX" not in line:
            continue
        # Same line
        if "MagentaKey" in line:
            violations.append(
                f"{path}:{li + 1}: _PG0500IX associated with MagentaKey. "
                f"Must use ImagePostprocess::None."
            )
            continue
        # Multi-line: check up to 5 lines after _PG0500IX
        for offset in range(1, 6):
            nli = li + offset
            if nli < len(lines) and "MagentaKey" in lines[nli]:
                violations.append(
                    f"{path}:{li + 1}: _PG0500IX associated with MagentaKey "
                    f"(multi-line at line {nli + 1}). "
                    f"Must use ImagePostprocess::None."
                )
                break
    return violations


def check_old_enum(text: str, path: str = "") -> list[str]:
    """Detect InvalidFormationMemberIndex (removed enum value)."""
    violations = []
    for i, line in enumerate(text.splitlines(), 1):
        if "InvalidFormationMemberIndex" in line:
            violations.append(
                f"{path}:{i}: References removed enum InvalidFormationMemberIndex. "
                f"Use InvalidFormationCell instead."
            )
    return violations


# ---------------------------------------------------------------------------
# Main runner — scans the source tree
# ---------------------------------------------------------------------------


def _paths_to_scan() -> Iterable[Path]:
    for p in sorted(SRC_DIR.rglob("*")):
        if not p.is_file():
            continue
        if p.suffix in (".cpp", ".hpp"):
            yield p


def main() -> int:
    violations: list[str] = []

    for path in _paths_to_scan():
        text = path.read_text(encoding="utf-8")
        fname = str(path)

        # Direct uninverted copy — only on AdventureWorldBuilder.cpp
        if path.name == "AdventureWorldBuilder.cpp":
            violations.extend(check_direct_uninverted_copy(text, fname))

        # SG parser POS-in-unit-loop — only on SgParser.cpp
        if path.name == "SgParser.cpp":
            violations.extend(check_sg_parser_loop_pairs_pos_with_unit(text, fname))

        # Large-unit formula — only on stack_inspection.cpp
        if path.name == "stack_inspection.cpp":
            violations.extend(check_large_formula(text, fname))

        # MagentaKey — only on the asset plan source
        if path.name == "stack_info_asset_plan.cpp":
            violations.extend(check_magenta_key_on_pg(text, fname))

        violations.extend(check_old_enum(text, fname))

    if violations:
        print("STACK-SEMANTICS VIOLATIONS:", file=sys.stderr)
        for v in violations:
            print(f"  {v}", file=sys.stderr)
        return 1

    print("guardrail_stack_semantics: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
