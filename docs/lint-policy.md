# Lint Policy

## Why strict lint

opendis2 is a binary format parser. The main failure modes are corrupt offsets,
bad record bounds, signed/unsigned mistakes, accidental unused state in parsing
loops, and copy-paste bugs in decoder logic.

The lint pipeline catches these at commit time instead of on real game data.

## Public targets

Use Make targets, not guarded scripts directly:

```bash
make lint              # full lint and guardrail gate
make lint-fix          # auto-fix formatting, then run make lint checks
make lint-changed      # format-check changed source/test files
make lint-changed-fix  # format changed source/test files
make verify            # build + test + lint
make verify-integration
```

The scripts under `tools/` are internal entrypoints for these targets. Guarded
scripts print the correct Make target if run directly.

## clangd-tidy

`make lint` runs `tools/lint-tidy-check.sh`, which uses `clangd-tidy` for fast
diagnostics through `clangd --clang-tidy`.

Do not add a compatibility lane using ordinary `clang-tidy` or
`run-clang-tidy`. The lint policy is the fast clangd-tidy allowlist only.
`make lint-fix` runs formatting fixes and then the final lint gate; it does not
run tidy fixes because clangd-tidy has no fix mode.

The `.clang-tidy` profile is explicit:

- `57` named checks actually enforced by clangd-tidy.
- `WarningsAsErrors: '*'`.
- The same profile applies to `src/**` and `tests/**`.
- Wildcard check families are intentionally not enabled.

Do not list checks that clangd disables internally. They are not enforced
policy. LLVM clangd 22 does not support static analyzer checks, so every
`clang-analyzer-*` check is forbidden. The centralized exact disabled list for
clangd 22 is `tools/clangd-tidy-unsupported-checks.txt`, and the canonical
allowlist is `tools/clangd-tidy-allowed-checks.txt`.
`tools/guardrail_clangd_tidy_policy.sh` fails lint if `.clang-tidy` differs from
that allowlist, contains any `clang-analyzer-*` check, contains a known disabled
check, loses `WarningsAsErrors: '*'`, or is run with a non-LLVM-22 `clangd`.

`JOBS` may be set by the caller. If unset, lint detects logical CPUs with:

1. `sysctl -n hw.logicalcpu`
2. `nproc`
3. fallback `4`

## clangd-tidy dependency

`clangd-tidy` is mandatory for `make lint`. The project pins it in:

```bash
tools/requirements-lint.txt
```

Bootstrap it explicitly:

```bash
python3 -m venv build/tools/clangd-tidy
build/tools/clangd-tidy/bin/python -m pip install -r tools/requirements-lint.txt
brew install llvm
```

`make lint` does not install network dependencies.

The current clangd-tidy policy is version-specific to LLVM `clangd` 22.x. Lint
checks `clangd --version` and fails on a different major version until the
allowlist/unsupported policy is reviewed.

## cppcheck

`tools/lint-cppcheck.sh` is a separate cppcheck error gate. It is not the
clangd-tidy WarningsAsErrors policy, and it does not make warning/style/
performance/portability cppcheck findings fatal.

Dead-code detection is separate:

```bash
tools/dead-code-check.sh  # internal; run through make lint
```

This is a focused whole-program cppcheck pass over `src/**` and `tests/**`
with `--enable=unusedFunction` only. It does not use `--enable=all`,
`--inconclusive`, or textual function-name counting. Any unsuppressed
`unusedFunction` or `unusedPrivateFunction` finding fails `make lint`.

`unusedFunction` findings mean:

- verify call sites;
- move test-only helpers to test support;
- delete real dead code;
- fix analysis scope for legitimate cross-TU usage.

Production `unusedFunction` suppressions are forbidden. Any
`unusedFunction:*/src/**` entry in `tools/cppcheck-suppressions.txt` fails
`make lint`.

The mandatory dead-code guarantee is for production code: any unsuppressed
`unusedFunction` under `src/**` fails `make lint`.

The dead-code pass defines minimal GoogleTest macros so cppcheck can parse test
bodies and see production API call sites. It suppresses generated `TEST`/`TEST_F`
function entrypoints under `tests/**`. With the current cppcheck parser this is
a tests-only blanket suppression, so test translation-unit unused helper
functions are intentionally excluded from this gate rather than pretending they
are enforced.

GoogleTest macro parser false positives are suppressed for test files via:

```text
syntaxError:*/tests/unit/test_*.cpp
syntaxError:*/tests/integration/test_*.cpp
```

Blanket `--suppress=syntaxError` in scripts/config is forbidden. Inline
`cppcheck-suppress syntaxError` in tests is also forbidden.

## NOLINT

Every NOLINT must name the exact check and include a reason.

Correct:

```cpp
// NOLINTNEXTLINE(check-name) reason: why this suppression is needed
some_code();

some_code(); // NOLINT(check-name) reason: why this suppression is needed
```

Forbidden:

```cpp
// NOLINT
some_code(); // NOLINT
```

Prefer real exception boundaries over `NOLINT(bugprone-exception-escape)` on
`main()`: catch `std::exception`, catch `...`, and return `EXIT_FAILURE`.

Narrow `NOLINT(concurrency-mt-unsafe)` for `std::getenv` is acceptable when the
call happens during guaranteed single-threaded initialization.

## Vendor code

Build dependencies under `build/_deps/` are not linted. They are third-party
code, often use older C/C++ style, and are not ours to fix.

## Filesystem path guardrail

`guardrail_filesystem_paths.sh` enforces:

- physical filesystem paths use `std::filesystem::path`;
- manual physical path construction with string concatenation is forbidden;
- logical asset/resource IDs are exempt.

See `docs/architecture/ff_asset_access.md` for the full architecture.
