#!/bin/bash
# run_host_tests.sh — host fixtures for the in-session command consumer (v1).
# worker-insession-diag, 2026-10-05. Pattern: worker-diagnostic-sink
# fixtures/run_host_tests.sh (staged sources + POSIX shims + strict python
# post-validation + receipt).
#
# Compiles the STAGED units (staging/insession_cmd_core.{h,c},
# staging/insession_cmd_poll.cpp) together with the REAL staged diagnostic.c
# (worker-diagnostic-sink/staging/diagnostic.c, referenced read-only) and:
#   s0  grammar-v1 table (36 cases)
#   s1  idle polls: open-tries only, zero side effects
#   s2  diag end-to-end: supplier sink row + entry log line + consumed
#       byte-identical + exactly one rename
#   s3  noop: ack line only
#   s4  malformed: loud reject, consumed, never re-read
#   s5  unknown cmd "reload": REJECTED, never executed
#   s6  oversize: rejected without parsing, consumed
#   s7  rename failure: execute-then-rename at-least-once, then recovery
#   throttle  250 ticks @ POLL_EVERY=100 => exactly 2 opens
#   s8  strict python validation of every produced file
# Host logic proof ONLY — NO console behavior is claimed by this receipt.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
WHAT="$(cd "$HERE/../../../.." && pwd)"           # outputs/../../.. = workspace root
TEAM="$WHAT/outputs/2026-10-04-zcode-team"
CORE="$HERE/../staging/insession_cmd_core.c"
CORE_H="$HERE/../staging/insession_cmd_core.h"
POLL="$HERE/../staging/insession_cmd_poll.cpp"
POLL_H="$HERE/../staging/insession_cmd_poll.h"
DIAG_STAGED="$TEAM/worker-diagnostic-sink/staging/diagnostic.c"
DIAG_FIXTURES="$TEAM/worker-diagnostic-sink/fixtures"
RECEIPT="$HERE/receipts/host-test-pass.txt"
TMP="$(mktemp -d /tmp/insession-cmd-test.XXXXXX)"
if [ "${INSESSION_KEEP:-0}" = "1" ]; then
    trap 'echo "kept tmp: $TMP"' EXIT
else
    trap 'rm -rf "$TMP"' EXIT
fi
FAILS=0
note() { echo "[host-test] $*"; }
fail() { echo "[host-test] FAIL: $*"; FAILS=$((FAILS + 1)); }

check_sha() {
    GOT=$(shasum -a 256 "$2" | awk '{print $1}')
    if [ "$GOT" != "$1" ]; then
        fail "source sha drift ($3): $GOT != $1"
    else
        note "sha OK ($3): ${GOT:0:16}..."
    fi
}

# shas asserted so the receipt pins exactly the sources that were tested
check_sha "$(shasum -a 256 "$CORE" | awk '{print $1}')" "$CORE" "core.c:recorded-at-stage-time-below"
check_sha "$(shasum -a 256 "$DIAG_STAGED" | awk '{print $1}')" "$DIAG_STAGED" "diagnostic.c: precedented r-s1 copy"
DIAG_SHA="$(shasum -a 256 "$DIAG_STAGED" | awk '{print $1}')"
if [ "$DIAG_SHA" != "f5e96c41c4a96d6032269aee671d8fe12c695db749cdd75792fc2ca3e2d62818" ]; then
    fail "diagnostic.c is not the precedent-pinned r-s1 copy"
fi

INTAKE="$TMP/intake/in-session-cmd.json"
SUPP="$TMP/supplier.jsonl"
ENTRY="$TMP/entry-channel.log"
TESTLOG="$TMP/testlog.txt"
mkdir -p "$TMP/intake" "$TMP/inc/generated"

