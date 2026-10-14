#!/usr/bin/env python3
"""make_baseline.py — freeze a gate baseline JSON from an accepted build dir.

Read-only on the build dir. Produces the file consumed by
gate_check_composition5_v2.py via --baseline. Run this ONLY for a newly
ACCEPTED lineage (see REPORT.md section 4, "Rolling the baseline forward"),
under a NEW baseline_id; never edit a frozen baseline in place.

Usage:
  python3 make_baseline.py --build-dir <candidate build dir> \
      --baseline-id composition5-r12 --out <path>.json
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path

CALL_RELOC_TYPES = {2, 4, 9, 41, 42}
IDENTITY_DEFAULT = ("observed executable path is empty; refusing to fabricate "
                    "game_executable_path")
D2_GUARDS_DEFAULT = ["Mapped metadata or spans exceed supplied readable bytes",
                     "Mapped text base must be registered readable executable bytes"]


def elf_sections(data: bytes) -> list[dict]:
    (e_shoff,) = struct.unpack_from("<Q", data, 0x28)
    e_shentsize, e_shnum = struct.unpack_from("<HH", data, 0x3A)
    e_shstrndx = struct.unpack_from("<H", data, 0x3E)[0]
    count = e_shnum or struct.unpack_from("<Q", data, e_shoff + 32)[0]
    secs = []
    for i in range(count):
        off = e_shoff + i * e_shentsize
        sh_name, sh_type = struct.unpack_from("<II", data, off)
        sh_offset, sh_size = struct.unpack_from("<QQ", data, off + 24)
        sh_link, = struct.unpack_from("<I", data, off + 40)
        secs.append(dict(nameoff=sh_name, type=sh_type, offset=sh_offset,
                         size=sh_size, link=sh_link))
    strtab = b""
    if 0 < e_shstrndx < len(secs):
        s = secs[e_shstrndx]
        strtab = data[s["offset"]:s["offset"] + s["size"]]
    for s in secs:
        o = s["nameoff"]
        s["name"] = strtab[o:strtab.find(b"\x00", o)].decode("utf-8", "replace") \
            if o < len(strtab) else ""
    return secs


def sym_und_defined(data: bytes, secs: list[dict]) -> tuple[set[str], set[str]]:
    und: set[str] = set()
    defined: set[str] = set()
    for st in (s for s in secs if s["type"] == 2):
        strsec = secs[st["link"]]
        strdata = data[strsec["offset"]:strsec["offset"] + strsec["size"]]
        for j in range(st["size"] // 24):
            off = st["offset"] + j * 24
            st_name, _, _, st_shndx = struct.unpack_from("<IBBH", data, off)
            if st_name >= len(strdata):
                continue
            end = strdata.find(b"\x00", st_name)
            name = strdata[st_name:end].decode("utf-8", "replace")
            if name:
                (defined if st_shndx else und).add(name)
    return und, defined


def bridge_external_callees(data: bytes, secs: list[dict]) -> set[str]:
    symtab = next(s for s in secs if s["type"] == 2)
    strsec = secs[symtab["link"]]
    strdata = data[strsec["offset"]:strsec["offset"] + strsec["size"]]
    defined = set()
    for j in range(symtab["size"] // 24):
        off = symtab["offset"] + j * 24
        st_name, _, _, st_shndx = struct.unpack_from("<IBBH", data, off)
        if st_name < len(strdata):
            end = strdata.find(b"\x00", st_name)
            name = strdata[st_name:end].decode("utf-8", "replace")
            if name and st_shndx:
                defined.add(name)

    def symname(idx: int) -> str:
        off = symtab["offset"] + idx * 24
        (st_name,) = struct.unpack_from("<I", data, off)
        if st_name >= len(strdata):
            return ""
        return strdata[st_name:strdata.find(b"\x00", st_name)].decode("utf-8", "replace")

    out: set[str] = set()
    for s in secs:
        if s["type"] != 4 or not s["name"].startswith(".rela.text"):
            continue
        for j in range(s["size"] // 24):
            off = s["offset"] + j * 24
            r_info = struct.unpack_from("<Q", data, off + 8)[0]
            if (r_info & 0xFFFFFFFF) not in CALL_RELOC_TYPES:
                continue
            sym = symname(r_info >> 32)
            if not sym or sym.startswith((".L", ".LCPI")):
                continue
            if sym.startswith(("_ZTI", "_ZTV", "__dso_handle")):
                continue
            if sym in defined:
                continue
            out.add(sym)
    return out


def sha256_file(p: Path) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-dir", required=True, type=Path)
    ap.add_argument("--baseline-id", required=True)
    ap.add_argument("--out", required=True, type=Path)
    ap.add_argument("--identity-string", default=IDENTITY_DEFAULT)
    ap.add_argument("--d2-guards", nargs="*", default=D2_GUARDS_DEFAULT)
    args = ap.parse_args()
    bd = args.build_dir.resolve()
    combined = bd / "build" / "combined-supplier.o"
    bridge = bd / "build" / "runtime_start_bridge.o"
    contract_path = bd / "supplier-contract.json"
    for p in (combined, bridge, contract_path):
        if not p.is_file():
            print(f"ERROR: missing {p}", file=sys.stderr)
            return 2
    cdata = combined.read_bytes()
    csecs = elf_sections(cdata)
    und, _ = sym_und_defined(cdata, csecs)
    bdata = bridge.read_bytes()
    bsecs = elf_sections(bdata)
    callees = bridge_external_callees(bdata, bsecs)
    contract = json.loads(contract_path.read_text())
    names = sorted(r["ImportSymbol"]["Name"] for r in contract["contract"]["imports"])
    doc = {
        "baseline_id": args.baseline_id,
        "date": "generated by worker-gate-evolution/tools/make_baseline.py",
        "build_dir": str(bd),
        "combined_object_sha256": hashlib.sha256(cdata).hexdigest(),
        "combined_und": sorted(und),
        "combined_und_count": len(und),
        "supplier_contract_sha256": sha256_file(contract_path),
        "contract_import_names": names,
        "contract_import_count": len(names),
        "bridge_call_refs": sorted(callees),
        "bridge_object_sha256": hashlib.sha256(bdata).hexdigest(),
        "bridge_call_refs_definition":
            "call-range relocs (.rela.text, PC32/PLT32/GOTPCREL/GOTPCRELX/"
            "REX_GOTPCRELX) against symbols NOT defined in "
            "runtime_start_bridge.o, excluding local labels (.L*/.LCPI*) and "
            "data-address GOT refs (_ZTI*/_ZTV*/__dso_handle)",
        "game_identity_evidence_strings": [args.identity_string],
        "game_executable_path_string_count": cdata.count(b"game_executable_path"),
        "d2_guard_strings": args.d2_guards,
    }
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(doc, indent=1) + "\n")
    print(f"baseline {args.baseline_id}: UND={len(und)} contract={len(names)} "
          f"bridge_callees={len(callees)} -> {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
