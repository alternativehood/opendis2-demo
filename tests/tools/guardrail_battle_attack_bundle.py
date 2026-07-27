#!/usr/bin/env python3
"""Guard the battle attack bundle architecture.

This checks the production bundle surface, rejects the old standalone
primary/secondary paths, and verifies the mandatory regression tests exist.
The checks are text-based but do not depend on whitespace or line numbers.
"""

from __future__ import annotations

import pathlib
import re
import sys
from typing import Optional


ROOT = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else None

REQUIRED_PRESENT = {
    "src/d2engine/assets/game_data_registry.cpp": [r"canonical_optional_attack_id"],
    "src/d2battle_rules/attack_support.cpp": [r"analyze_attack_bundle"],
    "src/d2battle_rules/detail/bundle_support.hpp": [r"ResolvedAttackBundleDefinition",
                                                      r"require_supported_attack_bundle",
                                                      r"enumerate_bundle_unit_targets"],
    "src/d2battle_rules/detail/bundle_support.cpp": [r"enumerate_bundle_unit_targets"],
    "src/d2battle_rules/detail/resolved_attack_context.hpp": [r"ResolvedAttackBundleContext",
                                                               r"resolve_validated_attack_bundle_context"],
    "src/d2battle_rules/detail/resolved_attack_context.cpp": [r"resolve_validated_attack_bundle_context"],
    "src/d2battle_rules/battle_valid_actions.cpp": [r"enumerate_bundle_unit_targets"],
    "tests/unit/test_battle_composite_attacks.cpp": [
        r"make_healrevive_large_state",
        r"SecondaryAttackNeverEmitsStandaloneAction",
        r"UnsupportedPrimaryBlocksSupportedSecondary",
        r"UnsupportedSecondaryBlocksSupportedPrimary",
        r"UnsupportedSecondaryMatrixBlocksPrimaryExecution",
        r"HealCureBundleEmitsOneCompositeAction",
        r"HealCureBundleExecutesBothComponentsInOneSuccessor",
        r"HealCureBundleAdvancesTurnExactlyOnce",
        r"HealCureBundleProcessesLargeAllyOnce",
        r"HealReviveBundleEnumeratesAliveAndDeadAllies",
        r"HealReviveAliveTargetExecutesHealOnly",
        r"HealReviveDeadTargetExecutesReviveOnly",
        r"HealReviveBundleAdvancesTurnExactlyOnce",
        r"HealReviveBundleProcessesLargeTargetOnce",
        r"CompositeActionRejectsWrongTargetRelation",
        r"CompositeActionRejectsIncompatibleReach",
        r"CompositeActionRequiresAtLeastOneApplicableComponent",
        r"CompositeActionPresentationShowsBothComponents",
        r"SecondaryComponentDropsTargetsKilledByPrimary",
        r"CompositeActionRejectsWrongTargetShape",
    ],
    "tests/unit/test_battle_sweep.cpp": [r"NoActionsDiagnosticReportsWholeBundleFailure"],
}

REQUIRED_CMAKE = [
    r"tests/unit/test_battle_composite_attacks\.cpp",
    r"tests/unit/test_battle_sweep\.cpp",
]

PRODUCTION_SCAN_DIRS = [
    "src/d2battle_rules",
    "src/opendis2_battle",
    "src/opendis2_battle_sweep",
]


