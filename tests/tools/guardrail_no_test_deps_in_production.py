#!/usr/bin/env python3
"""Verify that production source files (src/) contain no test/test-support includes.

Usage: python3 guardrail_no_test_deps_in_production.py <src directory>

Exit code 0 if clean, 1 if violations found.
"""

import os
import sys
import re


def main():
    if len(sys.argv) < 2:
        print("Usage: guardrail_no_test_deps_in_production.py <src_dir>")
        sys.exit(1)

    src_dir = sys.argv[1]
    if not os.path.isdir(src_dir):
        print(f"Not a directory: {src_dir}")
        sys.exit(1)

    pattern = re.compile(
        r'#include\s+["<]'
        r'(tests/|test_support/|testing/|\.\./tests/|\.\./test_support/|\.\./testing/)'
    )
    violations = []

    for root, dirs, files in os.walk(src_dir):
        # Skip any test directories within src (there shouldn't be any,
        # but be defensive)
        dirs[:] = [d for d in dirs if d not in ('tests', 'test_support', 'testing')]
        for fname in files:
            if not (fname.endswith('.hpp') or fname.endswith('.cpp') or fname.endswith('.h')):
                continue
            fpath = os.path.join(root, fname)
            with open(fpath, 'r', encoding='utf-8', errors='replace') as f:
                for lineno, line in enumerate(f, 1):
                    if pattern.search(line):
                        violations.append(f"{fpath}:{lineno}: {line.strip()}")

    if violations:
        print("ERROR: Production files contain test/test-support includes:")
        for v in violations:
            print(f"  {v}")
        print("\nProduction code must NOT depend on test code.")
        print("Test support helpers must live under tests/ and never be included from src/.")
        sys.exit(1)

    print("OK: No test/test-support includes found in production code")
    sys.exit(0)


if __name__ == "__main__":
    main()
