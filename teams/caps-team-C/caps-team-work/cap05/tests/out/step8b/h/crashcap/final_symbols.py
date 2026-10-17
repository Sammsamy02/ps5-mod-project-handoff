#!/usr/bin/env python3
"""final_symbols.py - exact, byte-verified final symbols for a packaged runtime PRX.

cap05 (crash capture), Mac-only, read-only on every input.

WHY: the shipped PRX has no .symtab, and the packaging sidecar's
"map_reconstructed" rows are order-assumed (alignment padding ignored), so they
drift. The build pipeline already KEEPS the exact relocatable object the final
writer consumed (pass-N/build/combined-supplier.o). This tool re-derives where
the final writer put every executable input section, then PROVES each
placement by comparing that section's bytes with the bytes in the final bare
PRX (relocation sites masked). Only placements that match byte-for-byte are
used; one mismatch anywhere fails the whole run closed.

Two independent sources for every shipped address:
  1. object symbol table + the final writer's placement rule
     (SharpProspero.Link/DynamicWriter.cs: Gather() + the code-group loop:
     executable alloc non-TLS sections, grouped by output name, .init/.text/
     .fini first then first-seen order, object/section order inside a group,
     each aligned to its sh_addralign, text group based at VMA 0)
  2. the final artifact's own bytes at that address (verification).

Binding to the deployed artifact:
  * bare PRX GNU build-id note (the PS5 kernel prints this same value as the
    module "fingerprint" in every crash block's library list)
  * packaged PRX sha256 + proof that the packaged file contains the bare PRX's
    executable segment verbatim and the build-id bytes.

Usage:
  final_symbols.py --object combined-supplier.o --bare bare-original/X.prx \
                   [--packaged X.prx] --out OUT.json

Exit 0 = every executable section verified, OUT written.
Exit 2 = input/verification failure (nothing written). Never exits 0 with
zero verified sections.
"""
import argparse
import hashlib
import json
import os
import sys

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import elf64  # noqa: E402

SCHEMA = "modbridge.final-symbols/v1"
# DynamicWriter.cs SectionPrefixes (longest first) and CodeOrder.
SECTION_PREFIXES = [".data.rel.ro", ".text", ".rodata", ".data", ".bss", ".init_array",
                    ".fini_array", ".tdata", ".tbss", ".gcc_except_table", ".eh_frame"]
CODE_ORDER = [".init", ".text", ".fini"]
# x86-64 relocation types whose patched field is 8 bytes wide.
RELOC_W8 = {1, 16, 17, 18, 24, 25, 26, 29, 30, 33, 38}
# Relaxable GOT/TLS forms: the writer may rewrite opcode bytes before r_offset.
RELOC_RELAX = {9, 19, 20, 22, 41, 42}
STT_FUNC = 2
STT_SECTION = 3
STT_FILE = 4


