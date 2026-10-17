#!/usr/bin/env python3
"""stage 2 — shape census + FMallocBinned3 anchor chain.

D1: for every rip-relative `mov rR,[rip+BSS]` site, capstone-decode forward:
    shape = mov rR,[rip+X] ; mov rD,[rR] ; call/jmp [rD+disp] (virtual dispatch
    through an object pointer). GMalloc is the only global dereferenced this way
    with Malloc/Realloc/Free-shaped disps from MANY independent functions.
D2: UTF-16 "FMallocBinned3" anchors -> lea xrefs -> function roots
    (GetDescriptiveName / DumpAllocatorStats / ctor log strings).
D3: find code-pointer arrays (vtables) in R-- segments containing those
    function addresses -> FMallocBinned3 vtable V.
D4: lea-to-V sites -> ctor; e8 calls to ctor followed by `mov [rip+B],rax`
    -> bootstrap assignment -> B = GMalloc candidate.
Offline read-only; no console contact.
"""
import hashlib, json, mmap, re, struct, sys
from collections import Counter, defaultdict

EB = "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin"
OUT = "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-gmalloc-derivation/gmalloc_stage2.json"
SHA = "3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc"
BASE = 0x400000
RM = b"\x05\x0d\x15\x1d\x25\x2d\x35\x3d"
CODE_END = 0x7479b1c  # linked code extent
P7_LO, P7_HI = 0x93085f2, 0x9633418
P3_LO, P3_HI = 0x813a4e8, 0x813c000

def i32(b): return struct.unpack("<i", b)[0]

def seg_of(t):
    if t < 0x7479b1c: return "code"
    if 0x747c000 <= t < 0x747c000 + 0xa3e918: return "ro"
    if 0x7ebc000 <= t < 0x7ebc000 + 0x1280000: return "rw"
    if 0x9633420 <= t < 0x9633420 + 0x262cea8: return "guard"
    return "?"

