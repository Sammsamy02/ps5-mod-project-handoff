#!/bin/sh
# run_audit_test.sh — AUDIT of the delivered wired entry
# (worker-module-start-wiring/runtime_module_entry.c, staged byte-exact,
# sha256 1c041f8bef1607994ce905591f7e9ed91326465e45081da55e56d6ea2e501b80).
#
# Question under audit: do write_ue4ss_log_line + write_status_failed cover the
# early probe-lifecycle returns (-20 entry-guard busy, -21 wrong state, -22
# diagnostic channel open failure), or only post-lifecycle wiring failures?
#
# Assertions are written as the AUDIT FINDINGS, i.e. the test PASSES when the
# entry behaves as delivered:
#   a1/a2/a3/a4: -20/-21/-22 leave ZERO marker lines anywhere  (the gap)
#   a5/a6/a7:    post-lifecycle failures DO write UE4SS.log + mfa 'failed'
# plus positive controls proving each scenario really reached the branch it
# claims (channel events, bridge calls, constructor count).
#
# HOST-ONLY: no console, no network. The delivered source file is never
# modified; the staged copy is hash-checked before every run.
set -eu

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$HERE/out"
WIRE="$HERE/../../../worker-module-start-wiring"

rm -rf "$OUT"
mkdir -p "$OUT"

echo "== staged-copy hash check =="
ACTUAL="$(shasum -a 256 "$WIRE/runtime_module_entry.c" | cut -d' ' -f1)"
WANT="1c041f8bef1607994ce905591f7e9ed91326465e45081da55e56d6ea2e501b80"
if [ "$ACTUAL" != "$WANT" ]; then
  echo "FAIL delivered entry sha256 $ACTUAL != pinned $WANT — refusing to audit a moving target"
  exit 1
fi
cp "$WIRE/runtime_module_entry.c" "$HERE/staging/lifecycle/runtime_module_entry.c"
echo "staged copy OK ($WANT)"

echo "== compile (host cc, arm64 native) =="
CFLAGS="-std=c11 -Wall -Wextra -Werror -O1 -g"
INC="-I$HERE/staging -I$HERE/staging/lifecycle -I$HERE/staging/probe"
cc $CFLAGS $INC -c "$HERE/staging/lifecycle/runtime_module_entry.c" -o "$OUT/entry.o"
cc $CFLAGS $INC "$HERE/test_audit.c" "$OUT/entry.o" \
  "$HERE/fakes/diagnostic_audit.c" "$HERE/fakes/kernel_audit.c" \
  "$HERE/fakes/probe_lifecycle_audit.c" "$HERE/fakes/mfa_audit.c" \
  "$HERE/fakes/bridge_audit.c" -o "$OUT/test_audit"
echo "compile OK"

fail=0
pass() { echo "PASS: $1"; }
deny() { echo "FAIL: $1"; fail=1; }

# expect <description> <file> <pattern>          (pattern must be present)
expect() {
  if grep -qF -- "$3" "$2" 2>/dev/null; then pass "$1"; else deny "$1 (missing '$3' in $2)"; fi
}
# expect_absent <description> <file> <pattern>   (pattern must NOT be present)
expect_absent() {
  if grep -qF -- "$3" "$2" 2>/dev/null; then deny "$1 (unexpected '$3' in $2)"; else pass "$1"; fi
}

no_marker_written() { # $1 scenario dir — audit finding: entry wrote NO failure marker
  if [ -f "$1/data/plugins/UE4SS.log" ]; then
    expect_absent "$1: zero 'modbridge module_start failed' lines in UE4SS.log (marker NOT called on this path)" \
      "$1/data/plugins/UE4SS.log" "modbridge module_start failed"
  else
    pass "$1: UE4SS.log never created (marker NOT called on this path)"
  fi
  if [ -f "$1/status.txt" ]; then
    expect_absent "$1: zero mfa status 'failed' lines (status sink NOT called on this path)" \
      "$1/status.txt" "failed"
  else
    pass "$1: runtime-status.txt never created (status sink NOT called on this path)"
  fi
}

# --- a1: -22 via diagnostic_open_channel() < 0, bindings PROVEN ---------------
# The discriminating scenario: write_ue4ss_log_line's own diagnostic_ready()
# guard is satisfied here, so if the entry called it on the -22 path the line
# WOULD appear. Zero lines proves the -22 return is silent by omission.
D="$OUT/a1"; mkdir -p "$D/data/plugins"
MRS_DIAG_PATH="$D/absent-dir/diag.jsonl" MRS_DATA_ROOT="$D/data" \
MRS_STATUS_PATH="$D/status.txt" MRS_BRIDGE_LOG="$D/bridge.log" \
  "$OUT/test_audit" plain > "$D/stdout.txt"
expect "a1 module_start returned -22"                 "$D/stdout.txt" "module_start=-22"
no_marker_written "$D"

# --- a2: -22 via !diagnostic_ready() (bindings unproven) ----------------------
D="$OUT/a2"; mkdir -p "$D/data/plugins"
MRS_DIAG_READY=0 MRS_DIAG_PATH="$D/diag.jsonl" MRS_DATA_ROOT="$D/data" \
MRS_STATUS_PATH="$D/status.txt" MRS_BRIDGE_LOG="$D/bridge.log" \
  "$OUT/test_audit" plain > "$D/stdout.txt"
expect "a2 module_start returned -22"                 "$D/stdout.txt" "module_start=-22"
no_marker_written "$D"
expect "a2 no diagnostic events possible (channel never opened)" "$D/stdout.txt" "module_start=-22"

