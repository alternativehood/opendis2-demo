#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/_require_make.sh"
require_make_target lint lint-fix

D2ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
CLANG_TIDY="${D2ROOT}/.clang-tidy"
UNSUPPORTED="${SCRIPT_DIR}/clangd-tidy-unsupported-checks.txt"
ALLOWED="${SCRIPT_DIR}/clangd-tidy-allowed-checks.txt"
export PATH="/opt/homebrew/opt/llvm/bin:${PATH}"

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

python3 - "${CLANG_TIDY}" "${UNSUPPORTED}" "${ALLOWED}" <<'PY'
import re
import sys
from pathlib import Path

config = Path(sys.argv[1]).read_text()
unsupported = {
    line.strip()
    for line in Path(sys.argv[2]).read_text().splitlines()
    if line.strip() and not line.lstrip().startswith("#")
}
allowed = [
    line.strip()
    for line in Path(sys.argv[3]).read_text().splitlines()
    if line.strip() and not line.lstrip().startswith("#")
]

match = re.search(r"(?ms)^Checks:\s*>?\s*(.*?)(?:\n[A-Za-z].*:|\Z)", config)
if not match:
    print("ERROR: .clang-tidy has no Checks allowlist", file=sys.stderr)
    sys.exit(1)

checks = []
for token in re.split(r"[,\s]+", match.group(1)):
    token = token.strip().strip("'\"")
    if not token or token == "-*":
        continue
    if "*" in token:
        print(f"ERROR: .clang-tidy uses wildcard check entry: {token}", file=sys.stderr)
        sys.exit(1)
    if token.startswith("-"):
        continue
    checks.append(token)

if len(checks) != 57:
    print(f"ERROR: .clang-tidy must contain exactly 57 clangd-tidy checks, found {len(checks)}",
          file=sys.stderr)
    sys.exit(1)

if checks != allowed:
    print("ERROR: .clang-tidy Checks differs from tools/clangd-tidy-allowed-checks.txt",
          file=sys.stderr)
    missing = [check for check in allowed if check not in checks]
    extra = [check for check in checks if check not in allowed]
    if missing:
        print("Missing:", file=sys.stderr)
        for check in missing:
            print(f"  {check}", file=sys.stderr)
    if extra:
        print("Extra:", file=sys.stderr)
        for check in extra:
            print(f"  {check}", file=sys.stderr)
    sys.exit(1)

bad = sorted({check for check in checks if check in unsupported or check.startswith("clang-analyzer-")})
if bad:
    print("ERROR: .clang-tidy lists clangd-disabled clang-tidy checks:", file=sys.stderr)
    for check in bad:
        print(f"  {check}", file=sys.stderr)
    print("clangd-tidy would not enforce these checks; remove them from the allowlist.",
          file=sys.stderr)
    sys.exit(1)

if "WarningsAsErrors: '*'" not in config and 'WarningsAsErrors: "*"' not in config:
    print("ERROR: .clang-tidy must keep WarningsAsErrors: '*'", file=sys.stderr)
    sys.exit(1)

print(f"guardrail_clangd_tidy_policy: OK enforced_checks={len(checks)}")
PY
