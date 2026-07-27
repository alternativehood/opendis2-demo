#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# verify.sh — Full project verification via the standard Makefile workflow.
#
# Usage:
#   ./tools/verify.sh            — make build + make test + make lint
#   ./tools/verify.sh --integration — also run integration tests (requires game data)
# ---------------------------------------------------------------------------
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/_require_make.sh"
require_make_target verify verify-integration
D2ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

RUN_INTEGRATION=false
while [[ $# -gt 0 ]]; do
    case "$1" in
        --integration)
            RUN_INTEGRATION=true
            shift
            ;;
        *)
            echo "Usage: $0 [--integration]" >&2
            exit 1
            ;;
    esac
done

cd "$D2ROOT"

echo "=== verify: build ==="
make build

echo "=== verify: test ==="
make test

echo "=== verify: lint ==="
make lint

if $RUN_INTEGRATION; then
    echo "=== verify: integration tests ==="
    make test-integration
fi

echo "=== verify: ALL PASSED ==="
