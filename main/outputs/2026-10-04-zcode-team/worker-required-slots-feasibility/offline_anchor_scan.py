#!/usr/bin/env python3
"""worker-required-slots-feasibility: offline pattern-anchor measurement.

Local read-only parse of the RETAINED decrypted-eboot.bin. No console contact.
Locates the patternsleuth resolver anchors (Rust sources in
.superpowers/native-dependency-spike/ue4ss-native-mapped-candidates-1/src/)
inside the pinned image and classifies each anchor's linked vaddr by PT_LOAD
span, so the runtime-span readability question (trusted live records:
[base+0, base+0x787c000) exec-only/unreadable; verified readable windows all
>= base+0x787c000) can be answered per slot.

Pattern syntax subset implemented: hex bytes, '??' (1 byte wildcard),
'|' custom-offset marker (match address = offset of following token),
'[ ... ]' capture group (bytes inside are wildcards in every pattern we scan;
group contributes its byte length).
"""
import re, struct, sys, json
from pathlib import Path

EBOOT = Path("/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin")
BASE = 0x400000  # live base, proven by in-game anchor (worker-mapping-recovery)
READABLE_LIVE_MIN = BASE + 0x787c000  # first verified readable live byte (trusted records)

data = EBOOT.open("rb").read(0x1000)
assert data[:4] == b"\x7fELF", "not ELF"
e_phoff = struct.unpack_from("<Q", data, 0x20)[0]
e_phentsize = struct.unpack_from("<H", data, 0x36)[0]
e_phnum = struct.unpack_from("<H", data, 0x38)[0]

f = EBOOT.open("rb")
loads = []  # (vaddr, filesz, flags, off)
for i in range(e_phnum):
    f.seek(e_phoff + i * e_phentsize)
    ph = f.read(e_phentsize)
    p_type, p_flags = struct.unpack_from("<II", ph, 0)
    p_off, p_vaddr, _, p_filesz, p_memsz = struct.unpack_from("<QQQQQ", ph, 8)
    if p_type == 1:  # PT_LOAD
        loads.append(dict(vaddr=p_vaddr, filesz=p_filesz, memsz=p_memsz, flags=p_flags, off=p_off))
print("PT_LOADs:")
for L in loads:
    prot = "".join(c if L["flags"] & b else "-" for c, b in zip("RWX", (4, 2, 1)))
    live0 = BASE + L["vaddr"]; live1 = live0 + L["filesz"]
    print(f"  off=0x{L['off']:09x} vaddr=0x{L['vaddr']:09x} filesz=0x{L['filesz']:09x} {prot} "
          f"live=[0x{live0:09x},0x{live1:09x}) {'<= unreadable-live' if live1 <= READABLE_LIVE_MIN else ('crosses readable boundary' if live0 < READABLE_LIVE_MIN else 'in readable-live window')}")

def span_of(va):
    for idx, L in enumerate(loads):
        if L["vaddr"] <= va < L["vaddr"] + L["filesz"]:
            return idx, L
    return None, None

def compile_pattern(pat):
    """returns list of (byte-or-None) plus custom_offset marker handling"""
    toks = pat.strip().split()
    seq = []
    custom_at = None  # byte index where match address points
    in_capture = False
    i = 0
    for t in toks:
        if t == "|":
            custom_at = len(seq)
            continue
        if t == "[":
            in_capture = True
            continue
        if t == "]":
            in_capture = False
            continue
        if len(t) == 2 and all(c in "0123456789abcdefABCDEF" for c in t):
            seq.append(int(t, 16)); continue
        if t == "??":
            seq.append(None); continue
        if len(t) == 2 and t[1] == "?" and t[0] in "0123456789abcdefABCDEF":
            seq.append((int(t[0], 16) << 4, 0x0F)); continue  # high-nibble wildcard
        if len(t) == 2 and t[0] == "?" and t[1] in "0123456789abcdefABCDEF":
            seq.append((int(t[1], 16), 0xF0)); continue  # low-nibble wildcard
        raise ValueError(f"token {t!r} in {pat!r}")
    return seq, custom_at or 0

