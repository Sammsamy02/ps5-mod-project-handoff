#!/bin/sh
# Round-6 gate (Mac-only, read-only inputs, no console). Usage: sh tests/run_tests.sh
# Ends with "RUN PASS ..." or "RUN FAIL: <why>" (exit 1). Fails on zero tests, any failure or skip,
# a zero-count gate, stale pins, a diff that no longer reproduces PATCH/tools, or a changed read-only input.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
R=$(cd "$HERE/.." && pwd)
WHAT=/WORKSPACE/.codex/worktrees/translation-core/what
L25=$WHAT/outputs/2026-10-08-l25-prep
MF=$L25/mod-manager/material-fallback
T=$R/work/tools
O=$HERE/output
mkdir -p "$O"
export PYTHONDONTWRITEBYTECODE=1
fail() { echo "RUN FAIL: $1"; exit 1; }

echo "== 0 pins and read-only inputs"
(cd "$R/PATCH" && shasum -a 256 -c SHA256SUMS) > "$O/pins.log" 2>&1 || { cat "$O/pins.log"; fail "PATCH/SHA256SUMS"; }
n=$(grep -c ': OK$' "$O/pins.log"); [ "$n" -gt 0 ] || fail "zero pinned files"
echo "pins: $n files OK"
for f in invisible_route.py mesh_sections.py base_material_route.py mesh_slot_repoint.py attribution.py; do
  cmp -s "$R/PATCH/tools/$f" "$T/$f" || fail "work/tools/$f differs from PATCH/tools/$f"
done
k=0
for f in "$MF"/*.py "$MF"/*.swift "$MF"/*.json; do
  b=$(basename "$f"); cmp -s "$f" "$T/$b" || fail "work/tools/$b differs from the integrated $b"; k=$((k+1))
done
for f in paktx.py overlay_builder.py; do
  cmp -s "$L25/pak-overlay/$f" "$T/$f" || fail "work/tools/$f differs from the integrated $f"
done
for f in texture_tiling_probe.py tile_reference.py ue_formats.py; do
  cmp -s "$L25/pak-intake/$f" "$T/$f" || fail "work/tools/$f differs from the integrated pak-intake/$f"
done
echo "integrated base: $k material-fallback files + paktx.py, overlay_builder.py + 3 pak-intake files equal"
TMPD=$(mktemp -d); cp "$R/PATCH/base/invisible_route.py" "$TMPD/x.py"
patch -s "$TMPD/x.py" "$R/PATCH/invisible_route.py.diff" && cmp -s "$TMPD/x.py" "$R/PATCH/tools/invisible_route.py" \
  || fail "invisible_route.py.diff does not reproduce PATCH/tools/invisible_route.py"
rm -rf "$TMPD"
(shasum -a 256 -c "$HERE/inputs.sha256") > "$O/inputs.log" 2>&1 || { cat "$O/inputs.log"; fail "a read-only input changed"; }
echo "read-only inputs: $(grep -c ': OK$' "$O/inputs.log") unchanged"

echo "== 1 round-6 rules + real meshes"
python3 -I "$HERE/test_round6.py" > "$O/round6.log" 2>&1; rc=$?
tail -1 "$O/round6.log"
r6=$(sed -n 's/^test_round6: \([0-9]*\) tests ran, 0 failed, 0 skipped$/\1/p' "$O/round6.log")
[ $rc -eq 0 ] && [ -n "$r6" ] && [ "$r6" -gt 0 ] || fail "test_round6"

echo "== 2 Team B invisible-route suite (round-6 copy)"
CAP12B_TOOLS=$T CAP12_TOOLS=$T python3 -I "$HERE/test_invisible_route_r6.py" > "$O/invisible_r6.log" 2>&1; rc=$?
tail -1 "$O/invisible_r6.log"
tb=$(sed -n 's/^test_invisible_route: \([0-9]*\) tests ran, 0 failed, 0 skipped$/\1/p' "$O/invisible_r6.log")
[ $rc -eq 0 ] && [ -n "$tb" ] && [ "$tb" -gt 0 ] || fail "test_invisible_route_r6"

echo "== 3 integrated material-fallback suites next to the new files (unchanged behaviour)"
for s in test_cap12 test_fulllook; do
  (cd "$T" && python3 -I "$s.py") > "$O/$s.log" 2>&1; rc=$?
  c=$(sed -n 's/^Ran \([0-9]*\) tests* in .*/\1/p' "$O/$s.log" | tail -1)
  grep -q '^OK' "$O/$s.log" && [ $rc -eq 0 ] && [ -n "$c" ] && [ "$c" -gt 0 ] || fail "$s"
  grep -q 'skipped' "$O/$s.log" && fail "$s has skips"
  echo "$s: $c OK"; eval "n_$s=$c"
done

echo "== 4 real Leon dry run (plan -> manifest -> paktx --dry-run -> attribution -> builder check)"
rm -rf "$O/real"
sh "$HERE/real_leon_r6.sh" "$O/real" > "$O/real.log" 2>&1; rc=$?
tail -3 "$O/real.log"
grep -q '^check_real_r6: [1-9][0-9]* checks, 0 failed$' "$O/real.log" && [ $rc -eq 0 ] || fail "real Leon dry run"
rc6=$(sed -n 's/^check_real_r6: \([0-9]*\) checks, 0 failed$/\1/p' "$O/real.log")
# keep the small outputs, drop the large sidecar build of the dry run
rm -rf "$O/real/leon12r6/targets"
echo "RUN PASS round6=$r6 invisible_r6=$tb mf_cap12=$n_test_cap12 mf_full=$n_test_fulllook real_checks=$rc6"
