#!/bin/bash
# ============================================================================
# harvest.sh — L12/L13 post-test diagnostics harvest (READ-ONLY, no console writes)
#
# USAGE:
#   bash /tmp/l12-harvest/harvest.sh          # run AFTER the test:
#                                              fresh console boot -> ONE Townfall
#                                              launch -> in-game -> L3+R3
#   bash /tmp/l12-harvest/harvest.sh -h       # this header
#
# What it does:
#   1. Downloads over FTP (ftp://ps5-host-not-configured.invalid:2121, spaces %20-encoded):
#        /data/plugins/UE4SS.log                              (full)
#        /data/modbridge/roots/PPSA33286/runtime-status.txt   (full)
#        /data/modbridge-supplier-diag.jsonl                  (full + tail -200)
#        /data/plugins/modbridge-copyin-{sink*,p*}                 (ls dir first,
#                                                             unique names only)
#        /data/shadowmount/debug.log                          (full + tail -40)
#        /data/ps5upload/activity.json                        (ps5upload engine
#                                                             activity log)
#        + directory listings of /data/plugins and the mod root
#          (/data/modbridge/roots/PPSA33286/ue4ss/Mods) for the
#          files-present verdict (runtime status is separate).
#   2. Prints grep summaries to stdout: [Seeds], [L12], pool admission
#      verdicts (admitted via|REFUTED), pool_copyin_probe_* tags, mod
#      lifecycle (Stopping mod|Big App started), R3Marshal mentions,
#      pad errors 0x80920004, startup deadline exceeded.
#   3. Prints an automated VERDICT section (checks a-d) and writes it to
#      verdict.txt alongside the evidence.
#
# Output:  ${HARVEST_BASE:-/tmp/l12-harvest}/out-<epoch>-<pid>/ (fresh per run)
# Exit:    0 = harvested (check VERDICT for meaning),
#          4 = FETCH_FAILED (UE4SS.log AND runtime-status.txt both
#              unreachable — wait ~60 s and re-run before touching staging)
#
# NOTE: klog TCP captures live in /tmp/l12-harvest/klog-*.log (written by
#       klog_capture.py, always NEW files). If present at harvest time they
#       are included in the grep summaries ('Big App started' lives there).
# ============================================================================
set -euo pipefail

FTP="${FTP:-ftp://ps5-host-not-configured.invalid:2121}"
BASE="${HARVEST_BASE:-/tmp/l12-harvest}"
TS="$(date +%s)"
OUT="$BASE/out-$TS-$$"

usage() { sed -n '2,39p' "$0" | sed 's/^# \{0,1\}//'; }
case "${1:-}" in
  -h|--help) usage; exit 0 ;;
  "") : ;;
  *) echo "unknown arg: $1 (use -h)" >&2; exit 64 ;;
esac

echo "harvest.sh — L12/L13 diagnostics harvest  $(date -u '+%Y-%m-%d %H:%M:%S UTC')"
echo "FTP=$FTP"
echo "OUT=$OUT"
echo "(test sequence this evidence belongs to: fresh boot -> ONE Townfall launch -> in-game -> L3+R3 -> harvest)"
mkdir -p "$OUT"

CURL=(curl -sS --connect-timeout 10 --max-time 90 --max-filesize 64M)

# ---- helpers ----------------------------------------------------------------
urlenc() { printf '%s' "$1" | sed 's/ /%20/g'; }

FAILURES=()
FETCHED=0

fetch() { # fetch <remote-abs-path> <local-name>  (records failure, never aborts)
  local remote="$1" name="$2" url
  url="$(urlenc "$FTP$remote")"
  if "${CURL[@]}" -o "$OUT/$name" "$url"; then
    FETCHED=$((FETCHED + 1))
    printf '  fetched  %-60s -> %s (%s bytes)\n' "$remote" "$name" \
      "$(wc -c < "$OUT/$name" | tr -d ' ')"
  else
    # A failed/partial transfer cannot serve as fetched evidence.
    rm -f "$OUT/$name"
    FAILURES+=("$remote")
    printf '  FAILED   %s\n' "$remote"
  fi
}

