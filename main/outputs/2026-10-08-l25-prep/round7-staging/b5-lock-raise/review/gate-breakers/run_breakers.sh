#!/bin/sh
# Reviewer reproducer (Mac-only, read-only on everything else). Each case is a lock-then-raise
# shape that SHOULD make lock_raise_gate.py exit 1. Prints how many cases were checked and how many
# the gate MISSED (exit 0 or exit 2). Exits 1 if any miss, 2 if zero cases were checked.
G=/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/round7-staging/b5-lock-raise/gate/lock_raise_gate.py
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
n=0; miss=0
for f in "$HERE"/*.cpp; do
  n=$((n+1))
  python3 -I "$G" "$f" >/dev/null 2>&1; rc=$?
  if [ "$rc" -ne 1 ]; then miss=$((miss+1)); echo "MISSED (rc=$rc): $(basename "$f")"; else echo "caught: $(basename "$f")"; fi
done
echo "checked $n cases, gate missed $miss"
[ "$n" -eq 0 ] && exit 2
[ "$miss" -ne 0 ] && exit 1
exit 0
