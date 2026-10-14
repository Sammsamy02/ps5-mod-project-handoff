#!/bin/sh
# run_host_tests.sh — reproducible host pipeline for the axes detector.
#   1. regenerate axes_table.h from upstream engine_version.rs (mechanical)
#   2. build + run host tests (T1 self-test, T2 straddle, T3 degenerate)
#   3. run T4 against the retained Townfall image
#   4. cross-check with the independent Python reference
# Usage: run_host_tests.sh [retained-eboot-path]
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)             # worker-axes-detector/
WHAT=$(cd "$ROOT/../../.." && pwd)       # what/  (outputs/2026-10-04-zcode-team/...)
RS="$WHAT/.superpowers/native-dependency-spike/ue4ss-native-patternsleuth-1/upstream/patternsleuth/src/resolvers/unreal/engine_version.rs"
EBOOT="${1:-$WHAT/.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin}"

echo "== 1. regenerate table =="
python3 "$ROOT/tools/gen_axes_table.py" "$RS"

echo "== 2. build + unit tests =="
cc -O2 -Wall -Wextra -std=c11 -o "$HERE/test_axes_detector" \
   "$HERE/test_axes_detector.c" "$ROOT/src/axes_detector.c"
"$HERE/test_axes_detector"

echo "== 3. real-image measurement =="
"$HERE/test_axes_detector" "$EBOOT"

echo "== 4. independent reference cross-check =="
python3 "$ROOT/tools/ref_axes.py" "$RS" "$EBOOT"
echo "== done (C and Python must agree on axes + present_count + costs) =="
