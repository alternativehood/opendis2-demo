#!/usr/bin/env python3
"""Guardrail: no hardcoded Screen UI geometry. Enforces TreeLayout/config rules.

This script is invoked through guardrail_ui_layout.sh which handles
require_make_target checks. See tools/guardrail_ui_layout.sh.
"""

import os, re, sys, argparse
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("--root", type=Path, default=None, help="Override repo root (for testing)")
args = parser.parse_args()

ROOT = (args.root or Path(__file__).resolve().parent.parent)
SCREEN_DIR = ROOT / "src/d2engine/app"
CONFIGS_DIR = ROOT / "configs"
SCREENS_CONFIG_DIR = CONFIGS_DIR / "screens"

# ── World-space / asset-source identifiers — excluded from UI rect checks ────
WORLD_SOURCE_TOKENS = {
    "center_camera", "logical_viewport", "canvas_width", "canvas_height",
    "source_rect", "src_rect", "native_w", "native_h",
    "atlas_", "sprite_rect", "tex_coords",
    "SDL_SetRenderLogicalPresentation",
    "tile_", "iso_", "map_coord", "world_",
}

FAILED = False

def error(msg):
    global FAILED
    print(f"guardrail_ui_layout: ERROR: {msg}", file=sys.stderr)
    FAILED = True

def camel_to_snake(name):
    result = []
    for i, c in enumerate(name):
        if c.isupper() and i > 0:
            result.append('_'); result.append(c.lower())
        else:
            result.append(c.lower())
    return ''.join(result).lstrip('_')

# ── 1. Discover Screen subclasses from BOTH .hpp and .cpp ───────────────────
screen_classes = []
for ext in ("*.hpp", "*.cpp"):
    for f in sorted(SCREEN_DIR.glob(ext)):
        for m in re.finditer(r'class\s+(\w+)\s*.*:\s*public\s+Screen\b',
                             f.read_text()):
            screen_classes.append((m.group(1), f))

if not screen_classes:
    error("No Screen subclasses found")

# ── 2. Verify canonical filename convention ──────────────────────────────────
for name, src_file in screen_classes:
    expected = camel_to_snake(name).removesuffix("_screen") + "_screen"
    expected_hpp = expected + ".hpp"
    expected_cpp = expected + ".cpp"

    hpp = SCREEN_DIR / expected_hpp
    cpp = SCREEN_DIR / expected_cpp

    if src_file.suffix == ".cpp":
        error(f"{name} declared in {src_file.name}: must be declared in {expected_hpp}")
        continue

    if not src_file.name.startswith(expected):
        error(f"{name} in {src_file.name}: must be in {expected_hpp} per convention")

    if not cpp.exists():
        error(f"{name}: missing implementation {expected_cpp}")

# ── 3. Config existence for every Screen ─────────────────────────────────────
for name, src_file in screen_classes:
    expected = camel_to_snake(name).removesuffix("_screen") + "_screen"
    config_file = SCREENS_CONFIG_DIR / f"{expected}.json"
    if not config_file.exists():
        error(f"{name}: missing config {config_file}")

# ── 4. Check production Screen .cpp files for manual UI geometry ─────────────
for name, src_file in screen_classes:
    expected = camel_to_snake(name).removesuffix("_screen") + "_screen"
    cpp = SCREEN_DIR / f"{expected}.cpp"
    if not cpp.exists():
        continue

    lines = cpp.read_text().split('\n')

    for i, line in enumerate(lines, 1):
        stripped = line.strip()
        if stripped.startswith('//') or stripped.startswith('*'):
            continue

        has_world = any(t in stripped for t in WORLD_SOURCE_TOKENS)

        # Hardcoded layout constants
        if re.search(r'\bconstexpr.*k(Popup[WH]|Leader|Portrait[WH]|CellGap[XY])', stripped):
            error(f"{cpp.name}:{i}: hardcoded layout constant: {stripped}")

        # Direct Rect literal with numeric coords (not world-space)
        if re.search(r'\bRect\b.*\{[^}]*\b\d{2,}\b', stripped) and not has_world:
            error(f"{cpp.name}:{i}: direct UI Rect: {stripped}")

        # Manual grid/pitch UI arithmetic
        if re.search(r'\b(origin_[xy]|popup_[xy]|formation_|leader_art_)\b', stripped):
            error(f"{cpp.name}:{i}: manual UI variable: {stripped}")

        # Screen-local layout/geometry helper classes
        if re.search(r'\bclass\s+(\w*(Layout|Geometry|Placement)\w*)', stripped):
            error(f"{cpp.name}:{i}: screen-local layout class: {stripped}")

    # ── 5. Local-variable destination Rect bypass detection ───────────────────
    # Find patterns like: int x=30; int y=40; ...Rect{x,y,w,h}
    # or: float px=..., py=...; draw_texture(tex, Rect{px, py, ...})
    full_text = cpp.read_text()
    # Check for multiple named int/float coords + Rect{} or {x,y,...} pattern
    coord_vars = set(re.findall(r'\b(int|float)\s+([a-z_]+)\s*=\s*\d+\s*;', full_text))
    coord_names = {v[1] for v in coord_vars}
    if len(coord_names) >= 2:
        # Find lines where these coordinate vars appear inside a Rect or brace initializer
        for i, line in enumerate(lines, 1):
            stripped = line.strip()
            if stripped.startswith('//'):
                continue
            if has_world:
                continue
            if re.search(r'\bRect\b', stripped) and any(
                re.search(rf'\b{v}\b', stripped) for v in coord_names):
                error(f"{cpp.name}:{i}: local-variable UI Rect: {stripped}")

# ── 6. Config file location enforcement ──────────────────────────────────────
for json_file in CONFIGS_DIR.rglob("*.json"):
    if json_file.parent == CONFIGS_DIR and json_file.name != ".DS_Store":
        error(f"production config at configs/ root: {json_file.name} "
              f"(must be under configs/screens/)")

for json_file in (ROOT / "src").rglob("*screen*.json"):
    if str(json_file.parent).startswith(str(SCREENS_CONFIG_DIR)):
        continue
    error(f"duplicate screen layout config outside configs/screens/: {json_file}")

if FAILED:
    print("guardrail_ui_layout: FAILED", file=sys.stderr)
    print("UI destination geometry must live in configs/ and be resolved through Screen TreeLayout.",
          file=sys.stderr)
    sys.exit(1)

print("guardrail_ui_layout: OK")
