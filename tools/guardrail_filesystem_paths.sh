#!/usr/bin/env bash
# guardrail_filesystem_paths.sh
#
# Wrapper that invokes the Python filesystem path guardrail scanner.
# Ensures physical filesystem paths use std::filesystem::path,
# and manual string-based path construction is forbidden.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/_require_make.sh"
require_make_target lint lint-fix

python3 "${SCRIPT_DIR}/guardrail_filesystem_paths.py"
