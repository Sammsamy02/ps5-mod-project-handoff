#!/usr/bin/env python3
"""Mac-only.  CAP12 milestone M1a-2: extends the M1a global-shader-cache codec
(verbatim copy next to this file: gsc_codec_m1a_copy.py, sha256 9d1e9de4...)
by decoding the per-shader object region that M1a kept as raw bytes
('objects_raw_hex', 3.74 MB in Townfall's GlobalShaderCache-SP_PS5.bin).

Everything below was learned from the file's own bytes (no Unreal source was
used or is available locally).  Field NAMES are descriptive labels; what is
proven is the byte structure and the cross-relations recorded in
roundtrip-receipt.json ('proofs') and summarised in the track report.

Learned layout (engine-level, UE5 memory image; verified on 14,645 objects):
  * the 23-bit field of a frozen pointer in the map's Shaders array is a
    1-based index into the section's type-dependency list -> it names the
    shader object's concrete class, and that dependency's size is the
    object's fixed size.
  * shader-object base = 220 bytes; class size - 220 = subclass fields
    (kept raw: 'subclass_fields_hex').
  * frozen array  = {u64 ptr (bit0=1, 23-bit field=0, 40-bit self-relative
    offset), i32 num, i32 max}; data placed elsewhere in the object region.
  * object base fields (offset: label [element layout]):
      0   parameters                    [u16 x4]
      16  resource_parameters           [u16, u8, u8]
      32  bindless_resource_parameters  [u16, u16, u8, u8]
      48  graph_uniform_buffers         [u16, u16]
      64  parameter_references          [u16, u16]
      80  structure_layout_hash u32, 84 root_parameter_buffer_index u16,
      86  pad_u16 u16
      88  uniform_buffers               [u16]
      104 texture_samplers              [u16, u8, u8]
      120 srvs                          [u16, u8, u8]
      136 loose_parameter_buffers       [u16 base, u16 size, u32 pad,
                                         array<[u16, u16]>]  (24 B)
      152 hash_u64 u64
      160 uniform_buffer_parameter_structs [u64]
      176 uniform_buffer_parameters     [u16]
      192 type_index   (indexed ptr: raw = index<<1 | 1, index into the
                        section's ptr_types table; 0 = null)
      200 vf_type_index (same form, into ptr_vf_types)
      208 target u32, 212 resource_index i32, 216 sort_key u32
  * de-duplication: identical data blocks are stored once and shared by
    several arrays (whole blocks only).  The encoder writes every array at
    its own location and REFUSES (ObjectsConflict) when two writes to the
    same byte disagree, so an edit can never silently lose to a sharer.
  * bytes not covered by any object or array data must be zero (padding).

Fail-closed: if any check fails for a section, that section keeps the M1a
raw bytes ('objects_raw_hex') with the reason recorded; the file still
round-trips.  Stdlib only; no per-game constants.

Usage:
  gsc_codec_ri.py roundtrip <in.bin> <receipt.json> [--summary <summary.json>]
  gsc_codec_ri.py decode    <in.bin> <model.json>
  gsc_codec_ri.py encode    <model.json> <out.bin>
"""
import sys
sys.dont_write_bytecode = True
import collections  # noqa: E402
import hashlib  # noqa: E402
import json  # noqa: E402
import os  # noqa: E402
import struct  # noqa: E402

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gsc_codec_m1a_copy as m1a  # noqa: E402

U64 = m1a.U64
SHADER_BASE_SIZE = 220
ARRAY_SIZE = 16
LOOSE_ELEM_SIZE = 24
LOOSE_NESTED_AT = 8

