#!/usr/bin/env python3
"""Verify that there is exactly one headless battle executable in the project.

Checks:
1. CMakeLists.txt contains exactly one add_executable(opendis2-dev-battle ...
2. CMakeLists.txt does NOT contain opendis2-dev-battle-sweep
3. src/opendis2_battle_sweep/main.cpp does not exist
4. src/opendis2_battle/main.cpp does not contain legacy interactive tokens
5. opendis2-dev-battle links libd2battle_sweep

Usage: python3 guardrail_battle_architecture.py <project_root>

Exit code 0 if clean, 1 if violations found.
"""

import os
import sys
import re


LEGACY_TOKENS = [
    "kParty1Coord",
    "kParty2Coord",
    "find_stacks_at",
    "require_exactly_one_stack",
    "std::cin",
    "std::getline",
    "Choice [",
    "User quit",
    "CONSUMER SELECTED ACTION",
]


def find_cmake_violations(cmake_path):
    violations = []
    with open(cmake_path, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read()

    if "opendis2-dev-battle-sweep" in content:
        violations.append(
            f"{cmake_path}: contains forbidden token opendis2-dev-battle-sweep"
        )

    matches = list(re.finditer(
        r'add_executable\s*\(\s*opendis2-dev-battle\b', content))
    if len(matches) == 0:
        violations.append(
            f"{cmake_path}: no add_executable(opendis2-dev-battle ... found")
    elif len(matches) > 1:
        violations.append(
            f"{cmake_path}: found {len(matches)} add_executable(opendis2-dev-battle ... entries, expected exactly 1"
        )

    target_start = content.find("add_executable(opendis2-dev-battle")
    if target_start != -1:
        block_end = content.find("add_executable(", target_start + 1)
        if block_end == -1:
            block_end = content.find("add_library(", target_start + 1)
        if block_end == -1:
            block_end = len(content)
        target_block = content[target_start:block_end]
        if "libd2battle_sweep" not in target_block:
            violations.append(
                f"{cmake_path}: opendis2-dev-battle does not link libd2battle_sweep"
            )

    return violations


def check_sweep_main_exists(project_root):
    path = os.path.join(project_root, "src", "opendis2_battle_sweep", "main.cpp")
    if os.path.exists(path):
        return [f"{path}: exists — must be deleted (single canonical main is src/opendis2_battle/main.cpp)"]
    return []


def check_battle_main_legacy(project_root):
    path = os.path.join(project_root, "src", "opendis2_battle", "main.cpp")
    if not os.path.exists(path):
        return [f"{path}: does not exist"]
    violations = []
    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        for lineno, line in enumerate(f, 1):
            for token in LEGACY_TOKENS:
                if token in line:
                    violations.append(
                        f"{path}:{lineno}: found forbidden legacy token '{token}' in line: {line.strip()}"
                    )
    return violations


def main():
    if len(sys.argv) < 2:
        print("Usage: guardrail_battle_architecture.py <project_root>")
        sys.exit(1)

    project_root = sys.argv[1]
    if not os.path.isdir(project_root):
        print(f"Not a directory: {project_root}")
        sys.exit(1)

    all_violations = []

    cmake_path = os.path.join(project_root, "CMakeLists.txt")
    if os.path.exists(cmake_path):
        all_violations.extend(find_cmake_violations(cmake_path))

    all_violations.extend(check_sweep_main_exists(project_root))
    all_violations.extend(check_battle_main_legacy(project_root))

    if all_violations:
        print("ERROR: battle architecture boundary violations:")
        for v in all_violations:
            print(f"  {v}")
        print()
        print("There must be exactly one headless battle executable: opendis2-dev-battle")
        print("It must be the stack sweep runner (no legacy interactive mode).")
        sys.exit(1)

    print("OK: Single headless battle executable architecture verified.")
    sys.exit(0)


if __name__ == "__main__":
    main()
