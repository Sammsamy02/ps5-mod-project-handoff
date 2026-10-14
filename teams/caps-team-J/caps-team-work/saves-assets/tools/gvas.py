#!/usr/bin/env python3
"""Read-only checker for a PC UE save file (.sav, 'GVAS' = UGameplayStatics SaveGameToSlot output).

STATUS: Mac-only prototype written from public UE format facts; NOT yet run on a real save of the
target game (none is on the Mac). It only reads, and it refuses (fails closed) on anything it does
not fully understand, so it can be used as the first gate of a save import.

Layout read:
  header   u32 'GVAS' magic, i32 SaveGameFileVersion (1..3 handled),
           FPackageFileVersion (i32 UE4, i32 UE5; v3+) or i32 UE4 (v<3),
           FEngineVersion {u16 major, u16 minor, u16 patch, u32 changelist, FString branch},
           (v2+) i32 custom-version format (3 = optimized) + i32 n + n x {16-byte guid, i32 version},
           FString SaveGameClassName.
  body     the save object's tagged properties until a tag named 'None', then a 4-byte bool
           (object GUID present), then nothing.
  tags     UE5 file version >= 1012 (complete type names):
             FString name; type tree = pre-order nodes {FString name, i32 inner count};
             i32 size; u8 flags (0x01 array index, 0x02 property guid, 0x04 extensions,
             0x08 binary/native value, 0x10 bool true); [i32 index]; [16-byte guid]; [u8 ext (refused
             if it carries overridable data)]; value (size bytes)
           older: FString name, FString type, i32 size, i32 array index, type extras
             (Struct: FString struct + 16-byte guid; Bool: u8; Byte/Enum: FString enum;
              Array/Set/Optional: FString inner; Map: FString key + FString value),
             u8 has-guid [+16], (UE5 >= 1011) u8 extensions; value (size bytes)
  FString  i32 n; n>0: n bytes UTF-8/ANSI incl. NUL; n<0: -n UTF-16 units incl. NUL.
Values are skipped by their declared size (no guessing); simple scalars are decoded for display.
New format only: struct values not flagged binary/native are walked as nested tagged lists (depth-capped);
old-format struct values are skipped by size, not walked.
"""
import argparse
import json
import struct
import sys

MAGIC = 0x53415647                 # 'GVAS'
UE5_EXTENSIONS = 1011              # PROPERTY_TAG_EXTENSION_AND_OVERRIDABLE_SERIALIZATION
UE5_COMPLETE_TYPE_NAME = 1012      # PROPERTY_TAG_COMPLETE_TYPE_NAME
MAX_DEPTH = 32
MAX_TAGS = 200_000
SCALARS = {'IntProperty': '<i', 'UInt32Property': '<I', 'Int64Property': '<q', 'UInt64Property': '<Q',
           'Int16Property': '<h', 'UInt16Property': '<H', 'Int8Property': '<b', 'FloatProperty': '<f',
           'DoubleProperty': '<d'}


class SaveError(ValueError):
    pass


def need(ok, why):
    if not ok:
        raise SaveError(why)


class Reader:
    def __init__(self, b):
        self.b, self.p = b, 0

    def take(self, n):
        need(n >= 0 and self.p + n <= len(self.b), f'truncated at {self.p} (+{n})')
        v = self.b[self.p:self.p + n]
        self.p += n
        return v

    def i32(self):
        return struct.unpack('<i', self.take(4))[0]

    def u32(self):
        return struct.unpack('<I', self.take(4))[0]

    def u16(self):
        return struct.unpack('<H', self.take(2))[0]

    def u8(self):
        return self.take(1)[0]

    def fstring(self):
        n = self.i32()
        if n == 0:
            return ''
        need(-65536 <= n <= 65536, f'implausible string length {n} at {self.p - 4}')
        if n > 0:
            raw = self.take(n)
            need(raw[-1:] == b'\0', 'string not NUL-terminated')
            return raw[:-1].decode('utf-8', 'replace')
        raw = self.take(-2 * n)
        need(raw[-2:] == b'\0\0', 'wide string not NUL-terminated')
        return raw[:-2].decode('utf-16-le', 'replace')