# element layouts: struct format, size, JSON kind
ELEM = {
    'u16': ('<H', 2),
    'u16x2': ('<HH', 4),
    'u16x4': ('<4H', 8),
    'u16u8u8': ('<HBB', 4),
    'u16u16u8u8': ('<HHBB', 6),
    'u64': ('<Q', 8),
    'loose': (None, LOOSE_ELEM_SIZE),
}
# sub-field byte offsets inside one element (used by negative controls)
ELEM_SUBFIELD_OFFSETS = {
    'u16': [0], 'u16x2': [0, 2], 'u16x4': [0, 2, 4, 6], 'u16u8u8': [0, 2, 3],
    'u16u16u8u8': [0, 2, 4, 5], 'u64': [0],
}
BASE_ARRAYS = [  # (label, field offset in object, element layout)
    ('parameters', 0, 'u16x4'),
    ('resource_parameters', 16, 'u16u8u8'),
    ('bindless_resource_parameters', 32, 'u16u16u8u8'),
    ('graph_uniform_buffers', 48, 'u16x2'),
    ('parameter_references', 64, 'u16x2'),
    ('uniform_buffers', 88, 'u16'),
    ('texture_samplers', 104, 'u16u8u8'),
    ('srvs', 120, 'u16u8u8'),
    ('loose_parameter_buffers', 136, 'loose'),
    ('uniform_buffer_parameter_structs', 160, 'u64'),
    ('uniform_buffer_parameters', 176, 'u16'),
]
BASE_SCALARS = [  # (label, offset, struct format)
    ('structure_layout_hash', 80, '<I'),
    ('root_parameter_buffer_index', 84, '<H'),
    ('pad_u16', 86, '<H'),
    ('hash_u64', 152, '<Q'),
    ('target', 208, '<I'),
    ('resource_index', 212, '<i'),
    ('sort_key', 216, '<I'),
]
BASE_INDEXED_PTRS = [('type_index', 192), ('vf_type_index', 200)]


class ObjectsConflict(ValueError):
    """Two writes disagree on one byte (an edit to a de-duplicated block)."""


# ---------------- decode ----------------

def _elem_decode(img, at, kind, n):
    fmt, es = ELEM[kind]
    if kind == 'u64':
        return ['%016x' % x for x in struct.unpack_from('<%dQ' % n, img, at)]
    if kind == 'u16':
        return list(struct.unpack_from('<%dH' % n, img, at))
    return [list(struct.unpack_from(fmt, img, at + i * es)) for i in range(n)]


def _array_decode(img, pos, kind, lo, hi):
    v = U64.unpack_from(img, pos)[0]
    num, mx = struct.unpack_from('<ii', img, pos + 8)
    if num < 0 or mx < num:
        raise ValueError('array at 0x%x: num %d max %d' % (pos, num, mx))
    if v == 0:
        if num:
            raise ValueError('array at 0x%x: null ptr with %d items' % (pos, num))
        return {'at': None, 'max': mx, 'items': []}
    p = m1a.dec_ptr(v)
    if p['frozen'] != 1 or p['tidx'] != 0:
        raise ValueError('array at 0x%x: unexpected ptr bits %r' % (pos, p))
    at = pos + p['off']
    es = ELEM[kind][1]
    if at < lo or at + num * es > hi:
        raise ValueError('array at 0x%x: data [0x%x,+%d) outside image' % (pos, at, num * es))
    if kind == 'loose':
        items = []
        for i in range(num):
            e = at + i * LOOSE_ELEM_SIZE
            base, size, pad = struct.unpack_from('<HHI', img, e)
            items.append({'base_index': base, 'size': size, 'pad_u32': pad,
                          'parameters': _array_decode(img, e + LOOSE_NESTED_AT, 'u16x2', lo, hi)})
    else:
        items = _elem_decode(img, at, kind, num)
    return {'at': at, 'max': mx, 'items': items}


def _iptr_decode(v, table_len, label, pos):
    if v == 0:
        return None
    if v & 1 != 1 or (v >> 1) >= table_len:
        raise ValueError('%s at 0x%x: raw 0x%x not a frozen index into a %d-entry table' % (label, pos, v, table_len))
    return v >> 1


def decode_object(img, o, size, n_types, n_vf, lo, hi):
    obj = {'at': o}
    for label, off, kind in BASE_ARRAYS:
        obj[label] = _array_decode(img, o + off, kind, lo, hi)
    for label, off, fmt in BASE_SCALARS:
        v = struct.unpack_from(fmt, img, o + off)[0]
        obj[label] = ('%016x' % v) if fmt == '<Q' else v
    obj['type_index'] = _iptr_decode(U64.unpack_from(img, o + 192)[0], n_types, 'type_index', o + 192)
    obj['vf_type_index'] = _iptr_decode(U64.unpack_from(img, o + 200)[0], n_vf, 'vf_type_index', o + 200)
    obj['subclass_fields_hex'] = img[o + SHADER_BASE_SIZE:o + size].hex()
    return obj


def object_targets(frozen):
    """[(shader k, object offset, 1-based dep index)] from the M1a Shaders array."""
    sh = frozen['array_data']['shaders']
    out = []
    if sh['ptr'] is None:
        return out
    for k, p in enumerate(sh['items']):
        if p is None:
            raise ValueError('null shader pointer %d' % k)
        out.append((k, sh['data_at'] + k * 8 + p['off'], p['tidx']))
    return out


