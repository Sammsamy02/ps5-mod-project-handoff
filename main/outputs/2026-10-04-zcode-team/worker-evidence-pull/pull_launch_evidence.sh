#!/bin/sh
# pull_launch_evidence.sh — ONE-COMMAND live evidence pull + analysis.
#
# worker-evidence-pull · 2026-10-05 · implements NEXT-LAUNCH-RUNBOOK.md §3/§6:
#   (a) one fetch, five files, LAUNCH-DAY-10 §3 order, fresh curl per file,
#       into a NEW timestamped run dir (never re-fetch into an old dir);
#   (b) analyze.py computes new-since-baseline per file, filters the sdiag to
#       the MAX pid, runs the runbook greps IN ORDER, the banner NUL check,
#       the timing/gap block, and emits the one-screen C0-C5 verdict table
#       (PROVEN/REFUSED/PENDING per the cascade) + results.json;
#   (c) pins THIS run's sdiag size+sha as the NEXT baseline cut (§6.5).
#
# PULL-ONLY: this script never launches the game and never writes to the
# console. Absence of a file is recorded honestly (exec-write-probe absence
# is NORMAL). No failed fetch is ever replayed inside a run.
#
# Usage:
#   sh pull_launch_evidence.sh                 # live pull + analyze
#   env OUT_ROOT=/path sh pull_launch_evidence.sh
#   env BASELINE_BYTES=318910 sh pull_launch_evidence.sh
#   env BUILD_ID=8ede717f-bare-sdk-l11-1 sh pull_launch_evidence.sh
#
# Live budget: 5 files x (--connect-timeout 5 --max-time 10) => worst case
# ~50 s even if every fetch hangs; typical LAN total is ~5-10 s including
# analysis. The sdiag is capped at 32 MiB (--max-filesize) against runaway.

set -u

SELF_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

FTP="${FTP:-ftp://ps5-host-not-configured.invalid:2121}"   # override ONLY for offline plumbing tests; live default is the console
CURL="curl -sS --connect-timeout 5 --max-time 10 --max-filesize 33554432"
OUT_ROOT="${OUT_ROOT:-$SELF_DIR/runs}"
BUILD_ID="${BUILD_ID:-8ede717f-bare-sdk-l11-1}"   # staged candidate: bare-sdk ELF sha 8ede717f..., prx-candidate-l11-1
BASELINE_BYTES="${BASELINE_BYTES:-}"

STAMP=$(python3 -c 'import datetime;print(datetime.datetime.now().strftime("%Y%m%d-%H%M%S"))')
RUN_DIR="$OUT_ROOT/$STAMP-pull"
mkdir -p "$RUN_DIR" || exit 2
echo "== pull_launch_evidence: run dir $RUN_DIR"

# ---- the five files, runbook §6.1 order -----------------------------------
# name            remote path
# 1 UE4SS.log     /data/plugins/UE4SS.log
# 2 status        /data/modbridge/roots/PPSA33286/runtime-status.txt
# 3 sdiag         /data/modbridge-supplier-diag.jsonl
# 4 probe         /data/modbridge-exec-write-probe.jsonl   (absence NORMAL)
# 5 entry log     /data/townfall_runtime_aligned_entry.log
fetch() { # $1=local name  $2=remote path
  $CURL "$FTP$2" -o "$RUN_DIR/$1" 2> "$RUN_DIR/fetch-$1.err"
  rc=$?
  b=0; [ -f "$RUN_DIR/$1" ] && b=$(wc -c < "$RUN_DIR/$1" | tr -d ' ')
  if [ "$rc" -eq 0 ] && [ "$b" -gt 0 ]; then
    st="PRESENT"
  else
    st="ABSENT (rc=$rc) — recorded honestly; never replayed"
    rm -f "$RUN_DIR/$1"
  fi
  echo "  [$st] $2 -> $1 ($b B)"
  echo "rc=$rc bytes=$b remote=$2" >> "$RUN_DIR/fetch-manifest.txt"
}

fetch UE4SS.log                              /data/plugins/UE4SS.log
fetch runtime-status.txt                     /data/modbridge/roots/PPSA33286/runtime-status.txt
fetch modbridge-supplier-diag.jsonl          /data/modbridge-supplier-diag.jsonl
fetch modbridge-exec-write-probe.jsonl       /data/modbridge-exec-write-probe.jsonl
fetch townfall_runtime_aligned_entry.log     /data/townfall_runtime_aligned_entry.log

# sha256 of whatever landed (evidence pinning)
( cd "$RUN_DIR" && shasum -a 256 UE4SS.log runtime-status.txt \
    modbridge-supplier-diag.jsonl modbridge-exec-write-probe.jsonl \
    townfall_runtime_aligned_entry.log 2>/dev/null ) > "$RUN_DIR/sha256.txt" || true

# ---- analysis (timed) -------------------------------------------------------
T0=$(python3 -c 'import time;print(time.time())')
ANALYZE_ARGS="--dir $RUN_DIR --build-id $BUILD_ID"
if [ -n "$BASELINE_BYTES" ]; then
  ANALYZE_ARGS="$ANALYZE_ARGS --baseline-bytes $BASELINE_BYTES"
else
  echo "note: no BASELINE_BYTES given — analyze.py defaults to the stage-time recorded cut (l10.6 recorded ABSENT -> 0 = whole-file window; max-pid correlation governs)"
fi
# shellcheck disable=SC2086
python3 "$SELF_DIR/analyze.py" $ANALYZE_ARGS
arc=$?
T1=$(python3 -c 'import time;print(time.time())')
python3 -c "print('analysis seconds: %.2f' % ($T1 - $T0))" | tee "$RUN_DIR/analysis-seconds.txt"
echo "fetch+analysis done (analyze rc=$arc); raw fetches + results.json live in $RUN_DIR"
exit "$arc"
