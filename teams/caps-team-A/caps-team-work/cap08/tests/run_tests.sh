#!/bin/bash
# cap08 host tests. Mac-only. No console contact, no builds.
# Prints how many tests ran per module and in total. FAILS (exit 1) when:
#   - any test fails or errors,
#   - any module ran zero tests, or the total is zero,
#   - ANY test was skipped (a missing real file, lua/luac or second source would
#     otherwise leave a PASS with no real-file check behind it).
set -u
cd "$(dirname "$0")"
export PYTHONDONTWRITEBYTECODE=1
MODULES="test_formats test_intake_synthetic test_diag_lua test_config_and_gate test_leon_real"
total=0; failed=0; skipped_total=0; empty=0
for m in $MODULES; do
  out=$(python3 -W ignore::ResourceWarning -m unittest -v "$m" 2>&1); rc=$?
  ran=$(printf '%s\n' "$out" | sed -n 's/^Ran \([0-9]*\) test.*/\1/p' | tail -1); ran=${ran:-0}
  skipped=$(printf '%s\n' "$out" | grep -c "\.\.\. skipped" || true)
  sk2=$(printf '%s\n' "$out" | sed -n 's/.*skipped=\([0-9]*\).*/\1/p' | tail -1); sk2=${sk2:-0}
  [ "$sk2" -gt "$skipped" ] && skipped=$sk2
  printf '%s\n' "$out" | grep -E "^  |\.\.\. (ok|FAIL|ERROR|skipped)"
  echo "== $m: ran=$ran rc=$rc skipped=$skipped"
  total=$((total+ran)); skipped_total=$((skipped_total+skipped))
  [ "$ran" -eq 0 ] && empty=$((empty+1))
  [ $rc -ne 0 ] && failed=$((failed+1))
  [ $rc -ne 0 ] && printf '%s\n' "$out" | tail -40
done
echo "== TOTAL tests ran: $total, failing modules: $failed, skipped: $skipped_total, modules with zero tests: $empty"
if [ "$total" -eq 0 ]; then echo "FAIL: zero tests ran"; exit 1; fi
if [ "$empty" -ne 0 ]; then echo "FAIL: $empty module(s) ran zero tests"; exit 1; fi
if [ "$skipped_total" -ne 0 ]; then echo "FAIL: $skipped_total test(s) skipped (real files, lua/luac or a second source missing)"; exit 1; fi
[ "$failed" -eq 0 ] || { echo "FAIL: $failed module(s) failing"; exit 1; }
echo "PASS"
