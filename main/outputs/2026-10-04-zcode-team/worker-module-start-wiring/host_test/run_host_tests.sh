#!/bin/sh
# run_host_tests.sh — host-test of the module_start wiring logic.
# Compiles the DELIVERED runtime_module_entry.c (staged copy for its relative
# includes) against the REAL lifecycle.c and fake probe/diagnostic/kernel/
# mfa/bridge bindings, then drives five scenarios and asserts the observable
# outputs of every failure path.
set -eu

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$HERE/out"
WS=/WORKSPACE/.codex/worktrees/translation-core/what
REAL_LIFECYCLE="$WS/outputs/ps5-mod-bridge/native-runtime-startup/lifecycle/lifecycle.c"

rm -rf "$OUT"
mkdir -p "$OUT"
CFLAGS="-std=c11 -Wall -Wextra -Werror -O1 -g"
INC="-I$HERE/staging -I$HERE/staging/lifecycle -I$HERE/staging/probe -I$HERE/.."

echo "== compile (host cc $CFLAGS) =="
cc $CFLAGS $INC \
  -c "$HERE/staging/lifecycle/runtime_module_entry.c" -o "$OUT/entry.o"
cc $CFLAGS -I"$WS/outputs/ps5-mod-bridge/native-runtime-startup/lifecycle" \
  -c "$REAL_LIFECYCLE" -o "$OUT/lifecycle.o"
cc $CFLAGS $INC \
  "$HERE/test_wiring.c" "$OUT/entry.o" "$OUT/lifecycle.o" \
  "$HERE/fakes/diagnostic_host.c" \
  "$HERE/fakes/kernel_host.c" \
  "$HERE/fakes/probe_host.c" \
  "$HERE/fakes/mfa_host.c" \
  "$HERE/fakes/bridge_host.c" \
  -o "$OUT/test_wiring"
echo "compile OK"

fail=0
check() { # check <name> <file> <pattern>
  if grep -qF -- "$3" "$2" 2>/dev/null; then
    echo "PASS: $1"
  else
    echo "FAIL: $1 (missing '$3' in $2)"; fail=1
  fi
}
check_absent() {
  if grep -qF -- "$3" "$2" 2>/dev/null; then
    echo "FAIL: $1 (unexpected '$3' in $2)"; fail=1
  else
    echo "PASS: $1"
  fi
}

# --- scenario 1: full success -------------------------------------------------
D="$OUT/s1"; mkdir -p "$D/data/plugins"
MRS_DATA_ROOT="$D/data" MRS_DIAG_PATH="$D/diag.jsonl" MRS_STATUS_PATH="$D/status.txt" \
MRS_BRIDGE_LOG="$D/bridge.log" \
  "$OUT/test_wiring" stop > "$D/stdout.txt"
check "s1 start returns 0"            "$D/stdout.txt" "module_start=0"
check "s1 stop returns 0"             "$D/stdout.txt" "module_stop=0"
check "s1 constructors ran"           "$D/diag.jsonl" "EVENT constructor_begin"
check "s1 wiring began"               "$D/diag.jsonl" "EVENT runtime_wiring_begin"
check "s1 activation consumed"        "$D/diag.jsonl" "EVENT mfa_activated_low"
check "s1 start ok"                   "$D/diag.jsonl" "EVENT runtime_start_ok"
check "s1 bridge got activation"      "$D/bridge.log" "activation=non-null"
check "s1 bridge got title"           "$D/bridge.log" "title=PPSA33286"
check "s1 bridge got mod root"        "$D/bridge.log" "root=/data/modbridge/roots/PPSA33286"
check "s1 stop called bridge"         "$D/bridge.log" "bridge_stop"
check_absent "s1 no failure log"      "$D/data/plugins/UE4SS.log" "modbridge module_start failed"

