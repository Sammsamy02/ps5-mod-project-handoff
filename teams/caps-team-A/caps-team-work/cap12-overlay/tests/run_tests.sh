#!/bin/sh
# Host tests for capability #12 overlay (Team A, cap12-overlay). Mac-only; no console contact.
# Usage: sh tests/run_tests.sh   (from anywhere)
# Runs: the FULL cap07 paktx suite + real Leon replay against the STAGED paktx,
#       the cap12 substitute suite, and the real-data cap12 dry run.
# Fails (exit 1) on any failure, any skip, zero tests, zero real items, a changed read-only input,
# or a stale staged diff.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
TEAM="$(dirname "$HERE")"
REPO="${PAKTX_REPO:-/WORKSPACE/.codex/worktrees/translation-core/what}"
L25="$REPO/outputs/2026-10-08-l25-prep"
OUT="$HERE/output"
mkdir -p "$OUT" "$HERE/work"
export PYTHONDONTWRITEBYTECODE=1
STAMP="$(date -u +%Y%m%dT%H%M%SZ)"
FAIL=0
note() { echo "$*" | tee -a "$OUT/run.log"; }
: > "$OUT/run.log"
note "== cap12-overlay host gate ($STAMP)"
# 0. read-only inputs pinned (integrated tools must be exactly these)
check_sha() {
  got="$(shasum -a 256 "$1" | cut -d' ' -f1)"
  if [ "$got" != "$2" ]; then note "INPUT CHANGED: $1 is $got, expected $2"; FAIL=1; else note "input ok: $(basename "$1")"; fi
}
check_sha "$L25/pak-overlay/paktx.py" a7446c1b750718d1f75f5d92877e8e07724c303d5b3b5025f081b5ed042012dd
check_sha "$L25/pak-overlay/overlay_builder.py" a72636fbdec542bb6ce9521d6ca5709b61b09739938544671212ec93b0c958e6
check_sha "$L25/mod-manager/material-fallback/mic_reparent.py" 520a805580a6750e9f799c8740095d4cc5d6f3e3a0a3574e6052914e82c06af5
check_sha "$L25/evidence/cap12-20261012/reparent-plan-r3-partial-opaque.json" 58ed3724cf569e1df7c45dfa06ea0393b324ab226776fc1fb8922d3d3f938f79
# 1. the staged diff reproduces the staged paktx from the integrated one
TMPD="$HERE/work/diffcheck"; rm -rf "$TMPD"; mkdir -p "$TMPD"
cp "$L25/pak-overlay/paktx.py" "$TMPD/paktx.py"
if patch -s "$TMPD/paktx.py" < "$TEAM/PATCH/paktx.py.diff" && cmp -s "$TMPD/paktx.py" "$TEAM/PATCH/tools/paktx.py"; then
  note "staged diff ok: integrated paktx + PATCH/paktx.py.diff == PATCH/tools/paktx.py"
else
  note "STALE DIFF: PATCH/paktx.py.diff does not reproduce PATCH/tools/paktx.py"; FAIL=1
fi
rm -rf "$TMPD"
suite() {  # name file
  log="$OUT/$1.log"
  python3 -W ignore "$HERE/$2" > "$log" 2>&1
  rc=$?
  S="$(grep '^SUMMARY tests_run=' "$log" | tail -1)"
  RUN="$(echo "$S" | sed -n 's/.*tests_run=\([0-9]*\).*/\1/p')"
  BAD="$(echo "$S" | sed -n 's/.*failed=\([0-9]*\).*/\1/p')"
  SKIP="$(echo "$S" | sed -n 's/.*skipped=\([0-9]*\).*/\1/p')"
  grep -q '^GATE PASS$' "$log" || rc=1
  if [ -z "$RUN" ] || [ "$RUN" -eq 0 ] || [ "${BAD:-1}" -ne 0 ] || [ "${SKIP:-1}" -ne 0 ]; then rc=1; fi
  note "$1: ${S:-no summary} rc=$rc"
  [ $rc -eq 0 ] || FAIL=1
  eval "N_$1=\${RUN:-0}"
}
real() {  # name file
  python3 -W ignore "$HERE/$2" > "$OUT/$1.json" 2> "$OUT/$1.stderr.log"
  rc=$?
  IT="$(python3 -c "import json,sys;d=json.load(open(sys.argv[1]));print(d['result'],d['items_checked'])" "$OUT/$1.json" 2>/dev/null)"
  case "$IT" in "PASS "[1-9]*) ;; *) rc=1 ;; esac
  note "$1: ${IT:-unreadable} rc=$rc"
  [ $rc -eq 0 ] || FAIL=1
}
suite cap07 test_paktx.py
real cap07_replay real_leon_replay.py
suite cap12 test_substitute.py
real cap12_real real_cap12_dryrun.py
rm -rf "$HERE"/work/t[0-9]* "$HERE"/work/s04-* "$HERE"/work/sub "$HERE"/work/diffcheck   # synthetic fixtures (rebuilt every run)
if [ $FAIL -ne 0 ]; then note "RUN FAIL"; exit 1; fi
note "RUN PASS cap07_tests=${N_cap07} cap12_tests=${N_cap12}"
