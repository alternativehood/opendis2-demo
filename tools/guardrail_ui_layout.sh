#!/usr/bin/env bash
# guardrail_ui_layout.sh — no hardcoded Screen UI geometry
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/_require_make.sh"
require_make_target lint lint-fix

exec python3 "${SCRIPT_DIR}/guardrail_ui_layout.py"
