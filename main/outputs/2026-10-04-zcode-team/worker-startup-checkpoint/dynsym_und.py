#!/usr/bin/env python3
"""Parse ELF x86-64 .dynsym and print undefined (import) symbols + counts."""
import struct, sys, json

def dynsym_und(path):
    d = open(path, 'rb').read()
    assert d[:4] == b'\x7fELF', 'not ELF'
    is64 = d[4] == 2
    e_shoff, = struct.unpack_from('<Q', d, 0x28)
    e_shentsize, e_shnum, e_shstrndx = struct.unpack_from('<HHH', d, 0x3A)
    secs = []
    for i in range(e_shnum):
        off = e_shoff + i * e_shentsize
        name, typ, flags, addr, offset, size, link, info, align, entsize = struct.unpack_from('<IIQQQQIIQQ', d, off)
        secs.append(dict(name=name, typ=typ, offset=offset, size=size, link=link, entsize=entsize))
    shstr = secs[e_shstrndx]
    def sname(n):
        s = shstr['offset'] + n
        e = d.index(b'\0', s)
        return d[s:e].decode()
    out = []
    for s in secs:
        nm = sname(s['name'])
        if nm in ('.dynsym', '.symtab') and s['entsize']:
            strtab = secs[s['link']]
            n = s['size'] // s['entsize']
            for i in range(n):
                off = s['offset'] + i * s['entsize']
                st_name, st_info, st_other, st_shndx, st_value, st_size = struct.unpack_from('<IBBHQQ', d, off)
                if st_shndx == 0 and st_name:  # SHN_UNDEF with a name => import
                    ps = strtab['offset'] + st_name
                    pe = d.index(b'\0', ps)
                    out.append((nm, d[ps:pe].decode(errors='replace')))
            break
    return out

if __name__ == '__main__':
    res = {}
    for p in sys.argv[1:]:
        syms = dynsym_und(p)
        res[p] = syms
        print(f'{p}: {len(syms)} UND imports')
    if len(sys.argv) == 3:
        a = set(n for _, n in res[sys.argv[1]])
        b = set(n for _, n in res[sys.argv[2]])
        only_a = sorted(a - b)
        only_b = sorted(b - a)
        print(f'-- only in {sys.argv[1]} ({len(only_a)}):')
        for s in only_a: print('  ', s)
        print(f'-- only in {sys.argv[2]} ({len(only_b)}):')
        for s in only_b: print('  ', s)
