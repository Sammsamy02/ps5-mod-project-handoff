#!/usr/bin/env python3
"""stage 4 — RELA-based vtable reconstruction + ctor/factory/store chain +
corrected FMemory-thunk shape census.

F1: parse .rela.dyn (DT_RELA 0x9653430, file 0x932F430, 1,661,599 entries);
    relative relocs reconstruct absolute pointers that are NOT stored in the
    file image (why stage-3 raw scan saw almost no vtables).
F2: group consecutive relocs (step 8, >=12 entries, addend in code range)
    -> relocated vtable-like arrays in RELRO (phdr1/phdr3).
F3: match FMallocBinned3 method roots (from stage-3 anchors) as entries.
F4: lea-to-vtable sites -> ctor; e8 calls to ctor -> post-call `mov [rip+B],rax`
    -> B = GMalloc candidate (BSS, 8-aligned).
F5: independent shape census: mov R,[rip+X]; mov D,[R]; (mov F,[D+disp])?
    call/jmp [D+disp] | call/jmp F.
"""
import hashlib, json, mmap, re, struct
from collections import Counter, defaultdict

EB = "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin"
OUT = "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-gmalloc-derivation/gmalloc_stage4.json"
SHA = "3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc"
BASE = 0x400000
RM = b"\x05\x0d\x15\x1d\x25\x2d\x35\x3d"
CODE_END = 0x7479b1c
RELA_OFF, RELA_SZ, RELA_N = 0x932F430, 0x2607ee8, 1661599
P7_LO, P7_HI = 0x93085f2, 0x9633418
P3_LO, P3_HI = 0x813a4e8, 0x813c000
RELRO1 = (0x747c000, 0x747c000 + 0xa3e918)
RELRO3 = (0x7ebc000, 0x7ebc000 + 0x127e4e8)
ANCHOR_ROOTS = {0xb2440, 0xb2160, 0xaf4e0, 0xb1440}  # stage-3 FMallocBinned3 fns

def i32(b): return struct.unpack("<i", b)[0]

