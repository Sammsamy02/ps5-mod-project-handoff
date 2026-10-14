#!/bin/bash
# One entry point for every lock-raise gate check (Mac-only, no console, no build). Prints one line per check with the
# number of things it checked, and exits 1 when any check fails or when no check ran.
# Writes only: results-r8/ (this directory) and a mktemp scratch dir that it removes.
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
B5=$(dirname "$HERE")
G="python3 -B -I $HERE/lock_raise_gate.py"
R=$HERE/results-r8; mkdir -p "$R"
T=$(mktemp -d "${TMPDIR:-/tmp}/lrg.XXXXXX"); trap 'rm -rf "$T"' EXIT
PB4="outputs/2026-10-08-l25-prep/round7-staging/b5-lock-raise/sites/base-candidate/="
P5="outputs/2026-10-08-l25-prep/round7-staging/b5-lock-raise/gate/patched/="
P6="outputs/2026-10-08-l25-prep/round7-staging/b5-lock-raise/b6-diagnostics/work/=outputs/2026-10-08-l26-runtime/typescan-r4/candidate/"
HARD="--helpers off --unprotected-calls --metamethod-calls"
ran=0; bad=0
check() {  # name want_rc grep_regex_in_output(or -) cmd...
  name=$1; want=$2; rx=$3; shift 3
  out=$("$@" 2>&1); rc=$?
  ran=$((ran+1)); ok=1
  [ "$rc" = "$want" ] || ok=0
  if [ "$rx" != "-" ]; then echo "$out" | grep -Eq "$rx" || ok=0; fi
  echo "$out" > "$R/check-$(printf %02d $ran).txt"
  if [ $ok = 1 ]; then echo "PASS  $name (exit $rc)"; else echo "FAIL  $name (wanted exit $want, got $rc; pattern '$rx')"; bad=$((bad+1)); fi
}
python3 -B "$HERE/make_lists.py" > "$R/make_lists.txt" || { echo "FAIL make_lists"; exit 1; }
check "selftest (synthetic cases + gate behaviour)" 0 'SELFTEST: [0-9]+/[0-9]+ PASS' $G --selftest
# --- real trees
check "B4 candidate, hard mode: the known bad sites are found (14 direct + 2 get_mod_ref + 1 lambda)" 1 '17 findings \(14 direct, 2 banned-call, 0 helper-call, 1 lambda\)' \
  $G --files-from "$HERE/compiled-files-b4.txt" --path-map "$PB4" $HARD
check "final-b5, hard mode: 0 findings" 0 '0 findings \(0 direct, 0 banned-call, 0 helper-call, 0 lambda\)' $G --files-from "$HERE/compiled-files-b5.txt" --path-map "$P5" $HARD
check "final-b5 + b6, hard mode, no std guard left on m_thread_actions_mutex: 0 findings, 46 tracked" 0 '46 tracked wrapper guard' \
  $G --files-from "$HERE/compiled-files-b5-b6.txt" --path-map "$P6" $HARD --forbid-raw-guard m_thread_actions_mutex
check "final-b5, helper tier vs reviewed-debt baseline: 0 NEW" 0 '38 known, 38 still present, 0 resolved, 0 NEW' \
  $G --files-from "$HERE/compiled-files-b5.txt" --path-map "$P5" --baseline "$HERE/baseline-b5-patched-UNREVIEWED.txt"
check "final-b5 + b6, helper tier vs the same baseline: 0 NEW" 0 '38 known, 38 still present, 0 resolved, 0 NEW' \
  $G --files-from "$HERE/compiled-files-b5-b6.txt" --path-map "$P6" --baseline "$HERE/baseline-b5-patched-UNREVIEWED.txt"