# --- a3: -21 wrong state (failed start, then re-entry) ------------------------
D="$OUT/a3"; mkdir -p "$D/data/plugins"
MRS_FAKE_START_RC=-1 MRS_FAKE_FINAL_STATE=3 MRS_DIAG_PATH="$D/diag.jsonl" \
MRS_DATA_ROOT="$D/data" MRS_STATUS_PATH="$D/status.txt" MRS_BRIDGE_LOG="$D/bridge.log" \
  "$OUT/test_audit" reentry > "$D/stdout.txt"
expect "a3 first module_start returns the failed start rc (-1)"  "$D/stdout.txt" "module_start=-1"
expect "a3 re-entry returns -21"                                 "$D/stdout.txt" "module_start_again=-21"
expect "a3 positive control: diag channel WAS open on the -1 path (runtime_start_skipped)" \
  "$D/diag.jsonl" "EVENT runtime_start_skipped 0xffffffff"
no_marker_written "$D"

# --- a4: -20 entry-guard busy (recursive re-entry) ----------------------------
D="$OUT/a4"; mkdir -p "$D/data/plugins"
MRS_RECURSIVE_REENTRY=1 MRS_DIAG_PATH="$D/diag.jsonl" MRS_DATA_ROOT="$D/data" \
MRS_STATUS_PATH="$D/status.txt" MRS_BRIDGE_LOG="$D/bridge.log" \
  "$OUT/test_audit" plain > "$D/stdout.txt"
expect "a4 inner re-entry rejected with -20"          "$D/stdout.txt" "inner_module_start=-20"
expect "a4 outer module_start still completed (0)"    "$D/stdout.txt" "module_start=0"
no_marker_written "$D"

# --- a5: POSITIVE CONTROL — post-lifecycle failure DOES mark ------------------
D="$OUT/a5"; mkdir -p "$D/data/plugins"
MRS_FAKE_MFA_FAIL=-2 MRS_DIAG_PATH="$D/diag.jsonl" MRS_DATA_ROOT="$D/data" \
MRS_STATUS_PATH="$D/status.txt" MRS_BRIDGE_LOG="$D/bridge.log" \
  "$OUT/test_audit" plain > "$D/stdout.txt"
expect "a5 module_start returns 0 (game continues UNMODDED — wired semantics)" "$D/stdout.txt" "module_start=0"
expect "a5 diag failure event"                        "$D/diag.jsonl" "EVENT mfa_activate_failed 0xfffffffe"
expect "a5 UE4SS.log marker WITH stage+code"          "$D/data/plugins/UE4SS.log" \
  "modbridge module_start failed: mfa_activate_failed code=-2"
expect "a5 UE4SS.log marker carries detail"           "$D/data/plugins/UE4SS.log" \
  "detail=MFA_E_NO_LOADLIST fake activation refusal loadlist.txt"
expect "a5 mfa status 'failed' line"                  "$D/status.txt" "failed	-2	"
expect_absent "a5 bridge never called after activation refusal" "$D/bridge.log" "bridge_start"

# --- a6: POSITIVE CONTROL — clean start stays silent and starts the runtime ---
D="$OUT/a6"; mkdir -p "$D/data/plugins"
MRS_DIAG_PATH="$D/diag.jsonl" MRS_DATA_ROOT="$D/data" \
MRS_STATUS_PATH="$D/status.txt" MRS_BRIDGE_LOG="$D/bridge.log" \
  "$OUT/test_audit" stop > "$D/stdout.txt"
expect "a6 module_start returned 0"                   "$D/stdout.txt" "module_start=0"
expect "a6 module_stop returned 0"                    "$D/stdout.txt" "module_stop=0"
expect "a6 diag module_start_entered"                 "$D/diag.jsonl" "EVENT module_start_entered"
expect "a6 wiring ran"                                "$D/diag.jsonl" "EVENT runtime_start_ok"
expect "a6 bridge received the activation"            "$D/bridge.log" "activation=non-null"
expect "a6 stop reached the bridge"                   "$D/bridge.log" "bridge_stop"
if [ -f "$D/data/plugins/UE4SS.log" ]; then
  expect_absent "a6 zero failure lines on the success path" "$D/data/plugins/UE4SS.log" "modbridge module_start failed"
else
  pass "a6 UE4SS.log never created on the success path"
fi

# --- a7: UE4SS.log append refused — the mfa status sink still fires -----------
D="$OUT/a7"; mkdir -p "$D/data/plugins"
MRS_FAKE_MFA_FAIL=-2 MRS_UE4SS_REFUSE=1 MRS_DIAG_PATH="$D/diag.jsonl" \
MRS_DATA_ROOT="$D/data" MRS_STATUS_PATH="$D/status.txt" MRS_BRIDGE_LOG="$D/bridge.log" \
  "$OUT/test_audit" plain > "$D/stdout.txt"
expect "a7 module_start still 0 (no crash, unmodded)" "$D/stdout.txt" "module_start=0"
if [ -f "$D/data/plugins/UE4SS.log" ]; then
  expect_absent "a7 UE4SS.log line refused (graceful no-op)" "$D/data/plugins/UE4SS.log" "modbridge module_start failed"
else
  pass "a7 UE4SS.log never created (open refused, graceful)"
fi
expect "a7 independent sink: mfa status 'failed' still written" "$D/status.txt" "failed	-2	"
expect "a7 diag channel still carries the failure"    "$D/diag.jsonl" "EVENT mfa_activate_failed 0xfffffffe"

echo
if [ "$fail" -eq 0 ]; then
  echo "AUDIT-TEST-PASS (delivered entry behaves as audited: -20/-21/-22 silent, post-lifecycle failures marked)"
else
  echo "AUDIT-TEST-FAIL"
  exit 1
fi