def decode_objects(img, sec):
    f = sec['frozen']
    deps = sec['type_deps']
    lo, hi = f['objects_at'], len(img)
    # array data may be a de-duplicated block anywhere in the image (also in
    # the root-level array data written by the M1a layer); objects may not
    cls = {}
    for k, o, tidx in object_targets(f):
        if not 1 <= tidx <= len(deps):
            raise ValueError('shader %d: class index %d outside %d deps' % (k, tidx, len(deps)))
        if cls.setdefault(o, tidx) != tidx:
            raise ValueError('object 0x%x referenced with two classes' % o)
    objs = []
    for o in sorted(cls):
        size = deps[cls[o] - 1]['size']
        if size < SHADER_BASE_SIZE or o < lo or o + size > hi:
            raise ValueError('object 0x%x size %d outside region' % (o, size))
        objs.append(decode_object(img, o, size, len(sec['ptr_types']), len(sec['ptr_vf_types']), 0, hi))
    return {'region_at': lo, 'region_size': hi - lo, 'objects': objs}


# ---------------- encode ----------------

class _Writer:
    def __init__(self, base, size):
        self.base = base
        self.buf = bytearray(size)
        self.mask = bytearray(size)

    def put(self, at, blob, what):
        a = at - self.base
        b = a + len(blob)
        if a < 0 or b > len(self.buf):
            raise ValueError('%s: write [0x%x,0x%x) outside image' % (what, at, at + len(blob)))
        seg = self.mask[a:b]
        if 1 not in seg:
            self.buf[a:b] = blob
            self.mask[a:b] = b'\x01' * len(blob)
            return
        for i in range(len(blob)):
            if seg[i] and self.buf[a + i] != blob[i]:
                raise ObjectsConflict('%s: byte 0x%x already written as %02x, new %02x (shared/de-duplicated block)'
                                      % (what, at + i, self.buf[a + i], blob[i]))
        self.buf[a:b] = bytes(blob[i] if not seg[i] else self.buf[a + i] for i in range(len(blob)))
        self.mask[a:b] = b'\x01' * len(blob)


def _elem_encode(kind, items):
    fmt, es = ELEM[kind]
    if kind == 'u64':
        return struct.pack('<%dQ' % len(items), *[int(x, 16) for x in items])
    if kind == 'u16':
        return struct.pack('<%dH' % len(items), *items)
    return b''.join(struct.pack(fmt, *it) for it in items)


def _array_encode(w, hdr, pos, arr, kind, what):
    """Write the 16-byte array header into hdr (bytearray of the enclosing
    block, hdr_base = pos inside it) and the data through the writer."""
    buf, rel = hdr
    n = len(arr['items'])
    if arr['at'] is None:
        if n:
            raise ValueError('%s: items without a data location' % what)
        U64.pack_into(buf, rel, 0)
    else:
        U64.pack_into(buf, rel, m1a.enc_ptr({'frozen': 1, 'tidx': 0, 'off': arr['at'] - pos}))
        if kind == 'loose':
            blob = bytearray(n * LOOSE_ELEM_SIZE)
            for i, it in enumerate(arr['items']):
                e = i * LOOSE_ELEM_SIZE
                struct.pack_into('<HHI', blob, e, it['base_index'], it['size'], it['pad_u32'])
                _array_encode(w, (blob, e + LOOSE_NESTED_AT), arr['at'] + e + LOOSE_NESTED_AT,
                              it['parameters'], 'u16x2', what + '[%d].parameters' % i)
            w.put(arr['at'], bytes(blob), what)
        else:
            w.put(arr['at'], _elem_encode(kind, arr['items']), what)
    struct.pack_into('<ii', buf, rel + 8, n, arr['max'])


def encode_object(w, obj, n_types, n_vf):
    sub = bytes.fromhex(obj['subclass_fields_hex'])
    o = obj['at']
    blk = bytearray(SHADER_BASE_SIZE + len(sub))
    for label, off, kind in BASE_ARRAYS:
        _array_encode(w, (blk, off), o + off, obj[label], kind, 'object 0x%x %s' % (o, label))
    for label, off, fmt in BASE_SCALARS:
        v = obj[label]
        struct.pack_into(fmt, blk, off, int(v, 16) if fmt == '<Q' else v)
    for label, off in BASE_INDEXED_PTRS:
        v = obj[label]
        lim = n_types if label == 'type_index' else n_vf
        if v is not None and not 0 <= v < lim:
            raise ValueError('object 0x%x %s=%r outside table of %d' % (o, label, v, lim))
        U64.pack_into(blk, off, 0 if v is None else (v << 1) | 1)
    blk[SHADER_BASE_SIZE:] = sub
    w.put(o, bytes(blk), 'object 0x%x' % o)


