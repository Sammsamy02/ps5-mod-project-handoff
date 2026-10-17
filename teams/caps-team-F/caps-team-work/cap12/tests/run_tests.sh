#!/bin/bash
# cap12 host tests (Mac-only). Python stdlib + Lua 5.4/5.5. No console, no builds.
# Output: tests/test-output.txt. Exit non-zero on any failure or zero tests.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(dirname "$HERE")"
OUT="$HERE/test-output.txt"
export PYTHONDONTWRITEBYTECODE=1
{
  echo "== cap12 host tests $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "== tool sha256"; (cd "$ROOT" && shasum -a 256 tools/*.py PATCH/zzmatdiag/Scripts/main.lua)
  echo "== staged copy == tools copy"
  SYNC=0; NF=0
  for f in "$ROOT"/PATCH/material-fallback/*.py; do
    b=$(basename "$f"); NF=$((NF+1))
    if [ "$b" = test_cap12.py ]; then cmp -s "$f" "$HERE/test_cap12.py" || { echo "DIFFERS: $b"; SYNC=1; }
    else cmp -s "$f" "$ROOT/tools/$b" || { echo "DIFFERS: $b"; SYNC=1; }; fi
  done
  for f in "$ROOT"/tools/*.py; do
    b=$(basename "$f"); [ -f "$ROOT/PATCH/material-fallback/$b" ] || { echo "ONLY IN tools: $b"; SYNC=1; }
  done
  echo "staged files compared: $NF, sync=$SYNC"
  echo "== python unittest"
  python3 -I "$HERE/test_cap12.py" 2>&1; PYRC=$?
  echo "== lua probe host test"
  LUA="${LUA:-lua}"
  luac -p "$ROOT/PATCH/zzmatdiag/Scripts/main.lua" && echo "luac syntax OK"; LCRC=$?
  "$LUA" "$HERE/lua/test_zzmatdiag.lua" "$ROOT/PATCH/zzmatdiag/Scripts/main.lua" 2>&1; LRC=$?
  echo "== exit codes: python=$PYRC luac=$LCRC lua=$LRC"
} > "$OUT" 2>&1
cat "$OUT" | tail -40
N=$(sed -nE "s/^Ran ([0-9]+) tests.*/\1/p" "$OUT" | tail -1); N=${N:-0}
L=$(sed -nE "s/^zzmatdiag host test: checked ([0-9]+) slot lines.*PASS$/\1/p" "$OUT" | tail -1); L=${L:-0}
D=$(grep -c "^    def test_" "$HERE/test_cap12.py")
echo "python tests run: $N (defined: $D); lua probe slot lines checked: $L"
grep -qE "python=0 luac=0 lua=0" "$OUT" || { echo "FAIL: a test failed (see exit codes above)"; exit 1; }
grep -qx OK "$OUT" || { echo "FAIL: python suite did not report OK"; exit 1; }
[ "$N" -gt 0 ] || { echo "FAIL: zero python tests ran"; exit 1; }
[ "$N" -eq "$D" ] || { echo "FAIL: ran $N tests but $D are defined"; exit 1; }
grep -qE "skipped|expected failure" "$OUT" && { echo "FAIL: a test was skipped"; exit 1; }
grep -qE "staged files compared: [1-9][0-9]*, sync=0" "$OUT" || { echo "FAIL: staged PATCH copy and tools copy differ (or none compared)"; exit 1; }
[ "$L" -gt 0 ] || { echo "FAIL: lua test checked nothing"; exit 1; }
echo "ALL PASS ($N python tests, lua probe test $L slot lines)"
