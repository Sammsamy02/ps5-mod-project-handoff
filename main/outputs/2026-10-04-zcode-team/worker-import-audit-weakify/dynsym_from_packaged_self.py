#!/usr/bin/env python3
"""dynsym_from_packaged_self.py -- worker-import-audit-weakify read-only verifier.

Enumerates the dynamic import surface of a fake-SELF packaged PRX (magic
4F 15 3D 1D, ELF64 payload at offset 0x1A0, section headers stripped) by
walking PT_DYNAMIC -> DT_SYMTAB/DT_STRTAB/DT_HASH. Pure stdlib, read-only on
the input; the only output is JSON on stdout plus an optional --json FILE
written under this worker's own directory.

Mirrors the classification rules of worker-import-fallback/import_fallback.py
(same ELF64 layouts, same STB_GLOBAL=1/STB_WEAK=2, same long-form dynstr
"<nid>#<lib>#<mod>" decoding) so the two audits are directly comparable.
"""
import argparse
import importlib.util
import json
import struct
import sys
from pathlib import Path

EHDR = struct.Struct("<16sHHIQQQIHHHHHH")
PHDR = struct.Struct("<IIQQQQQQ")
DYN = struct.Struct("<QQ")
SYM = struct.Struct("<IBBHQQ")

DT_NULL = 0
DT_STRTAB = 5
DT_SYMTAB = 6
DT_STRSZ = 10
DT_SYMENT = 11
DT_HASH = 4
DT_GNU_HASH = 0x6FFFFEF5

SELF_ELF_OFF = 416


def load_spec():
    here = Path(__file__).resolve().parent
    tool = here.parent / "worker-import-fallback" / "import_fallback.py"
    spec = importlib.util.spec_from_file_location("ifb", tool)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--elf", required=True, help="fake-SELF packaged prx")
    ap.add_argument("--json", help="copy output to this file")
    args = ap.parse_args(argv)

    ifb = load_spec()
    data = Path(args.elf).read_bytes()
    if data[:4] == b"\x7fELF":
        base = 0
    elif data[:4] == b"\x4f\x15\x3d\x1d" and data[SELF_ELF_OFF:SELF_ELF_OFF + 4] == b"\x7fELF":
        base = SELF_ELF_OFF
    else:
        print("not a fake-SELF nor raw ELF", file=sys.stderr)
        return 2

    (_ident, e_type, _mach, _v, _entry, phoff, _shoff, _flags, _eh,
     phentsize, phnum, _shent, _shnum, _shstr) = EHDR.unpack_from(data, base)
    assert phentsize == PHDR.size, phentsize

    loads, dynp = [], None
    for i in range(phnum):
        (p_type, p_flags, p_off, p_vaddr, _paddr, p_filesz, _memsz, _align) = \
            PHDR.unpack_from(data, base + phoff + i * PHDR.size)
        if p_type == 1:
            loads.append((p_vaddr, p_off, p_filesz))
        elif p_type == 2:
            dynp = (p_off, p_filesz)
    if dynp is None:
        print("no PT_DYNAMIC", file=sys.stderr)
        return 2

    def v2f(vaddr):
        for v, o, sz in loads:
            if v <= vaddr < v + sz:
                return o + (vaddr - v)
        raise ValueError(f"vaddr {vaddr:#x} not in any PT_LOAD")

    dt = {}
    off, end = dynp[0] + base, dynp[0] + base + dynp[1]
    while off + DYN.size <= end:
        tag, val = DYN.unpack_from(data, off)
        off += DYN.size
        if tag == DT_NULL:
            break
        dt.setdefault(tag, val)
    symtab, strtab = v2f(dt[DT_SYMTAB]), v2f(dt[DT_STRTAB])
    strsz, syment = dt[DT_STRSZ], dt.get(DT_SYMENT, SYM.size)
    assert syment == SYM.size, syment
    strblob = data[strtab:strtab + strsz]

    # symbol count: SysV DT_HASH nchain, else GNU_HASH max index+1
    if DT_HASH in dt:
        h = v2f(dt[DT_HASH])
        _nbucket, nchain = struct.unpack_from("<II", data, h)
        nsym = nchain
        hash_src = f"DT_HASH nchain={nchain}"
    else:
        g = v2f(dt[DT_GNU_HASH])
        nbucket, symoffset, bloom_size, bloom_shift = struct.unpack_from("<IIII", data, g)
        buckets_off = g + 16 + bloom_size * 8
        buckets = struct.unpack_from(f"<{nbucket}I", data, buckets_off)
        chain_off = buckets_off + nbucket * 4
        last = max(buckets) if buckets else symoffset
        if last:
            while True:
                ch = struct.unpack_from("<I", data, chain_off + (last - symoffset) * 4)[0]
                if ch & 1:
                    break
                last += 1
        nsym = last + 1
        hash_src = f"DT_GNU_HASH max_index={last}"

    imports = []
    for i in range(nsym):
        so = symtab + i * SYM.size
        st_name, st_info, _other, st_shndx, _value, _size = SYM.unpack_from(data, so)
        if st_shndx != 0 or st_name == 0:
            continue
        bind = st_info >> 4
        if bind not in (1, 2):
            continue
        text = ifb.cstr(strblob, st_name)
        imports.append({"symbol_index": i, "dynstr": text,
                        "identity": ifb.decode_name(text, False),
                        "bind": "GLOBAL" if bind == 1 else "WEAK",
                        "file_offset": so})

    out = {"elf": args.elf, "self_base": base, "e_type": e_type,
           "nsym_src": hash_src, "nsym_total": nsym,
           "imports": imports,
           "strong": sum(1 for x in imports if x["bind"] == "GLOBAL"),
           "weak": sum(1 for x in imports if x["bind"] == "WEAK")}
    text = json.dumps(out, indent=1)
    if args.json:
        Path(args.json).write_text(text + "\n")
    print(text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
