#!/bin/bash
# run_host_tests2.sh — host fixtures for the refusal-surviving in-session
# command service (grammar v2). worker-insession-refusal, 2026-10-05.
# Pattern: worker-insession-diag fixtures/run_host_tests.sh (staged sources +
# POSIX shims + the REAL precedent-pinned diagnostic.c + strict python
# post-validation + receipt).
#
# Compiles the STAGED units (staging/insession_cmd2_core.{h,c},
# staging/insession_cmd2_service.{h,cpp}) together with the REAL staged
# diagnostic.c (worker-diagnostic-sink/staging/diagnostic.c, read-only
# reference) and:
#   s0  grammar-v2 table (~55 rows; every v1 row keeps its v1 verdict)
#   s6  idle poll: one open-try, zero effects
#   s2  unwired io_probe / unregistered pool_probe: bounded loud replies,
#       bridge and controls never touched
#   s1  wired dispatch core: io_probe b (rc + tags), pool_probe (registered
#       addr/len, FRESH bridge call #1), log text line, noop ack
#   s7  freshness: second pool_probe command = SECOND bridge call
#   s4  rejects: malformed / unknown cmd / trailing / unknown key / bad op /
#       escape / oversize — loud, consumed, never re-read
#   s5  rename-failure: execute + loud failure + at-least-once repeat
#   s3  THE DIRECTIVE PROOF: real service thread running with the
#       "initializer refused" flag set and NO event loop / pump / engine;
#       dispatch held before settle_startup, diag executes + consumes after
#   s8  real-thread io_probe a + d through the intake file
#   s10 service_stop joins < 500 ms; nothing dispatches after stop
#   s8(python) strict validation of supplier sink / entry channel / UE4SS.log
# Host logic proof ONLY — NO console behavior is claimed by this receipt.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
WHAT="$(cd "$HERE/../../../.." && pwd)"           # outputs/../../.. = workspace root
TEAM="$WHAT/outputs/2026-10-04-zcode-team"
CORE="$HERE/../staging/insession_cmd2_core.c"
CORE_H="$HERE/../staging/insession_cmd2_core.h"
SVC="$HERE/../staging/insession_cmd2_service.cpp"
SVC_H="$HERE/../staging/insession_cmd2_service.h"
DIAG_STAGED="$TEAM/worker-diagnostic-sink/staging/diagnostic.c"
DIAG_FIXTURES="$TEAM/worker-diagnostic-sink/fixtures"
RECEIPT="$HERE/receipts/host-test-pass.txt"
TMP="$(mktemp -d /tmp/insession2-test.XXXXXX)"
if [ "${INSESSION_KEEP:-0}" = "1" ]; then
    trap 'echo "kept tmp: $TMP"' EXIT
else
    trap 'rm -rf "$TMP"' EXIT
fi
FAILS=0
note() { echo "[host-test2] $*"; }
fail() { echo "[host-test2] FAIL: $*"; FAILS=$((FAILS + 1)); }

check_sha() {
    GOT=$(shasum -a 256 "$2" | awk '{print $1}')
    if [ "$GOT" != "$1" ]; then
        fail "source sha drift ($3): $GOT != $1"
    else
        note "sha OK ($3): ${GOT:0:16}..."
    fi
}

check_sha "$(shasum -a 256 "$DIAG_STAGED" | awk '{print $1}')" "$DIAG_STAGED" "diagnostic.c: precedented r-s1 copy"
DIAG_SHA="$(shasum -a 256 "$DIAG_STAGED" | awk '{print $1}')"
if [ "$DIAG_SHA" != "f5e96c41c4a96d6032269aee671d8fe12c695db749cdd75792fc2ca3e2d62818" ]; then
    fail "diagnostic.c is not the precedent-pinned r-s1 copy"
fi

INTAKE="$TMP/intake/in-session-cmd.json"
UE4SSLOG="$TMP/UE4SS.log"
SUPP="$TMP/supplier.jsonl"
ENTRY="$TMP/entry-channel.log"
mkdir -p "$TMP/intake" "$TMP/inc"

