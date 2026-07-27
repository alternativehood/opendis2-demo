"""make_guard.py — shared Python guard for internal tools scripts.

Usage:
    from make_guard import require_make_target
    require_make_target("lint")

Verifies that the script is running under `make <expected_target>`.
Uses MAKELEVEL + OPENDIS2_MAKE_TARGET + a make-created guard token. Process
tree inspection is used when available, but sandboxed environments can block ps.
If invalid, prints the correct command and exits with code 2.
"""

import os
import subprocess
import sys


def _find_make_in_tree(max_depth: int = 6) -> bool:
    """Walk up the process tree looking for a 'make' process."""
    try:
        pid = os.getppid()
        for _ in range(max_depth):
            cmd = subprocess.run(
                ["ps", "-o", "comm=", "-p", str(pid)],
                capture_output=True,
                text=True,
                timeout=1,
            )
            name = cmd.stdout.strip()
            if "make" in name.lower():
                return True
            result = subprocess.run(
                ["ps", "-o", "ppid=", "-p", str(pid)],
                capture_output=True,
                text=True,
                timeout=1,
            )
            pid_str = result.stdout.strip()
            if not pid_str:
                break
            pid = int(pid_str)
    except (ValueError, subprocess.TimeoutExpired, OSError):
        pass
    return False


def require_make_target(expected_target: str, *additional_allowed_targets: str) -> None:
    level = int(os.environ.get("MAKELEVEL", "0"))
    actual_target = os.environ.get("OPENDIS2_MAKE_TARGET", "")
    guard = os.environ.get("OPENDIS2_MAKE_GUARD", "")

    make_found = level > 0 and _find_make_in_tree()
    guard_found = bool(guard) and os.path.isfile(guard)
    allowed_targets = (expected_target, *additional_allowed_targets)

    if actual_target not in allowed_targets or level <= 0 or not (make_found or guard_found):
        script_name = os.path.basename(sys.argv[0])
        print(f"ERROR: {script_name} must not be run directly.", file=sys.stderr)
        print("Run instead:", file=sys.stderr)
        print(f"  make {expected_target}", file=sys.stderr)
        sys.exit(2)
