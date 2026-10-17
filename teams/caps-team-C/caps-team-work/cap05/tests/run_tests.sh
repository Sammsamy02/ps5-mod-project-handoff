#!/bin/bash
# cap05 host tests (Mac-only; no console contact; no PRX build).
# Runs: (1) python unit/integration suite on real retained artifacts + evidence,
#       (2) the C++ crash-test core extracted from the PATCHED LuaMod.cpp, built
#           with /usr/bin/clang++ -std=c++20 at -O0 and -O2 (one compile at a time),
#       (3) the zzcrashtest Lua mod under stubs (host lua),
#       (4) the stage script refuses a double apply,
#       (5) the post-harvest wrapper on a copy of real launch evidence,
#       (6) build step 8b (final symbols) is NON-FATAL: bad inputs -> WARN, exit 0,
#           no RESULT=FAIL; real R11 inputs (located by sha) -> PASS.
# Every step prints its checked count; the run FAILS if any step checked zero.
set -u
CAP="$(cd "$(dirname "$0")/.." && pwd)"
T="$CAP/tests"; OUT="$T/out"; GEN="$OUT/gen"
mkdir -p "$OUT" "$GEN"
export PYTHONDONTWRITEBYTECODE=1
fail=0; steps=0

echo "== 1/6 python suite =="
python3 -I -B "$T/test_cap05.py" > "$OUT/python-suite.txt" 2>&1; rc=$?
tail -4 "$OUT/python-suite.txt"
n=$(grep -Eo '^Ran [0-9]+ test' "$OUT/python-suite.txt" | grep -Eo '[0-9]+'); n=${n:-0}
echo "python: ran $n tests rc=$rc"; [ "$rc" -eq 0 ] && [ "$n" -gt 0 ] || fail=1; steps=$((steps+1))

echo "== 2/6 C++ crash-test core (extracted from the patched LuaMod.cpp) =="
PATCHED="$CAP/PATCH/work/LuaMod.cpp.cap05"
python3 -I -B - "$PATCHED" "$GEN/cap05_core.inc" <<'EOF' || fail=1
import sys
s = open(sys.argv[1]).read()
b = s.index("    // [CAP05-CRASHTEST-BEGIN]"); e = s.index("    // [CAP05-CRASHTEST-END]")
open(sys.argv[2], "w").write(s[b:e])
print("extracted %d bytes between the markers" % (e - b))
EOF
echo "host arch: $(uname -m) ('trap' = SIGTRAP 5 on arm64; on the PS5 (x86-64) it is ud2 = SIGILL 4; the test accepts both; the launch uses only 'segv')"
cxx_checked=0
for opt in -O0 -O2; do
  /usr/bin/clang++ -std=c++20 $opt -Wall -Wextra -I "$GEN" "$T/crashtest_core_host.cpp" -o "$GEN/crashtest_core_host$opt" 2> "$OUT/cxx-build$opt.txt"; brc=$?
  if [ $brc -ne 0 ]; then echo "compile $opt FAILED"; cat "$OUT/cxx-build$opt.txt"; fail=1; continue; fi
  "$GEN/crashtest_core_host$opt" > /dev/null 2> "$OUT/cxx-run$opt.txt"; rrc=$?
  echo "[$opt]"; cat "$OUT/cxx-run$opt.txt"
  c=$(sed -n 's/.*checked \([0-9]*\) cases.*/\1/p' "$OUT/cxx-run$opt.txt"); cxx_checked=$((cxx_checked + ${c:-0}))
  [ $rrc -eq 0 ] || fail=1
done
echo "c++: checked $cxx_checked cases"; [ "$cxx_checked" -gt 0 ] || fail=1; steps=$((steps+1))

echo "== 3/6 zzcrashtest Lua mod under stubs =="
lua "$T/lua_mod_host_test.lua" "$CAP/PATCH/zzcrashtest/Scripts/main.lua" 2> "$OUT/lua-mod.txt"; lrc=$?
cat "$OUT/lua-mod.txt"
c=$(sed -n 's/.*checked \([0-9]*\), failed.*/\1/p' "$OUT/lua-mod.txt")
[ $lrc -eq 0 ] && [ "${c:-0}" -gt 0 ] || fail=1; steps=$((steps+1))

echo "== 4/6 stage script: re-apply onto the patched file must refuse =="
rm -f "$OUT/double.cpp" "$OUT/double.diff"
python3 -I -B "$CAP/PATCH/apply_cap05.py" --base "$PATCHED" --out-file "$OUT/double.cpp" --out-diff "$OUT/double.diff" 2> "$OUT/double-apply.txt"; drc=$?
cat "$OUT/double-apply.txt"
if [ $drc -eq 2 ] && [ ! -e "$OUT/double.cpp" ]; then echo "double-apply refused: PASS (1 check)"; else echo "double-apply: FAIL"; fail=1; fi
steps=$((steps+1))

