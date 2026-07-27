# Agent Instructions — opendis2

C++20 reverse-engineering project (Disciples II resource unpacker).

## Repository Layout

- `src/` — active C++ project (libd2res + libd2asset + CLI + engine + tests). This is where nearly all code work happens.
- `docs/formats/` — binary format documentation (MQDB, OPT, WDB, WDT, etc.). Findings must be written here.
- `research/` — research scripts, external tools, and notes.
- `notes/` — project notes (e.g., `unit-resource-linkage.md`).

## opendis2 — Build & Development

### Prerequisites

```bash
brew install cmake ninja llvm cppcheck pmd
# Ensure LLVM tools are on PATH
export PATH="/opt/homebrew/opt/llvm/bin:$PATH"
```

Optional: `npm install -g jscpd` (dedup check fallback if PMD unavailable).

### Quick Start (vcpkg manifest mode — recommended)

```bash
# 1. Set up .env
cp .env.example .env
# Edit .env to set VCPKG_ROOT to your vcpkg installation

# 2. Build everything
make build

# 3. Run unit tests
make test

# 4. Run lint/guardrail checks
make lint
```

### Makefile Targets

| Target      | Behavior                                                    |
|-------------|-------------------------------------------------------------|
| `configure` | CMake configure only (vcpkg manifest mode, Ninja)           |
| `build`     | configure + build dev binaries (opendis2, extractor, viewer, scenario-gen) |
| `test`      | configure + build test binaries + run CTest (unit tests)    |
| `test-integration` | configure + build + run integration tests (requires game data) |
| `validate-full-game` | full-game extraction validation (not in CTest, may exceed 3s) |
| `lint`      | run all lint/guardrail checks                               |
| `clean`     | remove `build/dev` (preserves vcpkg dependencies)           |
| `distclean` | remove `build/dev` and `vcpkg_installed`                    |
| `help`      | print available targets                                     |

### Running tests

```bash
# Unit tests (default)
make test

# Integration tests (requires DISCIPLES2_GAME_ROOT + game data)
make test-integration

# Specific test
ctest --test-dir build/dev --output-on-failure -R "AdventureWorldBuilder"
```

### Lint & Quality Gates

```bash
# Full lint suite
make lint

# Auto-fix format issues, then verify
./tools/lint-fix.sh && make build && make test

# Changed-file format check (fast)
./tools/lint-changed-check.sh
```

Integration tests need `DISCIPLES2_GAME_ROOT` set (in `.env` or as environment variable):

```bash
DISCIPLES2_GAME_ROOT="$HOME/Library/Application Support/Steam/steamapps/common/Disciples II Rise of the Elves"
```

Integration tests (require `DISCIPLES2_GAME_ROOT` + real game data) are run via `make test-integration`.

### Lint & Quality Gates

- **clang-format**: LLVM style, 100 columns, 4-space indent, `SortIncludes: Never`. Config: `.clang-format`.
- **clang-tidy**: Extensive checks, `WarningsAsErrors: '*'`. Config: `.clang-tidy`. Requires `compile_commands.json`.
- **cppcheck**: Enabled via `compile_commands.json`.
- **dead-code check**: cppcheck `unusedFunction` + inconclusive checks.
- **dedup check**: jscpd (or PMD CPD). Stricter in `src/` (25 lines / 80 tokens) than `tests/` (40 / 120).

**Quality gate order**: `lint-fix.sh` → `verify.sh`. If lint-fix modifies files, re-read them before finishing.

### Post-Edit Hooks

`.claude/settings.json` hooks automatically run `lint-changed-fix.sh` and `lint-changed-check.sh` after every `Edit`/`Write` in the opendis2 tree. A stop hook also runs `lint-check.sh` if project files changed.

**Rules**:
- All research, scripts, findings go to files (`research/`, `docs/formats/`) — never only in context.
- Binary format findings must be written to `docs/formats/*.md` as part of the task.
- Use D2ResExplorer as source of truth for format verification.

## UI Layout Architecture Rule

**Every Screen owns exactly one `TreeLayout` through the `Screen` base class.**

- Every concrete `Screen` subclass receives its `TreeLayout` via the base class constructor.
- Every production UI destination position/rectangle is defined in configuration under `configs/`.
- Screen implementations select semantic `TreeLayout` paths and call `layout_rect(path)` or `tree_layout().compose(path)`.
- Screen implementations MUST NOT contain their own UI layout engines or authoritative pixel coordinates.
- World/game-space coordinates and asset source metadata are outside this rule.
- All production runtime config files live under `configs/` (not repository root).
- No duplicate authoritative layout sources — one `TreeLayout` config per screen.