list_dir() { # list_dir <remote-dir> <local-name> -> 0 on success
  local dir="$1" name="$2" url
  url="$(urlenc "$FTP$dir/")"
  if "${CURL[@]}" --max-time 30 -o "$OUT/$name" "$url"; then
    return 0
  fi
  rm -f "$OUT/$name"
  return 1
}

# names = field 9+ of a UNIX LIST line (keeps embedded single spaces)
listing_names() { awk 'NF>=9 { n=""; for (i=9;i<=NF;i++) n = n (i>9?" ":"") $i; if (n != "." && n != "..") print n }' "$1"; }

grep_report() { # grep_report <mode F|E> <label> <pattern>
  local mode="$1" label="$2" pat="$3" total=0
  if [ "${#SUMMARY_FILES[@]}" -gt 0 ]; then
    total=$({ grep -ah"$mode" -- "$pat" "${SUMMARY_FILES[@]}" 2>/dev/null || [ "$?" -eq 1 ]; } | wc -l | tr -d ' ')
  fi
  echo "--- $label : $total match(es) ---"
  if [ "$total" -gt 0 ]; then
    grep -Ha"$mode" -n -- "$pat" "${SUMMARY_FILES[@]}" 2>/dev/null || true
  fi
}

# ---- 1. downloads ------------------------------------------------------------
echo
echo "=== 1. DOWNLOADS ==="

fetch /data/plugins/UE4SS.log UE4SS.log
fetch /data/modbridge/roots/PPSA33286/runtime-status.txt runtime-status.txt
fetch /data/modbridge-supplier-diag.jsonl modbridge-supplier-diag.jsonl
if [ -f "$OUT/modbridge-supplier-diag.jsonl" ]; then
  tail -n 200 "$OUT/modbridge-supplier-diag.jsonl" > "$OUT/modbridge-supplier-diag.tail200.jsonl"
fi
fetch /data/shadowmount/debug.log shadowmount-debug.log
if [ -f "$OUT/shadowmount-debug.log" ]; then
  tail -n 40 "$OUT/shadowmount-debug.log" > "$OUT/shadowmount-debug.tail40.log"
fi
fetch /data/ps5upload/activity.json ps5upload-activity.json

# ls /data/plugins FIRST, then fetch any unique copyin sink files left behind
echo "  listing  /data/plugins/ (for modbridge-copyin-sink* / modbridge-copyin-p* discovery)"
if list_dir /data/plugins plugins-dir-listing.txt; then
  echo "  --- /data/plugins listing (sink discovery) ---"
  sed -n '1,120p' "$OUT/plugins-dir-listing.txt" | sed 's/^/    /'
  listing_names "$OUT/plugins-dir-listing.txt" | grep -E '^modbridge-copyin-(sink|p)' | sort -u > "$OUT/sink-names.txt" || true
  echo "  --- unique sink files found: $(wc -l < "$OUT/sink-names.txt" | tr -d ' ') ---"
  while IFS= read -r sink; do
    [ -n "$sink" ] || continue
    fetch "/data/plugins/$sink" "$sink"
  done < "$OUT/sink-names.txt"
else
  FAILURES+=("/data/plugins/ (LIST)")
  echo "  FAILED   /data/plugins/ (LIST) — cannot discover sink files"
  : > "$OUT/sink-names.txt"
fi

# Mod files can remain on disk after runtime shutdown; report presence separately.
MODS_DIR=/data/modbridge/roots/PPSA33286/ue4ss/Mods
echo "  listing  $MODS_DIR/ (for files-present verdict)"
if ! list_dir "$MODS_DIR" mods-dir-listing.txt; then
  FAILURES+=("$MODS_DIR/ (LIST)")
  echo "  FAILED   $MODS_DIR/ (LIST)"
fi

