#!/usr/bin/env python3
"""Verify that libd2gamedata source files contain no d2scenario includes.

Usage: python3 guardrail_no_d2scenario_in_gamedata.py <src/d2gamedata directory>

Exit code 0 if clean, 1 if violations found.
"""

import os
import sys
import re


def main():
    if len(sys.argv) < 2:
        print("Usage: guardrail_no_d2scenario_in_gamedata.py <d2gamedata_dir>")
        sys.exit(1)

    gamedata_dir = sys.argv[1]
    if not os.path.isdir(gamedata_dir):
        print(f"Not a directory: {gamedata_dir}")
        sys.exit(1)

    pattern = re.compile(r'#include\s+["<]d2scenario/')
    violations = []

    for root, dirs, files in os.walk(gamedata_dir):
        for fname in files:
            if not (fname.endswith('.hpp') or fname.endswith('.cpp') or fname.endswith('.h')):
                continue
            fpath = os.path.join(root, fname)
            with open(fpath, 'r', encoding='utf-8', errors='replace') as f:
                for lineno, line in enumerate(f, 1):
                    if pattern.search(line):
                        violations.append(f"{fpath}:{lineno}: {line.strip()}")

    if violations:
        print("ERROR: d2gamedata files contain d2scenario includes:")
        for v in violations:
            print(f"  {v}")
        print("\nlibd2gamedata must NOT depend on libd2scenario.")
        print("Scenario-aware reporting lives in libd2analysis.")
        sys.exit(1)

    print("OK: No d2scenario includes found in d2gamedata")
    sys.exit(0)


if __name__ == "__main__":
    main()
