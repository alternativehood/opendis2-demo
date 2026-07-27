#!/usr/bin/env python3
"""Verify that CLI source files do not contain DBF scanning/indexing logic.

The CLI must remain a thin frontend that calls d2gamedata APIs.
It must not implement DBF directory scanning, global ID indexing, or
resolution logic.

Using DbfReader for data extraction in CLI commands is fine.
Building a GameDataRegistry/DbfGameDataIndex index in CLI code is not.

Exit code 0 if clean, 1 if violations found.
"""

import os
import sys
import re


# Patterns that indicate CLI-owning scanning/indexing/resolution logic (banned)
BANNED_PATTERNS = [
    re.compile(r'load_directory.*\.dbf'),
    re.compile(r'DbfScanReport'),
    re.compile(r'index_row_for_test'),
    re.compile(r'class.*GameDataRegistry'),
    re.compile(r'class.*GlobalIdResolver'),
    re.compile(r'class.*DbfGameDataIndex'),
    re.compile(r'GameDataRegistry\(\)'),  # Direct instantiation in CLI
    re.compile(r'DbfGameDataIndex\(\)'),  # Direct instantiation in CLI
]


def main():
    if len(sys.argv) < 2:
        print("Usage: guardrail_cli_no_dbf_scanning.py <cli_dir>")
        sys.exit(1)

    cli_dir = sys.argv[1]
    if not os.path.isdir(cli_dir):
        print(f"Not a directory: {cli_dir}")
        sys.exit(1)

    violations = []

    for fname in sorted(os.listdir(cli_dir)):
        if not (fname.endswith('.hpp') or fname.endswith('.cpp') or fname.endswith('.h')):
            continue
        fpath = os.path.join(cli_dir, fname)
        with open(fpath, 'r', encoding='utf-8', errors='replace') as f:
            for lineno, line in enumerate(f, 1):
                for pattern in BANNED_PATTERNS:
                    if pattern.search(line):
                        violations.append(f"{fpath}:{lineno}: {line.strip()}")

    if violations:
        print("ERROR: CLI source files contain DBF scanning/indexing logic:")
        for v in violations:
            print(f"  {v}")
        print("\nCLI must remain a thin frontend calling d2gamedata APIs.")
        print("DBF scanning, indexing, and resolver creation belong in libd2gamedata.")
        sys.exit(1)

    print("OK: No DBF scanning/indexing logic in CLI source files")
    sys.exit(0)


if __name__ == "__main__":
    main()
