#!/usr/bin/env python3
"""stage 5 — type-filtered vtable, widened-shape census (phdr3 data + phdr7),
ctor/call-site disassembly, store-site search.

G1: dump reloc TYPE of each slot of the 0x7ec26e0 array (which addends are real
    relative-reloc code pointers).
G2: shape census over ALL writable-data targets: mov R,[rip+X]; mov D,[R];
    mov F,[D+disp]; call/jmp [D+disp] or call/jmp F  (tail-call jmp reg accepted).
G3: rip-relative STORE census over the same window (assignment sites).
G4: disassemble ctor 0xafd40 head + both call sites for the factory context.
"""
import hashlib, json, mmap, re, struct
from collections import Counter, defaultdict

EB = "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin"
OUT = "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-gmalloc-derivation/gmalloc_stage5.json"
SHA = "3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc"
BASE = 0x400000
RM = b"\x05\x0d\x15\x1d\x25\x2d\x35\x3d"
CODE_END = 0x7479b1c
RELA_OFF, RELA_SZ = 0x932F430, 0x2607ee8
VT = 0x7ec26e0
RELSZ = 32  # array entries found in stage 4

def i32(b): return struct.unpack("<i", b)[0]

def main():
    f = open(EB, "rb"); mm = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
    assert hashlib.sha256(mm).hexdigest() == SHA, "sha mismatch"
    R = {}
    CODE = mm[0x4000:0x4000 + CODE_END]

    from capstone import Cs, CS_ARCH_X86, CS_MODE_64, CS_OP_MEM, CS_OP_REG, x86
    md = Cs(CS_ARCH_X86, CS_MODE_64); md.detail = True

    # ---- G1: reloc types for the vtable array --------------------------------
    rel = {}
    lo, hi = VT - 0x80, VT + RELSZ * 8 + 0x80
    for r_off, r_info, r_add in struct.iter_unpack("<QQq", mm[RELA_OFF:RELA_OFF + RELA_SZ]):
        if lo <= r_off < hi:
            rel[r_off] = (r_info & 0xffffffff, r_info >> 32, r_add)
    R["vt_slots"] = []
    for k in range(RELSZ + 16):
        o = VT - 0x10 + k * 8
        t, sym, add = rel.get(o, (None, None, None))
        R["vt_slots"].append({"off": hex(o), "linked": hex(o) if k >= 2 else "",
                              "type": t, "sym": sym, "addend": hex(add) if add is not None else None,
                              "file_bytes": hex(struct.unpack_from("<Q", mm, o + 0x4000 - 0x4000 + 0x4000)[0]) if False else None})
    print("== G1 vtable slots (addr, type, sym, addend):")
    for e in R["vt_slots"][:40]:
        print("   ", e["off"], "type", e["type"], "sym", e["sym"], "addend", e["addend"])

    # ---- G2/G3: widened census -----------------------------------------------
    # writable windows: phdr3 full (0x7ebc000..0x813c000), phdr7 full (0x913c000..0x9633418)
    W = [(0x7ebc000, 0x813c000), (0x913c000, 0x9633418)]
    def in_w(t):
        return any(a <= t < b for a, b in W)

    pat_load = re.compile(b"[\x48-\x4f]\x8b([" + RM + b"])")
    pat_store = re.compile(b"[\x48-\x4f]\x89([" + RM + b"])")

    shape_hist = Counter()
    disp_hist = defaultdict(Counter)
    shape_sites = defaultdict(list)
    n_sites = 0
    for m in pat_load.finditer(CODE):
        s = m.start()
        t = s + 7 + i32(CODE[s + 3:s + 7])
        if not (in_w(t) and t % 8 == 0):
            continue
        n_sites += 1
        ins = next(md.disasm(bytes(CODE[s:s + 16]), s), None)
        if ins is None or ins.mnemonic != "mov" or ins.address + ins.size != t:
            continue
        ops = ins.operands
        if len(ops) != 2 or ops[1].type != CS_OP_MEM or ops[0].type != CS_OP_REG or ops[1].mem.base != x86.X86_REG_RIP:
            continue
        r0 = md.reg_name(ops[0].reg)
        ins2 = list(md.disasm(bytes(CODE[s + ins.size:s + ins.size + 48]), s + ins.size))
        vt_reg = None; vt_idx = None
        for k, j in enumerate(ins2[:5]):
            if j.mnemonic == "mov" and len(j.operands) == 2 and \
               j.operands[1].type == CS_OP_MEM and j.operands[0].type == CS_OP_REG and \
               j.operands[1].mem.base != x86.X86_REG_RIP and \
               md.reg_name(j.operands[1].mem.base) == r0:
                vt_reg = md.reg_name(j.operands[0].reg); vt_idx = k; break
        if vt_reg is None:
            continue
        fn_reg = None; fn_disp = None
        for j in ins2[vt_idx + 1: vt_idx + 6]:
            if j.mnemonic == "mov" and len(j.operands) == 2 and \
               j.operands[1].type == CS_OP_MEM and \
               j.operands[1].mem.base != x86.X86_REG_RIP and \
               md.reg_name(j.operands[1].mem.base) == vt_reg and \
               j.operands[0].type == CS_OP_REG:
                fn_reg = md.reg_name(j.operands[0].reg); fn_disp = j.operands[1].mem.disp
                continue
            if j.mnemonic in ("call", "jmp"):
                if len(j.operands) == 1:
                    o = j.operands[0]
                    if o.type == CS_OP_MEM and md.reg_name(o.mem.base) == vt_reg and o.mem.index == 0:
                        d = o.mem.disp
                    elif o.type == CS_OP_REG and fn_reg and md.reg_name(o.reg) == fn_reg:
                        d = fn_disp
                    else:
                        break
                else:
                    break
                shape_hist[t] += 1
                disp_hist[t][d] += 1
                if len(shape_sites[t]) < 16:
                    shape_sites[t].append(hex(s))
                break

    R["census"] = {"sites": n_sites, "targets": len(shape_hist),
                   "top": [{"linked": hex(t), "runtime": hex(t + BASE), "hits": c,
                            "disps": {hex(d): n for d, n in disp_hist[t].most_common(8)},
                            "sites": shape_sites[t]}
                           for t, c in shape_hist.most_common(16)]}
    print("== G2 census:", n_sites, "sites,", len(shape_hist), "targets")
    for e in R["census"]["top"]:
        print("   ", e)

    # ---- G3: store census -----------------------------------------------------
    store_hist = Counter()
    store_sites = defaultdict(list)
    for m in pat_store.finditer(CODE):
        s = m.start()
        t = s + 7 + i32(CODE[s + 3:s + 7])
        if in_w(t) and t % 8 == 0:
            store_hist[t] += 1
            store_sites[t].append(s)
    R["store_top"] = [{"linked": hex(t), "runtime": hex(t + BASE), "stores": c,
                       "shape_hits": shape_hist.get(t, 0),
                       "sites": [hex(x) for x in sorted(store_sites[t])[:12]]}
                      for t, c in store_hist.most_common(40)
                      if shape_hist.get(t, 0) > 0 or c <= 4]
    print("== G3 stores for shape targets:")
    for e in R["store_top"]:
        print("   ", e)

    # ---- G4: ctor + call-site context ------------------------------------------
    def dump(addr, n, label):
        print(f"== G4 {label} @{hex(addr)}:")
        for j in md.disasm(bytes(CODE[addr:addr + n]), addr):
            print(f"   {j.address:#x}: {j.mnemonic} {j.op_str}")

    dump(0xafd40, 0x60, "root-of-lea(0xafdc3)")
    for cs in (0xafdc3, 0xb1076):
        print(f"== G4 lea site {hex(cs)}:")
        dump(cs - 0x20, 0x30, "around-lea")

    json.dump(R, open(OUT, "w"), indent=1)

if __name__ == "__main__":
    main()
