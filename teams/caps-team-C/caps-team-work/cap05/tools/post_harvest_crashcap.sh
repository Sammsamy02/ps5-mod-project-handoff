#!/bin/bash
# post_harvest_crashcap.sh - write CRASH-REPORT.md for one launch evidence folder.
# Mac-only: reads the klog tap file(s) + harvested logs already copied into the
# evidence folder; never contacts the console.
#
#   post_harvest_crashcap.sh <evidence-dir> <final-symbols.json> [game-image-elf]
#
# <final-symbols.json> = the deployed build's final_symbols.py output (made at
# build time next to the package; it is bound to the PRX by build-id, so a
# wrong one simply does not attach). [game-image-elf] = optional readable game
# executable copy (decrypt-off rules) for call-edge checks on game frames.
# Exit: crashcap's code (0 ok, 1 expectation failed, 2 input error, 3 empty klog).
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
EV="${1:?usage: post_harvest_crashcap.sh <evidence-dir> <final-symbols.json> [game-image]}"
FS="${2:?final-symbols.json required}"
IMG="${3:-}"
shift 3 2>/dev/null || shift $#
shopt -s nullglob
klogs=("$EV"/klog-*.log)
[ ${#klogs[@]} -gt 0 ] || { echo "post_harvest_crashcap: FAIL no klog-*.log in $EV (0 files) - was the klog tap running?" >&2; exit 3; }
args=()
for k in "${klogs[@]}"; do args+=(--klog "$k"); done
args+=(--symbols "$FS")
[ -n "$IMG" ] && args+=(--image "$IMG")
for l in "$EV"/harvest-*-ue4ss.log; do args+=(--log "ue4ss=$l"); done
for l in "$EV"/harvest-*-aligned-entry.log; do args+=(--log "aligned-entry=$l"); done
OUT="$EV/CRASH-REPORT.md"; J="$EV/crashcap.json"
[ -e "$OUT" ] && { echo "post_harvest_crashcap: $OUT exists; refusing to overwrite" >&2; exit 2; }
PYTHONDONTWRITEBYTECODE=1 python3 -I -B "$HERE/crashcap.py" "${args[@]}" --out "$OUT" --json "$J" "$@"
