#!/usr/bin/env python3
"""stage 3 — corrected shape census (capstone operands), full-file vtable
enumeration, anchor-string context dumps, improved function-root heuristic.

E1: shape census via ins.operands: mov R,[rip+X]; mov D,[R]; call/jmp [D+disp].
E2: enumerate vtable-like arrays (runs of >=12 qwords in code range) across
    phdr1+phdr3 file images; index them; fuzzy-match anchor roots.
E3: UTF-16 context of each FMallocBinned3 anchor.
E4: function roots via cc OR nop-padding runs.
"""
import hashlib, json, mmap, re, struct
from collections import Counter, defaultdict

EB = "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin"
OUT = "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-gmalloc-derivation/gmalloc_stage3.json"
SHA = "3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc"
BASE = 0x400000
RM = b"\x05\x0d\x15\x1d\x25\x2d\x35\x3d"
CODE_END = 0x7479b1c
P7_LO, P7_HI = 0x93085f2, 0x9633418
P3_LO, P3_HI = 0x813a4e8, 0x813c000

def i32(b): return struct.unpack("<i", b)[0]

def main():
    f = open(EB, "rb"); mm = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
    assert hashlib.sha256(mm).hexdigest() == SHA, "sha mismatch"
    R = {"sha256_ok": True}
    CODE = mm[0x4000:0x4000 + CODE_END]

    from capstone import Cs, CS_ARCH_X86, CS_MODE_64, CS_OP_MEM, CS_OP_REG, x86
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = True

    def in_bss(t): return (P7_LO <= t < P7_HI) or (P3_LO <= t < P3_HI)

    def reg_name(op):
        return md.reg_name(op.reg) if op.type == CS_OP_REG else None

    # ---- E1: shape census ----------------------------------------------------
    pat_load = re.compile(b"[\x48-\x4f]\x8b([" + RM + b"])")
    shape_hist = Counter()
    disp_hist = defaultdict(Counter)
    shape_sites = defaultdict(list)
    n_sites = 0
    for m in pat_load.finditer(CODE):
        s = m.start()
        t = s + 7 + i32(CODE[s + 3:s + 7])
        if not (in_bss(t) and t % 8 == 0):
            continue
        n_sites += 1
        ins = next(md.disasm(bytes(CODE[s:s + 16]), s), None)
        if ins is None or ins.mnemonic != "mov" or len(ins.operands) != 2:
            continue
        op0, op1 = ins.operands
        if op1.type != CS_OP_MEM or op0.type != CS_OP_REG or op1.mem.base != x86.X86_REG_RIP:
            continue
        if ins.size + ins.address != t:  # real instruction must target exactly t
            continue
        r0 = md.reg_name(op0.reg)
        # vtable load: mov D,[r0] within next 4 instructions
        ins2 = list(md.disasm(bytes(CODE[s + ins.size:s + ins.size + 40]), s + ins.size))
        vt_reg = None; vt_idx = None
        for k, j in enumerate(ins2[:4]):
            if j.mnemonic == "mov" and len(j.operands) == 2 and \
               j.operands[1].type == CS_OP_MEM and \
               j.operands[1].mem.base != x86.X86_REG_RIP and \
               j.operands[0].type == CS_OP_REG:
                if md.reg_name(j.operands[1].mem.base) == r0:
                    vt_reg = md.reg_name(j.operands[0].reg); vt_idx = k; break
        if vt_reg is None:
            continue
        for j in ins2[vt_idx + 1: vt_idx + 4]:
            if j.mnemonic in ("call", "jmp") and len(j.operands) == 1 and \
               j.operands[0].type == CS_OP_MEM:
                mem = j.operands[0].mem
                if md.reg_name(mem.base) == vt_reg and mem.index == 0:
                    shape_hist[t] += 1
                    disp_hist[t][mem.disp] += 1
                    if len(shape_sites[t]) < 12:
                        shape_sites[t].append(hex(s))
                break
    R["shape_census"] = {
        "bss_load_sites_decoded": n_sites,
        "targets_with_shape": len(shape_hist),
        "top": [{"linked": hex(t), "runtime": hex(t + BASE), "shape_hits": c,
                 "disps": {hex(d): n for d, n in disp_hist[t].most_common(8)},
                 "sites": shape_sites[t]}
                for t, c in shape_hist.most_common(16)],
    }

    # ---- E3: anchor string context ------------------------------------------
    ctx = {}
    for linked in (0x765b45a, 0x77d1b8c, 0x78788da, 0x78f7c14):
        off = linked + 0x4000
        raw = mm[off - 0x60: off + 0x80]
        try:
            s = raw.decode("utf-16-le", "replace")
        except Exception:
            s = repr(raw)
        ctx[hex(linked)] = s.replace("\x00", "·")
    R["anchor_contexts"] = ctx

    # ---- E4: improved function root ------------------------------------------
    PAD = re.compile(rb"[\xcc\x90]{4}|(?:\xcc{2,}|\x90{2,})\x0f\x1f")
    def root(site_linked):
        lo = max(0, site_linked - 0x3000)
        best = None
        i = site_linked + 0x4000
        start = lo + 0x4000
        # find last padding run (>=4 bytes of cc or 90) before site
        j = mm.rfind(b"\xcc\xcc\xcc\xcc", start, i)
        k = mm.rfind(b"\x90\x90\x90\x90", start, i)
        m = max(j, k)
        if m != -1:
            return m + 4 - 0x4000
        return None

    # ---- E2: vtable enumeration over data segments ---------------------------
    vt_arrays = []   # (file_off_of_first_entry, count)
    for lo, hi in ((0x7480000, 0x7480000 + 0xa3e918), (0x7ec0000, 0x7ec0000 + 0x127e4e8)):
        i = lo
        run = 0
        while i + 8 <= hi:
            v = struct.unpack_from("<Q", mm, i)[0]
            if v < CODE_END and v != 0:
                run += 1
            else:
                if run >= 12:
                    vt_arrays.append((i - run * 8, run))
                run = 0
            i += 8
    R["vtable_like_arrays"] = {"count": len(vt_arrays),
                               "max_run": max((c for _, c in vt_arrays), default=0)}

    anchors = {}
    for linked in (0x765b45a, 0x77d1b8c, 0x78788da, 0x78f7c14):
        pass
    # find lea xrefs to anchors, roots, then match roots inside vtable arrays
    pat_lea = re.compile(b"[\x48-\x4f]\x8d([" + RM + b"])")
    anchor_sites = defaultdict(list)
    anchor_set = {0x765b45a, 0x77d1b8c, 0x78788da, 0x78f7c14}
    for m in pat_lea.finditer(CODE):
        s = m.start()
        t = s + 7 + i32(CODE[s + 3:s + 7])
        if t in anchor_set:
            anchor_sites[t].append(s)
    anchor_roots = {}
    for a, sites in anchor_sites.items():
        anchor_roots[a] = sorted({root(s) for s in sites if root(s) is not None})
    R["anchor_lea_and_roots"] = {
        hex(a): {"n_sites": len(ss), "sites": [hex(x) for x in ss[:8]],
                 "roots": [hex(r) for r in rs]}
        for a, ss, rs in ((a, anchor_sites[a], anchor_roots[a]) for a in sorted(anchor_sites))}

    # match roots inside vtable arrays (exact + fuzzy +0x100 window)
    vt_by_entry = defaultdict(list)  # entry linked addr -> (array_file_off, idx)
    arr_info = []
    for aoff, cnt in vt_arrays:
        base_linked = aoff - 0x4000
        arr_info.append({"vtable_linked_base": hex(base_linked),
                         "runtime_base": hex(base_linked + BASE), "entries": cnt})
        for idx in range(cnt):
            e = struct.unpack_from("<Q", mm, aoff + idx * 8)[0]
            vt_by_entry[e].append((aoff, idx))
    R["vtable_arrays_sample"] = arr_info[:12]

    vt_match = {}
    for a, rs in anchor_roots.items():
        for r in rs:
            # exact
            if r in vt_by_entry:
                for aoff, idx in vt_by_entry[r]:
                    vt_match[f"{hex(r)}"] = {"array_base": hex(aoff - 0x4000),
                                             "runtime_base": hex(aoff - 0x4000 + BASE),
                                             "idx": idx}
            # fuzzy: entries in [r, r+0x100]
            for e, lst in vt_by_entry.items():
                if r < e <= r + 0x100:
                    for aoff, idx in lst:
                        vt_match[f"{hex(r)}~{hex(e)}"] = {"array_base": hex(aoff - 0x4000),
                                                          "runtime_base": hex(aoff - 0x4000 + BASE),
                                                          "idx": idx, "delta": e - r}
    R["vtable_matches"] = vt_match

    json.dump(R, open(OUT, "w"), indent=1)
    print("== shape census:", n_sites, "sites decoded,", len(shape_hist), "targets")
    for e in R["shape_census"]["top"][:14]:
        print("   ", e)
    print("== anchor contexts:")
    for k, v in ctx.items():
        print("   ", k, ":", v[:180])
    print("== anchors/roots:", json.dumps(R["anchor_lea_and_roots"])[:1200])
    print("== vtable arrays:", R["vtable_like_arrays"])
    print("== vtable matches:", json.dumps(vt_match, indent=1)[:2000])

if __name__ == "__main__":
    main()
