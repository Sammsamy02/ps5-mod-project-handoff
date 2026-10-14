#!/usr/bin/env python3
"""Shared read-only helpers for Team J's asset classifier (Mac-only).

Uses the project's EXISTING readers (outputs/2026-10-08-l25-prep/mod-manager/material-fallback:
zen.py, utoc.py, container_header.py, cityhash.py, texture_package.py, asset_registry.py,
mod_material_audit.py) by import, never by copy, and records the sha256 of every reader file it
loaded (provenance). Nothing here writes to any input. Every parse fails closed (raises or returns a
reason), and nothing names a game, mod or asset.
"""
import collections
import hashlib
import importlib
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
WHAT = '/WORKSPACE/.codex/worktrees/translation-core/what'
READERS_DEFAULT = os.path.join(WHAT, 'outputs/2026-10-08-l25-prep/mod-manager/material-fallback')
READER_MODULES = ('zen', 'utoc', 'container_header', 'cityhash', 'blake3', 'texture_package', 'ps5_tiling',
                  'asset_registry', 'mod_material_audit')

# EPackageFlags bits (public UE5 enum values)
PKG_EDITOR_ONLY = 0x40
PKG_DEVELOPER = 0x80
PKG_UNCOOKED_ONLY = 0x100
PKG_COOKED = 0x200
PKG_UNVERSIONED = 0x2000
PKG_FILTER_EDITOR_ONLY = 0x80000000
PKG_REFUSED = PKG_EDITOR_ONLY | PKG_DEVELOPER | PKG_UNCOOKED_ONLY
PKG_REQUIRED = PKG_COOKED | PKG_FILTER_EDITOR_ONLY

# EBulkDataFlags bits that change how a payload is stored (public UE5 enum values)
BULK_FLAG_NAMES = {0x1: 'PayloadAtEndOfFile', 0x2: 'SerializeCompressedZLIB', 0x4: 'ForceSingleElement',
                   0x8: 'SingleUse', 0x20: 'Unused', 0x40: 'ForceInlinePayload', 0x80: 'ForceStreamPayload',
                   0x100: 'PayloadInSeparateFile', 0x200: 'SerializeCompressedBitWindow',
                   0x400: 'ForceNotInlinePayload', 0x800: 'OptionalPayload', 0x1000: 'MemoryMappedPayload',
                   0x2000: 'Size64Bit', 0x4000: 'DuplicateNonOptionalPayload', 0x8000: 'BadDataVersion',
                   0x10000: 'NoOffsetFixUp', 0x20000: 'WorkspaceDomainPayload'}
BULK_REFUSED = 0x2 | 0x200 | 0x800 | 0x1000 | 0x4000 | 0x8000 | 0x20000

# Shader-format tokens: NUL-terminated upper-case identifiers with a known shader-format prefix.
FORMAT_TOKEN = re.compile(rb'(?<![A-Za-z0-9_])([A-Z][A-Z0-9]{1,15}_[A-Z0-9_]{2,24})\x00')
SHADER_PREFIXES = ('PCD3D_', 'SF_', 'SP_', 'VULKAN_', 'METAL_', 'GLSL_', 'OPENGL_')
# Audio codec format names a cooked SoundWave keys its compressed data by (public UE names).
AUDIO_FORMATS = ('BINKA', 'OGG', 'OPUS', 'ADPCM', 'PCM', 'RADA', 'AT9', 'XMA', 'XMA2')
PACKAGE_TAG = bytes.fromhex('c1832a9e')


class JError(ValueError):
    pass


def need(ok, why):
    if not ok:
        raise JError(why)


class Readers:
    """The existing project readers, imported from one directory, with provenance."""

    def __init__(self, path=READERS_DEFAULT):
        need(os.path.isdir(path), f'reader directory missing: {path}')
        self.path = os.path.abspath(path)
        if self.path not in sys.path:
            sys.path.insert(0, self.path)
        self.sha256 = {}
        for m in READER_MODULES:
            f = os.path.join(self.path, m + '.py')
            need(os.path.isfile(f), f'reader module missing: {f}')
            with open(f, 'rb') as fh:
                self.sha256[m + '.py'] = hashlib.sha256(fh.read()).hexdigest()
            mod = importlib.import_module(m)
            need(os.path.dirname(os.path.abspath(mod.__file__)) == self.path,
                 f'{m} was imported from {mod.__file__}, not from {self.path}')
            setattr(self, m, mod)


def file_reader(path):
    """Read-only random access; opens per call so no handle stays open."""
    def r(o, n):
        with open(path, 'rb') as fh:
            fh.seek(o)
            b = fh.read(n)
        need(len(b) == n, f'short read {path} @{o}+{n}')
        return b
    return r


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, 'rb') as fh:
        for b in iter(lambda: fh.read(1 << 20), b''):
            h.update(b)
    return h.hexdigest()


def script_objects(R, script_utoc, script_ucas):
    t = R.utoc.load(script_utoc)
    rows = t.rows_of_type(5)
    need(len(rows) == 1, f'expected exactly one ScriptObjects chunk, found {len(rows)}')
    so = R.zen.ScriptObjects(t.read_chunk(rows[0], file_reader(script_ucas)))
    need(so.count > 0, 'ScriptObjects is empty')
    return so


