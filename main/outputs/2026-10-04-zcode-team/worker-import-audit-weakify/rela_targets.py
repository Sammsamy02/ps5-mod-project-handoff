#!/usr/bin/env python3
"""rela_targets.py -- which relocation records bind the 5 weakify targets.

Read-only analysis of the composition-4 bare-sdk ELF (sections present).
For every SHT_RELA section, classifies R_X86_64 types and lists any record
whose symbol index belongs to one of the five target dynsym rows.
"""
import importlib.util
import struct
from collections import Counter, defaultdict
from pathlib import Path

EHDR = struct.Struct("<16sHHIQQQIHHHHHH")
SHDR = struct.Struct("<IIQQQQIIQQ")
SYM = struct.Struct("<IBBHQQ")
RELA = struct.Struct("<QQq")

RTYPE = {1: "R_X86_64_64", 2: "PC32", 4: "PLT32", 5: "PC32(copy?)",
         6: "GLOB_DAT", 7: "JUMP_SLOT", 8: "RELATIVE"}

BARE = ("outputs/2026-10-04-zcode-team/worker-composition-4-fix/"
        "prx-candidate-composition4-1/bare-sdk/modbridge_supplier_composition.prx")

spec = importlib.util.spec_from_file_location(
    "ifb", "outputs/2026-10-04-zcode-team/worker-import-fallback/import_fallback.py")
ifb = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ifb)

data = Path(BARE).read_bytes()
(_i, _t, _m, _v, _e, phoff, shoff, _f, _eh, _phe, phnum,
 shent, shnum, shstrndx) = EHDR.unpack_from(data, 0)
secs = []
for k in range(shnum):
    o = shoff + k * SHDR.size
    (nm, typ, fl, addr, off, size, link, info, align, ent) = SHDR.unpack_from(data, o)
    secs.append(dict(idx=k, name=nm, type=typ, addr=addr, off=off, size=size,
                     link=link, info=info, ent=ent))

# resolve section names
shstr = secs[shstrndx]
def sname(nm):
    end = data.index(b"\x00", shstr["off"] + nm)
    start = shstr["off"] + nm
    return data[start:end].decode("utf-8", "replace")
for s in secs:
    s["sname"] = sname(s["name"])

dynsym = [s for s in secs if s["type"] == 11][0]
strsec = secs[dynsym["link"]]
strblob = data[strsec["off"]:strsec["off"] + strsec["size"]]

t5 = {ifb.sce_nid(x): x for x in ("sceKernelDlsym", "sceKernelGetModuleInfo",
                                  "sceKernelLoadStartModule",
                                  "sceKernelStopUnloadModule", "qsort")}
t5idx = {}
nsym = dynsym["size"] // 24
for i in range(nsym):
    st_name, st_info, st_shndx, = SYM.unpack_from(data, dynsym["off"] + i * 24)[:3]
    if st_shndx == 0 and st_name:
        ident = ifb.decode_name(ifb.cstr(strblob, st_name), False)
        if ident in t5:
            t5idx[i] = t5[ident]
print("target dynsym indices:", t5idx)

want = set(t5idx)
for s in secs:
    if s["type"] != 4:  # SHT_RELA
        continue
    cnt = Counter()
    targets = []
    m = s["size"] // 24
    for k in range(m):
        r_off, r_info, r_add = RELA.unpack_from(data, s["off"] + k * 24)
        symidx = r_info >> 32
        rtype = r_info & 0xFFFFFFFF
        cnt[rtype] += 1
        if symidx in want:
            targets.append((t5idx[symidx], RTYPE.get(rtype, str(rtype)), hex(r_off),
                            hex(r_add)))
    print(f"section [{s['idx']}] {s['sname']}: entries={m} "
          f"types={ {RTYPE.get(t, hex(t)): c for t, c in cnt.items()} }")
    for row in targets:
        print("   TARGET:", row)
