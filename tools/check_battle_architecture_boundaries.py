#!/ usr / bin / env python3
"""Check battle architecture include boundaries — directory-level, CMake source list,
and CMake target link deps."""

from pathlib import Path
import re
import sys

import os
_guard_dir = os.path.dirname(os.path.realpath(__file__))
sys.path.insert(0, _guard_dir)
from make_guard import require_make_target
require_make_target("lint", "lint-fix")

ROOT = Path(__file__).resolve().parents[1]
INCLUDE_RE = re.compile(r'^\s*#\s*include\s+[<"]([^>"]+)[>"]')

# ── Per - file include rules(fine - grained, file - pattern based) ─────────────────
FILE_RULES = [
    (
        "animation primitives stay platform/asset free",
        ["src/d2engine/animation/**/*.hpp", "src/d2engine/animation/**/*.cpp"],
        ["SDL", "raw_resource_loader", "game_texture_cache"],
    ),
    (
        "battle domain events stay visual/render/backend free",
        [
            "src/d2engine/battle_view/battle_ids.hpp",
            "src/d2engine/battle_view/battle_slot.hpp",
            "src/d2engine/battle_view/battle_slot.cpp",
            "src/d2engine/battle_view/battle_visual_event.hpp",
        ],
        ["SDL", "battle_render_snapshot", "raw_resource_loader", "game_texture_cache"],
    ),
    (
        "visual runtime stays platform/asset free",
        [
            "src/d2engine/battle_view/battle_scene.*",
            "src/d2engine/battle_view/battle_unit.hpp",
            "src/d2engine/battle_view/visual_*.hpp",
            "src/d2engine/battle_view/visual_*.cpp",
            "src/d2engine/battle_view/life_visual_state.hpp",
            "src/d2engine/battle_view/track_render_layer.hpp",
        ],
        ["SDL", "raw_resource_loader", "game_texture_cache"],
    ),
    (
        "timeline and engine stay platform/asset free",
        [
            "src/d2engine/battle_view/command_timeline.*",
            "src/d2engine/battle_view/battle_animation_engine.*",
        ],
        ["SDL", "raw_resource_loader", "game_texture_cache"],
    ),
    (
        "D2 scripts stay backend free",
        [
            "src/d2engine/battle_view/battle_animation_scripts.*",
            "src/d2engine/battle_view/battle_effect_role_set.hpp",
            "src/d2engine/battle_view/death_visual_assets.hpp",
            "src/d2engine/battle_view/unit_animation_role_set.hpp",
            "src/d2engine/battle_view/unit_animations.hpp",
        ],
        ["SDL", "raw_resource_loader", "game_texture_cache"],
    ),
    (
        "render core cannot mutate animation state",
        [
            "src/d2engine/battle_view/battle_anchor_resolver.*",
            "src/d2engine/battle_view/battle_renderer.*",
            "src/d2engine/battle_view/battle_render_snapshot.hpp",
            "src/d2engine/battle_view/battle_texture_provider.hpp",
        ],
        ["command_timeline", "battle_animation_engine", "battle_presenter"],
    ),
    (
        "tuning state is data-only: must not pull in concrete factory",
        ["src/d2engine/app/battle_tuning_state.hpp"],
        ["battle_unit_factory"],
    ),
]

# ── Core target : forbidden include patterns and filename patterns ─────────────
#Forbidden tokens in DIRECT #include lines of core source files.
CORE_INCLUDE_FORBIDDEN = [
    "../assets/",
    "assets/",
    "../render/",
    "render/",
    "../app/",
    "app/",
    "SDL3/SDL.h",
    "SDL3/SDL_",
    "renderer2d.hpp",
    "sdl_texture.hpp",
    "game_texture_cache.hpp",
    "raw_resource_loader.hpp",
    "d2res/",
    "battle_viewer_action",
    "battle_presenter",
    "battle_unit_factory",
    "battle_debug_scene_controller",
    "battle_selection_controller",
    "debug_battle_outcome_resolver",
    "game_data_registry",
    "portrait_manifest",
]

#Forbidden filename substrings for files in the core source list.
CORE_FILENAME_FORBIDDEN = [
    "battle_presenter",
    "battle_debug_scene_controller",
    "debug_battle_outcome_resolver",
    "battle_selection_controller",
    "battle_render_command_builder",
    "battle_renderer",
    "battle_startup_texture_warmup",
    "battle_unit_factory",
    "battle_layout",
    "bottom_hud",
    "formation_status_panel",
    "portrait_render_item",
    "sdl_battle_renderer",
    "sdl_battle_texture_provider",
]

