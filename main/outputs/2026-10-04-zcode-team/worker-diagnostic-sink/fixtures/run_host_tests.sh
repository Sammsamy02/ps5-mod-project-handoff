#!/bin/bash
# run_host_tests.sh — host fixtures for the r-s1 supplier diagnostic sink.
# Compiles the STAGED diagnostic.c (outputs/2026-10-04-zcode-team/
# worker-diagnostic-sink/staging/diagnostic.c) against POSIX shims and runs:
#   s1  order + content + additive entry channel
#   s1b 200-event burst
#   s2a forced open failure (bounded 8 attempts) + recovery + self-report
#   s2b REAL read-only directory (chmod 0555) — tolerant, exit 0
#   s3  write failure + partial-short-write bounded retry
#   s4  oversized tag bounded, never invalid JSON
# plus a strict per-line python json.loads validation of every produced file
# (seq ascending, t_ms nondecreasing, tags in call order).
# Host logic proof only — NO console behavior is claimed by this receipt.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
WHAT="$(cd "$HERE/../../../.." && pwd)"
STAGED="$HERE/../staging/diagnostic.c"
SDK_DIAG_H_DIR="$WHAT/outputs/ps5-mod-bridge/native-runtime-startup"
RECEIPT="$HERE/receipts/host-test-pass.txt"
TMP="$(mktemp -d /tmp/diagsink-test.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT
FAILS=0
note() { echo "[host-test] $*"; }
fail() { echo "[host-test] FAIL: $*"; FAILS=$((FAILS + 1)); }

check_sha() {
    GOT=$(shasum -a 256 "$STAGED" | awk '{print $1}')
    if [ "$GOT" != "$1" ]; then
        fail "staged source sha drift: $GOT != $1"
    else
        note "staged source sha OK: ${GOT:0:16}..."
    fi
}

# sha asserted so the receipt pins exactly the source that was tested
check_sha "f5e96c41c4a96d6032269aee671d8fe12c695db749cdd75792fc2ca3e2d62818"

# ---- generated include (mirrors the real build's generated/ step) ----
mkdir -p "$TMP/ro"
ENTRY_LOG="$TMP/entry-channel.log"
cat > "$HERE/include/probe_config.h" <<EOF
#pragma once
#define PROBE_TITLE "PPSA33286"
#define PROBE_LOG_PATH "$ENTRY_LOG"
#define PROBE_MARKER "modbridge-runtime-aligned-entry/v1 build=f6530c7905934e5a8cc201aa9400ebb0"
#define PROBE_INPUT_PATH "/data/townfall_worker_input.lua"
#define PROBE_LUA_MARKER "modbridge-startup-lua/v1"
#define PROBE_INPUT_EXPECTED_BYTES 88u
#define PROBE_INPUT_MAX_BYTES 65536u
#define PROBE_LUA_INTEGER 136
#define PROBE_DELAY_SECONDS 5
EOF

CFLAGS="-Wall -Wextra -Werror -O1 -I$HERE/include -I$SDK_DIAG_H_DIR"
SUPP="$TMP/supplier.jsonl"

note "compiling main fixture binary"
cc $CFLAGS -DSUPPLIER_DIAG_PATH="\"$SUPP\"" \
    "$STAGED" "$HERE/host_shims.c" "$HERE/test_supplier_sink_host.c" \
    -o "$TMP/test_main" || fail "main fixture compile"

note "compiling read-only-dir fixture binary"
cc $CFLAGS -DSUPPLIER_DIAG_PATH="\"$TMP/ro/supplier.jsonl\"" \
    "$STAGED" "$HERE/host_shims.c" "$HERE/test_readonly_dir_host.c" \
    -o "$TMP/test_ro" || fail "ro fixture compile"

note "running s2b: real read-only directory"
chmod 555 "$TMP/ro"
if [ "$(id -u)" = "0" ]; then
    note "SKIP s2b (running as root bypasses directory permissions; the shim-forced EACCES path s2a covers the same tolerance deterministically)"
else
    "$TMP/test_ro" > "$TMP/ro-output.txt" 2>&1
    if [ $? -ne 0 ] || ! grep -q "HOST-TEST-PASS-RO" "$TMP/ro-output.txt"; then
        fail "s2b binary did not pass (crash or cap failure)"
    else
        if [ -e "$TMP/ro/supplier.jsonl" ]; then
            fail "s2b file appeared despite read-only dir"
        else
            note "s2b PASS: 80 events into read-only dir, exit 0, no file, $(grep 'ro open attempts' "$TMP/ro-output.txt")"
        fi
    fi
    chmod 755 "$TMP/ro"
