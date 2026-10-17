#!/usr/bin/env python3
"""Read-only: find UTF-16LE / ASCII needles in the pinned decrypted Townfall eboot; print image vaddr."""
import sys, hashlib, struct
sys.path.insert(0, "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/phasea-tools")
import pak_vtable_pins as P
IMG = "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin"
P.load_image(IMG, True)
def o2v(off):
    for p in P.PHDRS:
        if p["type"] == 1 and p["off"] <= off < p["off"] + p["filesz"]:
            return p["vaddr"] + (off - p["off"])
    return None
def ctx16(off, back=40, fwd=80):
    # walk back to string start (utf16 NUL), forward to end
    s = off
    while s >= 2 and struct.unpack_from("<H", P.BLOB, s-2)[0] != 0 and off - s < back*2: s -= 2
    e = off
    while struct.unpack_from("<H", P.BLOB, e)[0] != 0 and e - off < fwd*2: e += 2
    raw = P.BLOB[s:e]
    try: return raw.decode("utf-16le", "replace"), o2v(s)
    except Exception: return repr(raw), o2v(s)
for needle in sys.argv[1:]:
    for enc in ("utf-16le", "ascii"):
        pat = needle.encode(enc)
        pos = 0; hits = 0
        while True:
            i = P.BLOB.find(pat, pos)
            if i < 0 or hits >= 12: break
            hits += 1; pos = i + 1
            if enc == "utf-16le":
                if i % 2: continue
                text, sv = ctx16(i)
                print(f"[{needle}] utf16 off={i:#x} va={o2v(i) and hex(o2v(i))} str@{sv and hex(sv)}: {text!r}")
            else:
                s = i
                while s > 0 and 32 <= P.BLOB[s-1] < 127 and i - s < 60: s -= 1
                e = i
                while e < len(P.BLOB) and 32 <= P.BLOB[e] < 127 and e - i < 100: e += 1
                print(f"[{needle}] ascii off={i:#x} va={o2v(i) and hex(o2v(i))} str@{o2v(s) and hex(o2v(s))}: {P.BLOB[s:e].decode('ascii','replace')!r}")