# ---- generated include (mirrors the real build's generated/ step; v1 law) ----
cat > "$TMP/inc/probe_config.h" <<EOF
#pragma once
#define PROBE_TITLE "PPSA33286"
#define PROBE_LOG_PATH "$ENTRY"
#define PROBE_MARKER "modbridge-runtime-aligned-entry/v1 build=f6530c7905934e5a8cc201aa9400ebb0"
#define PROBE_INPUT_PATH "$TMP/unused-input.lua"
#define PROBE_LUA_MARKER "modbridge-startup-lua/v1"
#define PROBE_INPUT_EXPECTED_BYTES 88u
#define PROBE_INPUT_MAX_BYTES 65536u
#define PROBE_LUA_INTEGER 136
#define PROBE_DELAY_SECONDS 5
EOF

CFLAGS="-Wall -Wextra -Werror -O1 -I$TMP/inc -I$DIAG_FIXTURES/include -I$HERE/../staging -I$TEAM/worker-diagnostic-sink/staging -I$WHAT/outputs/ps5-mod-bridge/native-runtime-startup"
# -D values: the quotes around each path must be literal characters in the
# argv token (they arrive via variable expansion, which does no quote
# removal), so clang sees the macro defined as a C string literal.
SVCDEFS="-DINSESSION_CMD2_HOST_TEST -DINSESSION2_INTAKE_PATH=\"$INTAKE\" -DINSESSION2_UE4SS_LOG_PATH=\"$UE4SSLOG\""
TDEFS="-DINTAKE_PATH=\"$INTAKE\" -DCONSUMED_PATH=\"$INTAKE.consumed\" -DUE4SSLOG_PATH=\"$UE4SSLOG\" -DSUPPLIER_DIAG_PATH=\"$SUPP\""

note "compiling fixture binary (grammar v2 + service + REAL staged diagnostic.c)"
for objdep in "$CORE" "$DIAG_STAGED" "$HERE/host_shims2.c" "$HERE/test_insession2_host.c"; do
    # shellcheck disable=SC2086
    cc $CFLAGS $TDEFS -c "$objdep" -o "$TMP/$(basename "$objdep").o" || fail "compile $objdep"
done
# shellcheck disable=SC2086
c++ $CFLAGS $SVCDEFS -c "$SVC" -o "$TMP/service.o" || fail "compile service TU"
c++ "$TMP"/insession_cmd2_core.c.o "$TMP"/diagnostic.c.o "$TMP"/host_shims2.c.o \
    "$TMP"/test_insession2_host.c.o "$TMP/service.o" -o "$TMP/test_main" || fail "fixture link"

note "running s0-s10 (grammar + dispatch + refusal-boot service lifecycle)"
if [ -x "$TMP/test_main" ]; then
    "$TMP/test_main" | tee "$TMP/main-output.txt"
    if ! grep -q "^HOST-TEST-PASS$" "$TMP/main-output.txt"; then
        fail "scenarios did not print HOST-TEST-PASS"
    fi
else
    fail "fixture binary missing"
fi

note "strict python validation of supplier sink / entry channel / UE4SS.log"
python3 - "$SUPP" "$ENTRY" "$UE4SSLOG" <<'PYEOF' || fail "python validation"
import json, sys
supp, entry, ue4ss = sys.argv[1], sys.argv[2], sys.argv[3]

def dump():
    for name in (supp, entry, ue4ss):
        print(f"[dump] {name}:")
        try:
            print(open(name).read())
        except OSError as e:
            print(f"  (unreadable: {e})")

