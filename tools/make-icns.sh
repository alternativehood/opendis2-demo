#!/bin/bash
# Generate .icns from a source PNG using macOS iconutil + sips.
# Usage: make-icns.sh <source_png> <output_icns>
#
# Requires: macOS (sips + iconutil)
# The source PNG should ideally be >=1024x1024 square for best results.
# All standard icns sizes are generated from the single source.

set -euo pipefail

if [[ $# -ne 2 ]]; then
    echo "Usage: $0 <source_png> <output_icns>" >&2
    exit 1
fi

SRC="$1"
OUT="$2"

if [[ ! -f "$SRC" ]]; then
    echo "Error: source file not found: "$SRC"" >&2
    exit 1
fi

# Ensure output directory exists
mkdir -p "$(dirname "$OUT")"

# Create temporary iconset directory
TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

ICONSET="$TMPDIR/icon.iconset"
mkdir -p "$ICONSET"

# Generate all standard macOS icon sizes from the source.
# iconutil requires exact pixel dimensions matching the filename convention.
# Format: icon_{width}x{height}[@2x].png  where @2x = 2x resolution.
sips -Z 16   "$SRC" --out "$ICONSET/icon_16x16.png"        >/dev/null
sips -Z 32   "$SRC" --out "$ICONSET/icon_16x16@2x.png"     >/dev/null
sips -Z 32   "$SRC" --out "$ICONSET/icon_32x32.png"        >/dev/null
sips -Z 64   "$SRC" --out "$ICONSET/icon_32x32@2x.png"     >/dev/null
sips -Z 128  "$SRC" --out "$ICONSET/icon_128x128.png"      >/dev/null
sips -Z 256  "$SRC" --out "$ICONSET/icon_128x128@2x.png"   >/dev/null
sips -Z 256  "$SRC" --out "$ICONSET/icon_256x256.png"      >/dev/null
sips -Z 512  "$SRC" --out "$ICONSET/icon_256x256@2x.png"   >/dev/null
sips -Z 512  "$SRC" --out "$ICONSET/icon_512x512.png"      >/dev/null
sips -Z 1024 "$SRC" --out "$ICONSET/icon_512x512@2x.png"   >/dev/null

# Convert iconset to icns
iconutil -c icns "$ICONSET" -o "$OUT"

echo "Generated: $OUT" >&2
