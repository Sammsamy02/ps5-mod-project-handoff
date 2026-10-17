#!/usr/bin/env python3
"""
worker-early-load patch-apply.py — reference implementation for EARLY-LOAD-DESIGN.md.

Adds a DT_NEEDED entry for "mb.prx" to the Silent Hill Townfall (PPSA33286)
decrypted eboot.bin by recycling four low-value dynamic-table slots, and by
placing the string "mb.prx\\0" into the verified 7-byte gap between .dynstr
and .dynsym.

Proposal-only: this script was run OFF-console against a local copy of the
game-root eboot (console-toplevel-eboot.bin). It never contacts the console.
Every write is preceded by a byte-exact "before" assertion, so it can only
apply to the exact expected base bytes (failures abort with no writes).

Usage:
  python3 patch-apply.py <base-eboot> <out-eboot> [A1|A2]

  A1 = minimal variant: DT_DEBUG -> DT_NEEDED only (1 dynamic slot).
  A2 = full variant:    A1 + SCE metadata triple (3 more recycled slots).
Both variants also write the name string and bump DT_STRSZ.
"""

import hashlib
import struct
import sys

# ---------------------------------------------------------------- constants
# All offsets are FILE offsets in the game-root eboot.bin
# (sha256 afe0a2a5fc9e1c17b215bdbb3de3c80eb0b3e1aef5c794aea491805899030b7,
#  sha1   287c23b09366b52fc67d7112dac9acc707de39fa, 194,241,424 bytes).
# The identical-bytewise observations copy
# (.superpowers/native-observations/townfall-static-executable-2026-10-02/
#  decrypted-eboot.bin, sha256 3b6cfad4...22fc) has the same offsets; it
# differs from the game-root file ONLY in the trailing certificate TLV
# region (file >= 0xb93c32e), which this patch never touches.

DYN_BASE   = 0x0B93B278          # PT_DYNAMIC p_offset (261 entries x 16B)
STR_GAP    = 0x09317B61          # 7 free zero bytes after .dynstr end (strtab offset 0x8741)
NAME       = b"mb.prx\x00"       # exactly 7 bytes: 6 chars + NUL
NAME_STROFF = 0x8741             # offset of the string inside DT_STRTAB

# dynamic entry index -> (file offset of tag, of val)
def ent(i):
    return DYN_BASE + 16 * i, DYN_BASE + 16 * i + 8

# slot recycling plan (see EARLY-LOAD-DESIGN.md §4 for loss analysis)
SLOT_DEBUG     = 236   # DT_DEBUG      (tag 0x15)     -> DT_NEEDED          [A1]
SLOT_RELACOUNT = 240   # DT_RELACOUNT  (0x6ffffff9)   -> 0x61000045         [A2]
SLOT_FINI      = 257   # DT_FINI       (0x0d)         -> 0x61000049         [A2]
SLOT_SYMSIZE   = 258   # 0x6100003f (symtab byte size) -> 0x61000019         [A2]
SLOT_STRSZ     = 248   # DT_STRSZ value only: 0x8741 -> 0x8748              [A1]

MOD_IDX_45 = 0x003A    # next free module index for 0x61000045 (0x1..0x39 used)
LIB_IDX_49 = 0x003C    # next free lib     index for 0x61000049 (0x0..0x3b used)
FLAG_19    = 0x1       # matches libfmod.prx / libfmodstudio.prx (app-staged prx)

# expected "before" bytes (tag, value) for every slot we touch
EXPECTED = {
    236: (0x00000015, 0x0000000000000000),
    240: (0x6FFFFFF9, 0x0000000000192B94),
    248: (0x0000000A, 0x0000000000008741),
    257: (0x0000000D, 0x0000000007479AD0),
    258: (0x6100003F, 0x000000000000BD90),
}


def main():
    if len(sys.argv) != 4:
        print(__doc__)
        return 2
    base_path, out_path, variant = sys.argv[1], sys.argv[2], sys.argv[3]
    if variant not in ("A1", "A2"):
        print("variant must be A1 or A2")
        return 2

    data = bytearray(open(base_path, "rb").read())
    print(f"base: {base_path}")
    print(f"  bytes={len(data)} sha256={hashlib.sha256(data).hexdigest()}")

    # ---- pre-flight assertions (fail = no writes) ----------------------
    for i, (want_tag, want_val) in EXPECTED.items():
        to, vo = ent(i)
        got_tag, got_val = struct.unpack_from("<qQ", data, to)
        if got_tag != want_tag or got_val != want_val:
            print(f"ABORT: dynamic[{i}] = (0x{got_tag:x}, 0x{got_val:x}), "
                  f"expected (0x{want_tag:x}, 0x{want_val:x}) — wrong base file?")
            return 1
    gap = bytes(data[STR_GAP:STR_GAP + 8])
    if gap != b"\x00" * 8:
        print(f"ABORT: strtab gap not free: {gap.hex()}")
        return 1

    # ---- writes ---------------------------------------------------------
    def set_ent(i, tag, val):
        to, vo = ent(i)
        old = struct.unpack_from("<qQ", data, to)
        struct.pack_into("<qQ", data, to, tag, val)
        print(f"  dynamic[{i:3d}] @0x{to:x}: (0x{old[0]:08x}, 0x{old[1]:016x})"
              f" -> (0x{tag:08x}, 0x{val:016x})")

    # 1. name string into the .dynstr/.dynsym gap
    data[STR_GAP:STR_GAP + len(NAME)] = NAME
    print(f"  string   @0x{STR_GAP:x}: 00*07 -> {NAME!r}")

    # 2. DT_STRSZ bump so offset 0x8741 is inside the string table
    set_ent(SLOT_STRSZ, 0x0A, NAME_STROFF + len(NAME))          # 0x8748

    # 3. variant writes
    if variant == "A1":
        set_ent(SLOT_DEBUG, 0x01, NAME_STROFF)                   # DT_NEEDED "mb.prx"
    else:
        set_ent(SLOT_DEBUG,     0x01,       NAME_STROFF)         # DT_NEEDED "mb.prx"
        set_ent(SLOT_RELACOUNT, 0x61000045,
                (MOD_IDX_45 << 48) | (0x01 << 40) | (0x01 << 32) | NAME_STROFF)
        set_ent(SLOT_FINI,      0x61000049,
                (LIB_IDX_49 << 48) | (0x0001 << 32) | NAME_STROFF)
        set_ent(SLOT_SYMSIZE,   0x61000019, FLAG_19)

    # ---- post-flight readback -------------------------------------------
    need = 0
    for i in range(261):
        to, _ = ent(i)
        if struct.unpack_from("<q", data, to)[0] == 1:
            need += 1
    to, _ = ent(260)
    assert struct.unpack_from("<q", data, to)[0] == 0, "DT_NULL terminator must remain last"
    stro, _ = ent(247)
    strt = struct.unpack_from("<Q", data, stro)[0]
    got = bytes(data[0x930F420 + 0:0x930F420 + 0x8748])
    name = got[NAME_STROFF:got.index(b"\x00", NAME_STROFF) + 1]
    assert name == NAME, name
    print(f"  postcheck: DT_NEEDED count 48 -> {need}; DT_NULL intact at [260]; "
          f"strtab reads {name!r} at 0x{NAME_STROFF:x} (DT_STRTAB=0x{strt:x})")

    open(out_path, "wb").write(data)
    print(f"wrote: {out_path}")
    print(f"  bytes={len(data)} sha256={hashlib.sha256(data).hexdigest()}")
    print(f"  changed vs base: "
          f"{sum(1 for a, b in zip(open(base_path,'rb').read(), data) if a != b)} bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