def root_prefix(frozen):
    """Image bytes [0, objects_at) exactly as the M1a layer encodes them from
    the model (content root + root-level array data + padding)."""
    f2 = dict(frozen)
    f2['objects_raw_hex'] = ''
    return m1a.encode_frozen(f2)[:frozen['objects_at']]


def encode_objects(sec, so, return_writer=False):
    """Object region bytes from the decoded model (never from input bytes).
    Writes landing in the root part (de-duplicated blocks) must agree with
    what the M1a layer encodes there, else ObjectsConflict."""
    f = sec['frozen']
    deps = sec['type_deps']
    if so['region_at'] != f['objects_at'] or so['region_at'] + so['region_size'] != f['frozen_size']:
        raise ValueError('object region bounds disagree with the frozen image')
    w = _Writer(0, f['frozen_size'])
    prefix = root_prefix(f)
    w.buf[:len(prefix)] = prefix
    w.mask[:len(prefix)] = b'\x01' * len(prefix)
    by_at = {}
    for obj in so['objects']:
        if obj['at'] in by_at:
            raise ValueError('two objects at 0x%x' % obj['at'])
        by_at[obj['at']] = obj
    # the M1a Shaders pointers must point at exactly these objects, with a
    # class whose size matches the object's fixed size (fail closed)
    refd = set()
    for k, o, tidx in object_targets(f):
        if o not in by_at:
            raise ValueError('shader %d points at 0x%x: no object there' % (k, o))
        if not 1 <= tidx <= len(deps):
            raise ValueError('shader %d: class index %d outside deps' % (k, tidx))
        size = SHADER_BASE_SIZE + len(by_at[o]['subclass_fields_hex']) // 2
        if deps[tidx - 1]['size'] != size:
            raise ValueError('shader %d: class %d size %d != object size %d' % (k, tidx, deps[tidx - 1]['size'], size))
        refd.add(o)
    if refd != set(by_at):
        raise ValueError('objects not referenced by any shader pointer: %d' % len(set(by_at) - refd))
    for obj in so['objects']:
        encode_object(w, obj, len(sec['ptr_types']), len(sec['ptr_vf_types']))
    region = bytes(w.buf[so['region_at']:])
    if return_writer:
        return region, w
    return region


# ---------------- whole file ----------------

def decode(data):
    model = m1a.decode(data)
    model['format'] = 'ue5-global-shader-cache+shader-objects'
    for s in model['sections']:
        f = s.get('frozen')
        if not f:
            continue
        img = data[s['frozen_offset']:s['frozen_offset'] + s['frozen_size']]
        try:
            so = decode_objects(img, s)
            if encode_objects(s, json.loads(json.dumps(so))) != img[f['objects_at']:]:
                raise ValueError('object-region re-encode mismatch')
            f['shader_objects'] = so
            del f['objects_raw_hex']
        except (ValueError, struct.error, TypeError, KeyError) as e:
            f['shader_objects'] = None
            f['shader_objects_fallback_reason'] = str(e)
    return model


def encode(model):
    secs = []
    for s in model['sections']:
        f = s.get('frozen')
        if f and f.get('shader_objects'):
            f2 = dict(f)
            f2['objects_raw_hex'] = encode_objects(s, f['shader_objects']).hex()
            s2 = dict(s)
            s2['frozen'] = f2
            secs.append(s2)
        else:
            secs.append(s)
    m2 = dict(model)
    m2['sections'] = secs
    return m1a.encode(m2)


# ---------------- views used by proofs / cross-checks ----------------

def shader_rows(model):
    """One row per shader reference (section, k): resolved class + object."""
    rows = []
    for s in model['sections']:
        f = s.get('frozen')
        if not f or not f.get('shader_objects'):
            continue
        by_at = {o['at']: o for o in f['shader_objects']['objects']}
        ad = f['array_data']
        for k, o, tidx in object_targets(f):
            dep = s['type_deps'][tidx - 1]
            rows.append({'section': s['index'], 'k': k, 'obj': by_at[o], 'class': dep['name']['text'],
                         'class_size': dep['size'], 'class_dep_index': tidx - 1,
                         'type_hash': ad['shader_types']['items'][k],
                         'permutation': ad['shader_permutations']['items'][k]})
    return rows