def read_header(r):
    h = {}
    need(r.u32() == MAGIC, "not a GVAS save (magic missing): refuse (old/custom format)")
    h['save_game_file_version'] = v = r.i32()
    need(1 <= v <= 3, f'SaveGameFileVersion {v} not handled')
    if v >= 3:
        h['ue4_version'], h['ue5_version'] = r.i32(), r.i32()
    else:
        h['ue4_version'], h['ue5_version'] = r.i32(), 0
    h['engine'] = {'major': r.u16(), 'minor': r.u16(), 'patch': r.u16(), 'changelist': r.u32(),
                   'branch': r.fstring()}
    h['custom_versions'] = []
    if v >= 2:
        fmt = r.i32()
        need(fmt == 3, f'custom-version format {fmt} not handled (only optimized = 3)')
        n = r.i32()
        need(0 <= n <= 4096, 'implausible custom-version count')
        for _ in range(n):
            g = r.take(16)
            h['custom_versions'].append({'guid': g.hex(), 'version': r.i32()})
    h['save_game_class'] = r.fstring()
    need(h['save_game_class'], 'empty save game class name')
    return h


def _type_tree(r):
    """Pre-order {name, inner count} nodes -> nested [name, [children...]]."""
    nodes, remaining = [], 1
    while remaining > 0:
        need(len(nodes) < 256, 'type name too deep')
        name, inner = r.fstring(), r.i32()
        need(0 <= inner <= 64, f'implausible type parameter count {inner}')
        nodes.append((name, inner))
        remaining += inner - 1

    def build(i):
        name, inner = nodes[i]
        kids, j = [], i + 1
        for _ in range(inner):
            k, j = build(j)
            kids.append(k)
        return [name, kids], j
    tree, used = build(0)
    need(used == len(nodes), 'type tree does not consume all nodes')
    return tree


def _type_str(t):
    return t[0] + (('(' + ','.join(_type_str(k) for k in t[1]) + ')') if t[1] else '')


def read_tag(r, ue5):
    name = r.fstring()
    if name == 'None':
        return None
    tag = {'name': name}
    if ue5 >= UE5_COMPLETE_TYPE_NAME:
        tree = _type_tree(r)
        tag['type'], tag['type_tree'] = tree[0], tree
        tag['size'] = r.i32()
        flags = r.u8()
        need(not flags & ~0x3f, f'unknown tag flags {flags:#x}')
        tag['array_index'] = r.i32() if flags & 0x01 else 0
        if flags & 0x02:
            tag['guid'] = r.take(16).hex()
        if flags & 0x04:
            ext = r.u8()
            need(ext == 0, f'property extensions {ext:#x} (overridable/reserved data) refused')
        tag['binary'] = bool(flags & 0x08)
        need(not flags & 0x20, f'{name}: value skipped at save time (SkippedSerialize): refused')
        if tag['type'] == 'BoolProperty':
            tag['value'] = bool(flags & 0x10)
            need(tag['size'] == 0, f'{name}: bool with a {tag["size"]}-byte value')
    else:
        t = r.fstring()
        tag['type'] = t
        tag['size'] = r.i32()
        tag['array_index'] = r.i32()
        tree = [t, []]
        if t == 'StructProperty':
            s = r.fstring()
            r.take(16)
            tree[1] = [[s, []]]
        elif t == 'BoolProperty':
            tag['value'] = bool(r.u8())
            need(tag['size'] == 0, f'{name}: bool with a {tag["size"]}-byte value')
        elif t in ('ByteProperty', 'EnumProperty'):
            tree[1] = [[r.fstring(), []]]
        elif t in ('ArrayProperty', 'SetProperty', 'OptionalProperty'):
            tree[1] = [[r.fstring(), []]]
        elif t == 'MapProperty':
            tree[1] = [[r.fstring(), []], [r.fstring(), []]]
        tag['type_tree'] = tree
        tag['binary'] = None  # unknown in the old format: structs are walked only when they parse
        if r.u8():
            tag['guid'] = r.take(16).hex()
        if ue5 >= UE5_EXTENSIONS:
            ext = r.u8()
            need(ext == 0, f'property extensions {ext:#x} (overridable/reserved data) refused')
    need(0 <= tag['size'] <= len(r.b) - r.p, f"value size {tag['size']} of {name} exceeds the file")
    tag['type_str'] = _type_str(tag['type_tree'])
    return tag