echo
echo "downloads done: $FETCHED fetched, ${#FAILURES[@]} failure(s)"
if [ "${#FAILURES[@]}" -gt 0 ]; then
  printf '  failed: %s\n' "${FAILURES[@]}"
fi

# ---- 2. grep summaries (stdout) ----------------------------------------------
# grep the bounded/summary artifacts (tails), not the full duplicates, so no
# line is printed twice; klog captures are folded in when present.
echo
echo "=== 2. GREP SUMMARIES ==="

[ -f "$OUT/sink-names.txt" ] || : > "$OUT/sink-names.txt"
SUMMARY_FILES=()
for f in UE4SS.log runtime-status.txt modbridge-supplier-diag.tail200.jsonl \
         shadowmount-debug.tail40.log ps5upload-activity.json \
         plugins-dir-listing.txt mods-dir-listing.txt; do
  [ -f "$OUT/$f" ] && SUMMARY_FILES+=("$OUT/$f")
done
if [ -f "$OUT/modbridge-supplier-diag.tail200.jsonl" ]; then :; elif [ -f "$OUT/modbridge-supplier-diag.jsonl" ]; then
  SUMMARY_FILES+=("$OUT/modbridge-supplier-diag.jsonl")
fi
if [ -f "$OUT/shadowmount-debug.tail40.log" ]; then :; elif [ -f "$OUT/shadowmount-debug.log" ]; then
  SUMMARY_FILES+=("$OUT/shadowmount-debug.log")
fi
while IFS= read -r sink; do
  [ -n "$sink" ] && [ -f "$OUT/$sink" ] && SUMMARY_FILES+=("$OUT/$sink")
done < "$OUT/sink-names.txt" || true
for k in "$BASE"/klog-*.log; do
  [ -e "$k" ] && SUMMARY_FILES+=("$k")
done

if [ "${#SUMMARY_FILES[@]}" -eq 0 ]; then
  echo "summary file set: EMPTY (nothing fetched — see failures above)"
else
  echo "summary file set (${#SUMMARY_FILES[@]}):"
  printf '  %s\n' "${SUMMARY_FILES[@]}"
fi
echo

grep_report F "[Seeds] lines (seed arming / admission)" '[Seeds]'
grep_report F "[L12] lines (S0 armed / S1 rounds / S2 exhausted / PoolAdmission mode)" '[L12]'
grep_report E "pool admission verdicts (admitted via|REFUTED)" 'admitted via|REFUTED'
grep_report F "copyin probe tags (pool_copyin_probe_*)" 'pool_copyin_probe_'
grep_report E "mod lifecycle (Stopping mod|Big App started)" 'Stopping mod|Big App started'
grep_report F "R3Marshal mentions" 'R3Marshal'
grep_report F "pad errors (0x80920004)" '0x80920004'
grep_report F "startup deadline exceeded" 'Unreal initialization startup deadline exceeded'

# ---- 3. verdict ---------------------------------------------------------------
echo
echo "=== 3. VERDICT (automated checks) ==="

VERDICT="$OUT/verdict.txt"
: > "$VERDICT"
vlog() { printf '%s\n' "$*" | tee -a "$VERDICT"; }

RUNTIME_FAILED=0
vlog "(a) runtime-status reports startup failure (state 2 failed 1)?"
if [ -f "$OUT/runtime-status.txt" ] && grep -qE '^state[[:space:]]+2[[:space:]]+failed[[:space:]]+1([[:space:]]|$)' "$OUT/runtime-status.txt"; then
  RUNTIME_FAILED=1
  vlog "    YES — startup failed; preserve the reason (deadline and seed refusal are distinct):"
  grep -E '^state[[:space:]]+2[[:space:]]+failed[[:space:]]+1([[:space:]]|$)' "$OUT/runtime-status.txt" | sed 's/^/      /' | tee -a "$VERDICT"
elif [ -f "$OUT/runtime-status.txt" ] && grep -qE '^state[[:space:]]+2[[:space:]]+failed[[:space:]]+0([[:space:]]|$)' "$OUT/runtime-status.txt"; then
  vlog "    NO — runtime-status reports failed=0; this is not proof of current mod activity."