echo "== 5/6 post-harvest wrapper on a copy of the launch-3 evidence =="
WE="$OUT/wrapper-ev"; rm -rf "$WE"; mkdir -p "$WE"
L3=/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/evidence/launch-l26-B6b-launch3-20261012
cp "$L3/klog-20261012-162139.log" "$L3/harvest-l26-B6b-launch3-20261012-ue4ss.log" "$WE/"
bash "$CAP/tools/post_harvest_crashcap.sh" "$WE" "$CAP/evidence/final-symbols-B6b.json" "" --expect-crash 2> "$OUT/wrapper.txt"; wrc=$?
cat "$OUT/wrapper.txt"
if [ $wrc -eq 0 ] && grep -q "crash blocks=1" "$OUT/wrapper.txt" && [ -s "$WE/CRASH-REPORT.md" ]; then echo "wrapper: PASS (1 check)"; else echo "wrapper: FAIL"; fail=1; fi
EW="$OUT/wrapper-empty"; rm -rf "$EW"; mkdir -p "$EW"
bash "$CAP/tools/post_harvest_crashcap.sh" "$EW" "$CAP/evidence/final-symbols-B6b.json" 2>> "$OUT/wrapper.txt"; erc=$?
if [ $erc -eq 3 ]; then echo "wrapper with no klog -> exit 3: PASS (1 check)"; else echo "wrapper no-klog: FAIL rc=$erc"; fail=1; fi
steps=$((steps+1))

echo "== 6/6 build step 8b (final symbols) is non-fatal =="
S8="${OUT:?}/step8b"; rm -rf "${OUT:?}/step8b"
mkdir -p "$S8/h/crashcap" "$S8/ev-bad" "$S8/ev-exist" "$S8/ev-good" "$S8/pkg-bad" "$S8/pkg-exist" "$S8/pkg-good/pass-1"
cp "$CAP/tools/final_symbols.py" "$CAP/tools/elf64.py" "$S8/h/crashcap/"
python3 -I -B - "$CAP/PATCH/work/run_labelled_build.sh.cap05" "$S8/step8b.sh" <<'EOF' || fail=1
import sys
s = open(sys.argv[1]).read()
b = s.index('say "8b/9 '); e = s.index('say "9/9 record"')
open(sys.argv[2], "w").write(s[b:e])
print("extracted step 8b: %d bytes" % (e - b))
EOF
# same shell setup as run_labelled_build.sh: set -u -o pipefail, identical say/fail
run8b() {  # $1=PKG_A $2=EV $3=PRX
  LABEL=test8b PKG_A="$1" EV="$2" PRX="$3" H="$S8/h" STEP8B="$S8/step8b.sh" bash -c '
    set -u -o pipefail
    say()  { printf "[%s] %s\n" "$(date -u +%H:%M:%SZ)" "$*"; }
    fail() { say "FAIL at step: $*"; printf "RESULT=FAIL\nLABEL=%s\nSTEP=%s\n" "$LABEL" "$*" > "$EV/RESULT"; exit 1; }
    . "$STEP8B"
    echo "REACHED-STEP-9"'
}
c8=0
o=$(run8b "$S8/pkg-bad" "$S8/ev-bad" "$S8/pkg-bad/none.prx" 2>&1); r=$?; echo "$o" | tail -2
if [ $r -eq 0 ] && echo "$o" | grep -q REACHED-STEP-9 && [ ! -e "$S8/ev-bad/RESULT" ] \
   && grep -q '^FINAL_SYMBOLS=WARN: final_symbols rc=' "$S8/ev-bad/FINAL-SYMBOLS-STATUS"; then
  echo "8b bad inputs -> WARN, build continues: PASS"; c8=$((c8+1)); else echo "8b bad inputs: FAIL rc=$r"; fail=1; fi
echo keep > "$S8/pkg-exist/modbridge_supplier_composition.prx.final-symbols.json"
o=$(run8b "$S8/pkg-exist" "$S8/ev-exist" "$S8/pkg-exist/none.prx" 2>&1); r=$?; echo "$o" | tail -2
if [ $r -eq 0 ] && echo "$o" | grep -q REACHED-STEP-9 && [ ! -e "$S8/ev-exist/RESULT" ] \
   && grep -q 'existing .* left untouched' "$S8/ev-exist/FINAL-SYMBOLS-STATUS" \
   && [ "$(cat "$S8/pkg-exist/modbridge_supplier_composition.prx.final-symbols.json")" = keep ]; then
  echo "8b existing output -> WARN, file untouched, build continues: PASS"; c8=$((c8+1)); else echo "8b existing output: FAIL rc=$r"; fail=1; fi
R11LOC=$(cd "$T" && python3 -I -B -c 'import sys; sys.path.insert(0, "."); import test_cap05 as t; print(t.R11); print(t.R11_PACKAGED)' 2>/dev/null)
R11P1=$(echo "$R11LOC" | sed -n 1p); R11PRX=$(echo "$R11LOC" | sed -n 2p)
if [ -n "$R11P1" ] && [ -n "$R11PRX" ]; then
  ln -s "$R11P1/build" "$S8/pkg-good/pass-1/build"; ln -s "$R11P1/bare-original" "$S8/pkg-good/pass-1/bare-original"
  o=$(run8b "$S8/pkg-good" "$S8/ev-good" "$R11PRX" 2>&1); r=$?; echo "$o" | tail -3
  if [ $r -eq 0 ] && grep -qx 'FINAL_SYMBOLS=PASS' "$S8/ev-good/FINAL-SYMBOLS-STATUS" \
     && [ -s "$S8/ev-good/modbridge_supplier_composition.prx.final-symbols.json" ] \
     && grep -q 'final_symbols: PASS .*packaged=bfd95db6' "$S8/ev-good/final-symbols.txt"; then
    echo "8b real R11 inputs (located by sha) -> PASS: PASS"; c8=$((c8+1)); else echo "8b real R11: FAIL rc=$r"; fail=1; fi
else echo "8b: could not locate R11 by sha: FAIL"; fail=1; fi
echo "step 8b: checked $c8 cases"; [ "$c8" -eq 3 ] || fail=1; steps=$((steps+1))

echo "== summary: $steps steps run; overall $([ $fail -eq 0 ] && echo PASS || echo FAIL) =="
exit $fail
