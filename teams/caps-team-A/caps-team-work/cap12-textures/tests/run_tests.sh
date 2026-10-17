#!/bin/sh
# Host gate for capability #12 derived textures (Team A, cap12-textures). Mac-only; no console contact.
# Usage: sh tests/run_tests.sh   (from anywhere; about 35 minutes)
# Runs against the STAGED tools in PATCH/tools:
#   pins      texture-tool sha pins in derive_substitutes.py / paktx.py are current
#   diffs     PATCH/*.diff reproduce PATCH/tools from PATCH/base (and the bases equal Team A's staged rounds)
#   mf_cap12  material-fallback suite (Team F, adapted by Team A round 4)            51 tests
#   mf_full   round-4 full-look rules suite                                          24 tests
#   cap07     paktx suite (capability #7)                                            45 tests
#   cap07_replay  real Leon replay vs the console-proven derived index
#   cap12     cap12-overlay substitute suite, fixture adapted to round 5b              50 tests
#   textures  derived-texture suite (this round: swap, decoders, layout, PS5 tiling, paktx)
#   tex_real      real-data round-5 dry run (plan + manifest + paktx + independent re-read)
# Fails (exit 1) on any failure, skip, zero tests, zero real items, zero-count gate, changed read-only
# input or stale diff.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
TEAM="$(dirname "$HERE")"
TA="$(dirname "$TEAM")"
REPO="${PAKTX_REPO:-/WORKSPACE/.codex/worktrees/translation-core/what}"
L25="$REPO/outputs/2026-10-08-l25-prep"
OUT="$HERE/output"
mkdir -p "$OUT" "$HERE/work"
export PYTHONDONTWRITEBYTECODE=1
STAMP="$(date -u +%Y%m%dT%H%M%SZ)"
FAIL=0
note() { echo "$*" | tee -a "$OUT/run.log"; }
: > "$OUT/run.log"
note "== cap12-textures host gate ($STAMP)"
check_sha() {
  got="$(shasum -a 256 "$1" | cut -d' ' -f1)"
  if [ "$got" != "$2" ]; then note "INPUT CHANGED: $1 is $got, expected $2"; FAIL=1; else note "input ok: $(basename "$1")"; fi
}
# 0. read-only inputs
check_sha "$L25/pak-overlay/paktx.py" a7446c1b750718d1f75f5d92877e8e07724c303d5b3b5025f081b5ed042012dd
check_sha "$L25/pak-overlay/overlay_builder.py" a72636fbdec542bb6ce9521d6ca5709b61b09739938544671212ec93b0c958e6
check_sha "$L25/mod-manager/material-fallback/mic_reparent.py" 520a805580a6750e9f799c8740095d4cc5d6f3e3a0a3574e6052914e82c06af5
check_sha "$TEAM/PATCH/tools/mic_reparent.py" 520a805580a6750e9f799c8740095d4cc5d6f3e3a0a3574e6052914e82c06af5
TB="/WORKSPACE/.codex/worktrees/translation-core/caps-team-B/caps-team-work/cap12-layout-invisible/PATCH/tools"
check_sha "$TB/tile_reference.py" 3aa856217071c79555427765d2c56245f7a99ec0763d041a0e3df46b01250dab
check_sha "$TB/texture_tiling_probe.py" ec0d8db08ea1ca232911b6ca6bc520b15b3bc46ad179bc9938cdf201f2185bf5
# 1. pins + diffs
if python3 -I "$HERE/pin_tools.py" --check >> "$OUT/run.log" 2>&1; then note "pins ok"; else note "STALE PINS"; FAIL=1; fi
DIFFOK=1
for f in "$TEAM"/PATCH/base/material-fallback/*.py; do
  n="$(basename "$f")"
  cmp -s "$f" "$TA/cap12-fulllook/PATCH/tools/$n" || cmp -s "$f" "$TA/cap12-overlay/PATCH/tools/$n" || { note "BASE MOVED: $n"; DIFFOK=0; }
done
cmp -s "$TEAM/PATCH/base/pak-overlay/paktx.py" "$TA/cap12-overlay/PATCH/tools/paktx.py" || { note "BASE MOVED: paktx.py"; DIFFOK=0; }
TMPD="$HERE/work/diffcheck"; rm -rf "$TMPD"; mkdir -p "$TMPD"
cp -R "$TEAM/PATCH/base/material-fallback" "$TMPD/mf"; cp "$TEAM/PATCH/base/pak-overlay/paktx.py" "$TMPD/paktx.py"
( cd "$TMPD/mf" && patch -s -p1 < "$TEAM/PATCH/material-fallback.diff" ) || DIFFOK=0
patch -s "$TMPD/paktx.py" < "$TEAM/PATCH/paktx.py.diff" || DIFFOK=0
for f in "$TMPD"/mf/*.py "$TMPD"/mf/*.swift "$TMPD"/mf/*.json; do
  cmp -s "$f" "$TEAM/PATCH/tools/$(basename "$f")" || { note "DIFF MISMATCH: $(basename "$f")"; DIFFOK=0; }
done
cmp -s "$TMPD/paktx.py" "$TEAM/PATCH/tools/paktx.py" || { note "DIFF MISMATCH: paktx.py"; DIFFOK=0; }
NF="$(ls "$TMPD"/mf | wc -l | tr -d ' ')"; NT="$(ls "$TEAM"/PATCH/tools/*.py "$TEAM"/PATCH/tools/*.swift "$TEAM"/PATCH/tools/*.json | wc -l | tr -d ' ')"
[ "$NF" -eq "$((NT - 1))" ] || { note "FILE COUNT: patched base has $NF files, staged tools $NT (incl. paktx)"; DIFFOK=0; }
rm -rf "$TMPD"
if [ $DIFFOK -eq 1 ]; then note "staged diffs ok: base + PATCH/*.diff == PATCH/tools ($NT files)"; else FAIL=1; fi
unit() {  # name dir file : unittest file printing 'Ran N tests' + OK
  log="$OUT/$1.log"
  ( cd "$2" && python3 -I "$3" ) > "$log" 2>&1
  rc=$?
  RUN="$(grep '^Ran [0-9]* test' "$log" | tail -1 | sed 's/^Ran \([0-9]*\).*/\1/')"
  grep -q '^OK$' "$log" || rc=1
  grep -q 'skipped' "$log" && rc=1
  [ -n "$RUN" ] && [ "$RUN" -gt 0 ] || rc=1
  note "$1: tests_run=${RUN:-0} rc=$rc"
  [ $rc -eq 0 ] || FAIL=1
  eval "N_$1=\${RUN:-0}"
}
suite() {  # name file : suite printing SUMMARY + GATE PASS
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
unit mf_cap12 "$TEAM/PATCH/tools" test_cap12.py
unit mf_full "$TEAM/PATCH/tools" test_fulllook.py
suite cap07 test_paktx.py
real cap07_replay real_leon_replay.py
suite cap12 test_substitute.py
# round 5b: the round-3 MIC-only real dry run is retired (a shipped instance must point at derived PS5
# textures; tex_real covers the real path incl. the A/B check against the integrated paktx)
suite textures test_textures.py
grep '\[gate\]' "$OUT/textures.log" | tee -a "$OUT/run.log" > /dev/null
real tex_real real_textures_dryrun.py
rm -rf "$HERE"/work/t[0-9]* "$HERE"/work/s04-* "$HERE"/work/sub "$HERE"/work/tex "$HERE"/work/diffcheck
if [ $FAIL -ne 0 ]; then note "RUN FAIL"; exit 1; fi
note "RUN PASS mf_cap12=${N_mf_cap12} mf_full=${N_mf_full} cap07=${N_cap07} cap12=${N_cap12} textures=${N_textures}"
