#!/usr/bin/env python3
"""TIER1 step 1: independent lief structure check of decrypted-eboot.bin.

Reports phdrs, sections, entry, relocations — anything the controller's
5-PT_LOAD table might have missed. Read-only on the target.
"""
import json
import sys

import lief

BIN = "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin"

lief.logging.disable()

elf = lief.ELF.parse(BIN)
if elf is None:
    sys.exit("lief failed to parse")

out = {
    "type": str(elf.header.file_type),
    "machine": str(elf.header.machine_type),
    "entrypoint": hex(elf.entrypoint),
    "imagebase": hex(elf.imagebase),
    "phdrs": [],
    "sections": [],
    "segments_note": [],
    "relocations_count": len(list(elf.relocations)),
    "dynamic_entries": len(list(elf.dynamic_entries)),
    "static_syms": len(list(elf.symbols)),
    "dynamic_syms": len(list(elf.dynamic_symbols)),
}

for seg in elf.segments:
    out["phdrs"].append(
        {
            "type": str(seg.type),
            "flags": int(seg.flags),
            "offset": hex(seg.file_offset),
            "vaddr": hex(seg.virtual_address),
            "paddr": hex(seg.physical_address),
            "filesz": hex(seg.physical_size),
            "memsz": hex(seg.virtual_size),
            "align": hex(seg.alignment),
        }
    )

for s in elf.sections:
    out["sections"].append(
        {
            "name": s.name,
            "type": str(s.type),
            "addr": hex(s.virtual_address),
            "offset": hex(s.offset),
            "size": hex(s.size),
            "flags": hex(int(s.flags)),
        }
    )

print(json.dumps(out, indent=1))
