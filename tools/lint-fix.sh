#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/_require_make.sh"
require_make_target lint-fix

echo "=== lint-fix: format-fix ==="
"${SCRIPT_DIR}/lint-format-fix.sh"

echo "=== lint-fix: tidy-fix ==="
"${SCRIPT_DIR}/lint-tidy-fix.sh"

echo "=== lint-fix: format-fix (post-tidy) ==="
"${SCRIPT_DIR}/lint-format-fix.sh"

echo "=== lint-fix: lint-check ==="
"${SCRIPT_DIR}/lint-check.sh"

echo "=== lint-fix: ALL PASSED ==="
