#!/usr/bin/env python3
"""Filesystem path guardrail.

Ensures physical filesystem paths use std::filesystem::path and standard
filesystem operations. Manual string-based physical path construction,
separator replacement, and parsing are forbidden.

Logical asset IDs (e.g. imgs/grborder.ff, render/tree/node) are exempt.

Exit code 0 if clean, 1 if violations found.
"""

import os
import re
import sys

_guard_dir = os.path.dirname(os.path.realpath(__file__))
sys.path.insert(0, _guard_dir)
from make_guard import require_make_target
require_make_target("lint", "lint-fix")

# Directories to scan
SCAN_DIRS = ["src", "tools"]

# File extensions to scan
EXTENSIONS = {".cpp", ".hpp", ".h", ".c"}

# ---------------------------------------------------------------------------
# Forbidden patterns for physical filesystem paths
# ---------------------------------------------------------------------------

# A. string concatenation building paths: .string() + "/something"
#    or .string() + '/something'  or  .string() + "\\something"
PATTERN_STRING_CONCAT_PATH = re.compile(
    r'\.string\(\)\s*\+\s*(["\'])(?:/|\\\\)'
)

# B. find_last_of('/') or find_last_of('\\') in filesystem context.
#    Allowed in explicit logical-path contexts (render_tree, canonical_ff_container_key).
PATTERN_FIND_LAST_OF_SEP = re.compile(
    r'find_last_of\s*\(\s*["\'](?:/|\\\\)["\']\s*\)'
)

# C. std::replace doing separator swap on physical path strings.
#    Handles nested parentheses in iterator expressions (e.g. .begin(), .end()).
PATTERN_REPLACE_SEP = re.compile(
    r'std::replace\s*\((?:[^)]|\([^)]*\))*["\'\']\\\\["\'\'][^)]*["\'\']/["\'\'](?:[^)]|\([^)]*\))*\)'
    r'|'
    r'std::replace\s*\((?:[^)]|\([^)]*\))*["\'\']/["\'\'][^)]*["\'\']\\\\["\'\'](?:[^)]|\([^)]*\))*\)'
)

# D. std::string physical path variable names in internal code.
#    Function parameters in API boundaries are allowed.
PATTERN_STRING_PATH_VAR = re.compile(
    r'\bstd::string\b[^;]*\b(full_path|root_path|directory_path|game_root_path)\b'
)

# ---------------------------------------------------------------------------
# Allowlist: files or functions where logical paths legitimately use strings
# ---------------------------------------------------------------------------

ALLOWED_FILES = {
    # RenderTree uses logical node paths (not filesystem paths)
    "src/d2engine/render/render_tree.cpp",
    "src/d2engine/render/render_tree.hpp",
    # FfAssetStore canonicalization is explicitly a logical ID operation
    "src/d2engine/assets/ff_asset_store.cpp",
    "src/d2engine/assets/ff_asset_store.hpp",
    # Platform utilities legitimately handle string paths at boundaries
    "src/d2engine/platform/platform_path.hpp",
    "src/d2engine/platform/platform_path.cpp",
}

# Allowed function-level contexts (detected by preceding line with function signature)
ALLOWED_FUNCTION_CONTEXTS = {
    "canonical_ff_container_key",
}


def is_allowed_file(rel_path):
    """Check if file is globally allowed."""
    return rel_path in ALLOWED_FILES


def should_scan_file(rel_path):
    """Determine if a file should be scanned at all."""
    _, ext = os.path.splitext(rel_path)
    return ext.lower() in EXTENSIONS


def get_recent_function_context(lines, lineno):
    """Look backwards to find the most recent function/method name."""
    for i in range(lineno - 1, max(lineno - 20, -1), -1):
        line = lines[i]
        # Match function definitions: type name(args) { or type name(args)
        m = re.search(r'\b([a-zA-Z_][a-zA-Z0-9_]*)\s*\([^)]*\)\s*\{?\s*$', line)
        if m:
            name = m.group(1)
            if name in ALLOWED_FUNCTION_CONTEXTS:
                return name
    return None


def scan_file(fpath, rel_path, lines):
    """Scan a single file for violations. Returns list of violation strings."""
    violations = []

    for lineno, line in enumerate(lines, 1):
        stripped = line.strip()

        # Skip pure comments
        if stripped.startswith("//") or stripped.startswith("/*"):
            continue

        # Skip string literals that aren't code (heuristic: lines that are mostly string)
        if stripped.count('"') >= 2 and not any(
            kw in stripped for kw in ("=", ";", "(", ")", "{", "}", ">>")
        ):
            continue

        # A. .string() + path separator concatenation
        if PATTERN_STRING_CONCAT_PATH.search(stripped):
            violations.append(
                f"{rel_path}:{lineno}: physical path string concatenation: {stripped}"
            )

        # B. find_last_of separator in non-allowed contexts
        if PATTERN_FIND_LAST_OF_SEP.search(stripped):
            func_ctx = get_recent_function_context(lines, lineno)
            if func_ctx not in ALLOWED_FUNCTION_CONTEXTS:
                violations.append(
                    f"{rel_path}:{lineno}: manual path parsing with find_last_of: {stripped}"
                )

        # C. std::replace separator swap
        if PATTERN_REPLACE_SEP.search(stripped):
            violations.append(
                f"{rel_path}:{lineno}: manual separator replacement on path: {stripped}"
            )

        # D. std::string variable named like a physical path
        # Skip function parameters (lines inside function signatures)
        m = PATTERN_STRING_PATH_VAR.search(stripped)
        if m and not stripped.strip().startswith("//"):
            # Allow if it's clearly a function parameter declaration or
            # function signature (API boundary accepting string).
            # Match lines ending with ); ) { ), or , (parameter continuation)
            param_line = re.sub(r'//.*$', '', stripped)
            if not re.search(r'\)\s*;?\s*$|\)\s*\{\s*$|,\s*$', param_line):
                # It's a variable declaration, not a parameter
                # But allow if it's in a CLI command header (API boundary)
                if "cmd_" not in stripped and "int " not in stripped:
                    violations.append(
                        f"{rel_path}:{lineno}: physical path variable as std::string: {stripped}"
                    )

    return violations


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)

    all_violations = []

    for scan_dir_name in SCAN_DIRS:
        scan_dir = os.path.join(project_root, scan_dir_name)
        if not os.path.isdir(scan_dir):
            continue

        for root, _, files in os.walk(scan_dir):
            for fname in files:
                fpath = os.path.join(root, fname)
                rel_path = os.path.relpath(fpath, project_root)

                if not should_scan_file(rel_path):
                    continue

                if is_allowed_file(rel_path):
                    continue

                try:
                    with open(fpath, "r", encoding="utf-8", errors="replace") as f:
                        lines = f.read().splitlines()
                except Exception as e:
                    print(f"WARN: could not read {rel_path}: {e}", file=sys.stderr)
                    continue

                violations = scan_file(fpath, rel_path, lines)
                all_violations.extend(violations)

    if all_violations:
        print("ERROR: filesystem path guardrail violations found:")
        for v in all_violations:
            print(f"  {v}")
        print("\nPhysical filesystem paths must use std::filesystem::path.")
        print("Manual string-based path construction, separator replacement,")
        print("and find_last_of path parsing are forbidden.")
        print("Logical asset/resource IDs are exempt.")
        sys.exit(1)

    print("guardrail_filesystem_paths: OK")
    sys.exit(0)


if __name__ == "__main__":
    main()