def _arrays_of(obj):
    for label, off, kind in BASE_ARRAYS:
        a = obj[label]
        yield label, obj['at'] + off, kind, a
        if kind == 'loose':
            for i, it in enumerate(a['items']):
                yield '%s[%d].parameters' % (label, i), a['at'] + i * LOOSE_ELEM_SIZE + LOOSE_NESTED_AT, 'u16x2', it['parameters']


def dedup_accounting(structured):
    """How many non-empty arrays share how many data placements (whole-block
    sharing only).  Object arrays and root-level arrays counted separately."""
    obj_arrays = 0
    obj_places = set()          # (section, at, nbytes)
    obj_places_in_root = set()
    root_arrays = 0
    root_places = set()
    partial = 0
    for s in structured:
        f = s['frozen']
        so = f['shader_objects']
        spans = set()
        for obj in so['objects']:
            for label, pos, kind, a in _arrays_of(obj):
                if not a['items']:
                    continue
                obj_arrays += 1
                key = (s['index'], a['at'], len(a['items']) * ELEM[kind][1])
                obj_places.add(key)
                if a['at'] < so['region_at']:
                    obj_places_in_root.add(key)
                spans.add(key[1:])
        for nm, a in f['array_data'].items():
            if a['ptr'] is not None and a['items']:
                root_arrays += 1
                root_places.add((s['index'], a['data_at']))
        # partial overlap: two DIFFERENT (start, length) spans that intersect
        sp = sorted(spans)
        for i in range(1, len(sp)):
            (a0, n0), (a1, n1) = sp[i - 1], sp[i]
            if a1 < a0 + n0:
                partial += 1
    return {'nonempty_object_arrays': obj_arrays,
            'object_array_placements': len(obj_places),
            'object_array_placements_inside_object_region': len(obj_places) - len(obj_places_in_root),
            'object_array_placements_shared_with_root_data': len(obj_places_in_root),
            'nonempty_root_level_arrays': root_arrays,
            'root_level_array_placements': len(root_places),
            'partial_overlaps_between_distinct_object_spans': partial,
            'checked_items': obj_arrays + root_arrays}


RESOURCE_KINDS_TO_LIST_104 = {8}                       # observed, not named
RESOURCE_KINDS_TO_LIST_120 = {5, 6, 9, 12, 13, 17}    # observed, not named


def _partition_sets(o):
    rp = o['resource_parameters']['items']
    l104 = o['texture_samplers']['items']
    l120 = o['srvs']['items']
    return ({x[0] for x in l104 if x[2] == 2}, {i for _, i, k in rp if k in RESOURCE_KINDS_TO_LIST_104},
            {x[0] for x in l120}, {i for _, i, k in rp if k in RESOURCE_KINDS_TO_LIST_120},
            sorted(x[0] for x in l104 if x[2] == 5), sorted(b[0] for b in o['bindless_resource_parameters']['items']))


def list_partition(structured, seed=20261012):
    """Exact-set relations between resource_parameters kinds and the two
    [u16,u8,u8] lists at +104 / +120, plus the same test after shuffling
    which object's lists are compared (null baseline, fixed seed)."""
    import random
    objs = [o for s in structured for o in s['frozen']['shader_objects']['objects']]
    base = [o for o in objs if not o['subclass_fields_hex']]
    sub = [o for o in objs if o['subclass_fields_hex']]

    def rate(pairs):
        c = collections.Counter()
        for a, b in pairs:
            A, B = _partition_sets(a), _partition_sets(b)
            c['list104(u8[2]==2) set == {slot of kind 8}'] += int(A[0] == B[1])
            c['list120 set == {slot of kinds 5,6,9,12,13,17}'] += int(A[2] == B[3])
            c['both'] += int(A[0] == B[1] and A[2] == B[3])
            c['list104(u8[2]==5) u16s == bindless u16[0]s'] += int(A[4] == B[5])
        return dict(c)
    # only objects where the test is not trivially true (some list non-empty)
    nontriv = [o for o in base if any(_partition_sets(o)[:4])]
    rng = random.Random(seed)
    perm = list(nontriv)
    rng.shuffle(perm)
    vio = collections.Counter()
    for o in objs:
        A = _partition_sets(o)
        vio['list104 type2 superset of kind-8 slots'] += int(A[0] >= A[1])
        vio['list120 superset of kinds-5/6/9/12/13/17 slots'] += int(A[2] >= A[3])
    return {
        'objects_without_subclass_fields': len(base),
        'objects_without_subclass_fields_nontrivial': len(nontriv),
        'real_pairing (all objects without subclass fields)': rate((o, o) for o in base),
        'real_pairing (nontrivial only)': rate((o, o) for o in nontriv),
        'shuffled_pairing_baseline (nontrivial only, seed %d)' % seed: rate(zip(perm, nontriv)),
        'objects_with_subclass_fields': len(sub),
        'objects_with_subclass_fields_real_pairing': rate((o, o) for o in sub),
        'superset_relation_all_objects (of %d)' % len(objs): dict(vio),
        'u8[2] histogram list104': dict(collections.Counter(x[2] for o in objs for x in o['texture_samplers']['items'])),
        'u8[2] histogram list120': dict(collections.Counter(x[2] for o in objs for x in o['srvs']['items'])),
        'checked_items': len(objs),
    }


