#!/bin/sh
# Reproducible host build+run for worker-query-s2-arm. Writes ONLY inside the
# worker output dir. Sources (read-only): canonical runtime_owner.cpp
# (bundle-01 == composition-3 staged bytes), vendored modforge_activation.c,
# worker-query-harness src, this worker's staged headers + supplier unit.
set -e
WHAT=/WORKSPACE/.codex/worktrees/translation-core/what
TEAM=$WHAT/outputs/2026-10-04-zcode-team
ME=$TEAM/worker-query-s2-arm
BIN=$ME/host-tests/host_test_bin
mkdir -p "$BIN"

clang -c -O1 -g -I "$TEAM/worker-7-installer/vendored" -o "$BIN/modforge_activation.o" \
  "$TEAM/worker-installer-ps5io/vendored/modforge_activation.c" 2> "$BIN/build-c.stderr"

clang++ -std=c++20 -O1 -g -DNATIVE_QUERY_HARNESS_HOST_TEST -DNATIVE_ACK_SUPPLIER_HOST_TEST \
  -I "$ME/staging/include" \
  -I "$ME/integration-patch/new-units" \
  -o "$BIN/s2_arm_host_test" \
  "$ME/host-tests/test_s2_arm_and_ack_supplier.cpp" \
  "$ME/integration-patch/new-units/runtime_ack_supplier.cpp" \
  "$TEAM/worker-query-harness/src/NativeQueryHarness.cpp" \
  "$WHAT/outputs/ps5-mod-bridge/native-runtime-startup/bootstrap/runtime_owner.cpp" \
  "$BIN/modforge_activation.o" \
  2> "$BIN/build.stderr"

status=0
"$BIN/s2_arm_host_test" 2> "$BIN/run.stderr" || status=$?
cat "$BIN/run.stderr"
echo "exit=$status"
exit $status
