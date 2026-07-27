# Dedup Policy

## Tool

jscpd (Node.js). Installed via `npm install -g jscpd`.

PMD CPD (Java) would be preferred but Java is not available on this machine. The script (`tools/dedup-check.sh`) auto-selects PMD CPD if `pmd` is in PATH, falls back to jscpd.

## Thresholds (first dry-run: 0 hits in src/, 0 hits in tests/)

| Directory | Min lines | Min tokens |
|---|---|---|
| `src/` | 25 | 80 |
| `tests/` | 40 | 120 |

Tests get a higher threshold because fixture boilerplate (GAME_ROOT guards, temp dir setup, common EXPECT_EQ patterns) is intentional structural repetition, not a logic bug.

## What counts as a dangerous duplicate

A duplicate is dangerous when:
- The same business logic appears in two places and a bugfix in one won't reach the other
- The same parsing pattern is copy-pasted without a shared helper (e.g. two implementations of little-endian uint32 reading)
- Error handling is duplicated (e.g. two identical "if (GAME_ROOT.empty()) GTEST_SKIP()" blocks without a shared helper)

## When duplication is acceptable

1. **Test boilerplate** — `TempDir` setup, game root guards, basic ASSERT_EQ patterns. These are acceptable below the test threshold (40 lines / 120 tokens).
2. **Generated-style code** — command registration blocks where each entry is 2-5 lines of similar structure. These are mechanically similar but not logically duplicated.
3. **Short patterns** — any block under the threshold.

## How to fix

1. Extract the duplicated block into a named function or helper
2. If the duplication is in tests, consider a shared test fixture or helper in a `tests/common/` header
3. If the duplication is in binary parsing, extract to a free function in the appropriate `d2res/` module

## When to leave explicit duplication

Prefer explicit duplication when:
- The shared abstraction would require passing 5+ parameters
- The two "duplicated" blocks are likely to diverge in the future
- The abstraction would hide important differences in error handling

In these cases, document the decision in this file under the "Known acceptable duplicates" section.

## Why vendor/generated code is excluded

`build/_deps/` contains FetchContent dependencies. These are third-party code with their own style and duplication patterns. They're not ours to refactor.

## How to suppress a known false positive

Add an entry to the "Known acceptable duplicates" section below with:
- File paths and line ranges
- Why the duplication is acceptable
- Date added

## Known acceptable duplicates

*(none — first run found 0 clones)*
