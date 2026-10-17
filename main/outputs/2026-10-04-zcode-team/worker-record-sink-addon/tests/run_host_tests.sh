#!/bin/zsh
# run_host_tests.sh — worker-record-sink-addon (qdrv1, team 2026-10-04).
# Builds and runs the driver host tests against the REAL vendored sources and
# replays the produced JSONL through worker-rung-evidence-reader/rung_check.py
# (--from-dir fixture mode; RUNG1+RUNG2 expected PASS). stdlib only; no
# console contact. Receipt: receipts/host-test-receipt.txt
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
MY="$(dirname "$HERE")"
SRC="$MY/src"
TEAM="$(dirname "$MY")"
OUT="$MY/tests/build"
RUNG="$TEAM/worker-rung-evidence-reader/rung_check.py"
FIX="$MY/tests/fixtures-qdrv-sim"

CXX="${CXX:-clang++}"
rm -rf "$OUT" "$FIX"
mkdir -p "$OUT" "$FIX"

# The SAME define set the target build uses for the harness TU (the macro's
# only role here: exclude the RC::Unreal default bindings).
HARNESS_DEFS=(-DNATIVE_QUERY_HARNESS_HOST_TEST -DIG_BUILD_ID='"qdrv1-host"' -DIG_RT_GUOBJECT_ARRAY=0x97BA110ull)
DRIVER_DEFS=(-DQH_DRIVER_HOST_TEST -DIG_BUILD_ID='"qdrv1-host"' -DIG_RT_GUOBJECT_ARRAY=0x97BA110ull)
WARN=(-Wall -Wextra -Werror -std=c++17 -O1 -g)

echo "== compile =="
$CXX "${WARN[@]}" "${HARNESS_DEFS[@]}" -c "$SRC/NativeQueryHarness.cpp" -o "$OUT/harness.o"
$CXX "${WARN[@]}" "${DRIVER_DEFS[@]}" -c "$SRC/qh_driver.cpp" -o "$OUT/driver.o"
clang -Wall -Wextra -Werror -O1 -c "$SRC/record_sink.c" -o "$OUT/sink.o"
clang -Wall -Wextra -Werror -O1 -c "$SRC/ingame_libc.c" -o "$OUT/libc.o"
clang++ "${WARN[@]}" "${DRIVER_DEFS[@]}" -I "$SRC" -c "$HERE/driver_host_test.cpp" -o "$OUT/test.o"
$CXX "$OUT/test.o" "$OUT/driver.o" "$OUT/harness.o" "$OUT/sink.o" "$OUT/libc.o" -o "$OUT/driver_host_test"

echo "== refusal modes =="
MODES=(bad_counts:6 bad_gc:7 ambiguous:3 uncovered:1)
mkdir -p "$FIX/refusal-modes"
for m in bad_counts bad_gc ambiguous uncovered; do
  "$OUT/driver_host_test" "$m" "$FIX/refusal-modes/$m.jsonl"
done

echo "== main mode + rung_check replay =="
"$OUT/driver_host_test" main "$FIX/raw.jsonl"
# rung_check fixture companions (placeholders; RUNG3-5 counts only).
printf 'modforge-runtime-status\t1\nsession\t1\t2\nstate\t1\tengine-ready\t0\t\n' > "$FIX/runtime-status.txt"
: > "$FIX/UE4SS.log"
: > "$FIX/townfall_runtime_aligned_entry.log"
: > "$FIX/townfall_runtime_aligned_observer.log"

python3 "$RUNG" --from-dir "$FIX" --rung RUNG1
python3 "$RUNG" --from-dir "$FIX" --rung RUNG2
python3 "$RUNG" --from-dir "$FIX"
echo "ALL HOST TESTS PASS"