# --- mutants of the real trees (each must be caught)
mk() {  # out_dir src_tree_file rel old new   -> copies the compiled list with that file replaced by a mutated copy
  python3 - "$@" <<'PY'
import sys
out, src, old, new, lst_in, lst_out, match = sys.argv[1:8]
s = open(src, encoding='utf-8').read()
assert old in s, 'anchor missing'
import os
os.makedirs(out, exist_ok=True)
dst = os.path.join(out, os.path.basename(src))
open(dst, 'w', encoding='utf-8').write(s.replace(old, new))
lines = [l.rstrip('\n') for l in open(lst_in)]
res, hit = [], 0
for l in lines:
    if l.endswith(match):
        res.append(dst); hit += 1
    else:
        res.append(l)
assert hit == 1, 'list entry matched %d' % hit
open(lst_out, 'w').write('\n'.join(res) + '\n')
print('mutant written, list entries replaced=%d' % hit)
PY
}
WORKLM=$B5/b6-diagnostics/work/src/LuaMod.cpp
FINAL=$HERE/patched/outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/LuaMod.cpp
mk "$T/m1" "$FINAL" 'auto mod = get_mod_ref_cxx(lua);' 'auto mod = get_mod_ref(lua);' "$HERE/compiled-files-b5.txt" "$T/m1.txt" 'gate/patched/outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/LuaMod.cpp' > /dev/null
check "MUTANT final-b5 with get_mod_ref_cxx reverted to get_mod_ref (2 sites): hard mode fails" 1 '2 banned-call' $G --files-from "$T/m1.txt" --path-map "$P5" $HARD
mk "$T/m2" "$WORKLM" 'auto mod = get_mod_ref_cxx(lua);' 'auto mod = get_mod_ref(lua);' "$HERE/compiled-files-b5-b6.txt" "$T/m2.txt" 'b6-diagnostics/work/src/LuaMod.cpp' > /dev/null
check "MUTANT final-b5+b6 with get_mod_ref_cxx reverted (the review's exact mutant): hard mode fails" 1 '2 banned-call' $G --files-from "$T/m2.txt" --path-map "$P6" $HARD --forbid-raw-guard m_thread_actions_mutex
mk "$T/m3" "$WORKLM" 'b6diag::ActionsLock guard{LuaMod::m_thread_actions_mutex, __LINE__};' 'std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};' "$HERE/compiled-files-b5-b6.txt" "$T/m3.txt" 'b6-diagnostics/work/src/LuaMod.cpp' > /dev/null
check "MUTANT b6 with every ActionsLock swapped back to std::lock_guard: --forbid-raw-guard fails" 1 'untracked std guard' $G --files-from "$T/m3.txt" --path-map "$P6" $HARD --forbid-raw-guard m_thread_actions_mutex
mk "$T/m4" "$WORKLM" 'b6diag::ActionsLock guard{' 'b6diag::ActionsLockRenamed guard{' "$HERE/compiled-files-b5-b6.txt" "$T/m4.txt" 'b6-diagnostics/work/src/LuaMod.cpp' > /dev/null
check "MUTANT b6 with the lock wrapper renamed (B6-2 hole): the gate refuses instead of scanning fewer scopes" 2 'UNKNOWN-GUARD' $G --files-from "$T/m4.txt" --path-map "$P6" $HARD
mk "$T/m5" "$WORKLM" 'lua.throw_error("[get_mod_ref] Tried retrieving' 'lua.throw_error("[get_mod_ref] Tried retrieving' "$HERE/compiled-files-b5-b6.txt" "$T/m5.txt" 'b6-diagnostics/work/src/LuaMod.cpp' > /dev/null
check "control: an identity mutant stays PASS (the mutant plumbing itself is neutral)" 0 'PASS' $G --files-from "$T/m5.txt" --path-map "$P6" $HARD --forbid-raw-guard m_thread_actions_mutex
# --- the reviewer's gate-breaker shapes (copied to breakers/)
for f in alias autoctad h1 h2 typedef_; do
  check "breaker $f.cpp is caught" 1 'FINDING' $G "$HERE/breakers/$f.cpp" $HARD
done
check "breaker gettable.cpp is caught with --metamethod-calls" 1 'lua_gettable' $G "$HERE/breakers/gettable.cpp" $HARD
check "breaker stdfunc.cpp (callback called under a lock) is a KNOWN limit: not caught, documented in GATE.md" 0 'PASS' $G "$HERE/breakers/stdfunc.cpp" $HARD
echo "run_gates: checks=$ran failed=$bad"
[ "$ran" -gt 0 ] && [ "$bad" -eq 0 ]