Guardrail: `tools/guardrail_ui_layout.sh` enforces this. Runs via `make lint`.

## Architecture Notes

- **libd2res** — core parsing: MQDB containers, OPT metadata, image/anim decoders, DBF, DLG, DAT, WDT, WDB, game scanner.
- **libd2asset** — runtime asset package reader (manifests, engine contract). Must NOT link libd2res (enforced at CMake level).
- **libd2engine** — SDL3-based game engine (optional, `D2_ENABLE_ENGINE`):
  - `RawResourceLoader` — direct `.ff` loading (lazy-open, no pre-extraction)
  - `GameTextureCache` — `SDL_Texture` cache keyed by `(container, sprite)`
  - `Application` — render loop, event handling, diagnostics
  - Links libd2res + SDL3 + lodepng (not libd2asset)
- **opendis2-dev-extractor** — development CLI: `list`, `extract-*`, `scan`, `extract-all`, `compare-images`, `inspect`, etc. `extract-all` covers images, animations, sounds, and data tables (DBF, DAT, DLG) in one pass.
- **Entrypoint**: `src/cli/main.cpp` registers commands; each `commands_*.cpp` implements a subcommand.
- **Generated code**: none currently. Build artifacts go to `build/` (ignored).

## Testing

- **Unit tests**: `tests/unit/test_*.cpp` — fast, no game data required.
- **Integration tests**: `tests/integration/test_*.cpp` — require `DISCIPLES2_GAME_ROOT`.
- **Visual regression**: Reference dataset at `${DISCIPLES2_GAME_ROOT}/../d2_data_extracted/` (do not modify).

### Test policy

- Do not add tests that only prove aggregate initialization, assigned field values, enum integer values, std::string/std::vector/std::optional behavior, or placeholder no-op behavior.
- Tests must protect behavior, public contracts, regression bugs, parser/resolver correctness, runtime/world conversion, CLI contracts, or architecture dependency boundaries.
- Source-grep tests (scanning source files for patterns) are allowed only for important dependency boundaries, not exact implementation snippets.

### Test performance policy

**Every individual CTest test entry must complete within 3 seconds.**

No exceptions. No slow-test allowlist. No integration exception. No full-game exception.

- Long exhaustive validation (full-game extraction, bulk comparison) must live outside CTest, in dedicated targets like `make validate-full-game`.
- Per-test `TIMEOUT 3` is enforced via `gtest_discover_tests(... PROPERTIES TIMEOUT 3)` for all GoogleTest suites, and via explicit `set_tests_properties(... TIMEOUT 3)` for all `add_test()` entries.
- The guardrail `tools/guardrail_ctest_timeout.sh` verifies that no test registration path bypasses `TIMEOUT 3`. It runs as part of `make lint`.
- If a test needs more than 3 seconds, redesign it: use smaller fixture data, extract only targeted records, build synthetic fixtures, or move it outside CTest.
- Target practical headroom: prefer tests comfortably below 2 seconds so the 3-second hard limit is not flaky under parallel load.

## Important Constraints

- C++20, warnings-as-errors (`-Werror` on Clang/GCC, `/WX` on MSVC).
- Dependencies are managed through `vcpkg.json`. Normal build uses `.env` + `make build`. FetchContent fallback (`D2_USE_FETCHCONTENT_FALLBACK=ON`) exists but is not the normal workflow.
- Never commit proprietary game assets (`.ff`, `.mqdb`, `.opt`, `.sg`, `.sav`, `.mpq`).
- Keep `docs/formats/` updated when binary format knowledge changes.

## No Backward Compatibility

**This project does not maintain backward compatibility.** When a format, protocol, or data schema changes, the old format is **immediately rejected** — no fallbacks, no deprecation periods, no graceful degradation.

- **Hard switch only**: All format changes are breaking. Code must fail fast and loud when encountering obsolete data.
- **No dual-path code**: Do not write `if (new_format) { ... } else { old_format(); }`. Remove the old path entirely.
- **User action required**: When a format changes, users must regenerate or update their local files. The code prints a clear error message telling them exactly what to do (e.g., "Delete the file to regenerate a default").
- **Rationale**: This is a reverse-engineering project. Old formats are bugs, not features. Maintaining compatibility spreads complexity and hides rot. A hard switch forces everyone to stay on the correct, understood data model.

Example: `battle_demo.json` used to support `attackers`/`defenders` arrays. When `positions` array was introduced, the old arrays were **not** kept as a fallback. The loader now logs a fatal error and exits if `positions` is missing.


