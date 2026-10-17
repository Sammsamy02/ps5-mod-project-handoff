#!/usr/bin/env python3
"""Strict static acceptance of this freestanding candidate, never execution.

This verifies a restricted ELF subset. A passed inspection is not loader,
firmware, runtime, privilege, or gameplay compatibility evidence.
"""
# SPDX-License-Identifier: GPL-3.0-or-later
import argparse
import hashlib
import json
import pathlib
import struct
import sys

FORBIDDEN = ("kernel_", "__kernel_", "__crt_", "__patch_", "sceKernel", "socket", "connect", "send", "recv", "dlopen", "dlsym", "mprotect", "ptrace", "syscall")
SYSTEM_CALL_ENCODINGS = (b"\x0f\x05", b"\x0f\x34", b"\xcd\x80")
PAGE_SIZE = 16384
UINT64_MAX = (1 << 64) - 1

def verify_bytes(data):
    def require(condition, reason):
        if not condition: raise ValueError(reason)
    def span(offset, length):
        require(0 <= offset <= len(data) and 0 <= length <= len(data)-offset, "truncated ELF range")
        return data[offset:offset+length]
    def unpack(fmt, offset):
        return struct.unpack(fmt, span(offset, struct.calcsize(fmt)))
    def name(table, offset):
        require(0 <= offset < len(table), "invalid ELF string offset")
        end = table.find(b"\0", offset)
        require(end >= 0, "unterminated ELF string")
        try: return table[offset:end].decode("ascii")
        except UnicodeDecodeError: raise ValueError("non-ASCII ELF symbol/section name")
    def dynamic(content):
        require(len(content) % 16 == 0, "malformed dynamic table")
        terminated = not content
        for at in range(0, len(content), 16):
            tag, value = struct.unpack_from("<qQ", content, at)
            if tag == 0:
                terminated = True
                break
            require(tag not in (1, 22), "needed library/text relocation is forbidden")
            require(not (tag in (2, 8, 18, 35, 36, 37) and value), "dynamic relocation data is forbidden")
            require(not (tag in (12, 13, 25, 26, 27, 28, 32, 33) and value), "dynamic initialization/finalization is forbidden")
            require(not (tag == 30 and value & 4), "DF_TEXTREL is forbidden")
        require(terminated, "unterminated dynamic table")

    require(span(0, 7) == b"\x7fELF\x02\x01\x01", "expected ELF64 little-endian version 1, not desktop Mach-O")
    header = unpack("<16sHHIQQQIHHHHHH", 0)
    _, kind, machine, version, entry, phoff, shoff, _, ehsize, phsize, phnum, shsize, shnum, shnames = header
    require(kind == 3 and machine == 62 and version == 1, "expected x86-64 position-independent ELF")
    require(ehsize == 64 and phsize == 56 and shsize == 64, "unexpected ELF structure size")
    require(0 < phnum < 65535 and 0 < shnum < 65535 and shnames < shnum, "unsupported missing/extended ELF tables")
    span(phoff, phnum*phsize); span(shoff, shnum*shsize)
    executable_entry = False
    loads = 0
    load_pages = []
    for i in range(phnum):
        ptype, flags, offset, address, _, filesize, memsize, alignment = unpack("<IIQQQQQQ", phoff+i*phsize)
        require(ptype != 3, "interpreter dependency is forbidden")
        if ptype == 1:
            loads += 1
            span(offset, filesize)
            require(memsize >= filesize and alignment == PAGE_SIZE, "load segment must have 16 KiB alignment and valid size")
            require(offset % alignment == address % alignment, "incongruent load segment address/offset")
            if memsize:
                require(address % PAGE_SIZE == 0, "load segment start must be 16 KiB aligned")
                require(memsize <= UINT64_MAX - address, "load address range overflows uint64")
                end = address + memsize
                require(end <= UINT64_MAX - (PAGE_SIZE - 1), "page-rounded load address range overflows uint64")
                page_end = (end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1)
                require(all(page_end <= start or address >= stop for start, stop in load_pages),
                        "load segment pages overlap")
                load_pages.append((address, page_end))
            if flags & 1:
                require(not any(code in span(offset, filesize) for code in SYSTEM_CALL_ENCODINGS), "direct system-call encoding in executable load segment")
            if flags & 1 and address <= entry < address+filesize: executable_entry = True
        elif ptype == 2:
            dynamic(span(offset, filesize))
    require(loads > 0 and executable_entry, "entry is outside executable file-backed load segments")

    sections = [unpack("<IIQQQQIIQQ", shoff+i*shsize) for i in range(shnum)]
    names_section = sections[shnames]
    require(names_section[1] == 3, "missing section-name string table")
    section_names = span(names_section[4], names_section[5])
    symbol_tables = 0
    starts = []
    for section in sections:
        sname, stype, flags, _, offset, size, link, _, _, entsize = section
        section_name = name(section_names, sname)
        content = b"" if stype == 8 else span(offset, size)
        require(not (stype in (4, 9) and size), "relocation entries are forbidden")
        require(not (stype in (14, 15, 16) and size), "initialization/finalization arrays are forbidden")
        require(section_name != ".interp", "interpreter dependency is forbidden")
        if flags & 4:
            require(not any(code in content for code in SYSTEM_CALL_ENCODINGS), "direct system-call encoding in executable section")
        if stype == 6:
            dynamic(content)
        if stype in (2, 11):
            symbol_tables += 1
            require(entsize == 24 and size % 24 == 0 and link < shnum, "malformed symbol table")
            linked = sections[link]
            require(linked[1] == 3, "symbol string table is missing")
            table = span(linked[4], linked[5])
            for at in range(0, size, 24):
                symname, info, _, index, value, symsize = struct.unpack_from("<IBBHQQ", content, at)
                symbol = name(table, symname)
                if symbol:
                    require(index != 0, "undefined import: "+symbol)
                    require(not any(word in symbol for word in FORBIDDEN), "forbidden runtime/mutation symbol: "+symbol)
                    if symbol == "_start":
                        require(info & 15 == 2 and 0 < index < shnum, "entry symbol must be a defined function")
                        require(sections[index][2] & 4 and symsize > 0, "entry symbol must occupy an executable section")
                        require(sections[index][3] <= value and value + symsize <= sections[index][3] + sections[index][5], "entry symbol lies outside its section")
                        starts.append(value)
    require(symbol_tables > 0 and starts and all(value == entry for value in starts), "auditable _start function must match ELF entry")
    return {"format":"ELF64 little-endian PIE", "machine":"x86_64", "entry":entry, "load_segments":loads,
            "segment_alignment":PAGE_SIZE, "load_starts_aligned":True, "load_pages_disjoint":True,
            "load_address_ranges_bounded":True, "load_page_ranges":[{"start":start, "end":stop} for start, stop in load_pages],
            "needed_libraries":0, "undefined_imports":0, "relocations":0, "constructors":0,
            "runtime_execution":"unmeasured", "sha256":hashlib.sha256(data).hexdigest(), "size":len(data)}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("artifact", type=pathlib.Path)
    args = parser.parse_args()
    try:
        result = verify_bytes(args.artifact.read_bytes())
    except (OSError, ValueError) as error:
        print("native ELF rejected: "+str(error), file=sys.stderr)
        return 1
    print(json.dumps(result, indent=2))
    return 0

if __name__ == "__main__": sys.exit(main())
