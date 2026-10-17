#!/usr/bin/env python3
"""Read-only reader for a cooked UE5 AssetRegistry.bin (the game's own file,
shipped in its main pak). Mac-only. Independent Python implementation from the
file's own structure; no Epic or Sony source used.

Layout read (every count is checked and the store must end exactly on its end
magic, or the reader fails closed):
  16-byte version GUID, u32 version, u32 'filter editor-only' flag,
  name batch (same format as zen package name maps),
  tag-value store: u32 begin magic 0x12345679, 11 u32 counts
     (numberless names, names, numberless export paths, export paths, texts,
      ansi offsets, wide offsets, ansi bytes, wide chars, numberless pairs, pairs),
     u32 text bytes + texts, then the arrays in that order, u32 end magic 0x87654321,
  i32 asset count, then per asset: package path, class package, class name,
     package name, asset name (each a name reference: u32 index, high bit =
     a u32 number follows), u64 tag-map handle (low u32 = first pair, bits 32..47
     = pair count, bit 63 = numberless keys), u32 bundle count (must be 0),
     u32 chunk count + chunk ids, u32 package flags.
Anything after the asset list (dependency / package data) is not parsed; its
byte count is reported.
Tag values: low 3 bits of a value id = kind (0 ansi string, 1 wide string,
2 numberless name, 3 name, 4 numberless export path, 5 export path, 6 text),
the rest = index.
"""
import struct
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import zen  # noqa: E402

BEGIN_MAGIC = 0x12345679
END_MAGIC = 0x87654321
MAX_ASSETS = 2_000_000


class RegistryError(ValueError):
    pass


def need(ok, why):
    if not ok:
        raise RegistryError(why)


class _R:
    def __init__(self, d, pos, names):
        self.d, self.p, self.names = d, pos, names

    def u32(self):
        need(self.p + 4 <= len(self.d), 'truncated registry')
        v = struct.unpack_from('<I', self.d, self.p)[0]
        self.p += 4
        return v

    def name(self, numbered=True):
        v = self.u32()
        num = 0
        if numbered and v & 0x80000000:
            num = self.u32()
        i = v & 0x7FFFFFFF if numbered else v
        need(i < len(self.names), 'registry name index out of range')
        return self.names[i] + (f'_{num - 1}' if num else '')

    def raw(self, n):
        need(n >= 0 and self.p + n <= len(self.d), 'truncated registry array')
        b = self.d[self.p:self.p + n]
        self.p += n
        return b


class Registry:
    def __init__(self, d):
        need(len(d) > 64, 'registry too short')
        self.version = struct.unpack_from('<I', d, 16)[0]
        self.names, pos = zen.read_name_batch(d, 24)
        need(len(self.names) > 0, 'registry has no names')
        r = _R(d, pos, self.names)
        need(r.u32() == BEGIN_MAGIC, 'tag store begin magic missing (unsupported registry layout)')
        n = [r.u32() for _ in range(11)]
        tb = r.u32()
        t0 = r.p
        texts = []
        for _ in range(n[4]):
            ln = struct.unpack('<i', r.raw(4))[0]
            texts.append(r.raw(ln).rstrip(b'\0').decode('utf-8', 'replace') if ln >= 0
                         else r.raw(-2 * ln).decode('utf-16-le', 'replace').rstrip('\0'))
        need(r.p == t0 + tb, 'text block size mismatch')
        nl = [r.name(False) for _ in range(n[0])]
        nm = [r.name() for _ in range(n[1])]
        nep = [[r.name(False) for _ in range(4)] for _ in range(n[2])]
        ep = [[r.name() for _ in range(4)] for _ in range(n[3])]
        ao = struct.unpack('<%dI' % n[5], r.raw(4 * n[5]))
        wo = struct.unpack('<%dI' % n[6], r.raw(4 * n[6]))
        ansi = r.raw(n[7])
        wide = r.raw(2 * n[8])
        npairs = [(r.name(False), r.u32()) for _ in range(n[9])]
        pairs = [(r.name(), r.u32()) for _ in range(n[10])]
        need(r.u32() == END_MAGIC, 'tag store end magic missing (counts do not add up)')
        self._v = (texts, nl, nm, nep, ep, ao, wo, ansi, wide)
        na = struct.unpack('<i', r.raw(4))[0]
        need(0 < na <= MAX_ASSETS, 'bad asset count')
        self.assets = []
        for _ in range(na):
            path = r.name()
            cls = r.name() + '.' + r.name()
            pkg = r.name()
            aname = r.name()
            lo, hi = r.u32(), r.u32()
            need(r.u32() == 0, 'asset bundles present (layout not supported)')
            nch = r.u32()
            need(nch < 4096, 'bad chunk count')
            r.raw(4 * nch)
            flags = r.u32()
            need(hi & 0x7FFF0000 == 0, 'tag map handle has unknown bits set')
            cnt, numberless = hi & 0xFFFF, bool(hi >> 31)
            src = npairs if numberless else pairs
            need(lo + cnt <= len(src), 'tag map handle out of range')
            tags = {k: self._value(v) for k, v in src[lo:lo + cnt]}
            self.assets.append({'path': path, 'class': cls, 'package': pkg, 'name': aname,
                                'tags': tags, 'package_flags': flags})
        self.tail_bytes = len(d) - r.p
        self.counts = {'names': len(self.names), 'assets': na, 'tag_pairs': n[9] + n[10]}
        self.by_package = {}
        for a in self.assets:
            # the package's primary asset: the one named like the package
            if a['package'].rsplit('/', 1)[-1] == a['name']:
                self.by_package[a['package']] = a

    def _value(self, vid):
        texts, nl, nm, nep, ep, ao, wo, ansi, wide = self._v
        kind, i = vid & 7, vid >> 3
        if kind == 0:
            need(i < len(ao), 'ansi value index out of range')
            e = ansi.find(b'\0', ao[i])
            need(e >= 0, 'ansi value not terminated')
            return ansi[ao[i]:e].decode('utf-8', 'replace')
        if kind == 1:
            need(i < len(wo), 'wide value index out of range')
            o = 2 * wo[i]
            e = o
            while e + 2 <= len(wide) and wide[e:e + 2] != b'\0\0':
                e += 2
            need(e + 2 <= len(wide), 'wide value not terminated')
            return wide[o:e].decode('utf-16-le', 'replace')
        arrays = {2: nl, 3: nm, 4: nep, 5: ep, 6: texts}
        if kind in arrays:
            need(i < len(arrays[kind]), 'tag value index out of range')
        if kind == 2:
            return nl[i]
        if kind == 3:
            return nm[i]
        if kind == 4:
            c_pkg, c_obj, obj, pkg = nep[i]
            return f"{c_pkg}.{c_obj}'{pkg}.{obj}'"
        if kind == 5:
            c_pkg, c_obj, obj, pkg = ep[i]
            return f"{c_pkg}.{c_obj}'{pkg}.{obj}'"
        if kind == 6:
            return texts[i]
        raise RegistryError('unknown tag value kind')

    def asset(self, package):
        return self.by_package.get(package)

    def parent_package(self, package):
        """MaterialInstance 'Parent' tag -> parent package path, or None."""
        a = self.asset(package)
        if not a:
            return None
        v = a['tags'].get('Parent')
        if not v or v == 'None' or "'" not in v:
            return None
        return v.split("'")[1].rsplit('.', 1)[0]


def load(path):
    with open(path, 'rb') as fh:
        return Registry(fh.read())


if __name__ == '__main__':
    reg = load(sys.argv[1])
    print(f'asset_registry: {reg.counts}, {reg.tail_bytes} trailing bytes not parsed')
