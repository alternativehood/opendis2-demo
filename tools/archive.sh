#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# archive.sh — Lightweight source-only archive of opendis2.
#
# Packages all tracked + untracked non-ignored files into a .tar.gz,
# excluding build artifacts, system junk, and vcpkg_installed.
#
# Usage:
#   ./tools/archive.sh
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/_require_make.sh"
require_make_target archive
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

if [[ ! -d "${PROJECT_DIR}" ]]; then
    echo "ERROR: opendis2 project directory not found at ${PROJECT_DIR}" >&2
    exit 1
fi

if [[ ! -d "${PROJECT_DIR}/.git" ]]; then
    echo "ERROR: ${PROJECT_DIR} is not a git repository" >&2
    exit 1
fi

BRANCH="$(git rev-parse --abbrev-ref HEAD)"
if [[ "${BRANCH}" == "HEAD" ]]; then
    BRANCH="detached-head"
fi
SANITIZED_BRANCH="$(echo "${BRANCH}" | tr '/' '-')"

TIMESTAMP="$(date +%Y-%m-%d-%H%M%S)"
ARCHIVE_NAME="opendis2-${SANITIZED_BRANCH}-${TIMESTAMP}.tar.gz"
ARCHIVE_PATH="${PROJECT_DIR}/archive_artifacts/${ARCHIVE_NAME}"

FILE_LIST_RAW="$(mktemp)"
FILE_LIST="$(mktemp)"

cleanup() {
    rm -f "${FILE_LIST_RAW}" "${FILE_LIST}"
}
trap cleanup EXIT

cd "${PROJECT_DIR}"

# ---------------------------------------------------------------------------
# Step 1. Build file list
# ---------------------------------------------------------------------------
echo "=== Step 1: Building file list ==="

git ls-files > "${FILE_LIST_RAW}"
git ls-files --others --exclude-standard >> "${FILE_LIST_RAW}"

sort -u "${FILE_LIST_RAW}" > "${FILE_LIST}"

# Filter out deleted files (tracked in git but removed from disk)
while IFS= read -r f; do
    [ -f "$f" ] && echo "$f"
done < "${FILE_LIST}" > "${FILE_LIST}.tmp" && mv "${FILE_LIST}.tmp" "${FILE_LIST}"

# Filter out junk and build artifacts
awk '
    /(^|\/)\._[^\/]*$/ { next }
    /(^|\/)\.DS_Store$/ { next }
    /(^|\/)__MACOSX(\/|$)/ { next }
    /(^|\/)\.git(\/|$)/ { next }
    /^build\// { next }
    /^cmake-build-/ { next }
    /\/_deps\// { next }
    /CMakeCache\.txt/ { next }
    /CMakeFiles\// { next }
    /cmake_install\.cmake/ { next }
    /CTestTestfile\.cmake/ { next }
    /Testing\// { next }
    /\.ninja_/ { next }
    /build\.ninja/ { next }
    /rules\.ninja/ { next }
    /\.o$/ { next }
    /\.obj$/ { next }
    /\.a$/ { next }
    /\.lib$/ { next }
    /\.so/ { next }
    /\.dylib$/ { next }
    /\.dll$/ { next }
    /\.exe$/ { next }
    /\.app$/ { next }
    /vcpkg_installed\// { next }
    /prebuilt\// { next }
    { print }
' "${FILE_LIST}" > "${FILE_LIST}.tmp" && mv "${FILE_LIST}.tmp" "${FILE_LIST}"

FILE_COUNT="$(wc -l < "${FILE_LIST}" | tr -d " ")"
echo "  Files to archive: ${FILE_COUNT}"

if [[ "${FILE_COUNT}" == "0" ]]; then
    echo "ERROR: file list is empty" >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# Step 2. Create archive
# ---------------------------------------------------------------------------
echo "=== Step 2: Creating archive ==="

export COPYFILE_DISABLE=1
export COPY_EXTENDED_ATTRIBUTES_DISABLE=1

mkdir -p "$(dirname "${ARCHIVE_PATH}")"

tar -czf "${ARCHIVE_PATH}" \
    --format=ustar \
    -C "${PROJECT_DIR}" \
    -T "${FILE_LIST}"

ARCHIVE_SIZE="$(du -h "${ARCHIVE_PATH}" | cut -f1)"
echo "  Branch:   ${SANITIZED_BRANCH}"
echo "  Archive:  ${ARCHIVE_NAME} (${ARCHIVE_SIZE})"

# ---------------------------------------------------------------------------
# Step 3. Verify archive contents
# ---------------------------------------------------------------------------
echo "=== Step 3: Verifying archive ==="

ERRORS=0
ARCHIVE_LIST="$(mktemp)"
tar -tzf "${ARCHIVE_PATH}" > "${ARCHIVE_LIST}"

if ! grep -qE '^src/|\.cpp$|\.hpp$' "${ARCHIVE_LIST}"; then
    echo "ERROR: archive does not contain source files" >&2
    ERRORS=$((ERRORS + 1))
fi

if grep -qE '(^|/)\.git/' "${ARCHIVE_LIST}"; then
    echo "ERROR: archive contains .git directory" >&2
    ERRORS=$((ERRORS + 1))
fi

if grep -q '^build/' "${ARCHIVE_LIST}"; then
    echo "ERROR: archive contains build/ directory" >&2
    ERRORS=$((ERRORS + 1))
fi

if grep -q 'prebuilt/' "${ARCHIVE_LIST}"; then
    echo "ERROR: archive contains prebuilt/ directory" >&2
    ERRORS=$((ERRORS + 1))
fi

if grep -q 'vcpkg_installed/' "${ARCHIVE_LIST}"; then
    echo "ERROR: archive contains vcpkg_installed/ directory" >&2
    ERRORS=$((ERRORS + 1))
fi

JUNK="$(grep -E '(^|/)\._[^/]*$|(^|/)\.DS_Store$|(^|/)__MACOSX(/|$)' \
    "${ARCHIVE_LIST}" || true)"
if [[ -n "${JUNK}" ]]; then
    echo "ERROR: archive contains macOS junk files:" >&2
    echo "${JUNK}" >&2
    ERRORS=$((ERRORS + 1))
fi

rm -f "${ARCHIVE_LIST}"

if [[ "${ERRORS}" -gt 0 ]]; then
    echo "ERROR: ${ERRORS} verification failure(s) — deleting archive" >&2
    rm -f "${ARCHIVE_PATH}"
    exit 1
fi

echo "  Archive content verification PASSED"

# ---------------------------------------------------------------------------
# Step 4. Report
# ---------------------------------------------------------------------------
echo ""
echo "=== Archive created successfully ==="
echo "  Name:       ${ARCHIVE_NAME}"
echo "  Path:       ${ARCHIVE_PATH}"
echo "  Size:       ${ARCHIVE_SIZE}"
echo "  Files:      ${FILE_COUNT}"
