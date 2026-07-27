#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/_require_make.sh"
require_make_target lint lint-fix lint-tidy
D2ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${D2ROOT}/build/dev"
export PATH="${D2ROOT}/build/tools/clangd-tidy/bin:/opt/homebrew/opt/llvm/bin:${PATH}"

if ! command -v clangd-tidy &>/dev/null; then
    echo "ERROR: clangd-tidy not found." >&2
    echo "Install pinned dependency:" >&2
    echo "  python3 -m venv build/tools/clangd-tidy" >&2
    echo "  build/tools/clangd-tidy/bin/python -m pip install -r tools/requirements-lint.txt" >&2
    exit 1
fi
if ! command -v clangd &>/dev/null; then
    echo "ERROR: clangd not found. Install: brew install llvm" >&2
    exit 1
fi
CLANGD_VERSION="$(clangd --version | head -n 1)"
if [[ ! "${CLANGD_VERSION}" =~ version[[:space:]]+22\. ]]; then
    echo "ERROR: clangd-tidy policy is pinned to LLVM clangd 22.x" >&2
    echo "Detected: ${CLANGD_VERSION}" >&2
    exit 1
fi

COMPILE_DB="${BUILD_DIR}/compile_commands.json"
if [[ ! -f "${COMPILE_DB}" ]]; then
    echo "ERROR: compile_commands.json not found at ${COMPILE_DB}" >&2
    exit 1
fi

if [[ -z "${JOBS:-}" ]]; then
    if command -v sysctl &>/dev/null; then
        JOBS="$(sysctl -n hw.logicalcpu 2>/dev/null || true)"
    fi
    if [[ -z "${JOBS}" ]] && command -v nproc &>/dev/null; then
        JOBS="$(nproc 2>/dev/null || true)"
    fi
    JOBS="${JOBS:-4}"
fi
TMPFILES=$(mktemp)
trap 'rm -f "${TMPFILES}"' EXIT
python3 - "${COMPILE_DB}" "${D2ROOT}" > "${TMPFILES}" <<'PY'
import json
import pathlib
import sys

compile_db = pathlib.Path(sys.argv[1])
root = pathlib.Path(sys.argv[2]).resolve()
seen = set()
for entry in json.loads(compile_db.read_text()):
    file = pathlib.Path(entry["file"]).resolve()
    try:
        rel = file.relative_to(root)
    except ValueError:
        continue
    if rel.parts and rel.parts[0] in {"src", "tests"} and file.suffix in {".c", ".cc", ".cpp", ".cxx"}:
        key = str(file)
        if key not in seen:
            seen.add(key)
            print(key)
PY

FILES=()
while IFS= read -r file; do
    [[ -n "${file}" && -f "${file}" ]] && FILES+=("${file}")
done < "${TMPFILES}"

# Limit to changed files for performance (fall back to all files outside git)
CHANGED_FILES=()
if git -C "${D2ROOT}" rev-parse HEAD &>/dev/null 2>&1; then
    while IFS= read -r f; do
        [[ -z "${f}" ]] && continue
        abs="${D2ROOT}/${f}"
        [[ -f "${abs}" ]] && CHANGED_FILES+=("${abs}")
    done < <(
        git -C "${D2ROOT}" diff --name-only --diff-filter=ACMRTUXB HEAD 2>/dev/null
        git -C "${D2ROOT}" diff --name-only --diff-filter=ACMRTUXB --cached HEAD 2>/dev/null
    )
fi

if [[ ${#CHANGED_FILES[@]} -gt 0 ]]; then
    INTERSECT=()
    for f in "${FILES[@]}"; do
        for cf in "${CHANGED_FILES[@]}"; do
            [[ "${f}" == "${cf}" ]] && { INTERSECT+=("${f}"); break; }
        done
    done
    if [[ ${#INTERSECT[@]} -eq 0 ]]; then
        echo "lint-tidy-check: no changed source files, skipping"
        exit 0
    fi
    FILES=("${INTERSECT[@]}")
    echo "lint-tidy-check: checking ${#FILES[@]} changed file(s)"
fi

if [[ ${#FILES[@]} -eq 0 ]]; then
    echo "lint-tidy-check: no source files found"
    exit 0
fi

echo "lint-tidy-check: running clangd-tidy (-j ${JOBS}, warnings as errors)..."
clangd-tidy \
    --fail-on-severity warn \
    -p "${BUILD_DIR}" \
    -j "${JOBS}" \
    --clangd-executable "$(command -v clangd)" \
    "${FILES[@]}"

echo "lint-tidy-check: OK"
