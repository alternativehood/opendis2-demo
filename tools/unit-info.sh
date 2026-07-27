#!/usr/bin/env bash
# Show full unit info: parameters, animations, death animation mapping.
# Usage: ./tools/unit-info.sh "Unit Name"  (English, case-insensitive, partial ok)
# Or:    ./tools/unit-info.sh --id UU0068

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BIN="$REPO_ROOT/build/verify/opendis2-dev-extractor"
D2="${D2_PATH:-/path/to/disciples2}"
CACHE_DIR="/tmp/d2unit-info-cache"

die() { echo "ERROR: $*" >&2; exit 1; }

[[ -f "$BIN" ]] || die "opendis2-dev-extractor not found at $BIN — run 'cmake --build build/verify --target opendis2-dev-extractor' first"
[[ -d "$D2/Globals" ]] || die "D2 game directory not found at D2_PATH=$D2"

# ── ensure cache ──────────────────────────────────────────────────────────────
extract_dbf() {
    local dbf="$1" dir="$2"
    mkdir -p "$dir"
    [[ -n "$(ls "$dir"/*.records.json 2>/dev/null)" ]] && return
    "$BIN" extract-dbf "$dbf" --out "$dir" 2>/dev/null
}

mkdir -p "$CACHE_DIR"
extract_dbf "$D2/Globals/Tglobal.dbf" "$CACHE_DIR/tglobal"
extract_dbf "$D2/Globals/Gunits.dbf"  "$CACHE_DIR/gunits"