# --- scenario 2: mfa_activate failure ----------------------------------------
D="$OUT/s2"; mkdir -p "$D/data/plugins"
MRS_FAKE_MFA_FAIL=-2 MRS_DATA_ROOT="$D/data" MRS_DIAG_PATH="$D/diag.jsonl" \
MRS_STATUS_PATH="$D/status.txt" MRS_BRIDGE_LOG="$D/bridge.log" \
  "$OUT/test_wiring" plain > "$D/stdout.txt"
check "s2 returns 0 (game unmodded)"  "$D/stdout.txt" "module_start=0"
check "s2 diag failure event"         "$D/diag.jsonl" "EVENT mfa_activate_failed"
check "s2 diag os error"              "$D/diag.jsonl" "EVENT mfa_activate_os_error"
check "s2 UE4SS.log failure line"     "$D/data/plugins/UE4SS.log" "modbridge module_start failed: mfa_activate_failed code=-2"
check "s2 status failed line"         "$D/status.txt" "state	1	failed	-2	"
check_absent "s2 bridge never called" "$D/bridge.log" "bridge_start"

# --- scenario 3: start failure (conversion refusal) ---------------------------
D="$OUT/s3"; mkdir -p "$D/data/plugins"
MRS_FAKE_START_FAIL=1 MRS_FAKE_START_DETAIL="refusal: no readable mappings" \
MRS_DATA_ROOT="$D/data" MRS_DIAG_PATH="$D/diag.jsonl" MRS_STATUS_PATH="$D/status.txt" \
MRS_BRIDGE_LOG="$D/bridge.log" \
  "$OUT/test_wiring" plain > "$D/stdout.txt"
check "s3 returns 0 (game unmodded)"  "$D/stdout.txt" "module_start=0"
check "s3 diag failure event"         "$D/diag.jsonl" "EVENT runtime_start_failed"
check "s3 UE4SS.log code+detail"      "$D/data/plugins/UE4SS.log" "modbridge module_start failed: runtime_start_failed code=1 detail=refusal: no readable mappings"
check "s3 status has refusal"         "$D/status.txt" "refusal: no readable mappings"
check "s3 activation consumed once"   "$D/bridge.log" "activation_released"
check_absent "s3 dead owner never stopped" "$D/bridge.log" "bridge_stop"

# --- scenario 4: stop failure -------------------------------------------------
D="$OUT/s4"; mkdir -p "$D/data/plugins"
MRS_FAKE_STOP_RESULT=1 MRS_DATA_ROOT="$D/data" MRS_DIAG_PATH="$D/diag.jsonl" \
MRS_STATUS_PATH="$D/status.txt" MRS_BRIDGE_LOG="$D/bridge.log" \
  "$OUT/test_wiring" stopfail > "$D/stdout.txt"
check "s4 module_stop still 0"        "$D/stdout.txt" "module_stop=0"
check "s4 diag stop result"           "$D/diag.jsonl" "EVENT runtime_stop_result"
check "s4 UE4SS.log stop failure"     "$D/data/plugins/UE4SS.log" "modbridge module_start failed: runtime_stop_failed code=1"

# --- scenario 5: double module_start ------------------------------------------
D="$OUT/s5"; mkdir -p "$D/data/plugins"
MRS_DATA_ROOT="$D/data" MRS_DIAG_PATH="$D/diag.jsonl" MRS_STATUS_PATH="$D/status.txt" \
MRS_BRIDGE_LOG="$D/bridge.log" \
  "$OUT/test_wiring" double_start > "$D/stdout.txt"
check "s5 first start 0"              "$D/stdout.txt" "module_start=0"
check "s5 second start 0 (ACTIVE)"    "$D/stdout.txt" "module_start_again=0"
n=$(grep -c "bridge_start" "$D/bridge.log" || true)
if [ "$n" = "1" ]; then echo "PASS: s5 wiring ran exactly once"; else echo "FAIL: s5 bridge_start count=$n"; fail=1; fi

echo
if [ "$fail" = "0" ]; then
  echo "ALL HOST TESTS PASSED"
else
  echo "HOST TESTS FAILED"
  exit 1
fi