def read_props(r, ue5, end, depth=0, counter=None):
    counter = counter if counter is not None else [0]
    need(depth < MAX_DEPTH, 'struct nesting too deep')
    props = []
    while True:
        need(r.p < end, 'property list runs past its end (no None terminator)')
        tag = read_tag(r, ue5)
        if tag is None:
            return props
        counter[0] += 1
        need(counter[0] <= MAX_TAGS, 'too many properties')
        start = r.p
        val = r.take(tag['size'])
        t = tag['type']
        if t in SCALARS and len(val) == struct.calcsize(SCALARS[t]):
            tag['value'] = struct.unpack(SCALARS[t], val)[0]
        elif t in ('StrProperty', 'NameProperty') and val:
            tag['value'] = Reader(val).fstring()
        elif t == 'StructProperty' and tag.get('binary') is False:
            sub = Reader(r.b)
            sub.p = start
            tag['fields'] = read_props(sub, ue5, start + tag['size'], depth + 1, counter)
            need(sub.p == start + tag['size'], f"struct {tag['name']} fields do not fill its size")
        props.append(tag)


def read_save(data):
    r = Reader(data)
    h = read_header(r)
    props = read_props(r, h['ue5_version'], len(data))
    need(r.p + 4 == len(data), f'{len(data) - r.p} bytes after the property list (expected the 4-byte GUID flag)')
    need(r.u32() == 0, 'object GUID present: not handled')
    return {'header': h, 'properties': props}


def schema_check(save, schema):
    """schema: {'class': '<save class path>', 'properties': {name: type_str}} from an in-process dump of the
    game's own save class. Every saved top-level property must exist with the same type."""
    out = {'class_match': save['header']['save_game_class'] == schema.get('class'), 'missing': [], 'type_diff': []}
    want = schema.get('properties', {})
    for p in save['properties']:
        if p['name'] not in want:
            out['missing'].append(p['name'])
        elif want[p['name']] != p['type_str']:
            out['type_diff'].append({'name': p['name'], 'save': p['type_str'], 'game': want[p['name']]})
    out['ok'] = out['class_match'] and not out['missing'] and not out['type_diff']
    return out


def summary(s):
    def flat(props, pre=''):
        for p in props:
            yield {'name': pre + p['name'], 'type': p['type_str'], 'size': p['size'],
                   **({'value': p['value']} if 'value' in p else {})}
            if 'fields' in p:
                yield from flat(p['fields'], pre + p['name'] + '.')
    return {'header': s['header'], 'top_level_properties': len(s['properties']), 'properties': list(flat(s['properties']))}


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument('save')
    ap.add_argument('--expect-engine', default=None, help='MAJOR.MINOR[.PATCH] of the game, e.g. 5.6.1')
    ap.add_argument('--schema', default=None, help='JSON from the in-process dump of the game save class')
    ap.add_argument('--out', default=None)
    a = ap.parse_args(argv)
    with open(a.save, 'rb') as fh:
        data = fh.read()
    try:
        s = read_save(data)
    except SaveError as ex:
        print(f'gvas: REFUSED: {ex}')
        return 1
    rep = summary(s)
    verdict = 'READABLE'
    if a.expect_engine:
        e = s['header']['engine']
        want = [int(x) for x in a.expect_engine.split('.')]
        have = [e['major'], e['minor'], e['patch']][:len(want)]
        rep['engine_match'] = have == want
        if not rep['engine_match']:
            verdict = 'REFUSED (engine version differs: a different game version is out of scope)'
    if a.schema:
        with open(a.schema) as fh:
            rep['schema_check'] = schema_check(s, json.load(fh))
        if not rep['schema_check']['ok'] and verdict == 'READABLE':
            verdict = 'REFUSED (save does not match the game save class)'
    rep['verdict'] = verdict
    if a.out:
        with open(a.out, 'w') as fh:
            json.dump(rep, fh, indent=1)
    h = s['header']
    print(f"gvas (prototype, untested on a real save): {verdict}: class {h['save_game_class']}, engine {h['engine']['major']}.{h['engine']['minor']}."
          f"{h['engine']['patch']}, UE5 file version {h['ue5_version']}, {rep['top_level_properties']} top-level "
          f"properties")
    return 0 if verdict == 'READABLE' else 1


if __name__ == '__main__':
    sys.exit(main())