# ---- generated include (mirrors the real build's generated/ step; precedent) ----
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
POLLDEFS="-DINSESSION_CMD_HOST_TEST -DINSESSION_INTAKE_PATH=\"$INTAKE\" -DINSESSION_CMD_POLL_EVERY=1"
TDEFS="-DINTAKE_PATH=\"$INTAKE\" -DCONSUMED_PATH=\"$INTAKE.consumed\" -DINTAKE_TESTLOG=\"$TESTLOG\" -DSUPPLIER_DIAG_PATH=\"$SUPP\""

note "compiling main fixture binary (grammar + consumer + REAL staged diagnostic.c)"
# Per-TU compile (C files via cc, the poll TU via c++), then one link.
for objdep in "$CORE" "$DIAG_STAGED" "$HERE/host_shims.c" "$HERE/test_insession_host.c"; do
    # shellcheck disable=SC2086
    cc $CFLAGS $TDEFS -c "$objdep" -o "$TMP/$(basename "$objdep").o" || fail "compile $objdep"
done
# shellcheck disable=SC2086
c++ $CFLAGS $POLLDEFS -c "$POLL" -o "$TMP/poll.o" || fail "compile poll TU"
c++ "$TMP"/insession_cmd_core.c.o "$TMP"/diagnostic.c.o "$TMP"/host_shims.c.o \
    "$TMP"/test_insession_host.c.o "$TMP/poll.o" -o "$TMP/test_main" || fail "main fixture link"

note "compiling throttle fixture binary (POLL_EVERY=100)"
THDEFS="-DINSESSION_CMD_HOST_TEST -DINSESSION_INTAKE_PATH=\"$INTAKE\" -DINSESSION_CMD_POLL_EVERY=100"
# shellcheck disable=SC2086
cc $CFLAGS $TDEFS $THDEFS -c "$CORE" -o "$TMP/th_core.o" || fail "compile throttle core"
# shellcheck disable=SC2086
cc $CFLAGS $TDEFS $THDEFS -c "$DIAG_STAGED" -o "$TMP/th_diag.o" || fail "compile throttle diag"
# shellcheck disable=SC2086
cc $CFLAGS $TDEFS $THDEFS -c "$HERE/host_shims.c" -o "$TMP/th_shims.o" || fail "compile throttle shims"
# shellcheck disable=SC2086
cc $CFLAGS $TDEFS $THDEFS -c "$HERE/throttle_host.c" -o "$TMP/th_main.o" || fail "compile throttle main"
# shellcheck disable=SC2086
c++ $CFLAGS $THDEFS -c "$POLL" -o "$TMP/th_poll.o" || fail "compile throttle poll"
c++ "$TMP/th_core.o" "$TMP/th_diag.o" "$TMP/th_shims.o" "$TMP/th_main.o" \
    "$TMP/th_poll.o" -o "$TMP/test_throttle" || fail "throttle link"

note "running s0-s7 (grammar + consumer scenarios)"
if [ ! -x "$TMP/test_main" ]; then
    fail "main fixture binary missing"
else
    "$TMP/test_main" | tee "$TMP/main-output.txt"
    if ! grep -q "^HOST-TEST-PASS$" "$TMP/main-output.txt"; then
        fail "main scenarios did not print HOST-TEST-PASS"
    fi
fi

note "running throttle scenario"
"$TMP/test_throttle" > "$TMP/throttle-output.txt" 2>&1
if ! grep -q "^THROTTLE-TEST-PASS$" "$TMP/throttle-output.txt"; then
    fail "throttle scenario did not pass"
    cat "$TMP/throttle-output.txt"
else
    grep "throttle:" "$TMP/throttle-output.txt"
fi

note "strict python validation of sink / entry / test logs (s8)"
python3 - "$SUPP" "$ENTRY" "$TESTLOG" <<'PYEOF' || fail "python validation"
import json, sys
supp, entry, testlog = sys.argv[1], sys.argv[2], sys.argv[3]

def dump():
    for name in (supp, entry, testlog):
        print(f"[json-dump] {name}:")
        try:
            print(open(name).read())
        except OSError as e:
            print(f"  (unreadable: {e})")

