#!/usr/bin/env python3
"""host_checks.py — worker-code-anchor-reads host test runner.

Compiles the pure igf5 logic (anchor_pages.c) + the REAL byte-identical
lineage record writer (record_sink.c, ingame_libc.c) against a small host
harness, runs it, then parses EVERY emitted JSONL line with json.loads and
validates the anchor-record field shapes (method_used vocabulary, byte_match
domain, hex field shapes, refusal/no-read coupling). Local only; nothing
native is executed."""
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
SRC = HERE.parent / "src"
CC = "/usr/bin/cc"

hexfield = re.compile(r"^0x[0-9a-f]{16}$")
METHODS_OK = {"mprotect_then_read", "refused_mprotect_failed",
              "refused_outside_exec_span"}


def main() -> None:
    outdir = HERE / "out"
    outdir.mkdir(exist_ok=True)
    binpath = outdir / "host_main"
    jsonl = outdir / "records.jsonl"
    r = subprocess.run(
        [CC, "-Wall", "-Wextra", "-Werror", "-std=c11",
         "-I", str(SRC),
         str(HERE / "host_main.c"), str(SRC / "anchor_pages.c"),
         str(SRC / "record_sink.c"), str(SRC / "ingame_libc.c"),
         "-o", str(binpath)],
        capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit(f"host compile failed:\n{r.stderr}")
    r = subprocess.run([str(binpath), str(jsonl)],
                       capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit(f"host run failed: rc={r.returncode}\n{r.stdout}{r.stderr}")
    assert "HOST_CHECKS_PASSED" in r.stdout, r.stdout

    lines = [ln for ln in jsonl.read_text().splitlines() if ln.strip()]
    assert len(lines) == 3, f"expected 3 records, got {len(lines)}"
    parsed = []
    for i, ln in enumerate(lines):
        obj = json.loads(ln)  # hard failure on any malformed line
        assert obj["marker"] == "modbridge.ingamefacts"
        assert obj["schema"] == 1
        assert isinstance(obj["seq"], int) and isinstance(obj["rc"], int)
        parsed.append(obj)

    tick, sco, summary = parsed
    f = tick["fields"]
    assert f["method_used"] in METHODS_OK
    assert f["byte_match"] in (0, 1, -1)
    assert hexfield.match(f["rt_addr"]) and f["rt_addr"] == "0x00000000033ac8c0"
    assert hexfield.match(f["page_start"]) and f["page_start"] == "0x00000000033ac000"
    assert f["page_size"] == 16384 and f["pages"] == 1
    assert f["read_performed"] == 1 and f["mprotect_restored"] == 1
    assert len(f["expected_bytes_hex"]) == 32
    assert len(f["bytes_read_hex"]) == 32
    # Refusal coupling: no read => refusal present, byte_match absent.
    fs = sco["fields"]
    assert fs["read_performed"] == 0 and "byte_match" not in fs
    assert fs["refusal"] == "mprotect_failed_no_read_attempted"
    assert fs["mprotect_called"] == 1 and fs["mprotect_restored"] == 0
    # Summary gate fields exist.
    gs = summary["fields"]
    assert "anchors_confirmed" in gs and "promotion_gate_indicator" in gs
    assert "promotion_gate_indicator_anchors_only" in gs
    assert "mprotect_anomalies" in gs

    result = {"status": "HOST_CHECKS_PASSED", "records": len(lines),
              "jsonl_valid": True, "binary": str(binpath),
              "jsonl": str(jsonl)}
    (HERE / "host-checks.json").write_text(json.dumps(result, indent=1) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    main()