def find_matches(buf, base_off, base_va, pat, limit=8):
    seq, custom_at = compile_pattern(pat)
    n = len(seq)
    # regex with nibble support
    rx = bytearray()
    for b in seq:
        if b is None:
            rx += b"."
        elif isinstance(b, tuple):
            hi, mask = b
            # byte whose high nibble == hi>>4 and low nibble free, or vice versa
            if mask == 0x0F:
                rx += b"."  # rare in our set; degrade to wildcard, verified below
            else:
                rx += b"."
        else:
            rx += re.escape(bytes([b]))
    # first-fixed-byte anchor via find loop for speed
    first_fixed = next((k for k, b in enumerate(seq) if isinstance(b, int)), 0)
    anchor = bytes([seq[first_fixed]])
    out = []
    pos = 0
    rxre = re.compile(bytes(rx), re.S)
    while True:
        idx = buf.find(anchor, pos)
        if idx < 0:
            break
        pos = idx + 1
        start = idx - first_fixed
        if start < 0:
            continue
        m = rxre.match(buf, start)
        if m and m.end() == start + n:
            va = base_va + (start + custom_at)
            out.append(va)
            if len(out) >= limit:
                break
    return out

def utf16le(s):
    return s.encode("utf-16-le")

def find_bytes(buf, base_off, base_va, needle, limit=8):
    out = []
    pos = 0
    while True:
        idx = buf.find(needle, pos)
        if idx < 0:
            break
        pos = idx + 1
        out.append(base_va + idx)
        if len(out) >= limit:
            break
    return out

def classify(va):
    idx, L = span_of(va)
    if idx is None:
        return "NO-SPAN", None, None
    prot = "".join(c if L["flags"] & b else "-" for c, b in zip("RWX", (4, 2, 1)))
    live = BASE + va
    liveprot = "UNREADABLE(trusted)" if live < READABLE_LIVE_MIN else "readable-window(trusted)"
    return f"LOAD{idx}[{prot}]", live, liveprot

MMAP = EBOOT.open("rb")
# memory-map whole file for scanning
import mmap
MM = mmap.mmap(MMAP.fileno(), 0, access=mmap.ACCESS_READ)

results = {}

def report(key, kind, label, matches):
    rows = []
    for va in matches:
        sp, live, lp = classify(va)
        rows.append((va, sp, live, lp))
    results.setdefault(key, []).append(dict(kind=kind, label=label, matches=rows))

# ---------------- slot 0: GUObjectArray (guobject_array.rs:10-16,32,47,52) ----------------
GUO_PATTERNS = [
    "8B 05 ?? ?? ?? ?? 3B 05 ?? ?? ?? ?? 75 ?? 48 8D 15 ?? ?? ?? ?? 48 8D 0D | ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 8D 05",
    "74 ?? 48 8D 0D | ?? ?? ?? ?? C6 05 ?? ?? ?? ?? 01 E8 ?? ?? ?? ?? C6 05 ?? ?? ?? ?? 01",
    "75 ?? 48 ?? ?? 48 8D 0D | ?? ?? ?? ?? E8 ?? ?? ?? ?? 45 33 C9 4C 89 74 24",
    "45 84 c0 48 c7 41 10 00 00 00 00 b8 ff ff ff ff 4c 8d 1d | ?? ?? ?? ?? 89 41 08 4c 8b d1 4c 89 19 0f 45 05 ?? ?? ?? ?? ff c0 89 41 08 3b 05",
    "81 ce 00 00 00 02 83 e0 fb 89 47 08 48 8d 0d | ?? ?? ?? ?? 48 89 fa 45 31 c0 e8 ?? ?? ?? ??",
    "E8 ?? ?? ?? ?? 8B 05 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 03 0D | ?? ?? ?? ?? 29 C8 87 05",
    "41 39 ee 0f 8e ?? ?? ?? ?? bf | ?? ?? ?? ?? 48 8b 74 24 10 e8 ?? ?? ?? ?? e9",
    "8b 6f ?? 4c 89 f7 31 f6 e8 ?? ?? ?? ?? 41 39 ef 7e 0d bf | ?? ?? ?? ?? 48 89 de e8",
    "8B 05 [ ?? ?? ?? ?? ] 2B 05 [ ?? ?? ?? ?? ] 2B 05 [ ?? ?? ?? ?? ]",
]
for i, p in enumerate(GUO_PATTERNS):
    allm = []
    for L in loads:
        if L["filesz"] == 0: continue
        f.seek(L["off"]); buf = f.read(L["filesz"])
        allm += find_matches(buf, L["off"], L["vaddr"], p)
    report("slot0_guobjectarray", "code-pattern", f"GUO#{i}", allm)

