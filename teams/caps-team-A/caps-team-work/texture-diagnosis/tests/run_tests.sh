#!/bin/bash
# Host tests for texture-diagnosis (Mac-only). Writes only under tests/tmp and tests/out.
# Fails if any test fails or is skipped, if any suite ran zero tests, if any recorded
# item count is zero, or if a read-only input changed. Prints every count it checked.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
cd "$HERE"
rm -rf tmp out; mkdir -p tmp out
W="${WHAT_ROOT:-/WORKSPACE/.codex/worktrees/translation-core/what}"
export WHAT_ROOT="$W" PYTHONDONTWRITEBYTECODE=1
INPUTS=(
 "$W/.superpowers/native-observations/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P.utoc"
 "$W/.superpowers/native-observations/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P.ucas"
 "$W/.superpowers/native-dependency-spike/leon-iostore-adapter-4k-trial-1/source/zzz_Leon_Townfall_4K_P.utoc"
 "$W/.superpowers/native-dependency-spike/leon-iostore-adapter-4k-trial-1/source/zzz_Leon_Townfall_4K_P.ucas"
 "$W/.superpowers/native-observations/townfall-original-container-header-1/container-header.bin"
 "$W/.superpowers/native-observations/townfall-main-utoc-2026-10-01/pakchunk0-ps5.utoc"
 "$W/.superpowers/native-observations/townfall-global-scriptobjects-1/global.ucas"
 "$W/outputs/2026-10-08-l25-prep/pak-overlay/overlay_builder.py"
 "$W/outputs/2026-10-08-l25-prep/pak-overlay/test_overlay_builder.py"
 "$W/outputs/2026-10-08-l25-prep/pak-overlay/leon-substitutions-fixture.json"
 "$W/outputs/2026-10-08-l25-prep/pak-overlay/leon-replay-drift.json"
)
shasum -a 256 "${INPUTS[@]}" > out/inputs-before.sha256 || { echo "FAIL: a read-only input is missing"; echo "RESULT: FAIL"; exit 1; }
FAIL=0
TOTAL=0

# check_suite <label> <log>: needs "Ran N tests" with N > 0, "OK", and no skips.
check_suite() {
  local label="$1" log="$2" n
  n=$(sed -n 's/^Ran \([0-9][0-9]*\) tests\{0,1\} in .*/\1/p' "$log" | tail -1)
  n=${n:-0}
  TOTAL=$((TOTAL + n))
  if [ "$n" -eq 0 ]; then echo "FAIL: $label ran zero tests"; FAIL=1; return; fi
  if ! grep -q '^OK$' "$log" || grep -q '^FAILED' "$log"; then echo "FAIL: $label did not report OK"; FAIL=1; fi
  if grep -Eq 'skipped=|\.\.\. skipped' "$log"; then echo "FAIL: $label skipped tests (a skipped test checks nothing)"; FAIL=1; fi
  echo "  $label: $n tests"
}

for t in test_hashes test_iostore_lib test_texture_diag test_zen_import_rewrite test_base_fetch_plan test_material_plan; do
  echo "=== $t"
  python3 -W ignore::ResourceWarning -m unittest -v "$t" > "out/$t.txt" 2>&1
  RC=$?
  cat "out/$t.txt"
  [ $RC -eq 0 ] || { echo "FAIL: $t exit $RC"; FAIL=1; }
  check_suite "$t" "out/$t.txt"
done

echo "=== patched overlay_builder suite (relocated copy of PATCH/new under tests/tmp; outputs under tests/out/overlay)"
RUN=tmp/run-new
mkdir -p "$RUN" out/overlay
cp ../PATCH/new/overlay_builder.py ../PATCH/new/cityhash64.py "$RUN/"
# The suite finds its inputs relative to its own folder; point those two lines at the worktree.
python3 - "$W" ../PATCH/new/test_overlay_builder.py "$RUN/test_overlay_builder.py" <<'PY' || FAIL=1
import sys
w, src, dst = sys.argv[1:]
s = open(src).read()
subs = [('WHAT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))', 'WHAT = %r' % w),
        ('HERE, "..", "shader-fallback", "leon-classification.json"))',
         '%r, "leon-classification.json"))' % (w + "/outputs/2026-10-08-l25-prep/shader-fallback"))]
for old, new in subs:
    if s.count(old) != 1:
        print("FAIL: relocation line not found exactly once:", old); sys.exit(1)
    s = s.replace(old, new)
open(dst, "w").write(s)
print("  relocated copy written: %d substitutions" % len(subs))
PY
( cd "$RUN" && OVERLAY_TEST_OUT="$HERE/out/overlay" python3 test_overlay_builder.py > "$HERE/out/overlay-suite.txt" 2>&1 ); RC=$?
tail -5 out/overlay-suite.txt
[ $RC -eq 0 ] || { echo "FAIL: overlay suite exit $RC"; FAIL=1; }
check_suite "overlay suite" out/overlay-suite.txt

shasum -a 256 "${INPUTS[@]}" > out/inputs-after.sha256
if ! cmp -s out/inputs-before.sha256 out/inputs-after.sha256; then echo "FAIL: a read-only input changed"; FAIL=1; else echo "read-only inputs unchanged: ${#INPUTS[@]} files"; fi

echo "=== item counts (each must be > 0)"
python3 - <<'PY' || FAIL=1
import json, os, sys
if not os.path.exists("out/counts.json"):
    print("FAIL: no item counts were recorded"); sys.exit(1)
d = json.load(open("out/counts.json"))
for k, v in sorted(d.items()): print(f"  {k}: {v}")
z = [k for k, v in d.items() if v <= 0]
print(f"  categories: {len(d)}, total items: {sum(d.values())}")
if len(d) == 0 or z:
    print("FAIL: zero-count categories:", z); sys.exit(1)
PY
echo "tests run in total: $TOTAL"
[ "$TOTAL" -gt 0 ] || { echo "FAIL: zero tests ran"; FAIL=1; }
rm -rf tmp  # large scratch (retoc conversions) is not kept
if [ $FAIL -ne 0 ]; then echo "RESULT: FAIL"; exit 1; fi
echo "RESULT: PASS"