# ── CMake target link dependency rules ────────────────────────────────────────
#Targets that must NOT link certain libs.
CMAKE_LINK_RULES = [
    ("libd2battle_core",        {"SDL3::SDL3", "libd2res", "lodepng_lib", "libd2render_sdl",
                                 "libd2battle_d2_adapter", "libd2battle_sdl_renderer",
                                 "libd2assets_runtime", "libd2game_data", "libd2portrait_data"}),
    ("libd2battle_d2_adapter",  {"SDL3::SDL3"}),
    ("libd2battle_rules",       {"SDL3::SDL3", "libd2battle_core", "libd2battle_d2_adapter",
                                 "libd2battle_sdl_renderer", "libd2render_sdl", "libd2render_core",
                                 "libd2engine", "libd2adventure_render", "imgui_lib"}),
]

#Targets that MUST link certain libs(honesty check).
CMAKE_REQUIRED_LINKS = [
    ("libd2render_sdl",          "libd2assets_runtime"),  # texture_cache/game_texture_cache use RawResourceLoader/RuntimeAssetContext
    ("libd2render_sdl",          "libd2res"),              # game_texture_cache.cpp uses d2res symbols directly
    ("libd2battle_d2_adapter",   "libd2assets_runtime"),  # raw_ff_animation_catalog/raw_animation_role_resolver_factory use RawResourceLoader
    ("libd2battle_d2_adapter",   "libd2game_data"),       # battle_unit_factory.cpp uses GameDataRegistry symbols
    ("libd2battle_sdl_renderer", "libd2portrait_data"),   # formation_status_panel/portrait_render_item use PortraitManifestIndex
    ("libd2battle_rules",        "libd2runtime"),         # uses AdventureWorldState/AdventureStack
    ("libd2battle_rules",        "libd2game_data"),       # uses GameDataRegistry/UnitDef/AttackDef
    ("libd2battle_rules",        "libd2log"),             # uses d2log logging
]


def files_for(patterns):
    seen = set()
    for pattern in patterns:
        for path in ROOT.glob(pattern):
            if path.is_file() and path not in seen:
                seen.add(path)
                yield path


def parse_cmake_core_sources(cmake_path: Path) -> list:
    """Extract source files listed in add_library(libd2battle_core STATIC ...) block."""
    text = re.sub(r'#[^\n]*', '', cmake_path.read_text(encoding="utf-8"))
    m = re.search(
        r'add_library\s*\(\s*libd2battle_core\s+STATIC\s+(.*?)\)', text, re.DOTALL)
    if not m:
        return []
    return [tok.strip() for tok in m.group(1).split() if tok.strip().endswith('.cpp')]


def check_file_rules():
    violations = []
    for rule_name, patterns, forbidden in FILE_RULES:
        for path in files_for(patterns):
            for line_no, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
                match = INCLUDE_RE.match(line)
                if not match:
                    continue
                include = match.group(1)
                for token in forbidden:
                    if token in include:
                        rel = path.relative_to(ROOT)
                        violations.append(f"{rel}:{line_no}: {rule_name}: includes {include}")
    return violations


def check_core_source_list(cmake_path: Path):
    """Parse core source list from CMakeLists.txt and check each file for violations."""
    violations = []
    src_root = ROOT / "src/d2engine"
    sources = parse_cmake_core_sources(cmake_path)
    if not sources:
        violations.append("CMake: could not find libd2battle_core source list")
        return violations

    for rel_src in sources:
#Filename check : core must not contain presenter / debug / render / adapter files
        basename = Path(rel_src).name
        for pattern in CORE_FILENAME_FORBIDDEN:
            if pattern in basename:
                violations.append(
                    f"CMake: libd2battle_core source list contains forbidden file "
                    f"{rel_src!r} (matches pattern {pattern!r})"
                )

#Include check : verify no direct #include of forbidden tokens in the.cpp
        path = src_root / rel_src
        if not path.is_file():
            violations.append(f"CMake: libd2battle_core lists {rel_src!r} but file not found")
            continue
        for line_no, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            m = INCLUDE_RE.match(line)
            if not m:
                continue
            include = m.group(1)
            for token in CORE_INCLUDE_FORBIDDEN:
                if token in include:
                    violations.append(
                        f"src/d2engine/{rel_src}:{line_no}: core-source forbidden include: "
                        f"{include!r} (token={token!r})"
                    )

    return violations

