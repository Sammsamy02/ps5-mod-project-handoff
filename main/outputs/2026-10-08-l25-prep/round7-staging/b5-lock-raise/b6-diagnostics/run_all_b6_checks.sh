#!/bin/sh
# One entry point for every B6 check (all Mac-only, no console, no PRX build).
# Prints one PASS/FAIL line per step and the number of steps; exits 1 on any failure or when
# nothing ran. The host-test compile is skipped (reported as SKIP, counted as not-run) when the
# Mac has under 2 GB free.
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$HERE"
WT=/WORKSPACE/.codex/worktrees/translation-core/what
H=$WT/outputs/2026-10-08-l25-prep/harvest
SCR=/LOCAL_SCRATCH
ran=0; failed=0; skipped=0
step() { name=$1; shift; "$@" > "gates/step-$(echo "$name" | tr ' /' '__').txt" 2>&1; rc=$?
  ran=$((ran+1))
  if [ $rc -eq 0 ]; then echo "PASS  $name"; elif [ $rc -eq 3 ]; then echo "SKIP  $name (memory)"; skipped=$((skipped+1)); else echo "FAIL  $name (exit $rc)"; failed=$((failed+1)); fi; }
mkdir -p gates
step "make_diff rebuilds work/ and b6.diff" ./make_diff.sh
step "SITES.txt regenerated" python3 -B parts/make_sites.py
step "patch chain: candidate+B5+b6 == work" tests/verify_patch_chain.sh $SCR/chain
step "source gate (includes, removed lines, markers, slices, tracked locks, pump)" python3 -B tests/b6_source_gate.py
step "ONE gate, hard mode, B6 LuaMod.cpp alone (--forbid-raw-guard)" python3 -B lock_raise_gate_b6.py --require-tracked work/src/LuaMod.cpp
GD=$HERE/../gate
step "ONE gate, hard mode, whole compiled set = B4 list + final-b5 + b6 work files" python3 -B -I $GD/lock_raise_gate.py --files-from $GD/compiled-files-b5-b6.txt --path-map outputs/2026-10-08-l25-prep/round7-staging/b5-lock-raise/b6-diagnostics/work/=outputs/2026-10-08-l26-runtime/typescan-r4/candidate/ --helpers off --unprotected-calls --metamethod-calls --forbid-raw-guard m_thread_actions_mutex
step "ONE gate, helper tier: no site beyond the reviewed-debt baseline" python3 -B -I $GD/lock_raise_gate.py --files-from $GD/compiled-files-b5-b6.txt --path-map outputs/2026-10-08-l25-prep/round7-staging/b5-lock-raise/b6-diagnostics/work/=outputs/2026-10-08-l26-runtime/typescan-r4/candidate/ --baseline $GD/baseline-b5-patched-UNREVIEWED.txt
step "gate self-test" tests/gate_selftest.sh
step "no_detour_gate on work" python3 -B -I $H/no_detour_gate.py --root $HERE/work
step "fs_throw: same violation set as base" sh -c "python3 -B -I $H/fs_throw_gate.py --root $HERE/base > gates/fs_throw-base.txt 2>&1; python3 -B -I $H/fs_throw_gate.py --root $HERE/work > gates/fs_throw-work.txt 2>&1; for t in base work; do sed -E 's/^ *outputs[^ ]*(base|work)\\/src\\///; s/:[0-9]+:/:/' gates/fs_throw-\$t.txt | grep -E 'no error_code|throwing' | sort > gates/fs_throw-\$t.norm; done; n=\$(wc -l < gates/fs_throw-work.norm); echo \"violations compared: \$n\"; [ \"\$n\" -gt 0 ] && diff gates/fs_throw-base.norm gates/fs_throw-work.norm"
step "host + pump + shim tests (real Lua C)" tests/build_and_run.sh
step "mutation check" tests/mutation_check.sh
echo "steps=$ran failed=$failed skipped=$skipped"
[ "$ran" -gt 0 ] && [ "$failed" -eq 0 ] && [ "$skipped" -eq 0 ]
