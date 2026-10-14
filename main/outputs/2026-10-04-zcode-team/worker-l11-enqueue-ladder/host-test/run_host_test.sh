#!/bin/zsh
# HOST-TEST RUN — worker-l11-enqueue-ladder. Runs both harness worlds, captures
# stdout receipts, and writes the combined PASS receipt only if BOTH worlds
# exit 0. Every scenario assertion is inside the binaries; this script only
# records evidence.

set -uo pipefail

HT=/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-l11-enqueue-ladder/host-test
BUILD=$HT/build
RECEIPTS=$HT/receipts
mkdir -p "$RECEIPTS"

fail=0

$BUILD/host_test_control > "$RECEIPTS/host-test-control.stdout" 2> "$RECEIPTS/host-test-control.stderr"
rc_c=$?
echo "host_test_control exit=$rc_c (receipt: receipts/host-test-control.stdout)"
[ $rc_c -ne 0 ] && fail=1

$BUILD/host_test_fixed > "$RECEIPTS/host-test-fixed.stdout" 2> "$RECEIPTS/host-test-fixed.stderr"
rc_f=$?
echo "host_test_fixed exit=$rc_f (receipt: receipts/host-test-fixed.stdout)"
[ $rc_f -ne 0 ] && fail=1

if [ $fail -eq 0 ]; then
  {
    echo "HOST INTEGRATION TEST — PASS receipt (worker-l11-enqueue-ladder)"
    echo "date: $(date -u '+%Y-%m-%dT%H:%M:%SZ')"
    echo
    echo "worlds:"
    echo "  control = worker-l11-marshal-integration/staging/luamod-l11-marshal-consumer/LuaMod.cpp (04c9d91664dd82c7, UNPATCHED)"
    echo "  fixed   = worker-l11-enqueue-ladder/staging/luamod-l11-el-ladder/LuaMod.cpp (f50976ae1e2626b0, staged fix)"
    echo
    echo "driven code: REAL staged ExecuteInGameThread binding (default method EngineTick, single-arg call = the main.lua:695 default API),"
    echo "REAL queues + process_simple_actions drain + ensure_engine_tick_hooked/ensure_process_event_hooked throw sites + REAL port TRY,"
    echo "REAL Lua 5.4 (LuaRaw, compiled as C++ per shipped CMakeLists LANGUAGE CXX) and REAL LuaMadeSimple."
    echo "Declared shims (host-only): Unreal::Hook registration refusal knob, UE4SSRuntime address-availability knob, Output capture, FName, luauser lock hooks (single-threaded no-ops)."
    echo
    echo "results:"
    echo "  control world: exit=$rc_c — see receipts/host-test-control.stdout"
    echo "  fixed   world: exit=$rc_f — see receipts/host-test-fixed.stdout"
    echo
    echo "verdict: control reproduces the enqueue loss (throw before enqueue; queues empty; armed marshal bypassed);"
    echo "fixed lineage: armed world = enqueued+drained, W_FAILS world = enqueued+stalled-logged, working world = enqueued+armed+drained,"
    echo "address-unavailable ladder fallback = enqueued+drained (intact)."
    echo
    echo "overall: PASS"
  } > "$HT/PASS-receipt.txt"
  echo "PASS receipt written: PASS-receipt.txt"
else
  echo "HOST TEST FAILED — no PASS receipt written; see receipts/ for the raw outputs"
fi

exit $fail