#Forbidden tokens for TRANSITIVE includes(one header deep from core.cpp).
#Narrower than CORE_INCLUDE_FORBIDDEN : only catches the key layer violations
#(presenter / factory / SDL / D2res) that must never appear even transitively.
CORE_TRANSITIVE_FORBIDDEN = [
    "battle_presenter",
    "battle_unit_factory",
    "battle_debug_scene_controller",
    "battle_selection_controller",
    "debug_battle_outcome_resolver",
    "SDL3/SDL.h",
    "SDL3/SDL_",
    "renderer2d.hpp",
    "sdl_texture.hpp",
    "game_texture_cache.hpp",
    "raw_resource_loader.hpp",
    "runtime_asset_context.hpp",
    "game_data_registry.hpp",
    "portrait_manifest.hpp",
    "portrait_manifest_index.hpp",
    "d2res/",
]


def get_direct_includes(path: Path, src_root: Path) -> list:
    """Return list of (resolved_path, include_str) for project-local #includes in path."""
    result = []
    for line in path.read_text(encoding="utf-8").splitlines():
        m = INCLUDE_RE.match(line)
        if not m:
            continue
        inc = m.group(1)
#Only consider relative project includes(not<system> or absolute)
        if inc.startswith('<'):
            continue
        candidate = (path.parent / inc).resolve()
        if candidate.is_file() and str(candidate).startswith(str(src_root)):
            result.append((candidate, inc))
    return result


def check_core_transitive_includes(cmake_path: Path):
    """Check that headers directly included by core .cpp files are also free of forbidden tokens."""
    violations = []
    src_root = ROOT / "src/d2engine"
    sources = parse_cmake_core_sources(cmake_path)
    if not sources:
        return violations

    for rel_src in sources:
        cpp_path = src_root / rel_src
        if not cpp_path.is_file():
            continue
        for hdr_path, hdr_inc in get_direct_includes(cpp_path, src_root):
            for line_no, line in enumerate(hdr_path.read_text(encoding="utf-8").splitlines(), 1):
                m2 = INCLUDE_RE.match(line)
                if not m2:
                    continue
                nested = m2.group(1)
                for token in CORE_TRANSITIVE_FORBIDDEN:
                    if token in nested:
                        rel_hdr = hdr_path.relative_to(ROOT)
                        violations.append(
                            f"{rel_hdr}:{line_no}: core-transitive forbidden include "
                            f"(via src/d2engine/{rel_src}): {nested!r} (token={token!r})"
                        )
    return violations


def check_cmake_link_deps(cmake_path: Path):
    """Parse CMakeLists.txt and verify no forbidden links in target_link_libraries."""
    text = re.sub(r'#[^\n]*', '', cmake_path.read_text(encoding="utf-8"))
    violations = []
    for target, forbidden_links in CMAKE_LINK_RULES:
        pattern = re.compile(
            rf'target_link_libraries\s*\(\s*{re.escape(target)}\b(.*?)\)',
            re.DOTALL
        )
        for m in pattern.finditer(text):
            block = m.group(1)
            for lib in forbidden_links:
                if lib in block:
                    violations.append(
                        f"CMake: {target} must not link {lib!r} "
                        f"(found in target_link_libraries block)"
                    )
    return violations


def check_cmake_required_links(cmake_paths):
    """Verify that targets explicitly declare their required direct dependencies.
       Searches across all provided CMakeLists files as a single scope."""
    combined_text = ""
    for p in cmake_paths:
        combined_text += re.sub(r'#[^\n]*', '', p.read_text(encoding="utf-8")) + "\n"
    violations = []
    for target, required_lib in CMAKE_REQUIRED_LINKS:
        pattern = re.compile(
            rf'target_link_libraries\s*\(\s*{re.escape(target)}\b(.*?)\)',
            re.DOTALL
        )
        found = False
        for m in pattern.finditer(combined_text):
            if required_lib in m.group(1):
                found = True
                break
        if not found:
            violations.append(
                f"CMake: {target} must explicitly link {required_lib!r} "
                f"(uses its symbols directly, dependency must be declared)"
            )
    return violations


def check_rules_effect_module_boundaries():
    """Effect modules must not include effect_dispatch.hpp (only effect_dispatch.cpp may include them)."""
    violations = []
    rules_dir = ROOT / "src/d2battle_rules/detail"
    effect_modules = [
        "damage_effect.hpp", "damage_effect.cpp",
        "drain.hpp", "drain.cpp",
        "drain_overflow.hpp", "drain_overflow.cpp",
        "healing_primitive.hpp", "healing_primitive.cpp",
        "petrify.hpp", "petrify.cpp",
        "unit_effects.hpp", "unit_effects.cpp",
    ]
    for fname in effect_modules:
        path = rules_dir / fname
        if not path.is_file():
            continue
        text = path.read_text(encoding="utf-8")
        if "effect_dispatch.hpp" in text:
            rel = path.relative_to(ROOT)
            violations.append(f"{rel}: effect module must not include effect_dispatch.hpp")
    return violations


