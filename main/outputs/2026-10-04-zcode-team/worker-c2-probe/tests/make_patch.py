#!/usr/bin/env python3
"""make_patch.py — HOST TEST ONLY (worker-c2-probe). Emits the synthetic
window table (rt/len/hex per line) that the host test's read hook serves to
the probe. These are the OFFLINE expected values — used ONLY on the host to
feed the simulated game memory and to validate the emitted live_hex. They
never enter the console PRX."""
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
WORKER = HERE.parent
ev = json.loads((WORKER / "EXPECTED-VALUES-OFFLINE.json").read_text())
dst = Path(sys.argv[1]) if len(sys.argv) > 1 else HERE / "out" / "sim_patch.txt"
rows = ["# rt len hex (host-test synthetic windows; offline only)"]
for c in ev["candidates"]:
    rows.append(f"0x{int(c['rt_base_0x400000'], 16):x} {c['len']} {c['expected_hex']}")
dst.parent.mkdir(parents=True, exist_ok=True)
dst.write_text("\n".join(rows) + "\n")
print("wrote", dst)
