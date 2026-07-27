#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/_require_make.sh"
require_make_target lint lint-fix
D2ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

if ! command -v clang-format &>/dev/null; then
    echo "ERROR: clang-format not found. Install: brew install llvm && export PATH=\"/opt/homebrew/opt/llvm/bin:\$PATH\"" >&2
    exit 1
fi

FILES=()
while IFS= read -r f; do
    FILES+=("$f")
done < <(find "${D2ROOT}/src" "${D2ROOT}/tests" \
    -not \( -path "*/build/*" -o -path "*/_deps/*" -o -path "*/__MACOSX/*" \) \
    \( -name "*.cpp" -o -name "*.hpp" \))

if [[ ${#FILES[@]} -eq 0 ]]; then
    echo "lint-format-check: no source files found"
    exit 0
fi

echo "lint-format-check: checking ${#FILES[@]} files..."
clang-format --dry-run --Werror "${FILES[@]}"
echo "lint-format-check: OK"
