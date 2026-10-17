#!/usr/bin/env python3
"""Mac-only, read-only ELF64 section/symbol summary (stdlib only; no llvm-readelf on this Mac).

Usage:
  elf_sections.py sizes  DIR_C DIR_CPP     compare per-section sizes of the 33 luaraw-*.o in two dirs
  elf_sections.py syms   FILE.o            print undefined + a few interesting defined symbols
Section name -> total bytes; prints counts and fails (exit 1) when 0 objects were read.
"""
import struct, sys, glob, os, collections


def read_elf(path):
    b = open(path, 'rb').read()
    if b[:4] != b'\x7fELF' or b[4] != 2:
        raise ValueError(f'{path}: not ELF64')
    shoff, = struct.unpack_from('<Q', b, 0x28)
    shentsize, shnum, shstrndx = struct.unpack_from('<HHH', b, 0x3A)
    secs = []
    for i in range(shnum):
        o = shoff + i * shentsize
        name, typ, flags, addr, off, size, link, info, align, entsize = struct.unpack_from('<IIQQQQIIQQ', b, o)
        secs.append(dict(name_off=name, type=typ, flags=flags, off=off, size=size, link=link, info=info, entsize=entsize))
    stroff = secs[shstrndx]['off']
    for s in secs:
        e = b.index(b'\0', stroff + s['name_off'])
        s['name'] = b[stroff + s['name_off']:e].decode()
    return b, secs


def symbols(b, secs):
    out = []
    for s in secs:
        if s['type'] == 2:  # SYMTAB
            strsec = secs[s['link']]
            n = s['size'] // 24
            for i in range(n):
                o = s['off'] + i * 24
                nm, info, other, shndx, val, sz = struct.unpack_from('<IBBHQQ', b, o)
                e = b.index(b'\0', strsec['off'] + nm)
                out.append((b[strsec['off'] + nm:e].decode(), info >> 4, info & 15, shndx))
    return out


def merged_name(n):
    # fold per-function sections (-ffunction-sections) into their family
    for p in ('.text.', '.rodata.', '.data.rel.ro.', '.data.', '.bss.', '.rela.text.', '.rela.eh_frame', '.eh_frame', '.gcc_except_table.', '.gcc_except_table', '.rela.gcc_except_table', '.rela.rodata.', '.rela.data.rel.ro.', '.rela.data.'):
        if n.startswith(p):
            return p.rstrip('.') + ('*' if p.endswith('.') else '')
    return n


def totals(paths):
    t = collections.Counter()
    for p in paths:
        b, secs = read_elf(p)
        for s in secs:
            if s['type'] in (0,):
                continue
            t[merged_name(s['name'])] += s['size']
    return t


def main(a):
    if len(a) >= 2 and a[0] == 'sizes':
        pc = sorted(glob.glob(os.path.join(a[1], 'luaraw-*.o')))
        pp = sorted(glob.glob(os.path.join(a[2], 'luaraw-*.o')))
        tc, tp = totals(pc), totals(pp)
        print(f'objects: C={len(pc)} C++={len(pp)}')
        keys = ['.text*', '.rodata*', '.data*', '.data.rel.ro*', '.bss*', '.eh_frame', '.gcc_except_table*', '.rela.eh_frame', '.rela.text*', '.rela.gcc_except_table']
        print(f'{"section":28s}{"C bytes":>12s}{"C++ bytes":>12s}{"delta":>10s}')
        for k in keys:
            print(f'{k:28s}{tc.get(k,0):>12d}{tp.get(k,0):>12d}{tp.get(k,0)-tc.get(k,0):>+10d}')
        loaded_c = sum(tc.get(k, 0) for k in ['.text*', '.rodata*', '.data*', '.data.rel.ro*', '.eh_frame', '.gcc_except_table*'])
        loaded_p = sum(tp.get(k, 0) for k in ['.text*', '.rodata*', '.data*', '.data.rel.ro*', '.eh_frame', '.gcc_except_table*'])
        print(f'{"sum(text+ro+data+eh+lsda)":28s}{loaded_c:>12d}{loaded_p:>12d}{loaded_p-loaded_c:>+10d}')
        return 0 if (pc and pp) else 1
    if len(a) >= 2 and a[0] == 'syms':
        b, secs = read_elf(a[1])
        print('sections:', ', '.join(f"{s['name']}({s['size']})" for s in secs if s['name'].startswith(('.eh', '.gcc', '.text', '.init', '.fini')))[:2000])
        sy = symbols(b, secs)
        und = sorted({n for n, bind, typ, shndx in sy if shndx == 0 and n})
        print(len(und), 'undefined:', ' '.join(und))
        return 0
    if len(a) >= 2 and a[0] == 'info':
        # program headers + unwind-related sections of an ELF64 (PRX or object)
        b, secs = read_elf(a[1])
        phoff, = struct.unpack_from('<Q', b, 0x20)
        phentsize, phnum = struct.unpack_from('<HH', b, 0x36)
        names = {1: 'PT_LOAD', 2: 'PT_DYNAMIC', 3: 'PT_INTERP', 4: 'PT_NOTE', 6: 'PT_PHDR', 7: 'PT_TLS', 0x6474e550: 'PT_GNU_EH_FRAME',
                 0x6474e551: 'PT_GNU_STACK', 0x6474e552: 'PT_GNU_RELRO'}
        print(f'file={a[1]} size={len(b)}')
        for i in range(phnum):
            o = phoff + i * phentsize
            typ, flags, off, vaddr, paddr, filesz, memsz, align = struct.unpack_from('<IIQQQQQQ', b, o)
            print(f'  phdr[{i}] {names.get(typ, hex(typ)):16s} off=0x{off:x} vaddr=0x{vaddr:x} filesz=0x{filesz:x} memsz=0x{memsz:x} flags={flags}')
        for s in secs:
            if s['name'] in ('.eh_frame', '.eh_frame_hdr', '.gcc_except_table', '.init_array', '.fini_array', '.text', '.dynsym'):
                print(f"  section {s['name']:20s} size=0x{s['size']:x}")
        print(f'  sections total: {len(secs)}')
        return 0
    print(__doc__); return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
