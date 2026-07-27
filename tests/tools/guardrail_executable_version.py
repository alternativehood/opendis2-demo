#!/usr/bin/env python3
"""Verify every executable registers --version via libd2buildinfo.

Scans all main.cpp files under src/ (excluding test dirs) that use CLI::App,
and checks they call set_version_flag("--version"... using
d2buildinfo::format_build_version().

Exit code 0 if clean, 1 if violations found.
"""

import os
import sys
import re

SRC_DIR = os.path.join(os.path.dirname(__file__), '..', '..', 'src')
SKIP_DIRS = {'tests'}

# Patterns
RE_HAS_CLI = re.compile(r'CLI::App')
RE_HAS_BUILDINFO_INCLUDE = re.compile(r'#include\s+<d2buildinfo/build_info\.hpp>')
RE_HAS_VERSION_FLAG = re.compile(r'set_version_flag\("--version"')
RE_USES_FORMAT = re.compile(r'd2buildinfo::format_build_version')
RE_HAS_CLI_SETUP = re.compile(r'#include.*cli_setup\.hpp')


def main():
    if not os.path.isdir(SRC_DIR):
        print(f"Not a directory: {SRC_DIR}")
        sys.exit(1)

    violations = []

    for root, dirs, files in os.walk(SRC_DIR):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
        for fname in files:
            if fname != 'main.cpp':
                continue
            fpath = os.path.join(root, fname)
            with open(fpath, 'r', encoding='utf-8', errors='replace') as f:
                content = f.read()

            if not RE_HAS_CLI.search(content):
                continue

            # Delegates version handling to an included header (e.g. cli_setup.hpp)
            if RE_HAS_CLI_SETUP.search(content):
                continue

            problems = []
            if not RE_HAS_BUILDINFO_INCLUDE.search(content):
                problems.append('missing #include <d2buildinfo/build_info.hpp>')
            if not RE_HAS_VERSION_FLAG.search(content):
                problems.append('missing set_version_flag("--version")')
            if not RE_USES_FORMAT.search(content):
                problems.append('does not use d2buildinfo::format_build_version()')

            if problems:
                violations.append(f"{fpath}:\n  " + "\n  ".join(problems))

    if violations:
        print("ERROR: executable version flag violations:")
        for v in violations:
            print(f"  {v}")
        print("\nAll executables must provide --version via d2buildinfo::format_build_version().")
        sys.exit(1)

    print("OK: all executables have --version via libd2buildinfo")
    sys.exit(0)


if __name__ == "__main__":
    main()