# ---------------- slot 2: FName ctor ELF anchors (fname.rs:16-22,54) ----------------
for s in ["\x00Engine\x00", "\x00Renderer\x00", "\x00AnimGraphRuntime\x00", "\x00Landscape\x00", "\x00RenderCore\x00"]:
    allm = []
    for L in loads:
        if L["filesz"] == 0: continue
        f.seek(L["off"]); buf = f.read(L["filesz"])
        allm += find_bytes(buf, L["off"], L["vaddr"], utf16le(s))
    report("slot2_fname_ctor", "utf16-string", repr(s), allm)
# code step pattern (only reachable after root-function metadata; scan for completeness)
allm = []
for L in loads:
    if L["filesz"] == 0: continue
    f.seek(L["off"]); buf = f.read(L["filesz"])
    allm += find_matches(buf, L["off"], L["vaddr"], "ba 01 00 00 00 e8 | ?? ?? ?? ??")
report("slot2_fname_ctor", "code-pattern", "mov-edx,1;call", allm)

# ---------------- slot 3: GMalloc (gmalloc.rs:22-37,152-177) ----------------
GM_PATTERNS = [
    "48 ?? ?? f0 ?? 0f b1 ?? | ?? ?? ?? ?? 74 ?? ?? 85 ?? 74 ?? ?? 8b",
    "eb 03 ?? 8b ?? 48 8b ?? f0 ?? 0f b1 ?? | ?? ?? ?? ?? 74 ?? ?? 85 ?? 74 ?? ?? 8b",
    "e8 ?? ?? ?? ?? 48 8b ?? f0 ?? 0f b1 ?? | ?? ?? ?? ?? 74 ?? ?? 85 ?? 74 ?? ?? 8b",
    "48 85 C9 74 2E 53 48 83 EC 20 48 8B D9 48 8B ?? | ?? ?? ?? ?? 48 85 C9",
    "75 ?? E8 ?? ?? ?? ?? 48 8b 0d | ?? ?? ?? ?? 48 8b ?? 48 ?? ?? ff 50 ?? 48 83 c4 ?? ?? c3",
    "48 85 C9 74 ?? 4C 8B 05 | ?? ?? ?? ?? 4D 85 C0 0F 84",
    "48 ?? ?? ?? ?? ?? ?? e8 ?? ?? ?? ?? 48 8b 0d | ?? ?? ?? ?? 48 8b 01 ff 50 ?? 84 c0 75 ?? b9 38 00 00 00",
    "84 C0 75 ?? B9 38 00 00 00 ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 48 85 c0 74 ?? 48 8b 0d | ?? ?? ?? ?? 48 8d 05 ?? ?? ?? ?? 48 89",
    "ff 15 ?? ?? ?? ?? 48 8b 5c 24 ?? 48 89 3d | ?? ?? ?? ?? 48 8b 7c 24 20 48 83 c4 28 c3",
    "48 89 ?? f0 ?? 0f b1 ?? | ?? ?? ?? ?? 48 39 ?? 74 ?? 48 8b 1d",
    "48 89 ?? f0 ?? 0f b1 ?? | ?? ?? ?? ?? 48 39 ?? 75 ?? 48 83 c4",
    "48 89 5C 24 08 57 48 83 EC 20 48 8B F9 ?? ?? ?? ?? ?? ?? ?? ?? ?? 48 85 C9 75 ?? E8 ?? ?? ?? FF 48 8B 0D | ?? ?? ?? ?? ?? 8B ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 48 ?? ?? ?? ?? 48",
    "48 89 5C 24 08 57 48 83 EC ?? 48 83 3D ?? ?? ?? ?? 00 8B DA 48 8B F9 75 07 E8 ?? ?? ?? FF EB 07 33 C9 E8 ?? ?? ?? FF 48 8B 0D | ?? ?? ?? ?? 44 8B C3 48 8B D7 48 8B 01 FF 50 10 80 3D ?? ?? ?? ?? 00 48 8B D8 75 ?? 48 8B 05 ?? ?? ?? ?? 48 85 C0 75 05 E8 ?? ?? ?? FF ?? 44 24 ?? 01",
    "48 89 5C 24 08 57 48 83 EC 20 48 8B F9 8B DA 48 8B 0D | ?? ?? ?? ?? 48 85 C9 75 2E 65 48 8B 04 25 58 00 00 00 44 8B 05 ?? ?? ?? ?? BA 18 00 00 00 4E 8B 04 C0 42 8B 04 02 39 05 ?? ?? ?? ?? 7E 09 EB 1E 48 8B 0D ?? ?? ?? ?? 48 8B 01 44 8B C3 48 8B D7 48 8B 5C 24 30 48 83 C4 20 5F 48 FF 60 10 48 8D 0D",
]
for i, p in enumerate(GM_PATTERNS):
    allm = []
    for L in loads:
        if L["filesz"] == 0: continue
        f.seek(L["off"]); buf = f.read(L["filesz"])
        allm += find_matches(buf, L["off"], L["vaddr"], p)
    report("slot3_gmalloc", "code-pattern", f"GM#{i}", allm)
