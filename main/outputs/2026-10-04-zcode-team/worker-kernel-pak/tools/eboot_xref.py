#!/usr/bin/env python3
"""eboot_xref.py — locate code that references pak-related strings in the
decrypted Townfall PS5 eboot (ELF64 x86-64, PS5 SELF type 0xfe10).

Method: raw scan for rip-relative LEA/MOV encodings, compute the target vaddr,
keep sites whose target lands inside a given string's byte range, then
disassemble a window around each site with capstone for verification.

Segment map (from program headers, verified 2026-10-06):
  PH0 file 0x4000..0x747db1c -> vaddr 0x0..0x7479b1c  (RX code)
  PH1 file 0x7480000..       -> vaddr 0x747c000        (RO data, strings)
  For both, vaddr == fileoff - 0x4000.

Read-only against the eboot. No console contact.
"""
import struct
import sys

EB = "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin"
VADDR_DELTA = 0x4000
CODE_FILE_OFF = 0x4000
CODE_FILE_END = 0x4000 + 0x7479B1C  # PH0 off+filesz

# rip-relative opcodes: (prefix, opcode, modrm.reg-in-modrm) forms we scan.
# 48 8d /r  lea r64, [rip+disp32]  ; 4c 8d /r lea r8-15
# 48 8b /r  mov r64, [rip+disp32]  ; 4c 8b
SCAN_PREFIXES = (b"\x48\x8d", b"\x4c\x8d", b"\x48\x8b", b"\x4c\x8b")
MODRM_RIP = 0x05  # mod=00, rm=101


def load():
    with open(EB, "rb") as f:
        return f.read()


def string_table(strings_path):
    """Parse `strings -t x` output -> [(vaddr, text)]."""
    out = []
    with open(strings_path) as f:
        for line in f:
            off_text = line.strip().split(None, 1)
            if len(off_text) == 2:
                try:
                    off = int(off_text[0], 16)
                except ValueError:
                    continue
                out.append((off - VADDR_DELTA, off_text[1]))
    return out


def find_xrefs(blob, targets):
    """targets: {vaddr: name}. Returns {name: [(site_vaddr, mnemonic_kind)]}."""
    results = {}
    view = memoryview(blob)
    pos = CODE_FILE_OFF
    # opcode layout: prefix(2) modrm(1) disp(4) -> instr len 7
    while True:
        i = blob.find(SCAN_PREFIXES[0], pos, CODE_FILE_END)
        best = None
        for pfx in SCAN_PREFIXES:
            j = blob.find(pfx, pos, CODE_FILE_END)
            if j != -1 and (best is None or j < best):
                best = j
        if best is None:
            break
        pos = best + 1
        if best + 7 > CODE_FILE_END:
            continue
        if (blob[best + 2] & 0xC7) != MODRM_RIP:
            continue
        disp = struct.unpack_from("<i", blob, best + 3)[0]
        site_vaddr = best - VADDR_DELTA
        target = site_vaddr + 7 + disp
        if target in targets:
            kind = "lea" if blob[best + 1] == 0x8D else "mov"
            reg = ((blob[best + 2] >> 3) & 7) + (8 if blob[best] == 0x4C else 0)
            results.setdefault(targets[target], []).append((site_vaddr, kind, reg))
    return results


def disasm_window(blob, site_vaddr, back=0x40, fwd=0xC0):
    import capstone

    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    md.detail = False
    start_file = site_vaddr - back + VADDR_DELTA
    code = blob[start_file : site_vaddr + fwd + VADDR_DELTA]
    out = []
    for ins in md.disasm(code, site_vaddr - back):
        out.append(f"{ins.address:#x}  {ins.mnemonic} {ins.op_str}")
    return out


def main():
    strings_file = sys.argv[1] if len(sys.argv) > 1 else "/tmp/eboot_strings.txt"
    want = sys.argv[2:] or ["*.pak"]
    blob = load()
    table = string_table(strings_file)
    targets = {}
    for vaddr, text in table:
        if any(w in text for w in want):
            for k in range(len(text) + 1):
                targets[vaddr + k] = text  # any byte inside the string
    # de-overlap: keep only the start byte of each string
    starts = {v: t for v, t in table if any(w in t for w in want)}
    print(f"{len(starts)} candidate strings")
    res = find_xrefs(blob, starts)
    for name, sites in sorted(res.items(), key=lambda kv: -len(kv[1])):
        print(f"\n== {name!r} @ vaddr {next(v for v,t in table if t==name):#x} — {len(sites)} ref(s)")
        for site, kind, reg in sites[:8]:
            print(f"   site {site:#x} ({kind} r{reg})")
    return res


if __name__ == "__main__":
    main()
