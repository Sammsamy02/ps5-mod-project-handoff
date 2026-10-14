#!/bin/zsh
# Team D: the project's two pre-compile gates on every staged runtime source of
# #9 + #11 (Mac-only, read-only on the project trees; copies go to a temp dir).
#   1. no_detour_gate + fs_throw_gate on all NEW / replaced staged sources
#      (cap09 + cap11: new/include, new/src, candidate/src/Mod, cap09 candidate/include) -> must PASS
#   2. no_detour_gate --patch on the hunks for the two LIVE files and the cap11 diff
#   3. full-copy differential: the gates on the live UE4SSProgram.cpp / LuaMod.cpp
#      vs. the staged copies; any site the staged copy adds = FAIL
#   4. [F1] the gates' fs_throw rule on EVERY file the patch makes reachable
#      (compiler include closure of the patched units + the loader), not only
#      the staged ones: 0 new sites vs the project baseline over the closure,
#      and 0 sites at all in code never executed before (cppmod closure + loader)
#      except baseline debt in a function the unit provably never uses.
# Prints counts; any zero count or failure exits non-zero.
set -u
TEAM=${0:A:h}
ROOT=/WORKSPACE/.codex/worktrees/translation-core/what
H=$ROOT/outputs/2026-10-08-l25-prep/harvest
LIVE=$ROOT/outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src
TMP=${TMPDIR:-/tmp}/teamD-gates-$$
OUT=$TEAM/gates-output
mkdir -p $TMP/new $TMP/live/src $TMP/staged/src $OUT
fail=0
: > $OUT/SUMMARY.txt
note() { print -r -- "$*" | tee -a $OUT/SUMMARY.txt }

