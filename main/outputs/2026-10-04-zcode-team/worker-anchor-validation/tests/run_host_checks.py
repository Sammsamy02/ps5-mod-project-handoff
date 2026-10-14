#!/usr/bin/env python3
"""run_host_checks.py — igf4 host checks (worker-anchor-validation).

1. WRITER: compile+run the REAL src/record_sink.c (v3 lineage, byte-unchanged)
   against host ABI stubs; every non-truncated JSONL line must json.loads and
   carry the exact field expectations; the truncated line must flag
   "__trunc":1 and still parse structurally enough for that key.
2. PREDICATE: compile+run pred_host.cpp with the REAL vendored
   src/townfall_layout_probes.cpp — layout A and B synthetic validations,
   the header-span-only (cfgH) expected refuted-outside-span shape, and the
   code-shape classifier on the retained-image expected prologue bytes.
3. VMMAP WALK: compile+run vmmap_host.c with the REAL vendored v3 walker —
   checkpoint-span parsing, 36-bit round-trip, trust gate, insane-stop, and
   the coverage-inclusion formula used by the safety gate.
4. EXPECTED BYTES: the generated ingame_config.h (if present) must carry the
   exact expected hex from expected_bytes_tick_sco.txt.
"""
import json
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
SRC = ROOT / "src"
GEN = SRC / "generated"

cc = subprocess.run(["sh", "-c", "command -v cc || command -v clang"],
                    capture_output=True, text=True).stdout.strip()
if not cc:
    sys.exit("no host C compiler")
cxx = subprocess.run(["sh", "-c", "command -v c++ || command -v clang++"],
                     capture_output=True, text=True).stdout.strip()
if not cxx:
    sys.exit("no host C++ compiler")

results = {}
tmp = Path(tempfile.mkdtemp(prefix="igf4-host-"))

def run(argv, name):
    r = subprocess.run([str(a) for a in argv], capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stdout, r.stderr)
        sys.exit(f"HOST CHECK FAILED at {name}")
    return r

# ---- 1. writer ----
exe = tmp / "writer_host"
run([cc, "-Wall", "-Wextra", "-I", SRC, HERE / "writer_host.c",
     SRC / "record_sink.c", "-o", exe], "writer-compile")
r = run([exe], "writer-run")
lines = [ln for ln in r.stdout.splitlines() if ln.strip()]
parsed = 0
trunc_flagged = 0
for ln in lines:
    try:
        obj = json.loads(ln)
    except json.JSONDecodeError:
        # v3-lineage truncation contract: a truncated record is EXEMPT from
        # the parse guarantee (the tail is cut mid-value); it must still
        # carry the textual "__trunc":1 marker.
        assert '"__trunc":1' in ln, ln[-80:]
        trunc_flagged += 1
        continue
    if obj.get("__trunc") == 1:
        trunc_flagged += 1
        continue
    parsed += 1
assert parsed == 2, parsed
assert trunc_flagged == 1, trunc_flagged
f2 = json.loads(lines[1])
assert f2["fields"]["neg"] == -12345
assert f2["fields"]["wide"] == "0x0000000500000090"  # fixed 16-nibble form
assert f2["fields"]["esc"] == "quote\"back\\slash\u0001ctl"
assert f2["fields"]["arr"] == [1, 2]
assert f2["fields"]["after_arr"] == 7 and f2["fields"]["last"] == "end", f2["fields"]
results["writer"] = {"records": len(lines), "parsed": parsed,
                     "trunc_flagged": trunc_flagged,
                     "field_shapes": "exact"}

# ---- 2. predicate ----
exe = tmp / "pred_host"
run([cxx, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-O1", "-I", SRC,
     HERE / "pred_host.cpp", SRC / "townfall_layout_probes.cpp",
     "-o", exe], "pred-compile")
r = run([exe], "pred-run")
assert "PRED-HOST-CHECKS-PASSED" in r.stdout
results["predicate"] = {"summary": "PRED-HOST-CHECKS-PASSED",
                        "cases": r.stdout.count("ok   ")}

# ---- 3. vmmap walk ----
exe = tmp / "vmmap_host"
run([cc, "-Wall", "-Wextra", "-Werror", "-I", SRC, HERE / "vmmap_host.c",
     SRC / "vmmap_walk.c", "-o", exe], "vmmap-compile")
r = run([exe], "vmmap-run")
assert "VMMAP-HOST-CHECKS-PASSED" in r.stdout
results["vmmap_walk"] = {"summary": "VMMAP-HOST-CHECKS-PASSED",
                         "cases": r.stdout.count("ok   ")}

# ---- 4. expected bytes vs generated config ----
ev = json.loads((ROOT / "expected_bytes_tick_sco.txt").read_text())
cfg = GEN / "ingame_config.h"
if cfg.exists():
    text = cfg.read_text()
    for key, define in (("tick", "IG_TICK_EXPECTED_HEX"),
                        ("sco", "IG_SCO_EXPECTED_HEX")):
        want = ev[key]["expected16_hex"]
        line = [ln for ln in text.splitlines() if define in ln]
        assert line and f'"{want}"' in line[0], (define, want)
    results["expected_bytes"] = {"config": str(cfg), "match": True}
else:
    results["expected_bytes"] = {"config": "not-yet-generated (pre-build run)",
                                 "match": None}

results["status"] = "HOST-CHECKS-PASSED"
(ROOT / "host-checks.json").write_text(json.dumps(results, indent=1) + "\n")
print(json.dumps(results))
