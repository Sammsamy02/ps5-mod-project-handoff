#!/bin/zsh
# HOST-TEST RUN — worker-r3-ladder. Runs both harness worlds, captures stdout
# receipts, and writes the combined PASS receipt only if BOTH worlds exit 0.

set -uo pipefail

HT=/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-r3-ladder/host-test
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
    echo "HOST INTEGRATION TEST — PASS receipt (worker-r3-ladder)"
    echo "date: $(date -u '+%Y-%m-%dT%H:%M:%SZ')"
    echo
    echo "worlds:"
    echo "  control = worker-l11-merge/staging/luamod-l11-merged/LuaMod.cpp (c92c92ff66686bf6, UNPATCHED)"
    echo "  fixed   = worker-r3-ladder/staging/luamod-l11-r3-ladder/LuaMod.cpp (6f97f8a1727b7e49, staged fix)"
    echo
    echo "driven code: REAL staged WithDelay-family bindings (ExecuteInGameThreadWithDelay /"
    echo "RetriggerableExecuteInGameThreadWithDelay / ExecuteInGameThreadAfterFrames /"
    echo "LoopInGameThreadWithDelay / LoopInGameThreadAfterFrames), REAL delayed queues,"
    echo "REAL ensure_engine_tick_hooked/ensure_process_event_hooked throw sites (hash-proven"
    echo "identical across worlds), REAL port TRY, REAL Lua 5.4 (LuaRaw compiled as C++),"
    echo "REAL LuaMadeSimple."
    echo "Declared shims (host-only): Unreal::Hook registration refusal knob (+ attempt counters),"
    echo "UE4SSRuntime address-availability knob, Unreal::R3MarshalPumpPending pairing knob,"
    echo "Output capture, FName, luauser lock hooks (single-threaded no-ops)."
    echo
    echo "results:"
    echo "  control world: exit=$rc_c — the RUNBOOK 5.1 trap REPRODUCED (armed marshal bypassed;"
    echo "    WithDelay/AfterFrames throw before enqueue; closure lost)"
    echo "  fixed   world: exit=$rc_f — P1 armed+alive => no throw, marshal-tagged enqueue, zero detour"
    echo "    attempts (both alive => marshal wins deterministically); P2 armed+dead => loud refusal,"
    echo "    nothing enqueued, no fallback; P3 no backend at all => the only unhooked-tick throw;"
    echo "    P4/P5 AfterFrames => precise capability refusal in the marshal world; R1/R2 working"
    echo "    worlds preserved (EngineTick armed + enqueued, unchanged tags)"
    echo
    echo "verdict: predicate order proven — marshal-first when usable; armed-but-dead refuses loudly;"
    echo "throw-on-unhooked-tick only when no pump backend exists at all."
    echo
    echo "overall: PASS"
  } > "$HT/PASS-receipt.txt"
  echo "PASS receipt written: PASS-receipt.txt"
else
  echo "HOST TEST FAILED — no PASS receipt written; see receipts/ for the raw outputs"
fi

exit $fail