def slot_order(structured):
    """Per map: is resource_index order == ascending order of the sort_key's
    4 stored (little-endian) bytes?  Numeric u32 order shown as a contrast."""
    maps = multi = ok_bytes = ok_num = 0
    for s in structured:
        sl = {}
        for o in s['frozen']['shader_objects']['objects']:
            sl[o['resource_index']] = o['sort_key']
        ks = [sl[i] for i in range(len(sl))] if sorted(sl) == list(range(len(sl))) else None
        maps += 1
        if ks is None:
            continue
        if len(ks) > 1:
            multi += 1
            b = [struct.pack('<I', k) for k in ks]
            ok_bytes += int(all(b[i] < b[i + 1] for i in range(len(b) - 1)))
            ok_num += int(all(ks[i] < ks[i + 1] for i in range(len(ks) - 1)))
    return {'maps': maps, 'maps_with_2plus_slots': multi,
            'stored_bytes_strictly_ascending_in_resource_index_order': ok_bytes,
            'numeric_u32_strictly_ascending (contrast)': ok_num,
            'checked_items': multi}


def proofs(data, model):
    secs = model['sections']
    structured = [s for s in secs if s.get('frozen') and s['frozen'].get('shader_objects')]
    rows = shader_rows(model)
    c = collections.Counter()
    region_total = fixed_bytes = data_bytes_unique = pad_bytes = 0
    n_objects = n_arrays = n_items = 0
    blocks_unique = 0
    root_shared = 0
    dep_corr = collections.Counter()
    elemtype_of = {'loose_parameter_buffers': ('FShaderLooseParameterBufferInfo', 24),
                   'texture_samplers': ('FShaderResourceParameterInfo', 4),
                   'srvs': ('FShaderResourceParameterInfo', 4),
                   'uniform_buffers': ('FShaderUniformBufferParameterInfo', 2),
                   'uniform_buffer_parameters': ('FShaderUniformBufferParameter', 2),
                   'uniform_buffer_parameter_structs': ('FHashedName', 8),
                   'nested': ('FShaderLooseParameterInfo', 4)}
    for s in structured:
        f = s['frozen']
        so = f['shader_objects']
        region, w = encode_objects(s, so, return_writer=True)
        region_total += len(region)
        covered = w.mask[so['region_at']:].count(1)
        uncovered = len(region) - covered
        # uncovered bytes must be zero (they are, or encode would differ)
        pad_bytes += uncovered
        starts = set()
        used_types = collections.defaultdict(bool)
        for obj in so['objects']:
            n_objects += 1
            fixed_bytes += SHADER_BASE_SIZE + len(obj['subclass_fields_hex']) // 2
            for label, pos, kind, a in _arrays_of(obj):
                n_arrays += 1
                n_items += len(a['items'])
                if a['items']:
                    if a['at'] < so['region_at']:
                        root_shared += 1
                    else:
                        starts.add((a['at'], len(a['items']) * ELEM[kind][1]))
                    used_types['nested' if '.parameters' in label else label] = True
        blocks_unique += len(starts)
        data_bytes_unique += sum(n for _, n in starts)
        depnames = {(d['name']['text'], d['size']) for d in s['type_deps']}
        for label, et in elemtype_of.items():
            if label == 'uniform_buffer_parameter_structs':
                continue  # FHashedName is also used by the map root itself
            used = used_types[label] or (label == 'texture_samplers' and used_types['srvs']) or \
                (label == 'srvs' and used_types['texture_samplers'])
            dep_corr['%s non-empty <=> dep %s(%d) present' % (label, et[0], et[1])] += int(used == (et in depnames))
    # per-shader relations
    ri_by_sec = collections.defaultdict(dict)
    rel = collections.Counter()
    for r in rows:
        o = r['obj']
        s = secs[r['section']]
        rel['type_index -> ptr_types[] == ShaderTypes[k]'] += int(s['ptr_types'][o['type_index']] == r['type_hash'])
        rel['vf_type_index is null (global shaders)'] += int(o['vf_type_index'] is None)
        slot = ri_by_sec[r['section']].setdefault(o['resource_index'], (o['sort_key'], o['target'], r['type_hash']))
        if slot[0] != o['sort_key']:
            rel['CONTRADICTION same resource_index, different sort_key'] += 1
        if slot[1] != o['target']:
            rel['CONTRADICTION same resource_index, different target'] += 1
    dense = sum(1 for sec, mp in ri_by_sec.items() if sorted(mp) == list(range(len(mp))))
    slots = sum(len(mp) for mp in ri_by_sec.values())
    allkeys = collections.Counter(v[0] for mp in ri_by_sec.values() for v in mp.values())
    # binding relations (per unique object)
    bind = collections.Counter()
    for s in structured:
        for o in s['frozen']['shader_objects']['objects']:
            lp = set()
            for lb in o['loose_parameter_buffers']['items']:
                bind['loose buffer size == sum(parameter sizes)'] += int(lb['size'] == sum(x[1] for x in lb['parameters']['items']))
                for b2, s2 in lb['parameters']['items']:
                    lp.add((lb['base_index'], b2, s2))
            for buf, base, off, size in o['parameters']['items']:
                bind['parameters[i] (buffer, base, size) found in loose_parameter_buffers'] += int((buf, base, size) in lp)
            ub = o['uniform_buffers']['items']
            ubp = o['uniform_buffer_parameters']['items']
            root = o['root_parameter_buffer_index']
            want = sorted(set(ubp) | ({root} if root != 0xFFFF else set()))
            bind['uniform_buffers == sorted(uniform_buffer_parameters + root buffer)'] += int(ub == want)
            bind['len(uniform_buffer_parameter_structs) == len(uniform_buffer_parameters)'] += int(
                len(o['uniform_buffer_parameter_structs']['items']) == len(ubp))
            ubs = set(ubp)
            for a, _ in o['graph_uniform_buffers']['items'] + o['parameter_references']['items']:
                bind['graph_uniform_buffers/parameter_references buffer is a bound uniform buffer'] += int(a in ubs)
            ts = {x[0] for x in o['texture_samplers']['items']}
            srv = {x[0] for x in o['srvs']['items']}
            for off, idx, bt in o['resource_parameters']['items']:
                bind['resource base_type %d: slot in texture_samplers' % bt] += int(idx in ts)
                bind['resource base_type %d: slot in srvs' % bt] += int(idx in srv)
                bind['resource base_type %d: total' % bt] += 1
    return {
        'dedup_accounting': dedup_accounting(structured),
        'list_partition_vs_shuffled_baseline': list_partition(structured),
        'slot_order_vs_sort_key_bytes': slot_order(structured),
        'sections_with_decoded_objects': '%d/%d' % (len(structured), len(secs)),
        'object_fallback_reasons': sorted({s['frozen'].get('shader_objects_fallback_reason') for s in secs
                                           if s.get('frozen') and not s['frozen'].get('shader_objects')} - {None}),
        'shader_references': len(rows),
        'unique_shader_objects': n_objects,
        'arrays_decoded': n_arrays,
        'array_items_decoded': n_items,
        'unique_array_data_blocks': blocks_unique,
        'arrays_whose_data_is_shared_with_root_level_arrays': root_shared,
        'object_region_bytes': region_total,
        'object_region_bytes_fixed_fields': fixed_bytes,
        'object_region_bytes_array_data_unique': data_bytes_unique,
        'object_region_bytes_zero_padding': pad_bytes,
        'object_region_bytes_still_raw_subclass_fields': sum(len(o['subclass_fields_hex']) // 2 for s in structured
                                                             for o in s['frozen']['shader_objects']['objects']),
        'region_accounting_exact': fixed_bytes + data_bytes_unique + pad_bytes == region_total,
        'resource_index': {
            'sections_dense_0_to_N-1': '%d/%d' % (dense, len(ri_by_sec)),
            'map_code_slots_total': slots,
            'unique_sort_keys_over_all_slots': len(allkeys),
            'sort_keys_shared_by_2plus_slots': sum(1 for v in allkeys.values() if v > 1),
        },
        'per_shader_relations': dict(rel),
        'binding_relations': dict(sorted(bind.items())),
        'dependency_type_correlation_sections': dict(dep_corr),
    }