def main():
    f = open(EB, "rb"); mm = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
    assert hashlib.sha256(mm).hexdigest() == SHA, "sha mismatch"
    R = {"sha256_ok": True}
    CODE = mm[0x4000:0x4000 + CODE_END]

    from capstone import Cs, CS_ARCH_X86, CS_MODE_64
    md = Cs(CS_ARCH_X86, CS_MODE_64)

    def in_bss(t): return (P7_LO <= t < P7_HI) or (P3_LO <= t < P3_HI)

    # ---- D1: shape census ----------------------------------------------------
    pat_load = re.compile(b"[\x48-\x4f]\x8b([" + RM + b"])")
    shape_hist = Counter()
    disp_hist = defaultdict(Counter)   # target -> Counter of virtual disp
    shape_sites = defaultdict(list)
    n_sites = 0
    for m in pat_load.finditer(CODE):
        s = m.start()
        t = s + 7 + i32(CODE[s + 3:s + 7])
        if not (in_bss(t) and t % 8 == 0):
            continue
        n_sites += 1
        win = bytes(CODE[s:s + 48])
        ins = list(md.disasm(win, s))
        if not ins or ins[0].mnemonic != "mov" or "rip" not in ins[0].op_str:
            continue  # regex hit was mid-instruction noise
        # loaded register = ins[0] dst
        dst0 = ins[0].op_str.split(",")[0].strip()
        ok_vt = False; vt_reg = None
        for k in range(1, min(5, len(ins))):
            o = ins[k].op_str
            mm_ = o.split(",")[1].strip() if "," in o else ""
            if ins[k].mnemonic == "mov" and mm_.startswith("[") and dst0.lstrip("er").strip() in mm_:
                # crude: mem operand contains the register name
                if dst0.replace("r", "r") in mm_:
                    ok_vt = True; vt_reg = o.split(",")[0].strip(); break
        if not ok_vt:
            continue
        for k2 in range(k + 1, min(k + 4, len(ins))):
            mn = ins[k2].mnemonic; o = ins[k2].op_str
            if mn in ("call", "jmp") and o.startswith("qword ptr ["):
                im = re.search(r"\[r[a-z0-9]+ \+ (0x[0-9a-f]+)\]", o)
                if im and vt_reg and vt_reg in o:
                    d = int(im.group(1), 16)
                    shape_hist[t] += 1
                    disp_hist[t][d] += 1
                    if len(shape_sites[t]) < 12:
                        shape_sites[t].append(hex(s))
                break
    R["shape_census"] = {
        "bss_load_sites_decoded": n_sites,
        "targets_with_vtable_shape": len(shape_hist),
        "top": [{"linked": hex(t), "runtime": hex(t + BASE), "shape_hits": c,
                 "disps": {hex(d): n for d, n in disp_hist[t].most_common(6)},
                 "sites": shape_sites[t]}
                for t, c in shape_hist.most_common(20)],
    }

    # ---- D2: FMallocBinned3 string anchors -> lea xrefs ----------------------
    str_hits = {}
    for probe in ["FMallocBinned3", "MallocBinned2", "FMallocBinned2", "FMallocAnsi", "FMallocTBB"]:
        for mo in re.finditer(re.escape(probe.encode("utf-16-le")), mm):
            off = mo.start()
            # map file off -> linked vaddr (LOAD1/LOAD3 delta 0x4000)
            if 0x7480000 <= off < 0x7480000 + 0xa3e918: linked = off - 0x4000
            elif 0x7ec0000 <= off < 0x7ec0000 + 0x127e4e8: linked = off - 0x4000
            else: continue
            str_hits.setdefault(probe, set()).add(linked)
    pat_lea = re.compile(b"[\x48-\x4f]\x8d([" + RM + b"])")
    lea_x = defaultdict(list)
    wanted = {}
    for probe, addrs in str_hits.items():
        for a in addrs:
            for w in range(a - 0x40, a + 2):
                wanted[w] = (probe, a)
    for m in pat_lea.finditer(CODE):
        s = m.start()
        t = s + 7 + i32(CODE[s + 3:s + 7])
        if t in wanted:
            probe, a = wanted[t]
            lea_x[(probe, a)].append(s)
    R["allocator_string_anchors"] = {
        f"{p}@{hex(a)}": {"lea_sites": [hex(s) for s in sites[:12]], "n": len(sites)}
        for (p, a), sites in sorted(lea_x.items())}

    # function roots for lea sites (cc-run method, same as shipped game_loop route)
    def root(site):
        i = mm.rfind(b"\xcc\xcc\xcc\xcc", max(0, site + 0x4000 - 0x4000) and max(0, site - 0x2000 + 0x4000), site + 0x4000)
        return (i + 4 - 0x4000) if i != -1 else None
    roots = {}
    for (p, a), sites in lea_x.items():
        roots[(p, a)] = sorted({root(s) for s in sites if root(s)})
    R["anchor_function_roots"] = {f"{p}@{hex(a)}": [hex(r) if r else None for r in rs]
                                  for (p, a), rs in sorted(roots.items())}

    # ---- D3: vtable search in R-- segments -----------------------------------
    def find_qword_in_ro(val):
        patb = struct.pack("<Q", val)
        out = []
        for lo, hi in ((0x7480000, 0x7480000 + 0xa3e918), (0x7ec0000, 0x7ec0000 + 0x127e4e8)):
            start = lo
            while True:
                i = mm.find(patb, start, hi)
                if i == -1: break
                out.append(i)
                start = i + 1
        return out

    def vtable_profile(off):
        """count consecutive code-range qwords around a file offset"""
        n_before = 0
        i = off - 8
        while i >= 0 and struct.unpack_from("<Q", mm, i)[0] < CODE_END and struct.unpack_from("<Q", mm, i)[0] != 0:
            n_before += 1; i -= 8
        n_after = 0
        i = off + 8
        while i < len(mm) - 8:
            v = struct.unpack_from("<Q", mm, i)[0]
            if v >= CODE_END or v == 0: break
            n_after += 1; i += 8
        return n_before, n_after

    vt_report = {}
    all_roots = {r for rs in roots.values() for r in rs if r}
    for r in sorted(all_roots):
        hits = find_qword_in_ro(r)
        for h in hits[:6]:
            nb, na = vtable_profile(h)
            linked = h - 0x4000
            vt_report[f"{hex(r)}@file{hex(h)}"] = {
                "linked_slot": hex(linked), "runtime_slot": hex(linked + BASE),
                "code_qwords_before": nb, "code_qwords_after": na,
                "in": seg_of(linked)}
    R["vtable_slot_search"] = vt_report

    json.dump(R, open(OUT, "w"), indent=1)
    print("== shape census: decoded", n_sites, "bss load sites;",
          len(shape_hist), "targets with vtable shape")
    for e in R["shape_census"]["top"][:12]:
        print("   ", e)
    print("== anchors:", json.dumps(R["allocator_string_anchors"], indent=0)[:1500])
    print("== roots:", json.dumps(R["anchor_function_roots"])[:900])
    print("== vtable slots:")
    for k, v in list(vt_report.items())[:24]:
        print("   ", k, v)

if __name__ == "__main__":
    main()