def package_id(R, name):
    return R.cityhash.ue_name_hash(name)


def base_index(R, tocs):
    """{package_id: (toc basename, virtual path)} for every type-1 chunk of the game's own TOCs."""
    out = {}
    for p in tocs:
        t = R.utoc.load(p)
        for row in t.rows_of_type(1):
            out[int.from_bytes(t.chunk_ids[row][:8], 'little')] = (os.path.basename(p), t.files.get(row))
    need(out, 'base TOCs list no packages')
    return out


def base_store(R, headers):
    out = {}
    for hp in headers:
        with open(hp, 'rb') as fh:
            out.update(R.container_header.parse(fh.read())[1])
    return out


class Container:
    """One IoStore container (a mod's or the game's), read-only."""

    def __init__(self, R, utoc_path, ucas_path):
        self.R = R
        self.utoc_path, self.ucas_path = utoc_path, ucas_path
        self.toc = R.utoc.load(utoc_path)
        self.read = file_reader(ucas_path)
        t = self.toc
        self.type_counts = collections.Counter(t.chunk_type(c) for c in t.chunk_ids)
        self.bulk_len = {}       # package id -> length of its BulkData (type 2) chunk index 0
        self.other_chunks = collections.Counter()
        for row, cid in enumerate(t.chunk_ids):
            ct = t.chunk_type(cid)
            pid = int.from_bytes(cid[:8], 'little')
            if ct == 2 and int.from_bytes(cid[8:10], 'little') == 0:
                self.bulk_len[pid] = t.offlen[row][1]
            elif ct not in (1, 2, 6):
                self.other_chunks[R.utoc.TYPE_NAMES.get(ct, str(ct))] += 1

    def packages(self):
        """Yield (row, virtual path, package id, export chunk bytes)."""
        t = self.toc
        for row in t.rows_of_type(1):
            yield row, t.files.get(row), int.from_bytes(t.chunk_ids[row][:8], 'little'), t.read_chunk(row, self.read)


def export_slices(R, z):
    """[(export dict, bytes)] from each export's CookedSerialOffset (field 0 of the 72-byte export
    map entry, relative to the end of the header). Independent of zen.py's bundle-order offsets.
    The exports must tile the data region exactly; only the 4-byte package tag may follow."""
    d = z.data
    exp = struct.unpack_from('<i', d, 24 + 8)[0]
    rows = []
    for i, e in enumerate(z.exports):
        soff, ssize = struct.unpack_from('<QQ', d, exp + 72 * i)
        need(ssize == e['serial_size'], 'export size mismatch')
        rows.append((soff, ssize, e))
    pos = 0
    for soff, ssize, _e in sorted(rows, key=lambda r: r[0]):
        need(soff == pos, f'exports do not tile the data region (gap/overlap at {soff})')
        pos += ssize
    tail = len(d) - z.header_size - pos
    need(tail == 0 or (tail == 4 and d[-4:] == PACKAGE_TAG), f'{tail} unexpected bytes after the exports')
    return [(e, d[z.header_size + soff:z.header_size + soff + ssize]) for soff, ssize, e in rows]


def bulk_entries(R, data):
    """Header bulk-data map [(offset, dup, size, flags, pad)] via the existing texture_package reader."""
    entries, _pos = R.texture_package.bulk_map(data)
    return entries


def flag_names(v, table):
    names = [n for b, n in sorted(table.items()) if v & b]
    rest = v & ~sum(table)
    if rest:
        names.append(f'unknown:{rest:#x}')
    return names


def shader_tokens(blob):
    out = set()
    for m in FORMAT_TOKEN.finditer(blob):
        tok = m.group(1).decode()
        if tok.startswith(SHADER_PREFIXES):
            out.add(tok)
    return sorted(out)


def class_path(z, ref, so):
    """Script path of an export's class, 'export:<name>' for an in-package class, or None."""
    c = z.describe_ref(ref, so)
    if not c:
        return None
    if c.get('kind') == 'Script':
        return c.get('path') or f"unresolved-script:{c.get('raw')}"
    if c.get('kind') == 'Export':
        return 'export:' + c['name']
    return f"package:{c.get('package')}"


def script_refs(z):
    """Every ScriptImport global index the package uses (import map + export class/super/template)."""
    out = set()
    for kind, v in z.imports:
        if kind == 'ScriptImport':
            out.add(v)
    for e in z.exports:
        for k in ('class', 'super', 'template'):
            kind, v = e[k]
            if kind == 'ScriptImport':
                out.add(v)
    return out


# Engine functions through which cooked script writes or deletes the player's save data (on the console:
# the system save-data container). A package that imports them can change the user's saves.
SAVE_WRITERS = ('/Script/Engine/GameplayStatics/SaveGameToSlot', '/Script/Engine/GameplayStatics/AsyncSaveGameToSlot',
                '/Script/Engine/AsyncActionHandleSaveGame/AsyncSaveGameToSlot',
                '/Script/Engine/LocalPlayerSaveGame/SaveGameToSlotForLocalPlayer',
                '/Script/Engine/LocalPlayerSaveGame/AsyncSaveGameToSlotForLocalPlayer')
