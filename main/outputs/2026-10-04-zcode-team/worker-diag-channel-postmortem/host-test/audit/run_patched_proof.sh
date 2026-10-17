#!/bin/sh
# run_patched_proof.sh — proves the proposed 3-line patch for the delivered
# entry's silent -22 path, host-side. Compiles patched_proof_runtime_module_entry.c
# (delivered entry + exactly the 3-line replacement of runtime_module_entry.c
# lines 303-304) against the SAME audit fakes, then re-runs the -22 scenario:
# the marker line must now appear in UE4SS.log while module_start still
# returns -22 and the bindings-unproven sub-case stays a safe no-op.
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$HERE/patched-out"
rm -rf "$OUT"; mkdir -p "$OUT"

WANT="1c041f8bef1607994ce905591f7e9ed91326465e45081da55e56d6ea2e501b80"
ACTUAL="$(shasum -a 256 "$HERE/staging/lifecycle/runtime_module_entry.c" | cut -d' ' -f1)"
[ "$ACTUAL" = "$WANT" ] || { echo "base hash drift"; exit 1; }

CFLAGS="-std=c11 -Wall -Wextra -Werror -O1 -g"
INC="-I$HERE/staging -I$HERE/staging/lifecycle -I$HERE/staging/probe"
cc $CFLAGS $INC -c "$HERE/patched_proof_runtime_module_entry.c" -o "$OUT/entry.o"
cc $CFLAGS $INC "$HERE/test_audit.c" "$OUT/entry.o" \
  "$HERE/fakes/diagnostic_audit.c" "$HERE/fakes/kernel_audit.c" \
  "$HERE/fakes/probe_lifecycle_audit.c" "$HERE/fakes/mfa_audit.c" \
  "$HERE/fakes/bridge_audit.c" -o "$OUT/test_patched"
echo "compile OK"

fail=0
pass() { echo "PASS: $1"; }
deny() { echo "FAIL: $1"; fail=1; }
expect() { grep -qF -- "$3" "$2" 2>/dev/null && pass "$1" || deny "$1 (missing '$3' in $2)"; }
expect_absent() { grep -qF -- "$3" "$2" 2>/dev/null && deny "$1 (unexpected '$3')" || pass "$1"; }

# p1: -22 (channel open failed, bindings proven) — marker NOW written
D="$OUT/p1"; mkdir -p "$D/data/plugins"
MRS_DIAG_PATH="$D/absent-dir/diag.jsonl" MRS_DATA_ROOT="$D/data" \
MRS_STATUS_PATH="$D/status.txt" MRS_BRIDGE_LOG="$D/bridge.log" \
  "$OUT/test_patched" plain > "$D/stdout.txt"
expect "p1 module_start still returns -22 (return semantics unchanged)" "$D/stdout.txt" "module_start=-22"
expect "p1 marker line present in UE4SS.log with stage token" \
  "$D/data/plugins/UE4SS.log" "modbridge module_start failed: diag_channel_open_failed code=-22"
expect "p1 marker carries detail" \
  "$D/data/plugins/UE4SS.log" "detail=diagnostic channel unavailable before wiring"

# p2: -22 via bindings unproven — write_ue4ss_log_line's own guard keeps it a
# safe no-op (nothing can be proven-ready; structural limit preserved)
D="$OUT/p2"; mkdir -p "$D/data/plugins"
MRS_DIAG_READY=0 MRS_DIAG_PATH="$D/diag.jsonl" MRS_DATA_ROOT="$D/data" \
MRS_STATUS_PATH="$D/status.txt" MRS_BRIDGE_LOG="$D/bridge.log" \
  "$OUT/test_patched" plain > "$D/stdout.txt"
expect "p2 module_start still returns -22"            "$D/stdout.txt" "module_start=-22"
if [ -f "$D/data/plugins/UE4SS.log" ]; then
  expect_absent "p2 no line attempted when bindings unproven (guard preserved)" \
    "$D/data/plugins/UE4SS.log" "modbridge module_start failed"
else
  pass "p2 UE4SS.log never created (guard preserved)"
fi

# p3: success path unaffected
D="$OUT/p3"; mkdir -p "$D/data/plugins"
MRS_DIAG_PATH="$D/diag.jsonl" MRS_DATA_ROOT="$D/data" \
MRS_STATUS_PATH="$D/status.txt" MRS_BRIDGE_LOG="$D/bridge.log" \
  "$OUT/test_patched" plain > "$D/stdout.txt"
expect "p3 success path still 0"                      "$D/stdout.txt" "module_start=0"
expect "p3 success still emits module_start_entered"  "$D/diag.jsonl" "EVENT module_start_entered"
if [ -f "$D/data/plugins/UE4SS.log" ]; then
  expect_absent "p3 success path still writes no marker" "$D/data/plugins/UE4SS.log" "modbridge module_start failed"
else
  pass "p3 UE4SS.log never created on success"
fi

echo
[ "$fail" -eq 0 ] && echo "PATCHED-PROOF-PASS (3-line patch closes the silent -22, semantics preserved)" || { echo "PATCHED-PROOF-FAIL"; exit 1; }
