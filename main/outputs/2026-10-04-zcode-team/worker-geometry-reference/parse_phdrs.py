#!/usr/bin/env python3
"""
parse_phdrs.py — independent ELF64 program-header parser for the retained decrypted eboot.

Worker: worker-geometry-reference (offline, read-only outside its output dir).
Purpose: re-derive the static geometry of the game image from the retained
decrypted-eboot.bin so no worker re-derives it from memory. Every emitted fact
carries provenance: ELF field name + file byte offset within the eboot.

Usage:
    python3 parse_phdrs.py <eboot-path> [--json OUT.json]

Output: human table on stdout; optional machine dump via --json.

Notes:
- EI_OSABI for this image is 9 (ELFOSABI_FREEBSD); Sony-specific p_type values
  (PT_SCE_*) are printed by name where known, hex otherwise.
- p_flags: PF_X=1, PF_W=2, PF_R=4 (FreeBSD/PS5 convention, matches KVME_PROT_*).
- No interpretation beyond the ELF spec happens here; slide/mapping models live
  in GEOMETRY.md / GEOMETRY.json, not in this parser.
"""

import hashlib
import json
import struct
import sys

PF_X, PF_W, PF_R = 1, 2, 4

PT_NAMES = {
    0: "PT_NULL", 1: "PT_LOAD", 2: "PT_DYNAMIC", 3: "PT_INTERP", 4: "PT_NOTE",
    5: "PT_SHLIB", 6: "PT_PHDR", 7: "PT_TLS",
    0x6474e550: "PT_GNU_EH_FRAME", 0x6474e551: "PT_GNU_STACK",
    0x6474e552: "PT_GNU_RELRO", 0x6474e553: "PT_GNU_PROPERTY",
    0x61000000 + 0: "PT_SCE_COMMENT (0x61000000)",
    0x61000001: "PT_SCE_VERSION (0x61000001)",
    0x70000000 + 0: "PT_SCE_PTLS? (0x70000000)",
    0x6FFFFFFF: "PT_GNU_xxx(0x6FFFFFFF)",
}

PAGE_ALIGN_16K = 0x4000


def flags_str(f):
    return ("R" if f & PF_R else "-") + ("W" if f & PF_W else "-") + ("X" if f & PF_X else "-")


def page_round_up(x, page=PAGE_ALIGN_16K):
    return (x + page - 1) // page * page


def parse(path):
    with open(path, "rb") as fh:
        data = fh.read()
    sha = hashlib.sha256(data).hexdigest()

    if data[:4] != b"\x7fELF":
        raise SystemExit("not an ELF file")
    ei_class, ei_data, ei_osabi = data[4], data[5], data[7]
    if ei_class != 2 or ei_data != 1:
        raise SystemExit("not ELF64 LE")

    # ELF64 header fields with byte offsets (little-endian).
    # Layout: e_ident[16], e_type@0x10(H), e_machine@0x12(H), e_version@0x14(I),
    #         e_entry@0x18(Q), e_phoff@0x20(Q), e_shoff@0x28(Q), e_flags@0x30(I),
    #         e_ehsize@0x34(H), e_phentsize@0x36(H), e_phnum@0x38(H),
    #         e_shentsize@0x3a(H), e_shnum@0x3c(H), e_shstrndx@0x3e(H)
    (e_type, e_machine, e_version) = struct.unpack_from("<HHI", data, 0x10)
    (e_entry, e_phoff, e_shoff) = struct.unpack_from("<QQQ", data, 0x18)
    (e_flags,) = struct.unpack_from("<I", data, 0x30)
    (e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx) = \
        struct.unpack_from("<HHHHHH", data, 0x34)

    hdr = {
        "sha256": sha,
        "file_size": len(data),
        "ei_class": ei_class, "ei_data": ei_data, "ei_osabi": ei_osabi,
        "e_type": e_type, "e_machine": e_machine,
        "e_entry": e_entry, "e_entry_byte_offset": 0x18,
        "e_phoff": e_phoff, "e_phentsize": e_phentsize, "e_phnum": e_phnum,
        "e_phoff_byte_offset": 0x20, "e_phentsize_byte_offset": 0x36,
        "e_phnum_byte_offset": 0x38,
        "e_shoff": e_shoff, "e_shnum": e_shnum,
    }

    phdrs = []
    for i in range(e_phnum):
        off = e_phoff + i * e_phentsize  # provenance: record start offset
        (p_type, p_flags, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align) = \
            struct.unpack_from("<IIQQQQQQ", data, off)
        phdrs.append({
            "index": i,
            "record_file_offset": off,
            "p_type": p_type,
            "p_type_name": PT_NAMES.get(p_type, "0x%08x" % p_type),
            "p_flags": p_flags,
            "flags_str": flags_str(p_flags),
            "p_offset": p_offset,
            "p_vaddr": p_vaddr,
            "p_paddr": p_paddr,
            "p_filesz": p_filesz,
            "p_memsz": p_memsz,
            "p_align": p_align,
            "vaddr_end": p_vaddr + p_memsz,
            "file_end": p_vaddr + p_filesz,
            "bss_bytes": p_memsz - p_filesz,
            "vaddr_end_page_up_16k": page_round_up(p_vaddr + p_memsz),
        })
    return hdr, phdrs


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(2)
    path = sys.argv[1]
    out_json = None
    if "--json" in sys.argv:
        out_json = sys.argv[sys.argv.index("--json") + 1]

    hdr, phdrs = parse(path)

    print("file: %s" % path)
    print("size: %d bytes (0x%x)" % (hdr["file_size"], hdr["file_size"]))
    print("sha256: %s" % hdr["sha256"])
    print("e_type=0x%x e_machine=0x%x e_entry=0x%x (hdr+0x18)" %
          (hdr["e_type"], hdr["e_machine"], hdr["e_entry"]))
    print("e_phoff=0x%x e_phentsize=0x%x e_phnum=%d (hdr+0x20/0x36/0x38)" %
          (hdr["e_phoff"], hdr["e_phentsize"], hdr["e_phnum"]))
    print("")
    print("%-4s %-22s %-4s %-12s %-12s %-12s %-12s %-10s" %
          ("idx", "type", "fl", "vaddr", "memsz_end", "filesz", "memsz", "file_off"))
    for p in phdrs:
        print("%-4d %-22s %-4s 0x%08x   0x%08x   0x%08x   0x%08x 0x%08x" %
              (p["index"], p["p_type_name"], p["flags_str"], p["p_vaddr"],
               p["vaddr_end"], p["p_filesz"], p["p_memsz"], p["p_offset"]))
        print("     @file 0x%x  filesz_end=0x%08x bss=0x%x align=0x%x" %
              (p["record_file_offset"], p["file_end"], p["bss_bytes"], p["p_align"]))

    loads = [p for p in phdrs if p["p_type"] == 1]
    print("")
    print("PT_LOAD count: %d" % len(loads))
    for a, b in zip(loads, loads[1:]):
        gap = b["p_vaddr"] - a["vaddr_end"]
        gap_page = b["p_vaddr"] - a["vaddr_end_page_up_16k"]
        print("gap LOAD%d->LOAD%d: raw 0x%x bytes [%#x,%#x); after 16k page-up of prior end: 0x%x"
              % (a["index"], b["index"], gap, a["vaddr_end"], b["p_vaddr"], gap_page))

    if out_json:
        with open(out_json, "w") as fh:
            json.dump({"header": hdr, "phdrs": phdrs}, fh, indent=2)
        print("\nwrote %s" % out_json)


if __name__ == "__main__":
    main()
