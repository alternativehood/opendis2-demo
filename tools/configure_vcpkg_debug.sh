#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# configure_vcpkg_debug.sh — Configure opendis2 for vcpkg manifest debug build.
#
# Usage:
#   ./tools/configure_vcpkg_debug.sh
#
# Environment:
#   VCPKG_ROOT            — must be set to vcpkg installation root
#   VCPKG_TARGET_TRIPLET  — optional, auto-detected: arm64-osx / x64-osx
#
# Flags (pass through to CMake):
#   -DD2_ENABLE_ENGINE=OFF  — disable SDL3 engine deps
#   -DD2_USE_FETCHCONTENT_FALLBACK=ON  — enable network fallback
#
# If the build dir exists and is stale, the script prints a hint rather than
# deleting it automatically. To force a re-configure from scratch:
#
#   rm -rf build/vcpkg-debug && ./tools/configure_vcpkg_debug.sh
# ---------------------------------------------------------------------------
set -euo pipefail

REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$REPO_DIR/build/vcpkg-debug"

# ------ VCPKG_ROOT validation -----------------------------------------------
if [ -z "${VCPKG_ROOT:-}" ]; then
    echo "ERROR: VCPKG_ROOT is not set."
    echo ""
    echo "  export VCPKG_ROOT=\"\$HOME/dev/vcpkg\""
    echo "  export PATH=\"\$VCPKG_ROOT:\$PATH\""
    echo ""
    echo "Then re-run this script."
    exit 1
fi

TOOLCHAIN="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
if [ ! -f "$TOOLCHAIN" ]; then
    echo "ERROR: vcpkg toolchain not found at:"
    echo "  $TOOLCHAIN"
    echo ""
    echo "Verify VCPKG_ROOT points to a valid vcpkg installation."
    exit 1
fi

# ------ Architecture triplet detection ---------------------------------------
ARCH="$(uname -m)"
case "$ARCH" in
    arm64)  DEFAULT_TRIPLET="arm64-osx" ;;
    x86_64) DEFAULT_TRIPLET="x64-osx" ;;
    *)
        echo "ERROR: unknown architecture '$ARCH' (expected arm64 or x86_64)"
        exit 1
        ;;
esac
TRIPLET="${VCPKG_TARGET_TRIPLET:-$DEFAULT_TRIPLET}"

# ------ Stale build dir hint -------------------------------------------------
if [ -f "$BUILD_DIR/CMakeCache.txt" ]; then
    echo "NOTICE: Build directory '$BUILD_DIR' already exists."
    echo "  If the previous configuration was done without vcpkg toolchain,"
    echo "  delete it first and re-run:"
    echo ""
    echo "    rm -rf '$BUILD_DIR' && $0"
    echo ""
fi

# ------ Configure ------------------------------------------------------------
echo "Configuring opendis2 with vcpkg (triplet=$TRIPLET)..."
echo "  Build dir:  $BUILD_DIR"
echo "  Toolchain:  $TOOLCHAIN"
echo ""

cmake -S "$REPO_DIR" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
    -DD2_USE_FETCHCONTENT_FALLBACK=OFF \
    -DVCPKG_TARGET_TRIPLET="$TRIPLET" \
    "$@"

echo ""
echo "Done. Build with:"
echo ""
echo "  cmake --build $BUILD_DIR --target opendis2-dev-tests"
  echo "  ctest --test-dir $BUILD_DIR -LE integration --output-on-failure"
