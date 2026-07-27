#!/usr/bin/env python3
"""Verify DbfGameDataIndex naming consistency.

The old alias GameDataRegistry in d2gamedata has been removed.
- All d2gamedata .cpp method definitions must use DbfGameDataIndex::, not GameDataRegistry::.
- d2gamedata .hpp files must NOT contain `using GameDataRegistry` or `class GameDataRegistry`.
- d2gamedata test files must use DbfGameDataIndex, not GameDataRegistry.

The engine facade d2engine::GameDataRegistry is a different class and is exempt.

Exit code 0 if clean, 1 if violations found.
"""

import os
import sys
import re


def main():
    if len(sys.argv) < 2:
        print("Usage: guardrail_dbfindex_naming.py <src_dir> <test_dir>")
        sys.exit(1)

    src_dir = sys.argv[1]
    test_dir = sys.argv[2]

    violations = []

#Check 1 : d2gamedata.cpp files must not define GameDataRegistry::methods
    gamedata_dir = os.path.join(src_dir, "d2gamedata")
    if os.path.isdir(gamedata_dir):
        for fname in sorted(os.listdir(gamedata_dir)):
            if not fname.endswith('.cpp'):
                continue
            fpath = os.path.join(gamedata_dir, fname)
            with open(fpath, 'r', encoding='utf-8', errors='replace') as f:
                for lineno, line in enumerate(f, 1):
                    if re.search(r'GameDataRegistry::', line):
                        violations.append(
                            f"{fpath}:{lineno}: Use DbfGameDataIndex:: instead of GameDataRegistry::"
                        )

#Check 2 : d2gamedata.hpp files(except the alias line) must not define GameDataRegistry
    if os.path.isdir(gamedata_dir):
        for fname in sorted(os.listdir(gamedata_dir)):
            if not (fname.endswith('.hpp') or fname.endswith('.h')):
                continue
            fpath = os.path.join(gamedata_dir, fname)
            with open(fpath, 'r', encoding='utf-8', errors='replace') as f:
                for lineno, line in enumerate(f, 1):
                    if re.search(r'using GameDataRegistry', line):
                        violations.append(
                            f"{fpath}:{lineno}: GameDataRegistry alias has been removed, use DbfGameDataIndex directly"
                        )
                    if re.search(r'class GameDataRegistry', line):
                        violations.append(
                            f"{fpath}:{lineno}: Use DbfGameDataIndex instead of GameDataRegistry"
                        )

#Check 3 : d2gamedata test files must not instantiate GameDataRegistry
    if os.path.isdir(test_dir):
        for fname in sorted(os.listdir(test_dir)):
            if not fname.endswith('.cpp'):
                continue
            fpath = os.path.join(test_dir, fname)
            with open(fpath, 'r', encoding='utf-8', errors='replace') as f:
                for lineno, line in enumerate(f, 1):
#Ignore engine facade tests(GameDataRegistryTest suite)
                    if re.search(r'GameDataRegistryTest', line):
                        continue
                    if re.search(r'd2engine::GameDataRegistry', line):
                        continue
                    if re.search(r'GameDataRegistry\(', line):
                        violations.append(
                            f"{fpath}:{lineno}: Use DbfGameDataIndex instead of GameDataRegistry in d2gamedata tests"
                        )
                    if re.search(r'class GameDataRegistry', line):
                        violations.append(
                            f"{fpath}:{lineno}: Use DbfGameDataIndex instead of GameDataRegistry in d2gamedata tests"
                        )

    if violations:
        print("ERROR: DbfGameDataIndex naming violations:")
        for v in violations:
            print(f"  {v}")
        print("\nGameDataRegistry in d2gamedata is deprecated. Use DbfGameDataIndex.")
        print("The engine facade d2engine::GameDataRegistry is exempt.")
        sys.exit(1)

    print("OK: DbfGameDataIndex naming is consistent")
    sys.exit(0)


if __name__ == "__main__":
    main()
