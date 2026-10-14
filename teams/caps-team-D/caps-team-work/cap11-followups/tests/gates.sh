#!/bin/zsh
# The integrator's pre-compile gates (field_gate, no_detour_gate, fs_throw_gate from l25-prep/harvest, run
# read-only) on a SCRATCH copy of the live candidate tree with this patch's two .inc files swapped in, plus the
# two patched headers; the live tree itself is only read. Field gate: the live candidate is gated too, so the
# field lists can be compared (the patch must add no engine field read).
set -u
HERE=${0:A:h}
FU=${HERE:h}
LIVE=/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-runtime/typescan-r4
H=/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/harvest
OUT=$HERE/output/gates
SCR=${TMPDIR:-/tmp}/cap11-fu-gates-$$
mkdir -p $OUT $SCR
cp -R $LIVE/candidate $SCR/candidate
cp $FU/PATCH/post/candidate/src/Mod/NativeGuiEngineBackend.inc $FU/PATCH/post/candidate/src/Mod/NativeModEngineServices.inc $SCR/candidate/src/Mod/
mkdir -p $SCR/include/Platform
cp $FU/PATCH/post/include/Platform/NativeGuiHost.hpp $FU/PATCH/post/include/Platform/NativeModHostCore.hpp $SCR/include/Platform/
fail=0
python3 -I $H/field_gate.py --list-out $OUT/fields-live.txt > $OUT/field_gate_live.log 2>&1; r=$?; print "field_gate live candidate: exit=$r  $(tail -1 $OUT/field_gate_live.log)"
python3 -I $H/field_gate.py --root $SCR/candidate --list-out $OUT/fields-patched.txt > $OUT/field_gate_patched.log 2>&1; r=$?; print "field_gate patched copy:   exit=$r  $(tail -1 $OUT/field_gate_patched.log)"; (( r == 0 )) || fail=1
grep "distinct engine fields" $OUT/field_gate_live.log $OUT/field_gate_patched.log
if diff <(grep -v '^#' $OUT/fields-live.txt | cut -f1,2) <(grep -v '^#' $OUT/fields-patched.txt | cut -f1,2) > $OUT/fields.diff; then
  print "field list: identical (the patch adds no engine field read; $(grep -vc '^#' $OUT/fields-patched.txt) fields)"
else print "field list differs:"; cat $OUT/fields.diff; fail=1; fi
python3 -I $H/no_detour_gate.py --root $SCR/candidate --root $SCR/include > $OUT/no_detour_gate.log 2>&1; r=$?; print "no_detour_gate: exit=$r  $(grep -E 'PASS|FAIL|files' $OUT/no_detour_gate.log | tail -2 | tr '\n' ' ')"; (( r == 0 )) || fail=1
# fs_throw: (a) the four changed files alone must PASS; (b) the whole patched copy must show exactly the live
# tree's pre-existing sites (the gate's baseline is keyed by live paths, so a copy lists them as violations)
mkdir -p $SCR/changed
cp $SCR/candidate/src/Mod/NativeGuiEngineBackend.inc $SCR/candidate/src/Mod/NativeModEngineServices.inc $SCR/include/Platform/*.hpp $SCR/changed/
python3 -I $H/fs_throw_gate.py --root $SCR/changed > $OUT/fs_throw_gate_changed.log 2>&1; r=$?; print "fs_throw_gate (changed files): exit=$r  $(head -3 $OUT/fs_throw_gate_changed.log | tr '\n' ' ')"; (( r == 0 )) || fail=1
python3 -I $H/fs_throw_gate.py > $OUT/fs_throw_gate_live.log 2>&1; rl=$?
python3 -I $H/fs_throw_gate.py --root $SCR/candidate > $OUT/fs_throw_gate_copy.log 2>&1
grep -Eo '/candidate/src/[^:]+:[0-9]+: [a-z_]+' $OUT/fs_throw_gate_copy.log | sed 's|.*/candidate/|candidate/|' | sort > $OUT/fs_throw_copy_sites.txt
grep -Eo '/candidate/src/[^:]+:[0-9]+: [a-z_]+' $OUT/fs_throw_gate_live.log | sed 's|.*/candidate/|candidate/|' | sort > $OUT/fs_throw_live_sites.txt
if [[ -s $OUT/fs_throw_live_sites.txt ]] && diff $OUT/fs_throw_live_sites.txt $OUT/fs_throw_copy_sites.txt > /dev/null; then
  print "fs_throw_gate (whole copy): the same $(wc -l < $OUT/fs_throw_copy_sites.txt | tr -d ' ') pre-existing sites as the live tree, none in a changed file; the live tree's own default run reports the same list (exit=$rl: $(grep -E 'PASS|VIOLATIONS' $OUT/fs_throw_gate_live.log | head -1))"
else print "fs_throw_gate (whole copy): site lists differ or live list empty (see $OUT)"; fail=1; fi
rm -rf $SCR
print "gates result=$([[ $fail -eq 0 ]] && echo PASS || echo FAIL)"
exit $fail
