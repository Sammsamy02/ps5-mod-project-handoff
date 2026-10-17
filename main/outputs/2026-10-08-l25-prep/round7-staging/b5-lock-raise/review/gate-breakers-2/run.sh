#!/bin/sh
# Reviewer-2 breakers. Each case SHOULD exit 1 from the gate. Prints checked count; exit 2 if zero checked.
G=/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/round7-staging/b5-lock-raise/gate/lock_raise_gate.py
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
MODE="${MODE-"--helpers off --unprotected-calls --metamethod-calls"}"
n=0; miss=0
for f in "$HERE"/c*.cpp; do
  n=$((n+1))
  python3 -B -I "$G" $MODE "$f" >/dev/null 2>&1; rc=$?
  if [ "$rc" -ne 1 ]; then miss=$((miss+1)); echo "MISSED (rc=$rc): $(basename "$f")"; else echo "caught: $(basename "$f")"; fi
done
echo "checked $n cases, gate missed $miss (mode: $MODE)"
[ "$n" -eq 0 ] && exit 2
[ "$miss" -ne 0 ] && exit 1
exit 0