def read(path: pathlib.Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def fail(msg: str, violations: list[str]) -> int:
    print(f"ERROR: {msg}")
    for v in violations:
        print(f"  {v}")
    return 1


def extract_test_body(text: str, test_name: str) -> Optional[str]:
    needle = f"TEST(CompositeAttackTest, {test_name})"
    start = text.find(needle)
    if start < 0:
        return None

    brace_start = text.find("{", start)
    if brace_start < 0:
        return None

    depth = 0
    for idx in range(brace_start, len(text)):
        ch = text[idx]
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return text[brace_start + 1:idx]
    return None


def check_required(root: pathlib.Path) -> list[str]:
    violations: list[str] = []
    for rel, patterns in REQUIRED_PRESENT.items():
        path = root / rel
        if not path.exists():
            violations.append(f"{rel}: missing")
            continue
        text = read(path)
        for pat in patterns:
            if not re.search(pat, text, re.S):
                violations.append(f"{rel}: missing pattern {pat}")

    cmake = read(root / "CMakeLists.txt")
    for pat in REQUIRED_CMAKE:
        if not re.search(pat, cmake):
            violations.append(f"CMakeLists.txt: missing pattern {pat}")

    return violations


def check_production(root: pathlib.Path) -> list[str]:
    violations: list[str] = []
    bundle_files = [root / "src/d2engine/assets/game_data_registry.cpp"]
    for rel in PRODUCTION_SCAN_DIRS:
        base = root / rel
        if base.exists():
            bundle_files.extend(
                p for p in base.rglob("*") if p.is_file() and p.suffix in {".cpp", ".hpp", ".h"}
            )

    forbidden = {
        "find_attack_for_relation": [r"find_attack_for_relation"],
        "resolve_secondary_attack_context": [r"resolve_secondary_attack_context"],
        "resolve_validated_attack_context": [r"resolve_validated_attack_context"],
        "secondary fallback": [r"fallback.*secondary", r"secondary.*fallback"],
        "primary/secondary id equality skip": [r"primary_attack_id\s*==\s*secondary_attack_id",
                                                 r"secondary_attack_id\s*==\s*primary_attack_id"],
        "DBF sentinel in d2battle_rules": [r"g000000000"],
        "standalone bundle output labels": [r"write_attack_def_block"],
        "ordered union container in bundle enumeration": [r"std::set<[^>]+>\s+union_targets",
                                                            r"std::set<[^>]+>\s+ordered_targets"],
    }

    for path in bundle_files:
        if not path.exists():
            continue
        text = read(path)

        if path.name == "battle_action.hpp" and re.search(r"struct\s+AttackAction[\s\S]{0,160}\battack_id\b", text):
            violations.append(f"{path.relative_to(root)}: AttackAction must not contain attack_id")

        for label, patterns in forbidden.items():
            if label == "DBF sentinel in d2battle_rules" and not str(path).startswith(str(root / "src/d2battle_rules")):
                continue
            if label == "ordered union container in bundle enumeration" and path.name not in {"battle_valid_actions.cpp", "bundle_support.cpp"}:
                continue
            if label == "standalone bundle output labels" and path.name != "battle_log_writer.cpp":
                continue
            if label == "primary/secondary id equality skip" and path.name != "attack_support.cpp":
                continue
            if label == "secondary fallback" and path.name not in {"battle_valid_actions.cpp", "battle_action_validate.cpp", "bundle_support.cpp"}:
                continue
            for pat in patterns:
                if re.search(pat, text):
                    violations.append(f"{path.relative_to(root)}: forbidden pattern {label}: {pat}")
                    break

    return violations


def check_tests(root: pathlib.Path) -> list[str]:
    violations: list[str] = []

    composite = root / "tests/unit/test_battle_composite_attacks.cpp"
    if composite.exists():
        text = read(composite)
        if re.search(r"normalize_derived_side_state|normalize_battle_status|begin_round", text):
            violations.append(
                "tests/unit/test_battle_composite_attacks.cpp: forbidden legacy battle normalization helpers"
            )

        large_body = extract_test_body(text, "HealReviveBundleProcessesLargeTargetOnce")
        if large_body is not None:
            if "make_healrevive_state(" in large_body:
                violations.append(
                    "tests/unit/test_battle_composite_attacks.cpp: HealReviveBundleProcessesLargeTargetOnce must use make_healrevive_large_state"
                )
            if "make_healrevive_large_state(" not in large_body:
                violations.append(
                    "tests/unit/test_battle_composite_attacks.cpp: HealReviveBundleProcessesLargeTargetOnce must use make_healrevive_large_state"
                )
            if re.search(r"\.alive\s*=\s*false", large_body):
                violations.append(
                    "tests/unit/test_battle_composite_attacks.cpp: HealReviveBundleProcessesLargeTargetOnce must not mutate alive after bootstrap"
                )
            if re.search(r"\.current_hp\s*=\s*0", large_body):
                violations.append(
                    "tests/unit/test_battle_composite_attacks.cpp: HealReviveBundleProcessesLargeTargetOnce must not mutate current_hp after bootstrap"
                )
            if len(re.findall(r"cell_members", large_body)) < 2:
                violations.append(
                    "tests/unit/test_battle_composite_attacks.cpp: HealReviveBundleProcessesLargeTargetOnce must check both occupied cell_members entries"
                )
            if not re.search(r'std::count\(target_ids\.begin\(\),\s*target_ids\.end\(\),\s*"U1"\)\s*,\s*1u', large_body):
                violations.append(
                    "tests/unit/test_battle_composite_attacks.cpp: HealReviveBundleProcessesLargeTargetOnce must assert exact single UnitTarget for the large ID"
                )

        enumerate_body = extract_test_body(text, "HealReviveBundleEnumeratesAliveAndDeadAllies")
        if enumerate_body is not None:
            if re.search(r"\bstd::set\b", enumerate_body):
                violations.append(
                    "tests/unit/test_battle_composite_attacks.cpp: HealReviveBundleEnumeratesAliveAndDeadAllies must not use std::set"
                )
            if re.search(r"\bstd::sort\s*\(", enumerate_body):
                violations.append(
                    "tests/unit/test_battle_composite_attacks.cpp: HealReviveBundleEnumeratesAliveAndDeadAllies must not sort target IDs"
                )
            if re.search(r"auto\s+sorted_targets\s*=\s*targets|sorted_targets\.begin\(\)|sorted_targets\.end\(\)", enumerate_body):
                violations.append(
                    "tests/unit/test_battle_composite_attacks.cpp: HealReviveBundleEnumeratesAliveAndDeadAllies must not sort target IDs"
                )

        for test_name in ["HealReviveAliveTargetExecutesHealOnly", "HealReviveBundleAdvancesTurnExactlyOnce"]:
            body = extract_test_body(text, test_name)
            if body is not None and re.search(r"\bAttackAction\s+\w+\s*\{", body):
                violations.append(
                    f"tests/unit/test_battle_composite_attacks.cpp: {test_name} must not construct AttackAction directly"
                )
            if body is not None and re.search(r"BattleAction\s*\{\s*AttackAction\s*\{", body):
                violations.append(
                    f"tests/unit/test_battle_composite_attacks.cpp: {test_name} must not construct AttackAction directly"
                )

    battle_rules = root / "tests/unit/test_battle_rules.cpp"
    if battle_rules.exists():
        text = read(battle_rules)
        m = re.search(r"TEST\(AdjacentTest,\s*FrontTopToFrontBottomInvalid\)\s*\{([\s\S]*?)\n\}\n\nTEST\(", text)
        if m and re.search(r"EXPECT_THROW\s*\(\s*valid_actions\s*\(", m.group(1)):
            violations.append(
                "tests/unit/test_battle_rules.cpp: forbidden EXPECT_THROW(valid_actions(...)) in AdjacentTest.FrontTopToFrontBottomInvalid"
            )
        if m and not re.search(r"TargetOutOfAdjacentReach", m.group(1)):
            violations.append(
                "tests/unit/test_battle_rules.cpp: FrontTopToFrontBottomInvalid must assert TargetOutOfAdjacentReach"
            )
        if m and not re.search(r"acts\.empty\s*\(", m.group(1)):
            violations.append(
                "tests/unit/test_battle_rules.cpp: FrontTopToFrontBottomInvalid must assert valid_actions is empty"
            )
        if m and not re.search(r"compute_fingerprint\s*\(", m.group(1)):
            violations.append(
                "tests/unit/test_battle_rules.cpp: FrontTopToFrontBottomInvalid must assert fingerprint stability"
            )
        if not re.search(r"TEST\(AdjacentTest,\s*FrontTopToFrontMiddleAndBottomFiltersUnreachableTarget\)", text):
            violations.append(
                "tests/unit/test_battle_rules.cpp: missing FrontTopToFrontMiddleAndBottomFiltersUnreachableTarget regression"
            )
    return violations


def main() -> int:
    if ROOT is None:
        print("Usage: guardrail_battle_attack_bundle.py <project_root>")
        return 1
    if not ROOT.is_dir():
        print(f"Not a directory: {ROOT}")
        return 1

    violations = check_required(ROOT)
    violations.extend(check_production(ROOT))
    violations.extend(check_tests(ROOT))

    if violations:
        return fail("battle attack bundle violations", violations)

    print("OK: battle attack bundle architecture verified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
