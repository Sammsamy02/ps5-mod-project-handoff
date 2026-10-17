#!/usr/bin/env python3
"""check_argv_artifact.py — loud local gate for the ARGV_ECHO build.

Checks (all must pass, else nonzero exit):
  1. ELF magic + x86-64 machine + EXEC/DYN type;
  2. sane size (1 KiB .. 1 MiB);
  3. marker string "modbridge.argvprobe" and the exact log path are embedded
     (read-only bytes proof the payload targets OUR sink and OUR schema);
  4. build dir contains: final ELF, determinism copy, import-proof.txt;
     the two clean builds are byte-identical (proven from the files
     themselves, not from the receipt, so this gate runs pre-receipt);
  5. when build-receipt.json already exists, its final_sha256 must match.
"""
import hashlib
import json
import sys
from pathlib import Path

MAGIC = b"\x7fELF"


def fail(msg):
    sys.exit(f"ARTIFACT CHECK FAILED: {msg}")


def main() -> None:
    if len(sys.argv) != 2:
        fail("usage: check_argv_artifact.py <build-dir>")
    out = Path(sys.argv[1])
    elf = out / "ARGV_ECHO.elf"
    if not elf.is_file():
        fail(f"missing {elf}")

    data = elf.read_bytes()
    if data[:4] != MAGIC:
        fail("not an ELF")
    if data[4] != 2 or data[5] != 1:
        fail(f"unexpected EI_CLASS/EI_DATA: {data[4]},{data[5]}")
    machine = int.from_bytes(data[18:20], "little")
    etype = int.from_bytes(data[16:18], "little")
    if machine != 62:
        fail(f"machine {machine} != 62 (x86-64)")
    if etype not in (2, 3):
        fail(f"e_type {etype} neither EXEC nor DYN")
    size = len(data)
    if not (1024 <= size <= 1024 * 1024):
        fail(f"size {size} outside 1KiB..1MiB")

    for needle in (b"modbridge.argvprobe", b"/data/modbridge-argv-probe.jsonl"):
        if needle not in data:
            fail(f"embedded string missing: {needle!r}")

    twin = out / "determinism" / "ARGV_ECHO_b.elf"
    for p in (out / "import-proof.txt", twin):
        if not p.is_file():
            fail(f"missing deliverable {p}")
    sha = hashlib.sha256(data).hexdigest()
    sha_b = hashlib.sha256(twin.read_bytes()).hexdigest()
    if sha != sha_b:
        fail(f"nondeterministic build: {sha} != {sha_b}")

    receipt_path = out / "build-receipt.json"
    if receipt_path.is_file():
        receipt = json.loads(receipt_path.read_text())
        if sha != receipt.get("final_sha256"):
            fail(f"sha mismatch file={sha} receipt={receipt.get('final_sha256')}")
        if receipt.get("build_id", "x")[:5] != "argv-":
            fail("receipt build_id prefix wrong")

    print(json.dumps({
        "check": "argv-echo-artifact", "ok": True,
        "elf": str(elf), "bytes": size, "sha256": sha,
        "etype": etype, "machine": machine,
        "determinism_identical": True}))


if __name__ == "__main__":
    main()
