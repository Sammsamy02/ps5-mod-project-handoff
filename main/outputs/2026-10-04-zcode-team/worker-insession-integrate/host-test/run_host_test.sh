#!/bin/sh
# run_host_test.sh — compile + run the insession INTEGRATION host test.
# worker-insession-integrate, 2026-10-05.
set -e

SELF_DIR="$(cd "$(dirname "$0")" && pwd)"
TEAM_DIR="$(cd "$SELF_DIR/.." && pwd)"
WHAT_DIR="$(cd "$TEAM_DIR/../.." && pwd)"             # outputs/
BRIDGE="$TEAM_DIR/staging/runtime_start_bridge_r14_diag_spanpub_insession.cpp"
REFUSAL="$WHAT_DIR/2026-10-04-zcode-team/worker-insession-refusal/staging"
FINAL="$WHAT_DIR/2026-10-04-zcode-controller/launch10-build/final-units"
OUT="$SELF_DIR/build"

mkdir -p "$OUT"

# 1. Extract the VERBATIM integration hook region from the staged bridge
#    (the shipped TU contains the same bytes the test compiles).
sed -n '/INSESSION-INTEGRATION-HOST-TEST-BEGIN/,/INSESSION-INTEGRATION-HOST-TEST-END/p' \
    "$BRIDGE" > "$OUT/hook.inc"
grep -q "bridge_insession_hook" "$OUT/hook.inc"

# 2. Compile the REAL worker-insession-refusal units (grammar core as C,
#    service as C++ with the host-test rename hook).
clang -std=c11 -Wall -Wextra -DINSESSION_CMD2_HOST_TEST -I"$REFUSAL" \
    -c "$REFUSAL/insession_cmd2_core.c" -o "$OUT/core.o"
clang++ -std=c++17 -Wall -Wextra -DINSESSION_CMD2_HOST_TEST -I"$REFUSAL" \
    -c "$REFUSAL/insession_cmd2_service.cpp" -o "$OUT/service.o"

# 3. Compile the integration test (with the verbatim hook region) + link.
clang++ -std=c++17 -Wall -Wextra -I"$OUT" -I"$REFUSAL" -I"$FINAL" \
    -c "$SELF_DIR/test_insess_integration.cpp" -o "$OUT/test.o"
clang++ "$OUT/test.o" "$OUT/service.o" "$OUT/core.o" -o "$OUT/test_insess_integration"

# 4. Run.
"$OUT/test_insess_integration"
