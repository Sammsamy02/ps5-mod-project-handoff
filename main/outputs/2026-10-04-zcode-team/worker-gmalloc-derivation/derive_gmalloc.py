#!/usr/bin/env python3
"""worker-gmalloc-derivation — stage 1: rip-relative reference histogram over BSS.

NEW ROUTES vs worker-seed-table-derivation (whose slot-3 anchors were exhausted):
 R1 (histogram): every FMemory::Malloc/Free/Realloc thunk and engine allocator call
     site dereferences GMalloc rip-relatively: `mov rR,[rip+GMalloc]` then
     `mov rD,[rR]` (vtable) then `call/jmp [rD+disp]`. No shipped route histogrammed
     BSS-slot reference POPULATION to find the global by its usage shape.
 R2 (stores): GMalloc is assigned exactly once/twice in engine init
     (`mov [rip+GMalloc], rax` after the allocator factory call).
 R3 (adjacency): globals cluster; compare against the proven GUA slot 0x93ba110.
Offline read-only against the sha-pinned retained image. No console contact.
"""
import hashlib, json, mmap, re, struct, sys
from collections import Counter

EB = "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin"
OUT = "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-gmalloc-derivation/gmalloc_stage1.json"
SHA = "3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc"
BASE = 0x400000
GUA_LINKED = 0x93BA110

RM = b"\x05\x0d\x15\x1d\x25\x2d\x35\x3d"  # mod=00 rm=101 rip-relative

def i32(b):
    return struct.unpack("<i", b)[0]

def main():
    f = open(EB, "rb")
    mm = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
    assert hashlib.sha256(mm).hexdigest() == SHA, "sha mismatch"
    R = {"sha256_ok": True, "size": len(mm)}

    # phdr7 BSS + phdr3 BSS windows (from SEED-TABLE.json image record)
    P7_LO, P7_HI = 0x913c000 + 0x1cc5f2, 0x913c000 + 0x4f7418   # 0x93085f2..0x9633418
    P3_LO, P3_HI = 0x7ebc000 + 0x127e4e8, 0x7ebc000 + 0x1280000 # 0x813a4e8..0x813c000
    R["bss_windows"] = {"p7": [hex(P7_LO), hex(P7_HI)], "p3": [hex(P3_LO), hex(P3_HI)]}

    def in_bss(t):
        return (P7_LO <= t < P7_HI) or (P3_LO <= t < P3_HI)

    # LOAD0: file off 0x4000 == vaddr 0 -> buffer offset == linked vaddr
    CODE = mm[0x4000:0x4000 + 0x7479b1c]

    # ---- R1/R2: rip-relative histograms --------------------------------------
    pat_load = re.compile(b"[\x48-\x4f]\x8b([" + RM + b"])")
    pat_lea = re.compile(b"[\x48-\x4f]\x8d([" + RM + b"])")
    pat_store = re.compile(b"[\x48-\x4f]\x89([" + RM + b"])")
    pat_cmp0 = re.compile(b"\x48\x83\x3d(....)\x00", re.S)  # cmp qword [rip+X],0

    def census(pat, inslen, keep_null=True):
        hist = Counter()
        sites = {}
        for m in pat.finditer(CODE):
            s = m.start()
            t = s + inslen + i32(CODE[s + inslen - 4:s + inslen])
            if in_bss(t) and t % 8 == 0:
                hist[t] += 1
                sites.setdefault(t, []).append(s)
        return hist, sites

    h_load, s_load = census(pat_load, 7)
    h_lea, s_lea = census(pat_lea, 7)
    h_store, s_store = census(pat_store, 7)
    h_cmp0, s_cmp0 = census(pat_cmp0, 8)

    R["methodology_sanity_gua"] = {
        "linked": hex(GUA_LINKED),
        "load_refs": h_load.get(GUA_LINKED, 0),
        "lea_refs": h_lea.get(GUA_LINKED, 0),
        "store_refs": h_store.get(GUA_LINKED, 0),
        "cmp0_refs": h_cmp0.get(GUA_LINKED, 0),
    }
    R["bss_distinct_slots_touched"] = {
        "load": len(h_load), "lea": len(h_lea), "store": len(h_store), "cmp0": len(h_cmp0)}

    # GUA adjacency: all BSS slots with load-refs, sorted by address
    addr_sorted = sorted((a, n) for a, n in h_load.items() if n >= 2)
    near = [(hex(a), n, a - GUA_LINKED) for a, n in addr_sorted
            if abs(a - GUA_LINKED) <= 0x1000]
    R["gua_adjacent_load_refed_slots_pm4k"] = near

    top = [a for a, _ in h_load.most_common(60)]
    R["top_load_refed_bss"] = [
        {"linked": hex(a), "runtime": hex(a + BASE), "load_refs": h_load[a],
         "store_refs": h_store.get(a, 0), "lea_refs": h_lea.get(a, 0),
         "cmp0_refs": h_cmp0.get(a, 0),
         "gua_delta": a - GUA_LINKED}
        for a in top]

    # ---- allocator string probes (UTF-8 + UTF-16) ----------------------------
    probes = ["FMalloc", "MallocBinned", "FMallocBinned2", "FMallocBinned3",
              "FMallocAnsi", "FMallocTBB", "FMallocMimalloc", "FMallocJemalloc",
              "FMallocStomp", "FMallocDebug", "FMallocProfiler", "FMallocPS5",
              "FPageAllocator", "FMemStack", "GMemStack", "TLsaAllocStack",
              "GMallocCreate", "MallocBinned2", "MallocBinned3", "Binned2Alloc",
              " allocator", "AllocatorStats", "GetAllocatorStats",
              "Not enough memory", "Memory Allocation", "Out of memory"]
    sp = {}
    for s in probes:
        u8 = [m.start() for m in re.finditer(re.escape(s.encode()), mm)]
        u16 = [m.start() for m in re.finditer(re.escape(s.encode("utf-16-le")), mm)]
        if u8 or u16:
            sp[s] = {"utf8_hits": len(u8), "utf16_hits": len(u16),
                     "utf8_first": [hex(x) for x in u8[:4]],
                     "utf16_first": [hex(x) for x in u16[:4]]}
    R["string_probes"] = sp

    # persist site maps for stage 2 (top candidates only, to keep the file sane)
    stage2 = {}
    for a in top:
        stage2[hex(a)] = {
            "load_sites": [hex(x) for x in sorted(s_load[a])[:96]],
            "store_sites": [hex(x) for x in sorted(s_store.get(a, []))[:16]],
            "lea_sites": [hex(x) for x in sorted(s_lea.get(a, []))[:16]],
            "cmp0_sites": [hex(x) for x in sorted(s_cmp0.get(a, []))[:16]],
        }
    json.dump({"R": R, "stage2": stage2}, open(OUT, "w"), indent=1)

    print("== gua sanity:", json.dumps(R["methodology_sanity_gua"]))
    print("== distinct slots:", json.dumps(R["bss_distinct_slots_touched"]))
    print("== top 24:")
    for e in R["top_load_refed_bss"][:24]:
        print("   ", e)
    print("== gua-adjacent:", json.dumps(near)[:1200])
    print("== string probes:")
    for k, v in sp.items():
        print("   ", k, v)

if __name__ == "__main__":
    main()
