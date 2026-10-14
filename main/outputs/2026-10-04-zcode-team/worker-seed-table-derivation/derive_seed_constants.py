#!/usr/bin/env python3
"""worker-seed-table-derivation — offline derivation of REQUIRED-slot seed constants
against the sha-pinned retained decrypted-eboot.bin (READ-ONLY local parse).

Replicates the ps_scan ELF-route derivation logic (ue4ss-native-mapped-candidates-1
src/{guobject_array,fname,static_construct_object,gmalloc,game_loop,xrefs}.rs) with
full offline read access. runtime = linked_vaddr + 0x400000 (base proven in-game).
Outputs derivation.json + human summary. No console contact; no writes outside dir.
"""
import hashlib, json, mmap, re, struct, sys
from collections import Counter

EB = "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin"
OUT = "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-seed-table-derivation/derivation.json"
BASE = 0x400000
SHA = "3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc"

RIP_MODRM = b"\x05\x0d\x15\x1d\x25\x2d\x35\x3d"  # mod=00 rm=101 => rip-relative

def utf16le(s):
    return s.encode("utf-16-le")

def i32(b):
    return struct.unpack("<i", b)[0]

def main():
    f = open(EB, "rb")
    mm = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
    assert hashlib.sha256(mm).hexdigest() == SHA, "sha mismatch"
    R = {"eboot": EB, "sha256_ok": True, "size": len(mm)}

    # ---- 1. ELF64 parse ------------------------------------------------------
    assert mm[:4] == b"\x7fELF", "not ELF"
    ei_class, ei_data = mm[4], mm[5]
    assert ei_class == 2 and ei_data == 1, "expect ELF64 LE"
    e_type, e_machine = struct.unpack_from("<HH", mm, 16)
    e_phoff, = struct.unpack_from("<Q", mm, 32)
    e_phentsize, e_phnum = struct.unpack_from("<HH", mm, 54)
    loads = []
    for i in range(e_phnum):
        o = e_phoff + i * e_phentsize
        p_type, p_flags = struct.unpack_from("<II", mm, o)
        p_off, p_vaddr, p_paddr, p_filesz, p_memsz, p_align = struct.unpack_from("<QQQQQQ", mm, o + 8)
        if p_type == 1:  # PT_LOAD
            loads.append({"i": i, "off": p_off, "vaddr": p_vaddr, "filesz": p_filesz,
                          "memsz": p_memsz, "flags": p_flags, "align": p_align})
    R["e_type"] = e_type; R["e_machine"] = e_machine
    R["loads"] = [{k: (hex(v) if isinstance(v, int) else v) for k, v in L.items()} for L in loads]
    LN = [(L["vaddr"], L["vaddr"] + L["filesz"], L) for L in loads]
    LM = [(L["vaddr"], L["vaddr"] + L["memsz"], L) for L in loads]
    code = next(L for L in loads if (L["vaddr"], L["vaddr"] + L["filesz"]) == (0, L["filesz"]) or L["flags"] & 1)

    def off2vaddr(off):
        for lo, hi, L in ((L["off"], L["off"] + L["filesz"], L) for L in loads):
            if lo <= off < hi:
                return L["vaddr"] + (off - L["off"])
        return None
    def vaddr2off(v):
        for lo, hi, L in LN:
            if lo <= v < hi:
                return L["off"] + (v - L["vaddr"])
        return None
    def seg_of(v):
        for lo, hi, L in LM:
            if lo <= v < hi:
                return {"load": L["i"], "bss": v >= L["vaddr"] + L["filesz"],
                        "flags": L["flags"], "start": hex(L["vaddr"]), "memsz_end": hex(L["vaddr"] + L["memsz"])}
        return None
    CODE = loads[0]
    code_buf = mm[CODE["off"]:CODE["off"] + CODE["filesz"]]
    CODE_V0 = CODE["vaddr"]

    def cv(off_in_code):
        return CODE_V0 + off_in_code

    # ---- 2. pass A: ps_scan-encoding lea xrefs to known data targets ---------
    # targets: 5 fname anchors (content start = nul+2), SCO string, DeleteFile string,
    # causeevent strings.
    fname_names = ["Engine", "Renderer", "AnimGraphRuntime", "Landscape", "RenderCore"]
    targets = {}
    fname_content = {}
    for n in fname_names:
        pat = utf16le("\0" + n + "\0")
        hits = [m.start() for m in re.finditer(re.escape(pat), mm)]
        R.setdefault("fname_strings", {})[n] = [hex(x) for x in
                                                filter(lambda o: off2vaddr(o) is not None, hits)]
        # ps_scan scans the whole image; keep hits inside loaded file ranges
        loaded = [o for o in hits if off2vaddr(o) is not None and vaddr2off(off2vaddr(o)) == o]
        assert len(loaded) == 1, (n, [hex(h) for h in hits])
        fname_content[n] = off2vaddr(loaded[0]) + 2
    sco_pat = utf16le("NewObject with empty name can't be used to create default")
    hits = [m.start() for m in re.finditer(re.escape(sco_pat), mm)]
    sco_hits = [o for o in hits if vaddr2off(off2vaddr(o) or -1) == o]
    R["sco_string_hits"] = [hex(off2vaddr(o)) for o in sco_hits]
    assert len(sco_hits) == 1
    sco_str_vaddr = off2vaddr(sco_hits[0])
    del_pat = utf16le("DeleteFile %s\0")
    hits = [m.start() for m in re.finditer(re.escape(del_pat), mm)]
    del_hits = [o for o in hits if vaddr2off(off2vaddr(o) or -1) == o]
    R["deletefile_string_hits"] = [hex(off2vaddr(o)) for o in del_hits]
    ce_targets = []
    for s in ["causeevent=\0", "CAUSEEVENT \0"]:
        hits = [m.start() for m in re.finditer(re.escape(utf16le(s)), mm)]
        loaded = [o for o in hits if off2vaddr(o) is not None and vaddr2off(off2vaddr(o)) == o]
        R[f"ce_string_{s.strip(chr(0)).strip()}"] = [hex(off2vaddr(o)) for o in loaded]
        ce_targets += [off2vaddr(o) for o in loaded]

    watch = {}
    for n, v in fname_content.items():
        watch[v] = f"fname:{n}"
    watch[sco_str_vaddr] = "sco-string"
    for o in del_hits:
        watch[off2vaddr(o)] = "deletefile-string"
    for v in ce_targets:
        watch[v] = "causeevent-string"

    xrefs = {}
    rex_lea = re.compile(rb"[\x48\x4c]\x8d[\x05\x0d\x15\x1d\x25\x2d\x35\x3d]")
    for m in rex_lea.finditer(code_buf):
        s = m.start()
        t = cv(s) + 7 + i32(code_buf[s + 3:s + 7])
        if t in watch:
            xrefs.setdefault(watch[t], []).append(cv(s))
    for k in xrefs:
        xrefs[k].sort()
    R["xref_counts"] = {k: len(v) for k, v in xrefs.items()}
    R["xref_sites"] = {k: [hex(x) for x in v[:24]] for k, v in xrefs.items()}

    # ---- 3. slot 2: FName ctor (cluster + mov edx,1 / e8 rel32 rule) ---------
    rare = ["Renderer", "AnimGraphRuntime", "Landscape", "RenderCore"]
    clusters = []
    for rc_site in xrefs["fname:RenderCore"]:
        # all five anchors must have an xref site within +/- 0x400 of this site
        ok = all(any(abs(s - rc_site) <= 0x400 for s in xrefs[f"fname:{n}"]) for n in fname_names)
        if ok:
            clusters.append(rc_site)
    R["fname_clusters_rendercore_sites"] = [hex(c) for c in clusters]
    ctor_targets = Counter()
    ctor_windows = []
    for csite in clusters:
        lo = max(0, csite - 0x80) - CODE_V0
        hi = min(len(code_buf), csite + 0x80 - CODE_V0)
        window = code_buf[lo:hi]
        for m in re.finditer(rb"\xba\x01\x00\x00\x00", window):
            p = m.start()
            # nearest e8 rel32 within 16 bytes after mov edx,1
            for q in range(p + 5, min(p + 5 + 16, len(window) - 5)):
                if window[q] == 0xE8:
                    t = (csite - 0x80 + q) + 5 + i32(window[q + 1:q + 5])
                    if CODE_V0 <= t < CODE_V0 + CODE["filesz"]:
                        ctor_targets[t] += 1
                        ctor_windows.append({"cluster_site": hex(csite), "mov_edx1": hex(csite - 0x80 + p),
                                             "call_at": hex(csite - 0x80 + q), "target": hex(t)})
                    break
    R["fname_ctor_targets"] = {hex(k): v for k, v in ctor_targets.items()}
    R["fname_ctor_windows"] = ctor_windows[:24]
    ctor = None
    if ctor_targets:
        best, votes = ctor_targets.most_common(1)[0]
        if len(ctor_targets) == 1 or votes >= 2:
            ctor = best
            R["fname_ctor_consensus_note"] = (
                f"consensus {hex(ctor)} with {votes}/{sum(ctor_targets.values())} cluster windows; "
                f"others: { {hex(k): v for k, v in ctor_targets.items() if k != ctor} }")
    R["fname_ctor_linked"] = hex(ctor) if ctor else None
    R["fname_ctor_runtime"] = hex(ctor + BASE) if ctor else None
    if ctor:
        o = vaddr2off(ctor)
        R["fname_ctor_first_bytes"] = mm[o:o + 16].hex()
        R["fname_ctor_expected_prefix"] = "554889e553504889f04889fb"

    # ---- 4. slot 4: StaticConstructObjectInternal (call-pair intersection) ---
    sco_targets_all = {}
    for site in xrefs["sco-string"]:
        lo = site - CODE_V0
        window = code_buf[lo:lo + 130]
        ts = []
        for p in range(0, 125):
            if window[p] == 0xE8:
                t = site + p + 5 + i32(window[p + 1:p + 5])
                if CODE_V0 <= t < CODE_V0 + CODE["filesz"]:
                    ts.append(t)
        sco_targets_all[site] = ts
    R["sco_call_targets_per_xref"] = {hex(k): [hex(t) for t in v] for k, v in sco_targets_all.items()}
    first6 = dict(list(sorted(sco_targets_all.items()))[:6])  # Rust route: refs.take(6), ascending
    pair_sets = []
    for site, ts in first6.items():
        pair_sets.append(set(zip(ts, ts[1:])))
    inter = set.intersection(*pair_sets) if pair_sets else set()
    R["sco_pair_intersection_first6"] = [[hex(a), hex(b)] for a, b in sorted(inter)]
    inter_all = set.intersection(*[set(zip(ts, ts[1:])) for ts in sco_targets_all.values()]) \
        if sco_targets_all else set()
    R["sco_pair_intersection_all"] = [[hex(a), hex(b)] for a, b in sorted(inter_all)][:16]
    sco = None
    for coll in (inter, inter_all):
        seconds = {b for _, b in coll}
        if sco is None and len(seconds) == 1:
            sco = next(iter(seconds))
    R["sco_linked"] = hex(sco) if sco else None
    R["sco_runtime"] = hex(sco + BASE) if sco else None
    R["sco_recorded_runtime"] = "0x890aa0"
    if sco:
        o = vaddr2off(sco)
        R["sco_first_bytes"] = mm[o:o + 16].hex()
        R["sco_expected_prefix"] = "554889e54157415641554154534881ec"
        o2 = vaddr2off(0x490aa0)
        R["sco_at_recorded_first_bytes"] = mm[o2:o2 + 16].hex() if o2 else None

    # ---- 5. tick corroboration ----------------------------------------------
    ce_sites = sorted(set(xrefs["causeevent-string"]))
    R["ce_xref_sites"] = [hex(s) for s in ce_sites]
    roots = []
    for s in ce_sites:
        off = s - CODE_V0
        # root-function approximation: nearest preceding >=4-byte 0xCC padding run
        run_start = code_buf.rfind(b"\xcc\xcc\xcc\xcc", max(0, off - 0x2000), off)
        root = cv(run_start + 4) if run_start != -1 else None
        roots.append(root)
    R["ce_root_approx"] = [hex(r) if r else None for r in roots]
    uniq_roots = {r for r in roots if r}
    R["ce_root_unique_count"] = len(uniq_roots)
    if len(uniq_roots) == 1:
        R["tick_linked_approx"] = hex(next(iter(uniq_roots)))
    # byte checks at both recorded conventions
    for label, linked in (("linked_0x2fac8c0", 0x2fac8c0), ("linked_0x33ac8c0", 0x33ac8c0)):
        o = vaddr2off(linked)
        R[f"tick_{label}_bytes"] = mm[o:o + 16].hex() if o is not None else None
    R["tick_expected_prefix"] = "554889e54157415641554154534881ec"

    # ---- 6. slot 3: GMalloc via DeleteFile windows (bounded call recursion) --
    def win_candidates(start, size=0x400):
        lo = start - CODE_V0
        if lo < 0 or lo + size > len(code_buf):
            return [], []
        window = code_buf[lo:lo + size]
        cands = []
        for m in re.finditer(rb"\x48\x8b\x0d(....)\x48\x85\xc9", window):
            cands.append((start + m.start() + 7 + i32(m.group(1)),
                          "mov_rcx[test_rcx]", hex(start + m.start())))
        for m in re.finditer(rb"\x48\x83\x3d(....)\x00", window):
            cands.append((start + m.start() + 7 + i32(m.group(1)),
                          "cmp_qword0", hex(start + m.start())))
        calls = []
        for m in re.finditer(rb"\xe8", window):
            p = m.start()
            if p + 5 <= len(window):
                t = start + p + 5 + i32(window[p + 1:p + 5])
                if CODE_V0 <= t < CODE_V0 + CODE["filesz"]:
                    calls.append(t)
        return cands, calls

    gm_cands = Counter()
    gm_details = []
    visited = set()
    frontier = [(s, 0, "deletefile-xref") for s in xrefs.get("deletefile-string", [])]
    while frontier:
        site, depth, path = frontier.pop(0)
        if (site, depth) in visited or depth > 3:
            continue
        visited.add((site, depth))
        cands, calls = win_candidates(site)
        for c, kind, at in cands:
            gm_cands[c] += 1
            gm_details.append({"via": path, "depth": depth, "kind": kind, "at": at, "cand": hex(c)})
        if not cands:
            for t in calls[:32]:
                frontier.append((t, depth + 1, f"call@{hex(site)}"))
    R["gmalloc_candidates"] = {hex(k): v for k, v in gm_cands.most_common()}
    R["gmalloc_candidate_details"] = gm_details[:32]
    R["gmalloc_unique"] = len(gm_cands) == 1
    if len(gm_cands) >= 1:
        top = gm_cands.most_common(1)[0][0]
        R["gmalloc_top_segment"] = seg_of(top)
        R["gmalloc_top_aligned8"] = top % 8 == 0
        gm = top
    else:
        gm = None

    # ---- 7. slot 0: GUA segment coverage -------------------------------------
    gua_linked = 0x97ba110 - BASE
    R["gua_linked"] = hex(gua_linked)
    R["gua_segment"] = seg_of(gua_linked)
    R["gua_runtime_recorded"] = "0x97ba110"

    # ---- 8. pass B: rip-relative ref histogram for GUA family + GMalloc ------
    query = set()
    for d in (0x0, 0x8, 0x20, 0x24, 0x60, 0x68):
        query.add(gua_linked + d)
    if gm is not None:
        query.add(gm)
    counts = Counter()
    rex_all = re.compile(rb"[\x48\x49\x4c\x4d][\x8b\x8d\x89][\x05\x0d\x15\x1d\x25\x2d\x35\x3d]")
    qmap = {q: hex(q) for q in query}
    for m in rex_all.finditer(code_buf):
        s = m.start()
        t = cv(s) + 7 + i32(code_buf[s + 3:s + 7])
        if t in qmap:
            counts[qmap[t]] += 1
    R["ref_counts"] = dict(counts)
    # imm32 mov immediate refs to GUA linked
    imm = re.compile(rb"[\xb8-\xbf](....)", re.S)
    icount = 0
    for m in imm.finditer(code_buf):
        if struct.unpack("<I", m.group(1))[0] == gua_linked:
            icount += 1
    R["gua_imm32_mov_refs"] = icount

    # ---- 9. absence re-verification of shipped ps_scan anchors --------------
    def rust_pattern_regex(pat):
        toks = [t for t in pat.replace("|", " ").split() if t and t != "??"]
        parts = []
        for t in pat.split():
            if t == "??":
                parts.append(b".")
            elif t == "|":
                continue
            else:
                parts.append(re.escape(bytes([int(t, 16)])))
        return re.compile(b"(?s)" + b"".join(parts))

    absence = {}
    guo_pats = [
        "8B 05 ?? ?? ?? ?? 3B 05 ?? ?? ?? ?? 75 ?? 48 8D 15 ?? ?? ?? ?? 48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 8D 05",
        "74 ?? 48 8D 0D ?? ?? ?? ?? C6 05 ?? ?? ?? ?? 01 E8 ?? ?? ?? ?? C6 05 ?? ?? ?? ?? 01",
        "75 ?? 48 ?? ?? 48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 45 33 C9 4C 89 74 24",
        "45 84 c0 48 c7 41 10 00 00 00 00 b8 ff ff ff ff 4c 8d 1d ?? ?? ?? ?? 89 41 08 4c 8b d1 4c 89 19 0f 45 05 ?? ?? ?? ?? ff c0 89 41 08 3b 05",
        "81 ce 00 00 00 02 83 e0 fb 89 47 08 48 8d 0d ?? ?? ?? ?? 48 89 fa 45 31 c0 e8 ?? ?? ?? ??",
        "E8 ?? ?? ?? ?? 8B 05 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 03 0D ?? ?? ?? ?? 29 C8 87 05",
        "41 39 ee 0f 8e ?? ?? ?? ?? bf ?? ?? ?? ?? 48 8b 74 24 10 e8 ?? ?? ?? ?? e9",
        "8b 6f ?? 4c 89 f7 31 f6 e8 ?? ?? ?? ?? 41 39 ef 7e 0d bf ?? ?? ?? ?? 48 89 de e8",
        "8B 05 ?? ?? ?? ?? 2B 05 ?? ?? ?? ?? 2B 05 ?? ?? ?? ??",
    ]
    gm_pats = [
        "48 ?? ?? f0 ?? 0f b1 ?? ?? ?? ?? ?? 74 ?? ?? 85 ?? 74 ?? ?? 8b",
        "eb 03 ?? 8b ?? 48 8b ?? f0 ?? 0f b1 ?? ?? ?? ?? ?? 74 ?? ?? 85 ?? 74 ?? ?? 8b",
        "e8 ?? ?? ?? ?? 48 8b ?? f0 ?? 0f b1 ?? ?? ?? ?? ?? 74 ?? ?? 85 ?? 74 ?? ?? 8b",
        "48 85 C9 74 2E 53 48 83 EC 20 48 8B D9 48 8B ?? ?? ?? ?? ?? 48 85 C9",
        "75 ?? E8 ?? ?? ?? ?? 48 8b 0d ?? ?? ?? ?? 48 8b ?? 48 ?? ?? ff 50 ?? 48 83 c4 ?? ?? c3",
        "48 85 C9 74 ?? 4C 8B 05 ?? ?? ?? ?? 4D 85 C0 0F 84",
        "ff 15 ?? ?? ?? ?? 48 8b 5c 24 ?? 48 89 3d ?? ?? ?? ?? 48 8b 7c 24 20 48 83 c4 28 c3",
        "48 89 ?? f0 ?? 0f b1 ?? ?? ?? ?? ?? 48 39 ?? 74 ?? 48 8b 1d",
        "48 89 ?? f0 ?? 0f b1 ?? ?? ?? ?? ?? 48 39 ?? 75 ?? 48 83 c4",
    ]
    sc_pats = [
        "48 89 44 24 28 C7 44 24 20 00 00 00 00 E8 ?? ?? ?? ?? 48 8B 5C 24 ?? 48 8B ?? 24",
        "E8 ?? ?? ?? ?? 48 8B D8 48 39 75 30 74 15",
        "c6 44 24 30 00 0f 57 c0 0f 11 44 24 38 4c 89 ff e8 ?? ?? ?? ?? 48 89",
    ]
    for label, pats in (("guo", guo_pats), ("gm", gm_pats), ("sc", sc_pats)):
        hits = []
        for i, p in enumerate(pats):
            n = sum(1 for _ in rust_pattern_regex(p).finditer(mm))
            hits.append(n)
        absence[label] = hits
    R["pattern_absence_counts"] = absence
    R["utf8_string_absence"] = {
        "/proc/meminfo": mm.find(b"/proc/meminfo\x00"),
        "root_privileges": mm.find(b"Refusing to run with the root privileges.\n\x00"),
    }

    # ---- 10. file-offset cross-checks (label reconciliation across workers) --
    R["file_offsets"] = {
        "LOAD0_p_off": hex(CODE["off"]), "LOAD0_p_vaddr": hex(CODE["vaddr"]),
        "delta_off_minus_vaddr": hex(CODE["off"] - CODE["vaddr"]),
    }
    for n, v in fname_content.items():
        o = vaddr2off(v)
        R["file_offsets"][f"fname_{n}_content_vaddr"] = {"vaddr": hex(v), "file_off": hex(o) if o else None}
    for k, v in (("sco_string", sco_str_vaddr), ("ctor", ctor), ("sco", sco),
                 ("tick_root_approx", next(iter(uniq_roots)) if len(uniq_roots) == 1 else None)):
        if v is not None:
            o = vaddr2off(v)
            R["file_offsets"][k] = {"vaddr": hex(v), "file_off": hex(o) if o else None}

    with open(OUT, "w") as o:
        json.dump(R, o, indent=1)
    # console summary
    keys = ["fname_strings", "sco_string_hits", "xref_counts", "fname_ctor_targets",
            "fname_ctor_linked", "fname_ctor_runtime", "fname_ctor_first_bytes",
            "sco_pair_intersection_all", "sco_linked", "sco_runtime", "sco_first_bytes",
            "sco_at_recorded_first_bytes", "ce_xref_sites", "ce_root_approx",
            "tick_linked_0x2fac8c0_bytes", "tick_linked_0x33ac8c0_bytes",
            "gmalloc_candidates", "gmalloc_unique", "gmalloc_top_segment",
            "gua_linked", "gua_segment", "ref_counts", "gua_imm32_mov_refs",
            "pattern_absence_counts", "utf8_string_absence"]
    for k in keys:
        if k in R:
            print(f"== {k}: {json.dumps(R[k])[:600]}")

if __name__ == "__main__":
    main()
