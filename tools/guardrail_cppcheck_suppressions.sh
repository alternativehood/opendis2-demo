#!/usr/bin/env bash
set -euo pipefail

_guard_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${_guard_dir}/_require_make.sh"
require_make_target lint lint-fix
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FILE="${ROOT}/tools/cppcheck-suppressions.txt"

if [[ ! -f "${FILE}" ]]; then
    echo "ERROR: missing ${FILE}" >&2
    exit 1
fi

self="$(basename "${BASH_SOURCE[0]}")"
matches="$(find "${ROOT}/tools" -name '*.sh' -not -name "${self}" \
    -exec grep -In -- '--suppress=syntaxError' {} + 2>/dev/null || true)"
if [[ -n "${matches}" ]]; then
    echo "ERROR: blanket --suppress=syntaxError is forbidden" >&2
    echo "${matches}"
    exit 1
fi

# Check for inline cppcheck-suppress syntaxError in tests (forbidden)
found_inline=$(grep -RIn -- 'cppcheck-suppress syntaxError' "${ROOT}/tests" 2>/dev/null || true)
if [[ -n "${found_inline}" ]]; then
    echo "ERROR: inline cppcheck-suppress syntaxError is forbidden in tests; use tools/cppcheck-suppressions.txt" >&2
    echo "${found_inline}"
    exit 1
fi

bad=0
while IFS= read -r line; do
    line="${line%%#*}"
    line="$(echo "${line}" | xargs || true)"
    [[ -z "${line}" ]] && continue
    case "${line}" in
        syntaxError:'*/tests/unit/test_*.cpp') ;;
        syntaxError:'*/tests/integration/test_*.cpp') ;;
        syntaxError:*/tests/unit/test_*.cpp) ;;
        syntaxError:*/tests/integration/test_*.cpp) ;;
        *)
            if [[ "${line}" == syntaxError* ]]; then
                echo "ERROR: forbidden broad cppcheck syntaxError suppression: ${line}" >&2
                bad=1
            fi
            ;;
    esac
    if [[ "${line}" == unusedFunction:* ]] && [[ "${line}" == */src/* ]]; then
        echo "ERROR: forbidden production unusedFunction suppression: ${line}" >&2
        echo "Production dead code must be deleted, or legitimate usage must be visible to whole-project analysis." >&2
        echo "File-wide production unusedFunction suppressions are not allowed." >&2
        bad=1
    fi
done < "${FILE}"

exit "${bad}"