def main():
    f = open(EB, "rb"); mm = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
    assert hashlib.sha256(mm).hexdigest() == SHA, "sha mismatch"
    R = {"sha256_ok": True}
    CODE = mm[0x4000:0x4000 + CODE_END]

    from capstone import Cs, CS_ARCH_X86, CS_MODE_64, CS_OP_MEM, CS_OP_REG, CS_OP_IMM, x86
    md = Cs(CS_ARCH_X86, CS_MODE_64); md.detail = True

    # ---- F1: parse RELA ------------------------------------------------------
    ent = []
    it = struct.iter_unpack("<QQq", mm[RELA_OFF:RELA_OFF + RELA_SZ])
    n = 0; n_rel = 0; types = Counter()
    for r_off, r_info, r_add in it:
        n += 1
        t = r_info & 0xffffffff
        types[t] += 1
        if t in (0xdead, 8, 1):  # PS5 RELATIVE / R_X86_64_64-ish; record all with code addends
            pass
        if RELRO1[0] <= r_off < RELRO1[1] or RELRO3[0] <= r_off < RELRO3[1]:
            ent.append((r_off, r_add, t))
            n_rel += 1
    R["rela"] = {"total": n, "types": {hex(k): v for k, v in types.most_common(8)},
                 "in_relro": n_rel}

    # ---- F2: group consecutive into arrays -----------------------------------
    ent.sort()
    arrays = []
    cur = [ent[0]] if ent else []
    for e in ent[1:]:
        if e[0] == cur[-1][0] + 8:
            cur.append(e)
        else:
            if len(cur) >= 12: arrays.append(cur)
            cur = [e]
    if len(cur) >= 12: arrays.append(cur)
    code_arrays = []
    for a in arrays:
        codeish = sum(1 for _, ad, _ in a if 0 < ad < CODE_END)
        if codeish >= 8:
            code_arrays.append(a)
    R["relro_arrays"] = {"runs": len(arrays), "codeish_runs": len(code_arrays),
                         "top_sizes": sorted((len(a) for a in code_arrays), reverse=True)[:12]}

    # ---- F3: match FMallocBinned3 roots --------------------------------------
    hit_arrays = []
    for a in code_arrays:
        offs = [o for o, _, _ in a]
        addends = [ad for _, ad, _ in a]
        for i, ad in enumerate(addends):
            if any(r <= ad <= r + 0x200 for r in ANCHOR_ROOTS):
                hit_arrays.append((a, i, ad))
                break
    R["anchor_array_hits"] = []
    for a, i, ad in hit_arrays:
        base = a[0][0]
        R["anchor_array_hits"].append({
            "vtable_linked_base": hex(base), "runtime_base": hex(base + BASE),
            "n_entries": len(a), "match_idx": i, "match_addend": hex(ad),
            "entries": [hex(x) for x, _, _ in a[:40]],
            "addends": [hex(x) for _, x, _ in a[:48]]})
    print("== F2 arrays:", R["relro_arrays"])
    for h in R["anchor_array_hits"]:
        print("== F3 hit:", h["vtable_linked_base"], "n", h["n_entries"],
              "idx", h["match_idx"], "addend", h["match_addend"])
        print("   addends:", h["addends"])

    # ---- F4: lea-to-vtable -> ctor -> calls -> store --------------------------
    target_vts = [int(h["vtable_linked_base"], 16) for h in R["anchor_array_hits"]]
    vtset = set(target_vts)
    pat_lea = re.compile(b"[\x48-\x4f]\x8d([" + RM + b"])")
    lea_sites = defaultdict(list)
    for m in pat_lea.finditer(CODE):
        s = m.start()
        t = s + 7 + i32(CODE[s + 3:s + 7])
        if t in vtset:
            lea_sites[t].append(s)

    def fn_root(site):
        j = mm.rfind(b"\xcc\xcc\xcc\xcc", max(0, site - 0x3000 + 0x4000), site + 0x4000)
        k = mm.rfind(b"\x90\x90\x90\x90", max(0, site - 0x3000 + 0x4000), site + 0x4000)
        m = max(j, k)
        return (m + 4 - 0x4000) if m != -1 else None

    ctor_info = {}
    for vt, sites in lea_sites.items():
        for s in sites:
            ins = next(md.disasm(bytes(CODE[s:s + 24]), s), None)
            if ins is None: continue
            root = fn_root(s)
            key = hex(vt)
            ctor_info.setdefault(key, []).append(
                {"lea_site": hex(s), "root": hex(root) if root else None,
                 "decode": f"{ins.mnemonic} {ins.op_str}"})
    R["vtable_lea_sites"] = ctor_info
    print("== F4 lea-to-vtable:", json.dumps(ctor_info, indent=1)[:1500])

    # find e8 calls to each candidate ctor root; then store-scan post-call
    ctor_addrs = sorted({int(v["root"], 16) for sites in ctor_info.values()
                         for v in sites if v["root"]})
    R["ctor_candidates"] = [hex(c) for c in ctor_addrs]
    print("== ctor candidates:", R["ctor_candidates"])

    stores = defaultdict(list)
    if ctor_addrs:
        cset = set(ctor_addrs)
        # e8 rel32 scan
        pos = CODE.find(b"\xe8")
        idxs = []
        start = 0
        while True:
            i = CODE.find(b"\xe8", start)
            if i == -1: break
            t = i + 5 + i32(CODE[i + 1:i + 5])
            if t in cset and 0 <= t < CODE_END:
                idxs.append(i)
            start = i + 1
        R["calls_to_ctors"] = {"n": len(idxs), "sites": [hex(x) for x in idxs[:24]]}
        print("== calls to ctors:", R["calls_to_ctors"]["n"])
        for ci in idxs[:64]:
            win = list(md.disasm(bytes(CODE[ci + 5: ci + 5 + 0x140]), ci + 5))
            src = "rax"
            for k, j in enumerate(win[:24]):
                if j.mnemonic == "mov" and len(j.operands) == 2 and \
                   j.operands[0].type == CS_OP_MEM and j.operands[1].type == CS_OP_REG and \
                   j.operands[0].mem.base == x86.X86_REG_RIP and \
                   md.reg_name(j.operands[1].reg) == src:
                    t = j.address + j.size + j.operands[0].mem.disp
                    if (P7_LO <= t < P7_HI or P3_LO <= t < P3_HI) and t % 8 == 0:
                        stores[t].append(hex(ci))
                    break
                if j.mnemonic == "mov" and len(j.operands) == 2 and \
                   j.operands[0].type == CS_OP_REG and j.operands[1].type == CS_OP_REG and \
                   md.reg_name(j.operands[1].reg) == src:
                    src = md.reg_name(j.operands[0].reg)
    R["post_ctor_stores"] = {hex(t): ss for t, ss in sorted(stores.items())}
    print("== post-ctor stores to BSS:", json.dumps(R["post_ctor_stores"], indent=1)[:1200])

    json.dump(R, open(OUT, "w"), indent=1)

if __name__ == "__main__":
    main()
