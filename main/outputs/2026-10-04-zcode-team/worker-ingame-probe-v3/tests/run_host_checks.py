#!/usr/bin/env python3
"""run_host_checks.py — host-side validation for worker-ingame-probe-v3.

1. Compiles the REAL src/record_sink.c with tests/writer_host.c (sink trio
   stubbed) and json.loads-VALIDATES every emitted line: every NON-truncated
   line must parse (the v1/v2 logs did not), truncated lines must carry
   "__trunc":1. Checks 36-bit hex round-trips, INT64_MIN, empty arrays,
   nested objects/arrays, string escaping.
2. Compiles the REAL src/vmmap_walk.c with tests/vmmap_walk_host.c and
   asserts the kve_structsize-strided walk parses synthetic variable-size
   records exactly (incl. 36-bit addresses) and the fixed-stride v1/v2 walk
   is merely the flagged fallback.
"""
import json
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
SRC = HERE.parent / "src"
T = HERE

failures = []

def sh(*argv):
    r = subprocess.run([str(a) for a in argv], capture_output=True, text=True)
    if r.returncode != 0:
        failures.append(f"command failed {argv}: {r.stderr[-2000:]}")
    return r

# --- writer test ---
sh("cc", "-O1", "-Wall", "-Wextra", "-I", SRC,
   SRC / "record_sink.c", T / "writer_host.c", "-o", T / "writer_host")
r = subprocess.run([str(T / "writer_host"), str(T / "writer.jsonl")],
                   capture_output=True, text=True)
if r.returncode != 0:
    failures.append(f"writer_host rc={r.returncode}")

lines = (T / "writer.jsonl").read_text().splitlines()
if len(lines) != 5:
    failures.append(f"expected 5 lines, got {len(lines)}")

parsed = []
for i, ln in enumerate(lines):
    try:
        obj = json.loads(ln)
        parsed.append(obj)
    except json.JSONDecodeError as e:
        if '"__trunc":1' in ln:
            parsed.append(None)  # exempt: flagged truncation
        else:
            failures.append(f"line {i+1} does not parse and is NOT flagged truncated: {e}: {ln[:120]}")

f0 = parsed[0]["fields"] if parsed[0] else {}
exp = {
    "raw": 0,
    "raw_hex": "0x0000000000000000",
    "neg_min": -9223372036854775808,
    "neg": -9223372036854775807,
    "i64_max": 9223372036854775807,
    "rc_neg": -1,
    "span36_lo": "0x0000000100000090",
    "span36_hi": "0x00000007ed7f8000",
    "read_end": "0x0000000100118000",
    "esc": "quote\" back\\ ctrl\u0001\u007f",
    "empty_arr": [],
}
for k, v in exp.items():
    if f0.get(k) != v:
        failures.append(f"writer field {k}: got {f0.get(k)!r} want {v!r}")

if parsed[1]:
    f1 = parsed[1]["fields"]
    if len(f1.get("entries", [])) != 16:
        failures.append("vm_page entries != 16 (truncated early?)")
    if f1.get("entries"):
        # 0x100000090 + 16*0x40000000 = 0x500000090 exactly
        # (RESUME DELTA: the prior harness expected 0x510000090 — wrong
        # arithmetic; the writer's emitted value was correct all along).
        want_last_end = "0x0000000500000090"
        if f1["entries"][-1]["end"] != want_last_end:
            failures.append(f"36-bit array end {f1['entries'][-1]['end']} != {want_last_end}")
    if f1.get("after_array") != 1:
        failures.append("kv after array close lost (separator bug shape)")
    if f1.get("prot_histogram") != [{"prot": "0x0000000000000005", "count": 9}]:
        failures.append("prot_histogram wrong")

if parsed[2]:
    f2 = parsed[2]["fields"]
    if len(f2.get("hdrs", [{}])[0].get("w", [])) != 17:
        failures.append("raw hdr words != 17")
    if f2["hdrs"][0]["w"][0] != "0x00000007ed7f8000":
        failures.append("hdr w0 36-bit round-trip wrong")

if parsed[4] is None or parsed[4].get("fields") != {}:
    ok = parsed[4] is not None and parsed[4].get("fields") == {}
    if not ok:
        failures.append("empty fields object did not parse as {}")

trunc_lines = [i + 1 for i, o in enumerate(parsed) if o is None]
if trunc_lines != [4]:
    failures.append(f"expected only line 4 truncated, got {trunc_lines}")

# --- vmmap walk test ---
sh("cc", "-O1", "-Wall", "-Wextra", "-I", SRC,
   SRC / "vmmap_walk.c", T / "vmmap_walk_host.c", "-o", T / "vmmap_walk_host")
r = subprocess.run([str(T / "vmmap_walk_host")], capture_output=True, text=True)
if r.returncode != 0:
    failures.append(f"vmmap_walk_host rc={r.returncode}: {r.stdout[-2000:]} {r.stderr[-500:]}")
print(r.stdout.strip())

result = {
    "writer_lines": len(lines),
    "writer_parsed_nontrunc": sum(1 for p in parsed if p is not None),
    "writer_trunc_flagged": trunc_lines,
    "vmwalk": r.stdout.strip().splitlines()[-1] if r.stdout.strip() else "no-output",
    "status": "HOST-CHECKS-PASSED" if not failures else "HOST-CHECKS-FAILED",
    "failures": failures,
}
print(json.dumps(result, indent=1))
sys.exit(0 if not failures else 1)
