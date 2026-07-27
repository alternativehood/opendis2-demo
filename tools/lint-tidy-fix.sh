#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/_require_make.sh"
require_make_target lint-fix
echo "lint-tidy-fix: clangd-tidy has no fix mode; skipping tidy fixes"
echo "lint-tidy-fix: final make lint gate will enforce clangd-tidy diagnostics"
