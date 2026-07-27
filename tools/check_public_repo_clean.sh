#!/usr/bin/env bash
# Guard check: verify tracked files contain no prohibited content.
#
# The repository may contain authored configs, asset references, numeric ids,
# mappings, and compatibility data needed by the runtime.
#
# The repository must NOT contain:
#   - original game archives (*.ff, *.smk, *.bik, etc.)
#   - extracted database / data files (*.wdb, *.wdt, *.dbf, *.dat, *.dlg)
#   - extracted media files (*.wav, *.mp3, *.ogg, etc.)
#   - extracted fonts (*.mft)
#   - extracted images (*.png, *.jpg, etc.) outside allowed docs/tests paths
#   - generated dumps from original archives (*dump*.json, *seed*.json, *.generated.json)
#   - files in suspicious directories (extracted/, game_assets/, dumps/, etc.)
#   - local absolute paths (/Users/, local username)
#   - private agent/editor state (.opencode/, .claude/, .cursor/)
#   - .env files except .env.example
#
# BatUnits.ff and Interf.ff may be mentioned in documentation as placeholder
# archive names. Actual .ff files must never be tracked.

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

violations=0

check_names() {
    local desc="$1" pattern="$2"
    local results
    results=$(git ls-files | grep -E "$pattern" || true)
    if [[ -n "$results" ]]; then
        echo -e "${RED}[VIOLATION]${NC} $desc"
        echo "$results" | sed 's/^/  /'
        violations=$((violations + 1))
    fi
}

check_content() {
    local desc="$1" pattern="$2"
    local results
    results=$(git grep -l "$pattern" -- ':!tools/check_public_repo_clean.sh' 2>/dev/null || true)
    if [[ -n "$results" ]]; then
        echo -e "${RED}[VIOLATION]${NC} $desc"
        echo "$results" | sed 's/^/  /'
        violations=$((violations + 1))
    fi
}

is_allowed_env_file() {
    case "$1" in
        .env.example)
            return 0
            ;;
        .env|.env.*)
            return 1
            ;;
        *)
            return 0
            ;;
    esac
}

is_allowed_image_file() {
    case "$1" in
        docs/formats/*|tests/fixtures/synthetic/*|assets/app/icon.png)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

is_allowed_sg_file() {
    case "$1" in
        testdata/test_map.sg)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

echo "=== Public Repository Cleanliness Check ==="
echo ""

# ── File name checks ──────────────────────────────────────────────────────────

# Original game archive extensions (all cases)
check_names "Original game archives tracked (*.ff, *.smk, *.bik, etc.)" \
    '\.(ff|smk|bik|FF|SMK|BIK)$'

# MQDB / OPT / SG / SAV / MPQ / MFT extensions
PROPRIETARY_FILES=""
while IFS= read -r file; do
    case "$file" in
        *.mqdb|*.MQDB|*.opt|*.OPT|*.sav|*.SAV|*.mpq|*.MPQ|*.mft|*.MFT)
            PROPRIETARY_FILES+="$file"
            PROPRIETARY_FILES+=$'\n'
            ;;
        *.sg|*.SG)
            if ! is_allowed_sg_file "$file"; then
                PROPRIETARY_FILES+="$file"
                PROPRIETARY_FILES+=$'\n'
            fi
            ;;
    esac
done < <(git ls-files)
if [[ -n "$PROPRIETARY_FILES" ]]; then
    echo -e "${RED}[VIOLATION]${NC} Proprietary container/database files tracked (*.mqdb, *.opt, *.sg, *.sav, *.mpq, *.mft)"
    echo "$PROPRIETARY_FILES" | sed 's/^/  /'
    violations=$((violations + 1))
fi

# WDB / WDT / DBF / DAT / DLG extensions
check_names "Game database/data files tracked (*.wdb, *.wdt, *.dbf, *.dat, *.dlg)" \
    '\.(wdb|wdt|dbf|dat|dlg|WDB|WDT|DBF|DAT|DLG)$'

# Extracted audio / media
check_names "Extracted sound/media files tracked (*.wav, *.mp3, *.ogg, *.flac)" \
    '\.(wav|mp3|ogg|flac|WAV|MP3|OGG|FLAC)$'

# Extracted image files outside allowed paths
IMG_FILES=$(git ls-files | grep -iE '\.(png|jpg|jpeg|gif|webp|bmp|pcx|tga)$' || true)
if [[ -n "$IMG_FILES" ]]; then
    BAD_IMG=$(echo "$IMG_FILES" | while IFS= read -r file; do
        if is_allowed_image_file "$file"; then
            continue
        fi
        echo "$file"
    done)
    if [[ -n "$BAD_IMG" ]]; then
        echo -e "${RED}[VIOLATION]${NC} Image files tracked outside docs/formats/ and tests/fixtures/synthetic/"
        echo "$BAD_IMG" | sed 's/^/  /'
        violations=$((violations + 1))
    fi
fi

# Generated dumps / extracted metadata catalogs (anywhere)
check_names "Generated research dumps tracked (*dump*.json, *seed*.json, *.generated.json, *catalog*.json)" \
    '(dump|seed|generated|catalog).*\.json$'

# Specific deleted file must never reappear
check_names "Deleted animation catalog tracked" '^docs/data/adventure_isounit_animation_catalog\.json$'

# Private agent/editor state
check_names "Tracked .opencode/ files" '^\.opencode/'
check_names "Tracked .claude/ files" '^\.claude/'
check_names "Tracked .cursor/ or .windsurf/ files" '^\.(cursor|windsurf)/'

# .env files
ENV_FILES=""
while IFS= read -r file; do
    if is_allowed_env_file "$file"; then
        continue
    fi
    case "$file" in
        .env|.env.*)
            ENV_FILES+="$file"
            ENV_FILES+=$'\n'
            ;;
    esac
done < <(git ls-files)
if [[ -n "$ENV_FILES" ]]; then
    echo -e "${RED}[VIOLATION]${NC} Tracked .env files"
    echo "$ENV_FILES" | sed 's/^/  /'
    violations=$((violations + 1))
fi

# Suspicious directories
check_names "Files in extracted/ directory" '^extracted/'
check_names "Files in game_assets/ directory" '^game_assets/'
check_names "Files in original_assets/ directory" '^original_assets/'
check_names "Files in unpacked/ directory" '^unpacked/'
check_names "Files in dumps/ directory" '^dumps/'
check_names "Files in data/original/ directory" '^data/original/'
check_names "Files in data/extracted/ directory" '^data/extracted/'

# Specific known bad file
check_names "research/battle_visual_intent_matrix_seed.json tracked" \
    'research/battle_visual_intent_matrix_seed\.json'

# ── Content checks (documents containing local paths) ─────────────────────────

# Hardcoded local absolute paths
check_content "Hardcoded /Users/ paths in tracked files" '/Users/'

# Local username
check_content "Local username 'alternativehood' in tracked files" 'alternativehood'

echo ""
if [[ "$violations" -eq 0 ]]; then
    echo -e "${GREEN}All checks passed. Repository is clean for publishing.${NC}"
    exit 0
else
    echo -e "${RED}Found $violations violation(s). Fix before pushing.${NC}"
    exit 1
fi
