# _require_make.sh — shared Bash guard for internal tools scripts.
#
# Source this file, then call:
#   require_make_target <expected_target> [additional_allowed_targets...]
#
# It verifies that the script is running under `make <expected_target>`.
# Uses MAKELEVEL + OPENDIS2_MAKE_TARGET + a make-created guard token. Process
# tree inspection is used when available, but sandboxed environments can block ps.
# If not valid, prints the correct command and exits with code 2.

require_make_target() {
    local expected_target="$1"
    local actual_target="${OPENDIS2_MAKE_TARGET:-}"
    local level="${MAKELEVEL:-0}"
    local guard="${OPENDIS2_MAKE_GUARD:-}"

    local make_found=false
    if [[ "${level}" -gt 0 ]]; then
        local pid="${PPID}"
        local i
        for ((i = 0; i < 6; i++)); do
            local cmd
            cmd="$(ps -o comm= -p "${pid}" 2>/dev/null || echo "")"
            if echo "${cmd}" | grep -qi "make"; then
                make_found=true
                break
            fi
            pid="$(ps -o ppid= -p "${pid}" 2>/dev/null | tr -d ' ' || echo "")"
            [[ -z "${pid}" ]] && break
        done
    fi

    local guard_found=false
    if [[ -n "${guard}" && -f "${guard}" ]]; then
        guard_found=true
    fi

    local target_allowed=false
    local candidate
    for candidate in "$@"; do
        if [[ "${actual_target}" == "${candidate}" ]]; then
            target_allowed=true
            break
        fi
    done

    if ! $target_allowed || [[ "${level}" -le 0 ]] || { ! $make_found && ! $guard_found; }; then
        local script_name
        script_name="$(basename "${BASH_SOURCE[1]:-$0}")"
        echo "ERROR: ${script_name} must not be run directly." >&2
        echo "Run instead:" >&2
        echo "  make ${expected_target}" >&2
        exit 2
    fi
}