def sha256_path(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def output_section_name(name):
    if name == ".ctors" or name.startswith(".ctors."):
        return ".init_array"
    if name == ".dtors" or name.startswith(".dtors."):
        return ".fini_array"
    for p in SECTION_PREFIXES:
        if name == p or name.startswith(p + "."):
            return p
    return name


def code_layout(obj_sections):
    """[(section, offset_in_code_group)] in final-writer order."""
    ex = [s for s in obj_sections
          if (s["flags"] & elf64.SHF_ALLOC) and (s["flags"] & elf64.SHF_EXECINSTR)
          and not (s["flags"] & elf64.SHF_TLS)]
    by, seen = {}, []
    for s in ex:
        n = output_section_name(s["name"])
        if n not in by:
            by[n] = []
            seen.append(n)
        by[n].append(s)
    first = {n: i for i, n in enumerate(seen)}
    seen.sort(key=lambda n: (CODE_ORDER.index(n) if n in CODE_ORDER else len(CODE_ORDER), first[n]))
    cur = 0
    out = []
    for n in seen:
        for s in by[n]:
            a = max(1, s["align"])
            cur = (cur + a - 1) // a * a
            out.append((n, s, cur))
            cur += s["size"]
    return out, seen, cur


def mask_ranges(relocs, size):
    out = []
    for (ro, t) in relocs:
        w = 8 if t in RELOC_W8 else 4
        lo = ro - (3 if t in RELOC_RELAX else 0)
        out.append((max(0, lo), min(size, ro + w)))
    return out


def verify_and_place(obj, bare, text_base):
    secs = obj.sections()
    rel = obj.relas_by_target()
    layout, groups, end = code_layout(secs)
    placed = []
    compared = masked = 0
    for (grp, s, off) in layout:
        va = text_base + off
        if s["type"] == elf64.SHT_NOBITS or s["size"] == 0:
            placed.append((s, va))
            continue
        ob = bytearray(obj.read(s["offset"], s["size"]))
        pb = bare.read_va(va, s["size"])
        if pb is None:
            raise elf64.ElfError("section %s placed at 0x%x is outside the bare PRX's loaded bytes"
                                 % (s["name"], va))
        pb = bytearray(pb)
        for lo, hi in mask_ranges(rel.get(s["index"], []), s["size"]):
            if hi > lo:
                ob[lo:hi] = b"\0" * (hi - lo)
                pb[lo:hi] = b"\0" * (hi - lo)
                masked += hi - lo
        if ob != pb:
            k = next(i for i in range(len(ob)) if ob[i] != pb[i])
            raise elf64.ElfError("BYTE MISMATCH: section %s (index %d) at final VA 0x%x differs at +0x%x; "
                                 "the final-writer layout is NOT reproduced - refusing"
                                 % (s["name"], s["index"], va, k))
        compared += s["size"]
        placed.append((s, va))
    return placed, groups, text_base + end, compared, masked


def build(obj_path, bare_path, packaged_path):
    obj = elf64.Elf(obj_path)
    bare = elf64.Elf(bare_path)
    if obj.e_type != 1:
        raise elf64.ElfError("%s is not a relocatable object (e_type=%d)" % (obj_path, obj.e_type))
    bsecs = bare.sections()
    text = [s for s in bsecs if s["name"] == ".text"]
    if len(text) != 1:
        raise elf64.ElfError("bare PRX must carry exactly one .text section header (found %d)" % len(text))
    exec_loads = bare.exec_loads()
    if len(exec_loads) != 1:
        raise elf64.ElfError("bare PRX must have exactly one executable PT_LOAD (found %d)" % len(exec_loads))
    xl = exec_loads[0]
    build_id = bare.gnu_build_id()
    if not build_id:
        raise elf64.ElfError("bare PRX has no GNU build-id note; cannot bind to a crash fingerprint")
    # The first code input section's final address = final .text header address
    # minus nothing: the writer bases the code group at 0 and the leading bytes
    # (if any) come from earlier groups; we take the .text header start as the
    # start of the .text group and verify bytes, so a wrong guess fails closed.
    layout, groups, _end = code_layout(obj.sections())
    if not layout:
        raise elf64.ElfError("object has zero executable sections - nothing to verify")
    first_text = next(((g, s, off) for (g, s, off) in layout if g == ".text"), None)
    if first_text is None:
        raise elf64.ElfError("object has no .text group")
    text_base = text[0]["addr"] - first_text[2]
    placed, groups, end, compared, masked = verify_and_place(obj, bare, text_base)
    if not placed or compared == 0:
        raise elf64.ElfError("zero bytes verified - refusing")
    if end > xl["vaddr"] + xl["memsz"]:
        raise elf64.ElfError("code group end 0x%x beyond executable segment end 0x%x"
                             % (end, xl["vaddr"] + xl["memsz"]))
    sec_va = {s["index"]: (s, va) for (s, va) in placed}

    syms = []
    covered = set()
    for sym in obj.symbols():
        if sym["shndx"] not in sec_va or sym["type"] in (STT_SECTION, STT_FILE):
            continue
        s, va = sec_va[sym["shndx"]]
        if sym["type"] != STT_FUNC and not sym["name"]:
            continue
        if sym["value"] > s["size"] or sym["value"] + sym["size"] > s["size"]:
            raise elf64.ElfError("symbol %s exceeds its section %s" % (sym["name"], s["name"]))
        if sym["type"] != STT_FUNC:
            continue
        if not sym["name"] or sym["size"] == 0:
            continue
        syms.append({"name": sym["name"], "addr": va + sym["value"], "size": sym["size"],
                     "bind": sym["bind"]})
        covered.add(sym["shndx"])
    # Sections with code but no sized FUNC symbol: emit the section itself.
    for idx, (s, va) in sec_va.items():
        if idx not in covered and s["size"]:
            syms.append({"name": "<section %s>" % s["name"], "addr": va, "size": s["size"],
                         "bind": -1})
    syms.sort(key=lambda r: (r["addr"], -r["size"]))
    if not syms:
        raise elf64.ElfError("zero function symbols - refusing")

    packaged = None
    if packaged_path:
        pk_sha = sha256_path(packaged_path)
        with open(packaged_path, "rb") as f:
            pk = f.read()
        seg = bare.read(xl["offset"], xl["filesz"])
        at = pk.find(seg[:4096])
        seg_ok = at >= 0 and pk[at:at + len(seg)] == seg
        bid_ok = pk.find(bytes.fromhex(build_id)) >= 0
        if not (seg_ok and bid_ok):
            raise elf64.ElfError("packaged PRX %s does not contain the bare PRX executable segment "
                                 "(%s) and build-id (%s) - different build, refusing"
                                 % (packaged_path, seg_ok, bid_ok))
        packaged = {"path": os.path.abspath(packaged_path), "sha256": pk_sha, "bytes": len(pk),
                    "contains_bare_exec_segment_at": at, "contains_build_id": True}
        del pk

    out = {
        "schema": SCHEMA,
        "tool": "cap05/tools/final_symbols.py",
        "console_contact": False,
        "tier": "OBJ-LAYOUT-BYTE-VERIFIED",
        "address_convention": ("addr = final ELF virtual address in the bare PRX; executable PT_LOAD "
                               "vaddr=0x%x; crash consumer uses addr = RIP - xotext_start + exec_vaddr"
                               % xl["vaddr"]),
        "object": {"path": os.path.abspath(obj_path), "sha256": sha256_path(obj_path),
                   "bytes": os.path.getsize(obj_path)},
        "bare_prx": {"path": os.path.abspath(bare_path), "sha256": sha256_path(bare_path),
                     "bytes": os.path.getsize(bare_path), "build_id": build_id,
                     "exec_load": {"vaddr": xl["vaddr"], "memsz": xl["memsz"],
                                   "filesz": xl["filesz"], "align": xl["align"]}},
        "packaged_prx": packaged,
        "layout": {"groups": groups, "text_base": text_base, "code_end": end,
                   "sections_verified": sum(1 for (s, _v) in placed if s["size"]),
                   "bytes_compared": compared, "bytes_masked_reloc": masked},
        "symbol_count": len(syms),
        "symbols": [[r["name"], r["addr"], r["size"]] for r in syms],
    }
    obj.close()
    bare.close()
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--object", required=True)
    ap.add_argument("--bare", required=True)
    ap.add_argument("--packaged")
    ap.add_argument("--out", required=True)
    a = ap.parse_args(argv)
    if os.path.exists(a.out):
        print("final_symbols: refusing to overwrite %s" % a.out, file=sys.stderr)
        return 2
    try:
        res = build(a.object, a.bare, a.packaged)
    except (OSError, elf64.ElfError, ValueError) as e:
        print("final_symbols: FAIL: %s" % e, file=sys.stderr)
        return 2
    tmp = a.out + ".tmp"
    with open(tmp, "x") as f:
        json.dump(res, f, separators=(",", ":"))
    os.replace(tmp, a.out)
    lay = res["layout"]
    print("final_symbols: PASS sections_verified=%d bytes_compared=%d bytes_masked=%d symbols=%d "
          "build_id=%s packaged=%s"
          % (lay["sections_verified"], lay["bytes_compared"], lay["bytes_masked_reloc"],
             res["symbol_count"], res["bare_prx"]["build_id"],
             res["packaged_prx"]["sha256"][:8] if res["packaged_prx"] else "not-given"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
