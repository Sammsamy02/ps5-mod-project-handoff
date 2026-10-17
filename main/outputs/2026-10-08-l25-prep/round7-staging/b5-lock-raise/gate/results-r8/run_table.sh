#!/bin/bash
# Regenerates the numbers of the GATE.md results table (outputs in this directory). Mac-only.
cd "$(dirname "$0")/.."
G="python3 -B -I lock_raise_gate.py"
P5="outputs/2026-10-08-l25-prep/round7-staging/b5-lock-raise/gate/patched/="
P6="outputs/2026-10-08-l25-prep/round7-staging/b5-lock-raise/b6-diagnostics/work/=outputs/2026-10-08-l26-runtime/typescan-r4/candidate/"
PB4="outputs/2026-10-08-l25-prep/round7-staging/b5-lock-raise/sites/base-candidate/="
R=results-r8
python3 -B make_lists.py > $R/make_lists.txt
$G --selftest > $R/selftest.txt 2>&1;                                                        echo "selftest rc=$?" > $R/rc.txt
$G --files-from compiled-files-b4.txt --path-map "$PB4" --helpers off --unprotected-calls --metamethod-calls > $R/B4-hard.txt 2>&1;     echo "B4-hard rc=$?" >> $R/rc.txt
$G --files-from compiled-files-b4.txt --path-map "$PB4" > $R/B4-full.txt 2>&1;                                                       echo "B4-full rc=$?" >> $R/rc.txt
$G --files-from compiled-files-b5.txt --path-map "$P5" --helpers off --unprotected-calls --metamethod-calls > $R/b5-hard.txt 2>&1; echo "b5-hard rc=$?" >> $R/rc.txt
$G --files-from compiled-files-b5.txt --path-map "$P5" > $R/b5-full.txt 2>&1;                    echo "b5-full rc=$?" >> $R/rc.txt
$G --files-from compiled-files-b5.txt --path-map "$P5" --baseline baseline-b5-patched-UNREVIEWED.txt > $R/b5-baseline.txt 2>&1; echo "b5-baseline rc=$?" >> $R/rc.txt
$G --files-from compiled-files-b5-b6.txt --path-map "$P6" --helpers off --unprotected-calls --metamethod-calls --forbid-raw-guard m_thread_actions_mutex > $R/b5b6-hard.txt 2>&1; echo "b5b6-hard rc=$?" >> $R/rc.txt
$G --files-from compiled-files-b5-b6.txt --path-map "$P6" > $R/b5b6-full.txt 2>&1;               echo "b5b6-full rc=$?" >> $R/rc.txt
$G --files-from compiled-files-b5-b6.txt --path-map "$P6" --baseline baseline-b5-patched-UNREVIEWED.txt > $R/b5b6-baseline.txt 2>&1; echo "b5b6-baseline rc=$?" >> $R/rc.txt
$G --files-from compiled-files-b5.txt --path-map "$P5" --helpers off --unprotected-calls --metamethod-calls --list-locks > $R/b5-hard-list-locks.txt 2>&1
bash run_gates.sh > $R/run_gates.txt 2>&1;                                                       echo "run_gates rc=$?" >> $R/rc.txt
cat $R/rc.txt
for f in B4-hard B4-full b5-hard b5-full b5b6-hard b5b6-full; do echo "--- $f"; grep -E 'checked [0-9]+ files|findings \(' $R/$f.txt | cut -c1-330; done