def check_scheduler_module_boundary():
    """Scheduler (battle_turn) must not include effect or petrify headers."""
    violations = []
    rules_dir = ROOT / "src/d2battle_rules/detail"
    forbidden = ["unit_effects.hpp", "petrify.hpp", "battle_effect.hpp"]
    for fname in ["battle_turn.hpp", "battle_turn.cpp"]:
        path = rules_dir / fname
        if not path.is_file():
            continue
        text = path.read_text(encoding="utf-8")
        for token in forbidden:
            if token in text:
                rel = path.relative_to(ROOT)
                violations.append(f"{rel}: scheduler must not include {token}")
        for sym in ["PetrifiedEffect", "SkipActivationAction", "is_petrified"]:
            if sym in text:
                rel = path.relative_to(ROOT)
                violations.append(f"{rel}: scheduler must not mention {sym}")
    return violations


def main():
    cmake_path = ROOT / "src/d2engine/CMakeLists.txt"
    root_cmake = ROOT / "CMakeLists.txt"
    violations = []
    violations += check_file_rules()
    violations += check_core_source_list(cmake_path)
    violations += check_core_transitive_includes(cmake_path)
    violations += check_cmake_link_deps(cmake_path)
    violations += check_cmake_link_deps(root_cmake)
    violations += check_cmake_required_links([cmake_path, root_cmake])
    violations += check_rules_source_includes()
    violations += check_rules_unchecked_entrypoint()
    violations += check_rules_effect_module_boundaries()
    violations += check_scheduler_module_boundary()

    if violations:
        print("battle architecture boundary violations:", file=sys.stderr)
        for v in violations:
            print(f"  {v}", file=sys.stderr)
        return 1

    print("battle architecture boundary check: OK")
    return 0

RULES_FORBIDDEN_INCLUDES = [
    "SDL3/SDL.h", "SDL3/SDL_", "battle_presenter", "battle_scene",
    "battle_screen", "battle_viewer", "screen_manager", "screen\\.hpp",
    "renderer2d", "sdl_texture", "game_texture_cache",
    "battle_animation_engine", "battle_scenario_executor", "battle_scenario_runtime",
    "command_timeline", "visual_entity", "visual_track",
    "battle_ids\\.hpp",
    "app/", "../app/", "render/", "../render/",
    "battle_view/", "battle_adapters/",
]


def check_rules_source_includes():
    """Check src/d2battle_rules/ for forbidden include dependencies."""
    violations = []
    rules_dir = ROOT / "src/d2battle_rules"
    if not rules_dir.is_dir():
        return violations
    for ext in ("*.cpp", "*.hpp"):
        for path in rules_dir.rglob(ext):
            for line_no, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
                m = INCLUDE_RE.match(line)
                if not m:
                    continue
                inc = m.group(1)
                for token in RULES_FORBIDDEN_INCLUDES:
                    if re.search(token, inc):
                        rel = path.relative_to(ROOT)
                        violations.append(
                            f"{rel}:{line_no}: rules engine forbidden include: {inc!r}"
                        )
                        break
    return violations


def check_rules_unchecked_entrypoint():
    """Public headers in src/d2battle_rules/ must not export unchecked internal entrypoints."""
    violations = []
    rules_dir = ROOT / "src/d2battle_rules"
    if not rules_dir.is_dir():
        return violations
    forbidden = [
        "validate_action_on_valid_state",
        "apply_validated_action_on_valid_state",
        "resolve_validated_attack_context",
        "dispatch_attack_effect",
        "apply_raw_damage_to_targets",
        "resolve_unit_max_hp",
        "valid_actions_on_valid_state",
        "heal_alive_unit_up_to_max",
        "resolve_drain_effect",
        "resolve_petrify_effect",
        "apply_or_refresh_petrified",
        "consume_one_petrified_activation_skip",
        "clear_transient_effects_on_death",
        "find_petrified_effect",
        "is_petrified",
    ]
    for path in rules_dir.glob("*.hpp"):
        if "detail" in path.parts:
            continue
        text = path.read_text(encoding="utf-8")
        for name in forbidden:
            if name in text:
                violations.append(
                    f"{path.relative_to(ROOT)}: "
                    f"public header exports unchecked {name}"
                )
    return violations


if __name__ == "__main__":
    raise SystemExit(main())