else
  vlog "    UNKNOWN — runtime-status missing or no recognized failed=0/1 record."
  if [ -f "$OUT/runtime-status.txt" ]; then
    sed 's/^/        /' "$OUT/runtime-status.txt" | tee -a "$VERDICT"
  fi
fi

vlog "(b) UE4SS.log contains 'admitted via' (any source: shape / kernel-write-copyin-read / shadow corroboration)?"
if [ -f "$OUT/UE4SS.log" ] && grep -aqF 'admitted via' "$OUT/UE4SS.log"; then
  vlog "    YES — count: $(grep -acF 'admitted via' "$OUT/UE4SS.log" | tr -d ' ') ; lines:"
  grep -anF 'admitted via' "$OUT/UE4SS.log" | sed 's/^/      /' | tee -a "$VERDICT"
elif [ -f "$OUT/UE4SS.log" ]; then
  vlog "    NO — no admission line in this boot's UE4SS.log."
else
  vlog "    UNKNOWN — UE4SS.log not fetched."
fi

vlog "(c) UE4SS.log still contains 'Stopping mod' (Terminal uninstall path)?"
if [ -f "$OUT/UE4SS.log" ] && grep -aqF 'Stopping mod' "$OUT/UE4SS.log"; then
  vlog "    YES — a runtime stop was logged (files may remain installed on disk):"
  grep -anF 'Stopping mod' "$OUT/UE4SS.log" | sed 's/^/      /' | tee -a "$VERDICT"
elif [ -f "$OUT/UE4SS.log" ]; then
  vlog "    NO — no 'Stopping mod' line; absence does not prove current runtime activity."
else
  vlog "    UNKNOWN — UE4SS.log not fetched."
fi

vlog "(d) mod files present on disk? (separate from runtime activity)"
if [ -s "$OUT/mods-dir-listing.txt" ]; then
  if grep -q 'TownfallItemSpawner' "$OUT/mods-dir-listing.txt"; then
    vlog "    FILES PRESENT — TownfallItemSpawner appears in $MODS_DIR/:"
    grep 'TownfallItemSpawner' "$OUT/mods-dir-listing.txt" | sed 's/^/      /' | tee -a "$VERDICT"
  else
    vlog "    NO — TownfallItemSpawner NOT in the Mods listing (listing below):"
    sed -n '1,40p' "$OUT/mods-dir-listing.txt" | sed 's/^/      /' | tee -a "$VERDICT"
  fi
else
  vlog "    UNKNOWN — Mods dir listing unavailable."
fi
if [ "$RUNTIME_FAILED" -eq 1 ] || { [ -f "$OUT/UE4SS.log" ] && grep -aqF 'Stopping mod' "$OUT/UE4SS.log"; }; then
  vlog "    STOPPED/FAILED — runtime failure or stop is recorded; file presence does not override it."
else
  vlog "    UNKNOWN — current runtime activity is not proved by these file-presence/stop checks."
fi
uninstall_count=n/a
if [ -f "$OUT/UE4SS.log" ]; then
  uninstall_count=$({ grep -acF 'for uninstall' "$OUT/UE4SS.log" || [ "$?" -eq 1 ]; } | tr -d ' ')
fi
vlog "    cross-check: 'for uninstall' in UE4SS.log: $uninstall_count occurrence(s) (runtime log only; not a disk-deletion verdict)"

vlog ""
vlog "evidence dir: $OUT"
vlog "harvest complete: $FETCHED file(s) fetched, ${#FAILURES[@]} failure(s)"

# ---- exit code ----------------------------------------------------------------
if [ ! -f "$OUT/UE4SS.log" ] && [ ! -f "$OUT/runtime-status.txt" ]; then
  echo
  echo "FETCH_FAILED: both core sinks (UE4SS.log, runtime-status.txt) unreachable."
  echo "Wait ~60 s (FTP server settling after boot/launch) and re-run before touching staging."
  exit 4
fi
exit 0
