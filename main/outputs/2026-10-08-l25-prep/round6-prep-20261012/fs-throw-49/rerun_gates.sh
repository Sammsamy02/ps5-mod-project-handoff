#!/bin/zsh
# rerun_gates.sh -- fs-throw-49: reruns every fs_throw_gate.py check of this
# track and rewrites gate/*.txt. Mac-only, read-only on everything outside this
# directory (python -B: no __pycache__ is written into harvest/).
# Prints how many gate runs it did; exits 1 if any run's exit code differs from
# the expected one, 2 if ZERO runs happened.
set -u
WHAT=/WORKSPACE/.codex/worktrees/translation-core/what
D=$WHAT/outputs/2026-10-08-l25-prep/round6-prep-20261012/fs-throw-49
G=$D/gate
GATE=$WHAT/outputs/2026-10-08-l25-prep/harvest/fs_throw_gate.py
BL=$WHAT/outputs/2026-10-08-l25-prep/harvest/fs_throw_baseline-1e49188b.txt
cd $WHAT || exit 2
# keep the gate selftest's temporary files inside this track directory
mkdir -p $G/tmp && export TMPDIR=$G/tmp
runs=0; bad=0
run() { # name expected-exit args...
  local name=$1 want=$2; shift 2
  python3 -B $GATE "$@" > $G/$name.txt 2>&1
  local rc=$?
  echo "exit=$rc (expected $want)" >> $G/$name.txt
  runs=$((runs+1))
  if [[ $rc != $want ]]; then bad=$((bad+1)); fi
  printf '%-40s exit=%s expected=%s | %s\n' $name $rc $want "$(grep -m1 -E 'checked [0-9]+ files|SELFTEST' $G/$name.txt | cut -c1-150)"
  grep -E 'baseline .*known sites' $G/$name.txt | sed 's/^/    /'
}
run gate-selftest                           0 --selftest
run gate-current-compiled-set-vs-baseline   0 --files-from $G/compiled-set-current-l26A-pass1.list --baseline $BL
run gate-CONTROL-vs-rebased-baseline        0 --files-from $G/compiled-set-CONTROL.list --baseline $G/baseline-1e49188b-rebased-CONTROL.txt
run gate-P1-vs-rebased-baseline             0 --files-from $G/compiled-set-P1.list --baseline $G/baseline-1e49188b-rebased-P1.txt
run gate-P1-patched-root-only               0 --root $D/patched
run gate-CONTROL-root-only                  1 --root $D/control
run gate-extra-scope-prebuilt               0 --files-from $G/unscanned-prebuilt-deps.list
run gate-extra-scope-package-lineage        1 --root $WHAT/outputs/2026-10-08-l26-package-A/lineage
echo "RERUN-GATES: $runs gate runs, $bad with an unexpected exit code"
if [[ $runs == 0 ]]; then echo 'RERUN-GATES: FAIL -- ZERO runs'; exit 2; fi
if [[ $bad != 0 ]]; then exit 1; fi
exit 0
