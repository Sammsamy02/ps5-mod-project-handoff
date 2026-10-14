#!/usr/bin/env python3
"""Minimal read-only UE5 Zen package header + ScriptObjects reader (Mac-only).

Independent Python implementation of public format facts from retoc
(zen.rs:26-125, name_map.rs:47-66; MIT, retained locally under
.superpowers/native-dependency-spike/unreal-dependency-compatibility-1/source/retoc).
Supports only unversioned packages with the UE5.5+/5.6 summary layout (cell
import/export maps present, container header version 5). Fails closed otherwise.
Does NOT decode tagged/unversioned property data.
"""
import struct

HASH_ALGO = 0xC1640000
NULL = 0xFFFFFFFFFFFFFFFF


class ZenError(ValueError):
    pass


def need(ok, why):
    if not ok:
        raise ZenError(why)


def read_name_batch(d, pos):
    count = struct.unpack_from('<I', d, pos)[0]; pos += 4
    if count == 0:
        return [], pos
    need(count <= 200_000, 'name count over cap')
    nbytes, algo = struct.unpack_from('<IQ', d, pos); pos += 12
    need(algo == HASH_ALGO, 'unsupported name hash algorithm')
    pos += 8 * count
    lens = [struct.unpack_from('>H', d, pos + 2 * i)[0] for i in range(count)]; pos += 2 * count
    start = pos
    names = []
    for w in lens:
        wide = bool(w & 0x8000)
        n = (w & 0x7FFF) * (2 if wide else 1)
        need(pos + n <= len(d), 'name exceeds buffer')
        names.append(d[pos:pos + n].decode('utf-16-le' if wide else 'utf-8'))
        pos += n
    need(pos - start == nbytes, 'name batch byte count mismatch')
    return names, pos


def obj_index(raw):
    if raw == NULL:
        return ('Null', None)
    kind = raw >> 62
    need(kind < 3, 'invalid object index kind')
    if kind == 0:
        return ('Export', raw & 0xFFFFFFFF)
    if kind == 1:
        return ('ScriptImport', raw)
    v = raw & ((1 << 62) - 1)
    return ('PackageImport', (v >> 32, raw & 0xFFFFFFFF))


class ScriptObjects:
    def __init__(self, d):
        names, pos = read_name_batch(d, 0)
        count = struct.unpack_from('<I', d, pos)[0]; pos += 4
        need(pos + 32 * count == len(d), 'ScriptObjects length mismatch')
        self.objs = {}
        for i in range(count):
            w, num, gidx, outer, cdo = struct.unpack_from('<IIQQQ', d, pos + 32 * i)
            need(w >> 30 == 2 and (w & 0x3FFFFFFF) < len(names), 'bad script object name')
            nm = names[w & 0x3FFFFFFF] + (f'_{num - 1}' if num else '')
            self.objs[gidx] = (nm, outer)
        self.count = count

    def path(self, idx):
        parts, seen = [], set()
        while idx != NULL:
            need(idx not in seen and len(seen) < 64, 'cyclic script outer chain')
            seen.add(idx)
            if idx not in self.objs:
                return None
            nm, idx = self.objs[idx]
            parts.append(nm)
        return '/'.join(reversed(parts))


class ZenPackage:
    def __init__(self, d, header_only=False):
        """header_only=True: d may stop at HeaderSize (a fetched header prefix);
        the export map is parsed but export data is not available."""
        need(len(d) >= 64, 'package too short')
        (has_ver, self.header_size, nidx, nnum, self.flags, self.cooked_header_size) = struct.unpack_from('<6I', d, 0)
        need(has_ver == 0, 'versioned zen package unsupported')
        need(64 <= self.header_size <= len(d), 'bad header size')
        (iph, imp, exp, bundle, dep_h, dep_e, ipn, cimp, cexp) = struct.unpack_from('<9i', d, 24)
        order = [60, iph, imp, exp, cimp, cexp, bundle, dep_h, dep_e, ipn, self.header_size]
        need(order == sorted(order), 'non-monotonic zen header offsets')
        need((cimp - exp) % 72 == 0, 'export map misaligned')
        self.names, npos = read_name_batch(d, 60)
        need(npos <= iph, 'name batch overruns header')
        self.name = self.mapped(nidx, nnum)
        self.import_hashes = list(struct.unpack_from('<%dQ' % ((imp - iph) // 8), d, iph))
        self.imports = [obj_index(v) for v in struct.unpack_from('<%dQ' % ((exp - imp) // 8), d, imp)]
        self.exports = []
        for i in range((cimp - exp) // 72):
            (soff, ssize, eidx, enum, outer, cls, sup, tmpl, pub, oflags, filt, _pad) = \
                struct.unpack_from('<QQIIQQQQQIB3s', d, exp + 72 * i)
            self.exports.append({'index': i, 'name': self.mapped(eidx, enum), 'serial_size': ssize,
                                 'outer': obj_index(outer), 'class': obj_index(cls), 'super': obj_index(sup),
                                 'template': obj_index(tmpl), 'public_hash': pub, 'object_flags': oflags})
        # Export bundle: (local export index, command 0=Create 1=Serialize)
        entries = [struct.unpack_from('<II', d, bundle + 8 * i) for i in range((dep_h - bundle) // 8)]
        pos = self.header_size
        for ei, cmd in entries:
            if cmd == 1:
                need(ei < len(self.exports), 'bundle export index out of range')
                e = self.exports[ei]
                e['data_offset'] = pos
                pos += e['serial_size']
        need(header_only or pos <= len(d), 'export data exceeds package chunk')
        self.header_only = header_only
        self.imported_packages = []
        if ipn > 0 and ipn < self.header_size:
            names, p = read_name_batch(d, ipn)
            nums = struct.unpack_from('<%di' % len(names), d, p) if names else ()
            self.imported_packages = [n + (f'_{k - 1}' if k else '') for n, k in zip(names, nums)]
        self.data = d

    def mapped(self, idx, num):
        need(idx >> 30 == 0 and (idx & 0x3FFFFFFF) < len(self.names), 'bad mapped name')
        return self.names[idx & 0x3FFFFFFF] + (f'_{num - 1}' if num else '')

    def export_bytes(self, e):
        o = e.get('data_offset')
        if o is None or self.header_only:
            return None
        return self.data[o:o + e['serial_size']]

    def top_level_exports(self, script):
        """[{name, class (script path or None), public_hash}] for exports with no outer."""
        out = []
        for e in self.exports:
            if e['outer'][0] != 'Null':
                continue
            c = self.describe_ref(e['class'], script)
            out.append({'name': e['name'], 'class': c.get('path') if c and c.get('kind') == 'Script' else None,
                        'public_hash': f"{e['public_hash']:016x}"})
        return out

    def describe_ref(self, ref, script):
        kind, v = ref
        if kind == 'Null':
            return None
        if kind == 'Export':
            return {'kind': 'Export', 'name': self.exports[v]['name']}
        if kind == 'ScriptImport':
            return {'kind': 'Script', 'path': script.path(v) if script else None, 'raw': f'{v:016x}'}
        pkg_i, hash_i = v
        pkg = self.imported_packages[pkg_i] if pkg_i < len(self.imported_packages) else None
        h = self.import_hashes[hash_i] if hash_i < len(self.import_hashes) else None
        return {'kind': 'Package', 'package': pkg, 'public_hash': f'{h:016x}' if h is not None else None}
