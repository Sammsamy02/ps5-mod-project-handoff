#!/usr/bin/env python3
"""Shared vaddr <-> file-offset mapping from the actual phdrs."""
import lief

BIN = "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin"

lief.logging.disable()
_elf = lief.ELF.parse(BIN)
SEGS = []  # (vaddr_lo, vaddr_hi_filebacked, file_off_delta)
for s in _elf.segments:
    if str(s.type).endswith("LOAD"):
        SEGS.append((int(s.virtual_address),
                     int(s.virtual_address) + int(s.physical_size),
                     int(s.file_offset) - int(s.virtual_address),
                     int(s.virtual_address) + int(s.virtual_size)))


def v2f(v: int):
    """vaddr -> file offset, or None if BSS/unmapped-file."""
    for lo, hi, delta, _memhi in SEGS:
        if lo <= v < hi:
            return v + delta
    return None


if __name__ == "__main__":
    for lo, hi, delta, memhi in SEGS:
        print(f"LOAD v[{lo:#x},{hi:#x}) file=vaddr{delta:+#x} mem_end={memhi:#x}")