TGLOBAL=$(ls "$CACHE_DIR/tglobal"/*.records.json | head -1)
GUNITS=$(ls  "$CACHE_DIR/gunits"/*.records.json  | head -1)

# ── death_anim → Battle.ff animation name ─────────────────────────────────────
death_anim_name() {
    local da="$1" size_small="$2"
    case "$da" in
        1) echo "DEATH_HUMAN_S13" ;;   # L_HUMAN
        2) echo "DEATH_HERETIC_S13" ;; # L_HERETIC (Legions)
        3) echo "DEATH_DWARF_S15" ;;   # L_DWARF (Mountain Clans)
        4) echo "DEATH_UNDEAD_S15" ;;  # L_UNDEAD
        5) echo "DEATH_NEUTRAL_S10" ;; # L_NEUTRAL
        6) echo "DEATH_DRAGON_S15" ;;  # L_DRAGON
        7) echo "DEATH_GHOST_S15" ;;   # L_GHOST
        8) [[ "$size_small" == "T" ]] && echo "DEATH_ELF_S15" || echo "DEATH_ELF_L15" ;; # L_ELF
        *) echo "UNKNOWN_$da" ;;
    esac
}

# ── resolve unit record ────────────────────────────────────────────────────────
if [[ "${1:-}" == "--id" ]]; then
    [[ -n "${2:-}" ]] || die "Usage: --id <UNIT_ID>"
    RAW_ID="$2"
    # Normalise: accept UU0068 or g000uu0068
    RAW_ID_LC=$(echo "$RAW_ID" | tr '[:upper:]' '[:lower:]')
    if echo "$RAW_ID" | grep -qiE '^uu[0-9]+'; then
        SEARCH_ID="g000${RAW_ID_LC}"
    else
        SEARCH_ID="$RAW_ID_LC"
    fi
    UNIT_RECORD=$(jq -c --arg id "$SEARCH_ID" \
        '.[] | select((.UNIT_ID // "") | ascii_downcase | contains($id))' \
        "$GUNITS" | head -1)
    [[ -n "$UNIT_RECORD" ]] || die "No unit found with id: $RAW_ID"
else
    [[ -n "${1:-}" ]] || { echo "Usage: $0 \"Unit Name\" OR $0 --id UU0068"; exit 1; }
    QUERY=$(echo "$1" | tr '[:upper:]' '[:lower:]')

    # Find text IDs whose TEXT matches the query
    MATCHING_IDS=$(jq -r --arg q "$QUERY" \
        '.[] | select((.TEXT // "") | ascii_downcase | contains($q)) | .TXT_ID' \
        "$TGLOBAL")
    [[ -n "$MATCHING_IDS" ]] || die "No text found matching: $1"

    # Find unit whose NAME_TXT matches one of those IDs
    UNIT_RECORD=""
    while IFS= read -r txt_id; do
        UNIT_RECORD=$(jq -c --arg id "$txt_id" \
            '.[] | select((.NAME_TXT // "") == $id)' \
            "$GUNITS" | head -1)
        [[ -n "$UNIT_RECORD" ]] && break
    done <<< "$MATCHING_IDS"

    [[ -n "$UNIT_RECORD" ]] || die "No unit found with name matching: $1"
fi

# ── extract key fields ────────────────────────────────────────────────────────
UNIT_ID=$(echo "$UNIT_RECORD"   | jq -r '.UNIT_ID // "?"' | tr '[:upper:]' '[:lower:]' | sed 's/g000//')
NAME_TXT=$(echo "$UNIT_RECORD"  | jq -r '.NAME_TXT // "?"')
RACE_ID=$(echo "$UNIT_RECORD"   | jq -r '.RACE_ID // "?"')
SIZE_SMALL=$(echo "$UNIT_RECORD" | jq -r '.SIZE_SMALL // "?"')
DEATH_ANIM=$(echo "$UNIT_RECORD" | jq -r '.DEATH_ANIM // "?"')
LEVEL=$(echo "$UNIT_RECORD"     | jq -r '.LEVEL // "?"')
HIT_POINT=$(echo "$UNIT_RECORD" | jq -r '.HIT_POINT // "?"')
ARMOR=$(echo "$UNIT_RECORD"     | jq -r '.ARMOR // "?"')
REGEN=$(echo "$UNIT_RECORD"     | jq -r '.REGEN // "?"')
ATTACK_ID=$(echo "$UNIT_RECORD" | jq -r '.ATTACK_ID // "?"')
SUBRACE=$(echo "$UNIT_RECORD"   | jq -r '.SUBRACE // "?"')
UNIT_CAT=$(echo "$UNIT_RECORD"  | jq -r '.UNIT_CAT // "?"')
PREV_ID=$(echo "$UNIT_RECORD"   | jq -r '.PREV_ID // "?"')
DYN_UPG1=$(echo "$UNIT_RECORD"  | jq -r '.DYN_UPG1 // "?"')

EN_NAME=$(jq -r --arg id "$NAME_TXT" \
    '.[] | select(.TXT_ID == $id) | .TEXT' "$TGLOBAL" | head -1)
[[ -z "$EN_NAME" ]] && EN_NAME="(unknown)"

DEATH_ANIM_STR=$(death_anim_name "$DEATH_ANIM" "$SIZE_SMALL")
SIZE_LABEL="large"
[[ "$SIZE_SMALL" == "T" ]] && SIZE_LABEL="small"

# ── race name ─────────────────────────────────────────────────────────────────
case "$RACE_ID" in
    g000rr0000) RACE_NAME="Empire" ;;
    g000rr0001) RACE_NAME="Mountain Clans" ;;
    g000rr0002) RACE_NAME="Legions of the Damned" ;;
    g000rr0003) RACE_NAME="Undead Hordes" ;;
    g000rr0004) RACE_NAME="Neutral" ;;
    g000rr0005) RACE_NAME="Elven Alliance" ;;
    *)          RACE_NAME="$RACE_ID" ;;
esac

# ── Batunits.ff animation list ────────────────────────────────────────────────
SHORT_ID=$(echo "$UNIT_ID" | tr '[:lower:]' '[:upper:]')
BATUNITS_ENTRIES=$("$BIN" list "$D2/Imgs/Batunits.ff" 2>/dev/null \
    | grep -i "$SHORT_ID" | sed 's/^\[.*\] //' | sed 's/ size=.*//')

# Group by animation role (strip unit prefix, strip frame/side suffix)
ANIM_ROLES=$(echo "$BATUNITS_ENTRIES" \
    | sed "s/G000${SHORT_ID}//I" \
    | grep -oE '^[A-Z]+[0-9]+[A-Z]' \
    | sort -u)

# ── print ─────────────────────────────────────────────────────────────────────
echo "══════════════════════════════════════════════════════"
echo "  $EN_NAME  ($SHORT_ID)"
echo "══════════════════════════════════════════════════════"
echo ""
echo "  Race:        $RACE_NAME  ($RACE_ID)"
echo "  Subrace:     $SUBRACE"
echo "  Category:    $UNIT_CAT"
echo "  Size:        $SIZE_LABEL  (SIZE_SMALL=$SIZE_SMALL)"
echo "  Level:       $LEVEL"
echo "  HP:          $HIT_POINT"
echo "  Armor:       $ARMOR"
echo "  Regen:       $REGEN"
echo "  Attack:      $ATTACK_ID"
echo "  Prev unit:   $PREV_ID"
echo "  Upgrade 1:   $DYN_UPG1"
echo ""
echo "  DEATH_ANIM:  $DEATH_ANIM  →  $DEATH_ANIM_STR  (Battle.ff)"
echo ""
echo "  Animations in Batunits.ff:"
if [[ -n "$ANIM_ROLES" ]]; then
    while IFS= read -r role; do
        printf "    %s\n" "$role"
    done <<< "$ANIM_ROLES"
else
    echo "    (none found — check unit ID)"
fi
echo ""
echo "  All Batunits.ff entries:"
if [[ -n "$BATUNITS_ENTRIES" ]]; then
    while IFS= read -r entry; do
        printf "    %s\n" "$entry"
    done <<< "$BATUNITS_ENTRIES"
else
    echo "    (none)"
fi
echo "══════════════════════════════════════════════════════"