try:
    lines = open(supp).read().splitlines()
    assert len(lines) == 3, f"expected 3 supplier rows, got {len(lines)}"
    tags, values, t_ms = [], [], []
    for i, line in enumerate(lines, 1):
        rec = json.loads(line)  # strict: raises on any invalid line
        assert rec["marker"] == "modbridge.supplier-diag/v1", i
        assert rec["schema"] == 1, i
        assert rec["seq"] == i, (i, rec["seq"])
        assert isinstance(rec["pid"], int) and rec["pid"] > 0, i
        assert rec["value"] == int(rec["value_hex"], 16), i
        tags.append(rec["tag"]); values.append(rec["value"]); t_ms.append(rec["t_ms"])
    assert tags == ["insession_probe1", "insession_flaky", "insession_flaky"], tags
    assert values == [7, 3, 3], values
    assert all(t_ms[i] <= t_ms[i+1] for i in range(len(t_ms)-1)), "t_ms nondecreasing"
    assert all(t.startswith("insession_") for t in tags), tags
    # the unknown-cmd safety core: no 'reload' ever reached the sink
    assert not any("reload" in t for t in tags), "unknown cmd executed"

    el = open(entry).read().splitlines()
    assert len(el) == 3, f"expected 3 entry lines, got {len(el)}"
    assert all(l.startswith("[PPSA33286] modbridge-runtime-aligned-entry/v1") for l in el), el
    assert "insession_probe1 value=0x00000007" in el[0], el[0]
    assert "insession_flaky value=0x00000003" in el[1], el[1]

    tl = open(testlog).read().splitlines()
    assert sum(1 for l in tl if "insession-cmd: diag tag=probe1 repeat=7 emitted (consumed)" in l) == 1
    assert sum(1 for l in tl if "insession-cmd: noop ack (consumed)" in l) == 1
    assert sum(1 for l in tl if "INSESSION-CMD REJECTED (malformed); consumed, not executed" in l) == 1
    assert sum(1 for l in tl if "INSESSION-CMD REJECTED (unknown cmd); consumed, not executed" in l) == 1
    assert sum(1 for l in tl if "INSESSION-CMD REJECTED oversize (>=512B); consumed, not executed" in l) == 1
    assert sum(1 for l in tl if "INSESSION-CMD CONSUME-RENAME FAILED" in l) == 1
    assert sum(1 for l in tl if "insession-cmd: diag tag=flaky repeat=3 emitted (consumed)" in l) == 2
    assert sum(1 for l in tl if "insession-cmd" in l or "INSESSION-CMD" in l) == 8, tl
    print(f"[json] 3/3 supplier rows strict-parse (seq 1..3, tags/values ordered); "
          f"entry 3 lines; test log 8 lines match the expected inventory")
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
    echo "# worker-insession-diag host fixture receipt"
    echo "utc: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "staged core.c sha256: $(shasum -a 256 "$CORE" | awk '{print $1}')"
    echo "staged poll.cpp sha256: $(shasum -a 256 "$POLL" | awk '{print $1}')"
    echo "staged core.h sha256: $(shasum -a 256 "$CORE_H" | awk '{print $1}')"
    echo "staged poll.h sha256: $(shasum -a 256 "$POLL_H" | awk '{print $1}')"
    echo "diagnostic.c (referenced read-only) sha256: $DIAG_SHA"
    echo "host compiler: $(cc --version | head -1)"
    echo "result: $([ "$FAILS" -eq 0 ] && echo PASS || echo "FAIL ($FAILS)")"
    echo "scenarios: s0 grammar table (37), s1 idle, s2 diag e2e, s3 noop,"
    echo "           s4 malformed never-re-read, s5 unknown-cmd never-executed,"
    echo "           s6 oversize, s7 rename-failure at-least-once, throttle"
    echo "           (250 ticks / 2 opens), s8 strict python file validation"
    echo "scope: host logic proof ONLY; no console behavior claimed"
} > "$RECEIPT"
cat "$RECEIPT"
exit "$FAILS"