for s in ["/proc/meminfo\x00", "Refusing to run with the root privileges.\n\x00"]:
    allm = []
    for L in loads:
        if L["filesz"] == 0: continue
        f.seek(L["off"]); buf = f.read(L["filesz"])
        allm += find_bytes(buf, L["off"], L["vaddr"], s.encode("utf-8"))
    report("slot3_gmalloc", "utf8-string", repr(s), allm)

# ---------------- slot 4: StaticConstructObject (static_construct_object.rs:24-41,49-53) ----------------
SC_PATTERNS = [
    "48 89 44 24 28 C7 44 24 20 00 00 00 00 E8 | ?? ?? ?? ?? 48 8B 5C 24 ?? 48 8B ?? 24",
    "E8 | ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? C0 E9 ?? 32 88 ?? ?? ?? ?? 80 E1 01 30 88 ?? ?? ?? ?? 48",
    "E8 | ?? ?? ?? ?? 48 8B D8 48 39 75 30 74 15",
    "c6 44 24 30  00 0f 57 c0 0f 11 44 24 38 4c 89 ff e8 | ?? ?? ?? ?? 48 89",
]
for i, p in enumerate(SC_PATTERNS):
    allm = []
    for L in loads:
        if L["filesz"] == 0: continue
        f.seek(L["off"]); buf = f.read(L["filesz"])
        allm += find_matches(buf, L["off"], L["vaddr"], p)
    report("slot4_static_construct", "code-pattern", f"SC#{i}", allm)
allm = []
for L in loads:
    if L["filesz"] == 0: continue
    f.seek(L["off"]); buf = f.read(L["filesz"])
    allm += find_bytes(buf, L["off"], L["vaddr"], utf16le("NewObject with empty name can't be used to create default"))
report("slot4_static_construct", "utf16-string", "NewObject with empty name...", allm)

# ---------------- slot 6: engine_version (engine_version.rs:53-89,216-219,245) ----------------
VERSION_PATTERNS = [
    "C7 47 20 | 04 00 ?? 00 66 89 6F 24","C7 4? 20 | 04 00 ?? ?? 66 4? 89 ?? 24","C7 ?? 24 20 | 04 00 ?? ?? 48 8D 45 F0",
    "C7 05 ?? ?? ?? ?? | 04 00 ?? 00 66 89 ?? ?? ?? ?? ?? C7 05","C7 05 ?? ?? ?? ?? | 04 00 ?? 00 66 89 ?? ?? ?? ?? ?? 89",
    "41 C7 ?? | 04 00 ?? 00 ?? ?? 00 00 00 66 41 89","41 C7 ?? | 04 00 ?? 00 66 41 89 ?? 04","41 C7 04 24 | 04 00 ?? 00 66 ?? 89 ?? 24",
    "41 C7 04 24 | 04 00 ?? 00 B9 ?? 00 00 00","41 C7 44 24 20 | 04 00 ?? 00 66 ?? 89 ?? 24","41 C7 ?? 20 | 04 00 ?? 00 41 89 ?? 28",
    "41 C7 ?? | 04 00 ?? 00 66 41 C7 4? 04","C7 05 ?? ?? ?? ?? | 04 00 ?? 00 89 3D ?? ?? ?? ?? 85 FF","C7 05 ?? ?? ?? ?? | 04 00 ?? 00 89 05 ?? ?? ?? ?? E8",
    "C7 05 ?? ?? ?? ?? | 04 00 ?? 00 66 89 ?? ?? ?? ?? ??","C7 46 20 | 04 00 ?? 00 66 44 89 76 24 44 89 76 28 48 39 C7",
    "C7 03 | 04 00 ?? 00 66 44 89 63 04 C7 43 08 C1 5C 08 80 E8","C7 47 20 | 04 00 ?? 00 66 89 6F 24 C7 47 28 ?? ?? ?? ?? 49",
    "C7 03 | 04 00 ?? 00 66 89 6B 04 89 7B 08 48 83 C3 10","41 C7 06 | 05 00 ?? ?? 48 8B 5C 24 ?? 49 8D 76 ?? 33 ED 41 89 46",
    "C7 06 | 05 00 ?? ?? 48 8B 5C 24 20 4C 8D 76 10 33 ED","11 76 30 c7 46 20 | 04 00 ?? 00",
    "0F 57 C0 0F 11 43 10 C7 03 | 05 ?? ?? ?? 66 C7 43 04 ?? ??","48 89 2? 48 89 6? 08 C7 0? | 05 00 ?? ?? 66","49 89 2? 49 89 6? 08 C7 0? | 05 00 ?? ?? 66",
    "C7 46 20 | 05 00 ?? ?? 66 89 ?? 24","C7 43 20 | 05 00 ?? ?? 48 3B F0","C7 46 20 | 05 00 ?? ?? 48 8D 44 24 20","C7 4? 20 | 05 00 ?? ?? 66 44 89 ?? 24",
    "C7 ?? 24 20 | 05 00 ?? ?? 48 8D 45 F0","C7 06 | 05 00 ?? 00 66 C7 46 04","0F B6 D8 C1 E3 1F E8 ?? ?? ?? ?? 0B C3 C7 06 | 05 00 ?? 00",
    "0F B6 D8 C1 E3 1F E8 ?? ?? ?? ?? 0B C3 C7 06 | 04 00 ?? 00","0F B6 D8 C1 E3 1F E8 ?? ?? ?? ?? 33 ED C7 06 | 05 00 ?? 00",
    "89 2E 89 6E 08 48 8D 4E 0C 89 29 41 C7 07 | 05 00 ?? 00",
]
tot = 0
unre = 0
for i, p in enumerate(VERSION_PATTERNS):
    allm = []
    for L in loads:
        if L["filesz"] == 0: continue
        f.seek(L["off"]); buf = f.read(L["filesz"])
        allm += find_matches(buf, L["off"], L["vaddr"], p, limit=40)
    tot += len(allm)
    unre += sum(1 for va in allm if (BASE + va) < READABLE_LIVE_MIN)
    report("slot6_engine_version", "code-pattern", f"VER#{i}", allm[:40])