SUMMARY_COLUMNS = ['section', 'k', 'class', 'type_hash', 'permutation', 'object_at', 'target',
                   'resource_index', 'sort_key', 'structure_layout_hash', 'root_parameter_buffer_index',
                   'subclass_field_bytes'] + ['n_' + label for label, _, _ in BASE_ARRAYS]


def summary_rows(model):
    """Compact per-shader table (one row per shader reference)."""
    out = []
    for r in shader_rows(model):
        o = r['obj']
        out.append([r['section'], r['k'], r['class'], r['type_hash'], r['permutation'], o['at'], o['target'],
                    o['resource_index'], '%08x' % o['sort_key'], '%08x' % o['structure_layout_hash'],
                    o['root_parameter_buffer_index'], len(o['subclass_fields_hex']) // 2] +
                   [len(o[label]['items']) for label, _, _ in BASE_ARRAYS])
    return out


def main(argv):
    if len(argv) >= 3 and argv[0] == 'roundtrip':
        data = open(argv[1], 'rb').read()
        model = decode(data)
        model2 = json.loads(json.dumps(model))  # encoder sees only the serialised model
        rebuilt = encode(model2)
        a, b = hashlib.sha256(data).hexdigest(), hashlib.sha256(rebuilt).hexdigest()
        rec = {'label': 'Mac-only', 'tool': 'gsc_codec_ri.py', 'input': argv[1], 'input_size': len(data),
               'input_sha256': a, 'rebuilt_size': len(rebuilt), 'rebuilt_sha256': b,
               'byte_identical': data == rebuilt,
               'objects_raw_hex_left_in_model': sum(1 for s in model2['sections']
                                                    if s.get('frozen') and 'objects_raw_hex' in s['frozen'])}
        if data != rebuilt:
            off = m1a.first_diff(data, rebuilt)
            rec['first_diff_offset'] = off
            rec['first_diff_field'] = m1a.field_at(model, off)
        # the M1a layer's own proofs, re-run on a fresh M1a decode (unchanged layer)
        rec['m1a_layer_proofs'] = m1a.proofs(data, m1a.decode(data))
        pr = proofs(data, model2)
        rec['proofs'] = pr
        with open(argv[2], 'w') as fh:
            json.dump(rec, fh, indent=1)
        if '--summary' in argv:
            with open(argv[argv.index('--summary') + 1], 'w') as fh:
                json.dump({'label': 'Mac-only', 'input_sha256': a, 'columns': SUMMARY_COLUMNS,
                           'note': 'target: low 4 bits = shader frequency class (observed), bits 4+ = 33 in every row',
                           'rows': summary_rows(model2)}, fh)
        checked = pr['shader_references']
        print(json.dumps({'checked_shader_references': checked, 'unique_objects': pr['unique_shader_objects'],
                          'input_sha256': a, 'rebuilt_sha256': b, 'byte_identical': data == rebuilt,
                          'sections_with_decoded_objects': pr['sections_with_decoded_objects']}))
        sub_checks = {k: pr[k]['checked_items'] for k in
                      ('dedup_accounting', 'list_partition_vs_shuffled_baseline', 'slot_order_vs_sort_key_bytes')}
        print(json.dumps({'sub_proof_items_checked': sub_checks,
                          'dedup': {k: v for k, v in pr['dedup_accounting'].items() if k != 'checked_items'},
                          'slot_order': pr['slot_order_vs_sort_key_bytes']}))
        if checked == 0 or 0 in sub_checks.values():
            print('FAIL: zero items checked (shader refs %d, sub-proofs %r)' % (checked, sub_checks))
            return 1
        return 0 if data == rebuilt and rec['objects_raw_hex_left_in_model'] == 0 else 1
    if len(argv) == 3 and argv[0] == 'decode':
        model = decode(open(argv[1], 'rb').read())
        with open(argv[2], 'w') as fh:
            json.dump(model, fh)
        return 0
    if len(argv) == 3 and argv[0] == 'encode':
        with open(argv[1]) as fh:
            model = json.load(fh)
        with open(argv[2], 'wb') as fh:
            fh.write(encode(model))
        return 0
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