SAVE_DELETERS = ('/Script/Engine/GameplayStatics/DeleteGameInSlot',)
SAVE_CLASSES = ('/Script/Engine/SaveGame', '/Script/Engine/LocalPlayerSaveGame')


SAVE_FUNC = re.compile(r'^(Save|Delete|Clear|Remove|Reset|Write).*(Game|Data|Slot|Save)', re.I)


def save_flags(z, so):
    """Safety flags: does this package's script touch save data or define a save-game class?
    Generic: the engine's slot functions by path, plus (any game/plugin) every imported function of a class
    whose name contains 'Save', and every function named like Save/Delete/Clear/...(Game|Data|Slot)."""
    paths = {p for p in (so.path(v) for v in script_refs(z)) if p}
    flags = []
    if paths & set(SAVE_WRITERS):
        flags.append('writes-save-slots')
    if paths & set(SAVE_DELETERS):
        flags.append('deletes-save-slots')
    other = set()
    for p in paths - set(SAVE_WRITERS) - set(SAVE_DELETERS):
        parts = p.split('/')
        if len(parts) < 5:          # /Script/<Module>/<Class>/<Function>
            continue
        klass, func = parts[3], parts[-1]
        if func.startswith('Default__') or func.endswith('__DelegateSignature'):
            continue
        if 'save' in klass.lower() or SAVE_FUNC.match(func):
            other.add(p)
    if other:
        flags.append('calls-save-functions:' + ','.join(sorted(other)[:6]))
    for e in z.exports:
        if e['outer'][0] == 'Null' and e['super'][0] == 'ScriptImport' and so.path(e['super'][1]) in SAVE_CLASSES:
            flags.append('defines-save-class')
            break
    return flags


def fingerprint(R, z, data, so):
    """Platform-relevant facts of one zen package (no property decoding)."""
    fp = {'package': z.name, 'bytes': len(data), 'flags': f'{z.flags:#x}', 'names': len(z.names),
          'exports': len(z.exports), 'imported_packages': len(z.imported_packages)}
    cls = collections.Counter(class_path(z, e['class'], so) or 'null' for e in z.exports)
    fp['export_classes'] = dict(sorted(cls.items()))
    fp['top_level_classes'] = sorted({class_path(z, e['class'], so) or 'null'
                                      for e in z.exports if e['outer'][0] == 'Null'})
    unresolved = sorted(f'{v:016x}' for v in script_refs(z) if so.path(v) is None)
    fp['unresolved_script_imports'] = unresolved
    try:
        bm = bulk_entries(R, data)
        fp['bulk_entries'] = len(bm)
        fp['bulk_flags'] = sorted({f'{e[3]:#x}' for e in bm})
        fp['bulk_refused_flags'] = sorted({f'{e[3]:#x}' for e in bm if e[3] & BULK_REFUSED})
    except Exception as ex:  # fail closed: unreadable bulk map is reported, not guessed
        fp['bulk_entries'] = None
        fp['bulk_error'] = str(ex)[:160]
    try:
        sl = export_slices(R, z)
        toks = set()
        for _e, b in sl:
            toks.update(shader_tokens(b))
        fp['shader_tokens'] = sorted(toks)
        fp['export_slices'] = 'tiled'
    except Exception as ex:
        fp['shader_tokens'] = None
        fp['export_slices'] = f'error: {str(ex)[:120]}'
    fp['audio_format_names'] = sorted(n for n in z.names if n in AUDIO_FORMATS)
    fp['save_flags'] = save_flags(z, so)
    return fp


def read_pak_entry_count(path):
    """Classic .pak (footer v8..v11, 221-byte footer, unencrypted index): -> (version, entries, mount).
    Fails closed (JError) on any other layout."""
    size = os.path.getsize(path)
    need(size >= 221, 'pak shorter than its footer')
    with open(path, 'rb') as fh:
        fh.seek(size - 221)
        foot = fh.read(221)
        enc = foot[16]
        magic, ver, ioff, isz = struct.unpack_from('<IIqq', foot, 17)
        need(magic == 0x5A6F12E1, 'pak footer magic not found at the v8-v11 position')
        need(enc == 0, 'encrypted pak index: not readable')
        need(8 <= ver <= 11, f'pak version {ver} not handled')
        need(0 <= ioff and ioff + isz <= size - 221 and isz >= 8, 'pak index range out of file')
        fh.seek(ioff)
        idx = fh.read(isz)
    n = struct.unpack_from('<i', idx, 0)[0]
    p = 4
    if n < 0:
        mount = idx[p:p - 2 * n].decode('utf-16-le', 'replace').rstrip('\0')
        p += -2 * n
    else:
        mount = idx[p:p + n].decode('utf-8', 'replace').rstrip('\0')
        p += n
    entries = struct.unpack_from('<i', idx, p)[0]
    need(0 <= entries < 10_000_000, 'implausible pak entry count')
    return ver, entries, mount
