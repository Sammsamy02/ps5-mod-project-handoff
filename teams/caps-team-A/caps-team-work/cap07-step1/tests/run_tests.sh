#!/bin/sh
# Host tests for capability #7 step 1 (Mac-only; no console contact).
# Usage: sh tests/run_tests.sh   (from the cap07-step1 folder or anywhere)
# Fails (exit 1) on any failure, any skip, zero tests, or zero replay items.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$HERE/output"
mkdir -p "$OUT" "$HERE/work"
export PYTHONDONTWRITEBYTECODE=1
STAMP="$(date -u +%Y%m%dT%H%M%SZ)"
echo "== paktx unit/integration tests ($STAMP)" | tee "$OUT/host-tests.log"
python3 -W ignore "$HERE/test_paktx.py" >> "$OUT/host-tests.log" 2>&1
RC1=$?
tail -6 "$OUT/host-tests.log"
SUMMARY="$(grep '^SUMMARY tests_run=' "$OUT/host-tests.log" | tail -1)"
RUN="$(echo "$SUMMARY" | sed -n 's/.*tests_run=\([0-9]*\).*/\1/p')"
BAD="$(echo "$SUMMARY" | sed -n 's/.*failed=\([0-9]*\).*/\1/p')"
SKIP="$(echo "$SUMMARY" | sed -n 's/.*skipped=\([0-9]*\).*/\1/p')"
grep -q '^GATE PASS$' "$OUT/host-tests.log" || RC1=1
if [ -z "$RUN" ] || [ "$RUN" -eq 0 ] || [ "${BAD:-1}" -ne 0 ] || [ "${SKIP:-1}" -ne 0 ]; then RC1=1; fi
echo "== real Leon replay (read-only inputs)" | tee "$OUT/real-leon-replay.stderr.log"
python3 -W ignore "$HERE/real_leon_replay.py" > "$OUT/real-leon-replay.json" 2>> "$OUT/real-leon-replay.stderr.log"
RC2=$?
ITEMS="$(python3 -c "import json,sys;d=json.load(open(sys.argv[1]));print(d['result'],d['items_checked'])" "$OUT/real-leon-replay.json" 2>/dev/null)"
echo "real replay: ${ITEMS:-unreadable}"
case "$ITEMS" in "PASS "[1-9]*) ;; *) RC2=1 ;; esac
if [ $RC1 -ne 0 ] || [ $RC2 -ne 0 ]; then
  echo "RUN FAIL (tests_run=${RUN:-0} failed=${BAD:-?} skipped=${SKIP:-?} rc=$RC1; replay rc=$RC2 ${ITEMS:-})"
  exit 1
fi
rm -rf "$HERE"/work/t[0-9]*   # synthetic fixtures (rebuilt every run)
echo "RUN PASS tests_run=$RUN replay_items_checked=${ITEMS#PASS }"