# 1. new / replaced staged sources
n=0
for d in $TEAM/cap09/PATCH/new $TEAM/cap09/PATCH/candidate/src/Mod $TEAM/cap09/PATCH/candidate/include $TEAM/cap11/PATCH/new $TEAM/cap11/PATCH/candidate/src/Mod; do
  tag=${${d#$TEAM/}//\//_}
  mkdir -p $TMP/new/$tag
  for f in $(find $d -type f \( -name '*.h' -o -name '*.hpp' -o -name '*.inc' -o -name '*.cpp' \)); do
    rel=${f#$d/}; mkdir -p $TMP/new/$tag/${rel:h}; cp $f $TMP/new/$tag/$rel; n=$((n+1))
  done
done
note "staged new/replaced sources copied: $n"
[[ $n -eq 0 ]] && { note "FAIL: zero sources"; fail=1 }
python3 -I $H/no_detour_gate.py --root $TMP/new > $OUT/no_detour_new.log 2>&1; rc=$?
note "no_detour_gate (new sources): exit=$rc | $(grep -E 'files|scanned|PASS|FAIL' $OUT/no_detour_new.log | tail -2 | tr '\n' ' ')"
[[ $rc -ne 0 ]] && fail=1
python3 -I $H/fs_throw_gate.py --root $TMP/new > $OUT/fs_throw_new.log 2>&1; rc=$?
note "fs_throw_gate (new sources): exit=$rc | $(grep -E 'checked|PASS|FAIL|VIOL' $OUT/fs_throw_new.log | tail -2 | tr '\n' ' ')"
[[ $rc -ne 0 ]] && fail=1

# 2. patch mode on the hunks
python3 -I $H/no_detour_gate.py --patch $TEAM/cap09/PATCH/diffs/UE4SSProgram.cpp.diff --patch $TEAM/cap09/PATCH/diffs/LuaMod.cpp.diff \
  --patch $TEAM/cap09/PATCH/diffs/CppMod.cpp.diff --patch $TEAM/cap09/PATCH/diffs/NativeCppModPath.hpp.diff \
  --patch $TEAM/cap11/PATCH/diffs/CppMod.cpp.on-top-of-cap09.diff > $OUT/no_detour_patches.log 2>&1; rc=$?
note "no_detour_gate (5 patches): exit=$rc | $(grep -E 'patch|PASS|FAIL' $OUT/no_detour_patches.log | tail -2 | tr '\n' ' ')"
[[ $rc -ne 0 ]] && fail=1

# 3. full-copy differential (same relative names, so site keys compare 1:1)
for f in UE4SSProgram.cpp LuaMod.cpp; do cp $LIVE/$f $TMP/live/src/$f; cp $TEAM/cap09/PATCH/candidate/src/$f $TMP/staged/src/$f; done
for g in no_detour_gate fs_throw_gate; do
  python3 -I $H/$g.py --root $TMP/live > $OUT/${g}_live.log 2>&1; rl=$?
  python3 -I $H/$g.py --root $TMP/staged > $OUT/${g}_staged.log 2>&1; rs=$?
  # site lines without line numbers / temp prefixes
  norm() { grep -E '^\s*(src/|.*\.cpp:)' $1 | sed -E 's#^.*/(live|staged)/##; s#:[0-9]+:#:#' | sort }
  added=$(comm -13 <(norm $OUT/${g}_live.log) <(norm $OUT/${g}_staged.log) | wc -l | tr -d ' ')
  nl=$(norm $OUT/${g}_live.log | wc -l | tr -d ' '); ns=$(norm $OUT/${g}_staged.log | wc -l | tr -d ' ')
  scanned=$(grep -Eo '(scanned|checked) [0-9]+' $OUT/${g}_staged.log | head -1)
  note "$g full copies: ${scanned:-NO COUNT} | live exit=$rl sites=$nl | staged exit=$rs sites=$ns | sites added by the staged hunks=$added"
  [[ -z "$scanned" || "${scanned##* }" -eq 0 ]] && fail=1
  [[ $added -ne 0 ]] && { fail=1; comm -13 <(norm $OUT/${g}_live.log) <(norm $OUT/${g}_staged.log) | head -5 | tee -a $OUT/SUMMARY.txt }
  [[ $rs -ne 0 && $rl -eq 0 ]] && fail=1
done
# 4. [F1] fs_throw_gate on EVERY file the patch makes reachable, not only the staged ones.
#    reach_files.py = clang -MM closure of the patched units (R11's own compile commands,
#    staged include dirs first) + the native module loader the patch now runs.
R=$TMP/reach.txt
python3 -I $TEAM/reach_files.py --unit cppmod=$TEAM/cap11/PATCH/candidate/src/Mod/CppMod.cpp \
  --unit program=$TEAM/cap09/PATCH/candidate/src/UE4SSProgram.cpp --unit luamod=$TEAM/cap09/PATCH/candidate/src/LuaMod.cpp \
  --include $TEAM/cap11/PATCH/candidate/src/Mod --include $TEAM/cap09/PATCH/candidate/src/Mod --include $TEAM/cap09/PATCH/candidate/include \
  --include $TEAM/cap11/PATCH/new/include --include $TEAM/cap09/PATCH/new/include \
  --odr-unused 'RC::Platform::ValidateNativeProgramContext' --odr-used-control 'RC::Platform::FindNativeCppModModule' \
  --out $R > $OUT/reach_files.log 2>&1; rc=$?
nreach=$(grep -Eo 'items_checked=[0-9]+' $OUT/reach_files.log | cut -d= -f2)
note "reach_files: exit=$rc | $(grep -E 'items_checked|odr_' $OUT/reach_files.log | tr '\n' ' ')"
[[ $rc -ne 0 || -z "$nreach" || "$nreach" -eq 0 ]] && fail=1
grep -q 'cap09/PATCH/candidate/include/Platform/NativeCppModPath.hpp$' $R.cppmod 2>/dev/null || { note "FAIL: the closure does not use the STAGED NativeCppModPath.hpp"; fail=1 }
# 4a. whole closure against the project's baseline: no NEW throwing call anywhere reachable.
#     The two live full copies are left out here (section 3 already proves their hunks add 0 sites;
#     their staged path would not match the baseline's candidate-tree keys).
grep -v -e '/cap09/PATCH/candidate/src/UE4SSProgram.cpp$' -e '/cap09/PATCH/candidate/src/LuaMod.cpp$' $R > $R.nofull
FSB=$H/fs_throw_baseline-1e49188b.txt
python3 -I $H/fs_throw_gate.py --files-from $R.nofull --baseline $FSB > $OUT/fs_throw_reachable.log 2>&1; rc=$?
note "fs_throw_gate (reachable closure, $(wc -l < $R.nofull | tr -d ' ') files, baseline $(basename $FSB)): exit=$rc | $(grep -E 'checked|NEW|PASS' $OUT/fs_throw_reachable.log | tail -2 | tr '\n' ' ')"
[[ $rc -ne 0 ]] && fail=1
# 4b. the code that was NEVER executed on the console before (cppmod unit closure + loader):
#     ZERO throwing calls, except baseline debt inside a function the cppmod unit provably never
#     uses (-O0 symbol check above; pinned file sha so the line range stays valid).
python3 -I $H/fs_throw_gate.py --files-from $R.cppmod > $OUT/fs_throw_newly_executed.log 2>&1
python3 -I - $OUT/fs_throw_newly_executed.log $OUT/reach_files.log >> $OUT/fs_throw_newly_executed.log 2>&1 <<'EOF'
import hashlib, re, sys
log = open(sys.argv[1]).read(); reach = open(sys.argv[2]).read()
NPC = "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/include/Platform/NativeProgramContext.hpp"
EXEMPT = [  # (relpath suffix, pinned sha256, first line, last line, function, reason)
    ("port/ue4ss/include/Platform/NativeProgramContext.hpp", NPC, "0a068bb9d9b25f9b7213d584dabf26cadc793c42c93a0bb125f37d83c0a58f48", 92, 136, "RC::Platform::ValidateNativeProgramContext",
     "called only by the program unit at startup (already live and console-proven in R11); the cppmod unit never uses it (-O0 symbol check)")]
m = re.search(r"checked (\d+) files", log)
files = int(m.group(1)) if m else 0
viol = re.findall(r"^\s+(\S+?):(\d+): (\w+) \(", log, re.M)
bad = []
for rel, line, fn in viol:
    ok = False
    for suffix, path, pin, lo, hi, func, why in EXEMPT:
        same_file = hashlib.sha256(open(path, "rb").read()).hexdigest() == pin
        if rel.endswith(suffix) and same_file and lo <= int(line) <= hi and f"never uses {func}" in reach:
            ok = True
    (print(f"  accepted (ODR-proven unused function): {rel}:{line} {fn}") if ok else bad.append(f"{rel}:{line} {fn}"))
for suffix, path, pin, lo, hi, func, why in EXEMPT:
    now = hashlib.sha256(open(path, "rb").read()).hexdigest()
    print(f"  exemption {suffix} lines {lo}-{hi} ({func}): pinned sha {pin[:8]} {'matches' if now == pin else 'CHANGED -> exemption void'}; {why}")
for b in bad:
    print(f"  FAIL newly executed throwing call: {b}")
print(f"newly_executed files_checked={files} sites_flagged={len(viol)} accepted={len(viol) - len(bad)} failed={len(bad)}")
sys.exit(2 if files == 0 else (1 if bad else 0))
EOF
rc=$?
note "fs_throw_gate (newly executed: cppmod closure + loader): exit=$rc | $(tail -1 $OUT/fs_throw_newly_executed.log)"
[[ $rc -ne 0 ]] && fail=1

note "GATES result=$([[ $fail -eq 0 ]] && echo PASS || echo FAIL)"
rm -rf $TMP
exit $fail