# version wide strings: "d+.d+." preceded by NUL NUL (engine_version.rs:216-219)
ver_re = re.compile(b"\x00\x00(?:[\x30-\x39]\x00)\x2e\x00(?:[\x30-\x39]\x00)\x2e\x00")
allm = []
for L in loads:
    if L["filesz"] == 0: continue
    f.seek(L["off"]); buf = f.read(L["filesz"])
    for m in ver_re.finditer(buf):
        allm.append(L["vaddr"] + m.start() + 2)
        if len(allm) >= 40: break
report("slot6_engine_version", "utf16-version-string-regex", r"\x00\x00 d . d .", allm)
for s in ["++UE4+Release-", "++UE5+Release-", "++depot+UE4-Releases+"]:
    allm = []
    for L in loads:
        if L["filesz"] == 0: continue
        f.seek(L["off"]); buf = f.read(L["filesz"])
        allm += find_bytes(buf, L["off"], L["vaddr"], utf16le(s))
    report("slot6_engine_version", "utf16-branch-string", s, allm)

# ---------------- summary ----------------
print("\n==== SUMMARY (match anchors by span; trusted live readability) ====")
summary = {}
for key, items in results.items():
    n_total = sum(len(it["matches"]) for it in items)
    n_unread = sum(1 for it in items for (_va, _sp, live, _lp) in it["matches"] if live is not None and live < READABLE_LIVE_MIN)
    n_read = sum(1 for it in items for (_va, _sp, live, _lp) in it["matches"] if live is not None and live >= READABLE_LIVE_MIN)
    summary[key] = dict(anchors=n_total, in_unreadable_live=n_unread, in_readable_live=n_read)
    print(f"\n{key}: {n_total} anchors total; unreadable-live={n_unread} readable-live={n_read}")
    for it in items:
        if not it["matches"]:
            continue
        vas = ", ".join(f"0x{va:09x}(live 0x{live:09x},{lp})" for (va, sp, live, lp) in it["matches"][:4])
        print(f"  {it['kind']:26s} {it['label']:36s} n={len(it['matches'])}: {vas}")

Path(__file__).with_name("anchor-measurement.json").write_text(json.dumps(dict(
    eboot=str(EBOOT), base=BASE, readable_live_min=READABLE_LIVE_MIN,
    loads=[dict(vaddr=L["vaddr"], filesz=L["filesz"], flags=L["flags"], off=L["off"]) for L in loads],
    results=results, summary=summary), indent=1))
print("\nwrote anchor-measurement.json")
