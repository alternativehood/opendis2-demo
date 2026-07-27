#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/_require_make.sh"
require_make_target lint-changed-fix
D2ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
LOCKDIR="${D2ROOT}/build/dev"
LOCKFILE="${LOCKDIR}/.claude-lint.lock"

# Prevent recursive execution
if [[ -f "${LOCKFILE}" ]]; then
    exit 0
fi
mkdir -p "${LOCKDIR}"
touch "${LOCKFILE}"
trap 'rm -f "${LOCKFILE}"' EXIT

if ! command -v clang-format &>/dev/null; then
    echo "WARNING: clang-format not found, skipping format fix" >&2
    exit 0
fi

# Get changed files (staged or unstaged vs HEAD)
CHANGED=()
if git -C "${D2ROOT}" rev-parse HEAD &>/dev/null 2>&1; then
    while IFS= read -r f; do
        [[ -n "$f" ]] && CHANGED+=("$f")
    done < <(
        git -C "${D2ROOT}" diff --name-only --diff-filter=ACMRTUXB HEAD 2>/dev/null
        git -C "${D2ROOT}" diff --name-only --diff-filter=ACMRTUXB --cached HEAD 2>/dev/null
    )
else
    while IFS= read -r f; do
        CHANGED+=("$f")
    done < <(find "${D2ROOT}/src" "${D2ROOT}/tests" \( -name "*.cpp" -o -name "*.hpp" \))
fi

FILES=()
for f in "${CHANGED[@]:-}"; do
    [[ -z "${f}" ]] && continue
    abs="${D2ROOT}/${f}"
    [[ ! -f "${abs}" ]] && continue
    case "${abs}" in *.cpp|*.hpp) ;; *) continue ;; esac
    case "${abs}" in */build/*|*/_deps/*) continue ;; esac
    FILES+=("${abs}")
done

# Remove duplicates (bash 3 compatible)
if [[ ${#FILES[@]} -gt 0 ]]; then
    UNIQ_FILES=()
    for f in "${FILES[@]}"; do
        found=0
        if [[ ${#UNIQ_FILES[@]} -gt 0 ]]; then
            for u in "${UNIQ_FILES[@]}"; do
                if [[ "$u" == "$f" ]]; then
                    found=1
                    break
                fi
            done
        fi
        if [[ $found -eq 0 ]]; then
            UNIQ_FILES+=("$f")
        fi
    done
    FILES=("${UNIQ_FILES[@]}")
fi

if [[ ${#FILES[@]} -eq 0 ]]; then
    exit 0
fi

echo "lint-changed-fix: formatting ${#FILES[@]} changed file(s)..."
clang-format -i "${FILES[@]}"