try:
    lines = open(supp).read().splitlines()
    assert len(lines) == 3, f"expected 3 supplier rows, got {len(lines)}"
    from collections import Counter
    tags, values, t_ms = [], [], []
    for i, line in enumerate(lines, 1):
        rec = json.loads(line)  # strict: raises on any invalid line
        assert rec["marker"] == "modbridge.supplier-diag/v1", i
        assert rec["schema"] == 1, i
        assert rec["seq"] == i, (i, rec["seq"])
        assert isinstance(rec["pid"], int) and rec["pid"] > 0, i
        assert rec["value"] == int(rec["value_hex"], 16), i
        tags.append(rec["tag"]); values.append(rec["value"]); t_ms.append(rec["t_ms"])
    # scenario order puts the two flaky diags (s5) before probe1 (s3):
    # compare as an execution-ordered multiset, t_ms nondecreasing throughout
    assert Counter(tags) == Counter(["insession_probe1", "insession_flaky", "insession_flaky"]), tags
    assert Counter(values) == Counter([7, 3, 3]), values
    assert all(t_ms[i] <= t_ms[i+1] for i in range(len(t_ms)-1)), "t_ms nondecreasing"
    assert all(t.startswith("insession_") for t in tags), tags
    assert not any("reload" in t for t in tags), "unknown cmd executed"

    el = open(entry).read().splitlines()
    assert len(el) == 3, f"expected 3 entry lines, got {len(el)}"
    assert all(l.startswith("[PPSA33286] modbridge-runtime-aligned-entry/v1") for l in el), el
    assert sum(1 for l in el if "insession_probe1 value=0x00000007" in l) == 1, el
    assert sum(1 for l in el if "insession_flaky value=0x00000003" in l) == 2, el

    ul = open(ue4ss).read().splitlines()
    assert len(ul) == 21, f"expected 21 UE4SS.log lines, got {len(ul)}: {ul}"
    assert all(l.startswith("[t=") and "ms pid=" in l for l in ul), "freshness prefix on every line"

    def n(needle):
        return sum(1 for l in ul if needle in l)

    assert n("insession-cmd: initializer refused (recoverable); in-session service remains callable") == 1
    assert n("insession-cmd: noop ack (consumed)") == 1
    assert n("insession-cmd: io_probe op=a: controls not wired (consumed)") == 1
    assert n("insession-cmd: io_probe op=b rc=42 tags=iob-ok (consumed)") == 1
    assert n("insession-cmd: io_probe op=d rc=0 tags=iod-ok (consumed)") == 1
    assert n("insession-cmd: io_probe op=a rc=0 tags=ioa-ok (consumed)") == 1
    assert n("insession-cmd: pool_probe: target not registered (consumed)") == 1
    assert n("insession-cmd: pool_probe addr=0x0000000123456000 len=0x00020000 class=1 fresh (consumed)") == 2
    assert n("insession-cmd log: hello townfall") == 1
    assert n("INSESSION-CMD REJECTED (") == 6
    assert n("INSESSION-CMD REJECTED oversize (>=512B)") == 1
    assert n("INSESSION-CMD REJECTED (unknown cmd)") == 1
    assert n("INSESSION-CMD CONSUME-RENAME FAILED") == 1
    assert n("insession-cmd: diag tag=probe1 repeat=7 emitted (consumed)") == 1
    assert n("insession-cmd: diag tag=flaky repeat=3 emitted (consumed)") == 2
    # the v1 safety core carries into v2: no unknown command ever executed
    assert not any("reload" in l for l in ul)
    print("[py] 3/3 supplier rows strict-parse (seq 1..3, ordered); entry 3 lines;")
    print(f"     UE4SS.log 21 lines with freshness prefix, full inventory matched")
except AssertionError:
    import traceback
    traceback.print_exc()
    dump()
    raise
PYEOF

if [ "$FAILS" -eq 0 ]; then
    note "ALL HOST TESTS PASS"
else
    note "$FAILS stage failure(s)"
fi

{
    echo "# worker-insession-refusal host fixture receipt"
    echo "utc: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "staged core.c sha256: $(shasum -a 256 "$CORE" | awk '{print $1}')"
    echo "staged core.h sha256: $(shasum -a 256 "$CORE_H" | awk '{print $1}')"
    echo "staged service.cpp sha256: $(shasum -a 256 "$SVC" | awk '{print $1}')"
    echo "staged service.h sha256: $(shasum -a 256 "$SVC_H" | awk '{print $1}')"
    echo "diagnostic.c (referenced read-only) sha256: $DIAG_SHA"
    echo "host compiler: $(cc --version | head -1)"
    echo "result: $([ "$FAILS" -eq 0 ] && echo PASS || echo "FAIL ($FAILS)")"
    echo "scenarios: s0 grammar-v2 table (~55), s6 idle, s2 unwired/not-registered"
    echo "           replies, s1 wired dispatch (io rc+tags, fresh pool probe,"
    echo "           log, noop), s7 fresh-not-cached pool probe, s4 rejects"
    echo "           never-re-read, s5 rename-failure at-least-once,"
    echo "           s3 REFUSED-FLAG end-to-end on the real thread (held before"
    echo "           settle, diag after), s8 real-thread io probes, s10 stop"
    echo "           join bound, python strict file validation"
    echo "scope: host logic proof ONLY; no console behavior claimed"
} > "$RECEIPT"
cat "$RECEIPT"
exit "$FAILS"