fi

note "running main scenarios (s1, s1b, s2a, s3, s4)"
if [ ! -x "$TMP/test_main" ]; then
    fail "main fixture binary missing"
else
    "$TMP/test_main" "$SUPP" "$ENTRY_LOG" | tee "$TMP/main-output.txt"
    if ! grep -q "^HOST-TEST-PASS$" "$TMP/main-output.txt"; then
        fail "main scenarios did not print HOST-TEST-PASS"
    fi
fi

note "strict JSON + order validation (python)"
python3 - "$SUPP" "$ENTRY_LOG" <<'PYEOF' || fail "python validation"
import json, sys
supp, entry = sys.argv[1], sys.argv[2]
lines = open(supp).read().splitlines()
assert len(lines) == 207, f"expected 207 supplier lines, got {len(lines)}"
tags, t_ms = [], []
expected_seqs = list(range(1, 206)) + [207, 208]  # seq 206 = the dropped write-failure line (by-design gap)
for i, (line, want_seq) in enumerate(zip(lines, expected_seqs), 1):
    rec = json.loads(line)  # strict: raises on any invalid line
    assert rec["marker"] == "modbridge.supplier-diag/v1", i
    assert rec["schema"] == 1, i
    assert rec["seq"] == want_seq, (i, rec["seq"], want_seq)
    assert isinstance(rec["pid"], int) and rec["pid"] > 0, i
    assert rec["value"] == int(rec["value_hex"], 16), i
    assert isinstance(rec["t_ms"], int), i
    tags.append(rec["tag"]); t_ms.append(rec["t_ms"])
assert "open_errno" in json.loads(lines[0]), "self-report on first recovered line"
assert json.loads(lines[0])["open_errno"] == 13, "recorded open errno is 13"
assert all("open_errno" not in json.loads(l) for l in lines[1:]), "no repeats"
assert tags[:2] == ["bridge_recovered", "bridge_after"], tags[:2]
assert tags[2:5] == ["bridge_vmentry_entries", "bridge_lease_reg", "bridge_exepath_sig"], tags[2:5]
assert all(t == "bridge_burst" for t in tags[5:205])
assert tags[205] == "bridge_partial", tags[205]
assert tags[206].startswith("B"), tags[206]
assert sum(1 for t in tags if t == "bridge_noopen") == 0, "failed-open events dropped"
assert sum(1 for t in tags if t == "bridge_writefail") == 0, "failed-write events dropped"
nondec = all(t_ms[i] <= t_ms[i+1] for i in range(len(t_ms)-1))
assert nondec, "t_ms must be nondecreasing (monotonic clock)"
el = open(entry).read().splitlines()
assert len(el) == 205, f"entry channel lines: {len(el)}"
assert el[0].startswith("[PPSA33286] modbridge-runtime-aligned-entry/v1 build=f6530c7905934e5a8cc201aa9400ebb0 ")
assert "bridge_vmentry_entries" in el[0] and "bridge_lease_reg value=0x00000006" in el[1]
assert sum(1 for l in el if "bridge_noopen" in l or "bridge_writefail" in l) == 0
print(f"[json] 207/207 supplier lines strict-parse; seq 1..207 ascending; "
      f"tags ordered; t_ms nondecreasing; entry channel 205 legacy lines intact")
PYEOF

if [ "$FAILS" -eq 0 ]; then
    note "ALL HOST TESTS PASS"
else
    note "$FAILS stage failure(s)"
fi

{
    echo "# worker-diagnostic-sink host fixture receipt"
    echo "utc: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "staged source: $STAGED"
    echo "staged sha256: $(shasum -a 256 "$STAGED" | awk '{print $1}')"
    echo "host compiler: $(cc --version | head -1)"
    echo "result: $([ "$FAILS" -eq 0 ] && echo PASS || echo "FAIL ($FAILS)")"
    echo "scenarios: A startup-open-failure + recovery, B order/content/additive,"
    echo "           C 200-burst, D write-fail + partial retry, E long tag, s2b real"
    echo "           read-only dir, strict python json validation of all 207 lines"
    echo "scope: host logic proof ONLY; no console behavior claimed"
} > "$RECEIPT"
cat "$RECEIPT"
exit "$FAILS"
