#!/usr/bin/env python3
"""Mac-only. Decoder/encoder for an Unreal Engine 5.x cooked global shader
cache file (GlobalShaderCache-<ShaderPlatform>.bin).  Stdlib only.

Everything is learned from the file's own bytes; there are no per-game
constants.  Engine-level layout knowledge used (UE5 memory-image format):
  * frozen pointer  = u64: bit0 frozen flag, bits1..23 type-index field,
                      bits24..63 signed self-relative byte offset
  * frozen array    = {frozen ptr, i32 Num, i32 Max}            (16 bytes)
  * frozen hashtable= {ptr Hash, ptr NextIndex, u32 HashMask, u32 IndexSize}
  * shader-map content root = hashtable, ShaderTypes[u64], Permutations[i32],
    Shaders[ptr], Pipelines[ptr], platform-name slot (8), then subclass
    fields (raw).  Root size is taken from the file's own type-dependency
    list (entry 0) and cross-checked, never hard-coded.

Archive grammar (all little-endian), proven by exact tiling of the file:
  i32 NumSections
  per section:
    u32 LayoutMaxFieldAlignment, u32 LayoutFlags
    u32 FrozenSize, u8[FrozenSize] FrozenImage
    u32 NumTypeDeps, per dep: FString Name, u32 TypeSize, u8[20] LayoutHash
    u32 NumPtrTableTypes, u32 NumPtrTableVFTypes, u64[...] types, u64[...] vf
    u32 NumVTables, u32 NumScriptNames, u32 NumMinimalNames
      vtable  : u64 TypeNameHash, u32 NumPatches, {u32 VTableOffset,u32 Offset}*
      name    : FString Name, u32 NumPatches, u32 Offset*
    u32 bShareCode (UE bool)
      if 1: FString ShaderPlatformName, u8[20] ResourceHash
      if 0: inline resource code -> NOT present in the observed file; the
            decoder refuses (raises) rather than guessing.

Usage:
  gsc_codec.py roundtrip <in.bin> <receipt.json> [--summary <summary.json>]
  gsc_codec.py decode    <in.bin> <model.json>     (full model, ~2x file size)
  gsc_codec.py encode    <model.json> <out.bin>
"""
import hashlib
import json
import struct
import sys

U32 = struct.Struct('<I')
I32 = struct.Struct('<i')
U64 = struct.Struct('<Q')

ARRAY_SIZE = 16
HASHTABLE_SIZE = 24
PTR_SIZE = 8


class Reader:
    def __init__(self, data):
        self.d = data
        self.p = 0

    def need(self, n):
        if n < 0 or self.p + n > len(self.d):
            raise ValueError('read past end at 0x%x (+%d)' % (self.p, n))

    def u32(self):
        self.need(4)
        v = U32.unpack_from(self.d, self.p)[0]
        self.p += 4
        return v

    def u64(self):
        self.need(8)
        v = U64.unpack_from(self.d, self.p)[0]
        self.p += 8
        return v

    def raw(self, n):
        self.need(n)
        v = self.d[self.p:self.p + n]
        self.p += n
        return v

    def fstring(self):
        n = I32.unpack_from(self.d, self.p)[0]
        self.p += 4
        if n < 0:  # UTF-16 FString
            b = self.raw(-n * 2)
            return {'enc': 'utf16', 'hex': b.hex(), 'text': b.decode('utf-16-le', 'replace').rstrip('\x00')}
        b = self.raw(n)
        if n and b[-1:] != b'\x00':
            raise ValueError('FString not NUL terminated at 0x%x' % self.p)
        return {'enc': 'ansi', 'hex': b.hex(), 'text': b[:-1].decode('latin-1')}


def w_fstring(out, s):
    b = bytes.fromhex(s['hex'])
    if s['enc'] == 'utf16':
        out += I32.pack(-(len(b) // 2))
    else:
        out += I32.pack(len(b))
    out += b


# ---------------- frozen memory image (content root) ----------------

def dec_ptr(v):
    if v == 0:
        return None
    off = v >> 24
    if off >= 1 << 39:
        off -= 1 << 40
    return {'frozen': v & 1, 'tidx': (v >> 1) & 0x7FFFFF, 'off': off}


def enc_ptr(p):
    if p is None:
        return 0
    return ((p['off'] & ((1 << 40) - 1)) << 24) | (p['tidx'] << 1) | p['frozen']


def decode_frozen(img, root_size):
    """Structured view of the shader-map content root.  Returns a dict whose
    encode_frozen() output must equal img, or raises ValueError."""
    if root_size < HASHTABLE_SIZE + 4 * ARRAY_SIZE + 8 or root_size > len(img):
        raise ValueError('root size %r unusable' % root_size)
    ptr = lambda o: dec_ptr(U64.unpack_from(img, o)[0])
    ht_hash, ht_next = ptr(0), ptr(8)
    hash_mask, index_size = U32.unpack_from(img, 16)[0], U32.unpack_from(img, 20)[0]
    arrays = {}
    regions = []  # (field_offset, ptr, count, elem_size, kind, name)
    regions.append((0, ht_hash, (hash_mask + 1) if ht_hash else 0, 4, 'u32', 'hash_buckets'))
    regions.append((8, ht_next, index_size if ht_next else 0, 4, 'u32', 'hash_next'))
    names = [('shader_types', 8, 'u64'), ('shader_permutations', 4, 'i32'),
             ('shaders', 8, 'ptr'), ('shader_pipelines', 8, 'ptr')]
    o = HASHTABLE_SIZE
    for nm, es, kind in names:
        p = ptr(o)
        num, mx = struct.unpack_from('<ii', img, o + 8)
        arrays[nm] = {'num': num, 'max': mx}
        regions.append((o, p, num if p else 0, es, kind, nm))
        o += ARRAY_SIZE
    platform_slot = img[o:o + 8]
    subclass_tail = img[o + 8:root_size]
    # place each array's data at its own target.  The memory-image writer
    # de-duplicates identical blocks, so two arrays may share bytes; that is
    # accepted only when the shared bytes agree.  Bytes covered by nothing
    # must be zero and are reported as alignment padding.
    data = {}
    covered = bytearray(len(img))
    covered[:root_size] = b'\x01' * root_size
    shared = []
    max_end = root_size
    for fo, p, cnt, es, kind, nm in regions:
        if p is None:
            if cnt:
                raise ValueError('null ptr with count for ' + nm)
            data[nm] = {'ptr': None, 'items': []}
            continue
        tgt = fo + p['off']
        end = tgt + cnt * es
        if tgt < root_size or end > len(img):
            raise ValueError('%s data [0x%x,0x%x) outside image' % (nm, tgt, end))
        if any(covered[tgt:end]):
            shared.append(nm)
        covered[tgt:end] = b'\x01' * (end - tgt)
        if kind == 'u32':
            items = list(struct.unpack_from('<%dI' % cnt, img, tgt))
        elif kind == 'i32':
            items = list(struct.unpack_from('<%di' % cnt, img, tgt))
        elif kind == 'u64':
            items = ['%016x' % x for x in struct.unpack_from('<%dQ' % cnt, img, tgt)]
        else:
            items = [dec_ptr(x) for x in struct.unpack_from('<%dQ' % cnt, img, tgt)]
        data[nm] = {'ptr': p, 'items': items, 'data_at': tgt}
        max_end = max(max_end, end)
    cur = max_end
    padding = []
    i = root_size
    while i < cur:
        if not covered[i]:
            j = i
            while j < cur and not covered[j]:
                j += 1
            if img[i:j].strip(b'\x00'):
                raise ValueError('non-zero uncovered bytes at 0x%x' % i)
            padding.append({'at': i, 'len': j - i})
            i = j
        else:
            i += 1
    objects_at = cur
    objects_raw = img[cur:]
    # FShader object starts (absolute in image) from the Shaders pointer array
    shader_objs = []
    sh = data['shaders']
    if sh['ptr'] is not None:
        base = sh['data_at']
        for i, sp in enumerate(sh['items']):
            shader_objs.append(None if sp is None else base + i * 8 + sp['off'])
    return {
        'root_size': root_size,
        'hashtable': {'hash_mask': hash_mask, 'index_size': index_size},
        'arrays': arrays,
        'array_data': data,
        'platform_name_slot_hex': platform_slot.hex(),
        'subclass_fields_hex': subclass_tail.hex(),
        'frozen_size': len(img),
        'padding': padding,
        'shared_array_data': shared,
        'objects_at': objects_at,
        'objects_raw_hex': objects_raw.hex(),  # FShader objects + their sub-data: unknown (preserved as raw bytes)
        'shader_object_offsets': shader_objs,
    }


def encode_frozen(f):
    root = bytearray(f['root_size'])
    ad = f['array_data']
    def put_ptr(o, p):
        U64.pack_into(root, o, enc_ptr(p))
    put_ptr(0, ad['hash_buckets']['ptr'])
    put_ptr(8, ad['hash_next']['ptr'])
    U32.pack_into(root, 16, f['hashtable']['hash_mask'])
    U32.pack_into(root, 20, f['hashtable']['index_size'])
    o = HASHTABLE_SIZE
    order = ['shader_types', 'shader_permutations', 'shaders', 'shader_pipelines']
    for nm in order:
        put_ptr(o, ad[nm]['ptr'])
        struct.pack_into('<ii', root, o + 8, f['arrays'][nm]['num'], f['arrays'][nm]['max'])
        o += ARRAY_SIZE
    root[o:o + 8] = bytes.fromhex(f['platform_name_slot_hex'])
    tail = bytes.fromhex(f['subclass_fields_hex'])
    root[o + 8:o + 8 + len(tail)] = tail
    out = bytearray(f['frozen_size'])
    out[:len(root)] = root
    for nm in ['hash_buckets', 'hash_next'] + order:
        a = ad[nm]
        if a['ptr'] is None:
            continue
        it = a['items']
        if nm in ('hash_buckets', 'hash_next'):
            blob = struct.pack('<%dI' % len(it), *it)
        elif nm == 'shader_permutations':
            blob = struct.pack('<%di' % len(it), *it)
        elif nm == 'shader_types':
            blob = struct.pack('<%dQ' % len(it), *[int(x, 16) for x in it])
        else:
            blob = struct.pack('<%dQ' % len(it), *[enc_ptr(x) for x in it])
        out[a['data_at']:a['data_at'] + len(blob)] = blob
    tail = bytes.fromhex(f['objects_raw_hex'])
    out[f['objects_at']:f['objects_at'] + len(tail)] = tail
    return bytes(out)


# ---------------- archive level ----------------

def decode(data):
    r = Reader(data)
    n = I32.unpack_from(data, 0)[0]
    r.p = 4
    sections = []
    for si in range(n):
        s = {'index': si, 'offset': r.p}
        s['layout_max_field_alignment'] = r.u32()
        s['layout_flags'] = r.u32()
        fsize = r.u32()
        s['frozen_offset'] = r.p
        img = r.raw(fsize)
        s['frozen_size'] = fsize
        s['frozen_sha256'] = hashlib.sha256(img).hexdigest()
        deps = []
        for _ in range(r.u32()):
            nm = r.fstring()
            deps.append({'name': nm, 'size': r.u32(), 'layout_hash': r.raw(20).hex()})
        s['type_deps'] = deps
        nt, nvf = r.u32(), r.u32()
        s['ptr_types'] = ['%016x' % r.u64() for _ in range(nt)]
        s['ptr_vf_types'] = ['%016x' % r.u64() for _ in range(nvf)]
        nvt, nsn, nmn = r.u32(), r.u32(), r.u32()
        vts = []
        for _ in range(nvt):
            h = r.u64()
            k = r.u32()
            vts.append({'type_name_hash': '%016x' % h, 'patches': [[r.u32(), r.u32()] for _ in range(k)]})
        def names(cnt):
            out = []
            for _ in range(cnt):
                nm = r.fstring()
                k = r.u32()
                out.append({'name': nm, 'patch_offsets': [r.u32() for _ in range(k)]})
            return out
        s['vtables'] = vts
        s['script_names'] = names(nsn)
        s['minimal_names'] = names(nmn)
        share = r.u32()
        s['share_code'] = share
        if share == 1:
            s['platform_name'] = r.fstring()
            s['resource_hash'] = r.raw(20).hex()
        else:
            raise ValueError('section %d: bShareCode=%d (inline resource code) not seen in '
                             'reference file; refusing to guess its layout' % (si, share))
        # structured view of the frozen image, only kept if it re-encodes exactly
        root_size = deps[0]['size'] if deps else None
        try:
            fz = decode_frozen(img, root_size)
            if encode_frozen(fz) != img:
                raise ValueError('structured frozen re-encode mismatch')
            s['frozen'] = fz
        except (ValueError, struct.error, TypeError) as e:
            s['frozen'] = None
            s['frozen_raw_hex'] = img.hex()
            s['frozen_fallback_reason'] = str(e)
        s['end'] = r.p
        sections.append(s)
    trailer = data[r.p:]
    return {'format': 'ue5-global-shader-cache', 'num_sections': n,
            'sections': sections, 'trailer_hex': trailer.hex()}


def encode(model):
    out = bytearray(I32.pack(model['num_sections']))
    for s in model['sections']:
        out += U32.pack(s['layout_max_field_alignment']) + U32.pack(s['layout_flags'])
        img = encode_frozen(s['frozen']) if s.get('frozen') else bytes.fromhex(s['frozen_raw_hex'])
        out += U32.pack(len(img)) + img
        out += U32.pack(len(s['type_deps']))
        for d in s['type_deps']:
            w_fstring(out, d['name'])
            out += U32.pack(d['size']) + bytes.fromhex(d['layout_hash'])
        out += U32.pack(len(s['ptr_types'])) + U32.pack(len(s['ptr_vf_types']))
        for h in s['ptr_types'] + s['ptr_vf_types']:
            out += U64.pack(int(h, 16))
        out += U32.pack(len(s['vtables'])) + U32.pack(len(s['script_names'])) + U32.pack(len(s['minimal_names']))
        for v in s['vtables']:
            out += U64.pack(int(v['type_name_hash'], 16)) + U32.pack(len(v['patches']))
            for a, b in v['patches']:
                out += U32.pack(a) + U32.pack(b)
        for nm in s['script_names'] + s['minimal_names']:
            w_fstring(out, nm['name'])
            out += U32.pack(len(nm['patch_offsets']))
            for o in nm['patch_offsets']:
                out += U32.pack(o)
        out += U32.pack(s['share_code'])
        w_fstring(out, s['platform_name'])
        out += bytes.fromhex(s['resource_hash'])
    out += bytes.fromhex(model['trailer_hex'])
    return bytes(out)


# ---------------- proofs ----------------

def first_diff(a, b):
    for i in range(min(len(a), len(b))):
        if a[i] != b[i]:
            return i
    return None if len(a) == len(b) else min(len(a), len(b))


def field_at(model, off):
    for s in model['sections']:
        if s['offset'] <= off < s['end']:
            if s['frozen_offset'] <= off < s['frozen_offset'] + s['frozen_size']:
                return 'section %d frozen image +0x%x' % (s['index'], off - s['frozen_offset'])
            return 'section %d archive fields' % s['index']
    return 'header/trailer'


def proofs(data, model):
    secs = model['sections']
    # 1. tiling: sections contiguous from offset 4 to EOF
    cur, gaps = 4, []
    for s in secs:
        if s['offset'] != cur:
            gaps.append([cur, s['offset']])
        cur = s['end']
    tiling = {'sections_start': 4, 'sections_end': cur, 'file_size': len(data),
              'gaps': gaps, 'trailer_bytes': len(model['trailer_hex']) // 2,
              'tiles_exactly': not gaps and cur == len(data)}
    structured = [s for s in secs if s.get('frozen')]
    fz_tiling_ok = 0
    pad_total = 0
    nshaders = 0
    types_in_ptr_table = 0
    types_total = 0
    hash_index_match = 0
    plat_patch_match = 0
    root_size_match = 0
    obj_offsets_ok = 0
    obj_ascending = 0
    obj_dupes = 0
    shared_secs = 0
    perms_total = 0
    for s in structured:
        f = s['frozen']
        ad = f['array_data']
        n = f['arrays']['shaders']['num']
        nshaders += n
        pad_total += sum(p['len'] for p in f['padding'])
        if f['arrays']['shader_types']['num'] == n == f['arrays']['shader_permutations']['num']:
            fz_tiling_ok += 1
        st = set(s['ptr_types'])
        types_total += len(ad['shader_types']['items'])
        types_in_ptr_table += sum(1 for t in ad['shader_types']['items'] if t in st)
        if f['hashtable']['index_size'] == n:
            hash_index_match += 1
        slot = HASHTABLE_SIZE + 4 * ARRAY_SIZE
        if any(slot in nm['patch_offsets'] for nm in s['minimal_names']):
            plat_patch_match += 1
        if s['type_deps'] and s['type_deps'][0]['size'] == f['root_size']:
            root_size_match += 1
        offs = [o for o in f['shader_object_offsets'] if o is not None]
        if offs and min(offs) == f['objects_at'] and max(offs) < s['frozen_size']:
            obj_offsets_ok += 1
        if offs and offs == sorted(offs):
            obj_ascending += 1
        obj_dupes += len(offs) - len(set(offs))
        if f['shared_array_data']:
            shared_secs += 1
        perms_total += len(ad['shader_permutations']['items'])
    frozen_total = sum(s['frozen_size'] for s in secs)
    srcnames = [s['frozen']['subclass_fields_hex'] for s in structured]
    uniq_types = set()
    for s in secs:
        uniq_types.update(s['ptr_types'])
    return {
        'tiling': tiling,
        'sections': len(secs),
        'sections_structured_frozen': len(structured),
        'sections_raw_frozen_fallback': len(secs) - len(structured),
        'fallback_reasons': sorted({s.get('frozen_fallback_reason') for s in secs if not s.get('frozen')} - {None}),
        'share_code_sections': sum(1 for s in secs if s['share_code'] == 1),
        'inline_code_sections': sum(1 for s in secs if s['share_code'] != 1),
        'inline_shader_code_bytes': 0,
        'platform_names': sorted({s['platform_name']['text'] for s in secs}),
        'unique_resource_hashes': len({s['resource_hash'] for s in secs}),
        'total_shaders': nshaders,
        'total_permutation_entries': perms_total,
        'total_pipelines': sum(s['frozen']['arrays']['shader_pipelines']['num'] for s in structured),
        'unique_shader_type_hashes': len(uniq_types),
        'frozen_bytes_total': frozen_total,
        'frozen_alignment_padding_bytes_total': pad_total,
        'frozen_object_region_bytes_total': sum(len(s['frozen']['objects_raw_hex']) // 2 for s in structured),
        'archive_metadata_bytes_total': len(data) - 4 - frozen_total - tiling['trailer_bytes'],
        'checks': {
            'types/permutations/shaders arrays same length': '%d/%d' % (fz_tiling_ok, len(structured)),
            'every frozen ShaderTypes entry is in the section pointer table': '%d/%d' % (types_in_ptr_table, types_total),
            'hashtable IndexSize == shader count': '%d/%d' % (hash_index_match, len(structured)),
            'minimal-name patch targets the platform-name slot': '%d/%d' % (plat_patch_match, len(structured)),
            'type-dep[0] size == content root size': '%d/%d' % (root_size_match, len(structured)),
            'shader objects all inside image, lowest right after arrays': '%d/%d' % (obj_offsets_ok, len(structured)),
            'shader object offsets in ascending order (observation)': '%d/%d' % (obj_ascending, len(structured)),
            'duplicate shader object pointers (observation)': obj_dupes,
            'sections whose arrays share de-duplicated bytes (observation)': shared_secs,
            'unique hashed-source-filename (subclass field) per section': '%d/%d' % (len(set(srcnames)), len(structured)),
        },
    }


def summary(model):
    rows = []
    for s in model['sections']:
        f = s.get('frozen') or {}
        rows.append({
            'i': s['index'], 'offset': s['offset'], 'end': s['end'],
            'frozen_offset': s['frozen_offset'], 'frozen_size': s['frozen_size'],
            'frozen_sha256': s['frozen_sha256'],
            'root_type': s['type_deps'][0]['name']['text'] if s['type_deps'] else None,
            'hashed_source_filename': f.get('subclass_fields_hex'),
            'num_shaders': f.get('arrays', {}).get('shaders', {}).get('num'),
            'num_pipelines': f.get('arrays', {}).get('shader_pipelines', {}).get('num'),
            'shader_types': f.get('array_data', {}).get('shader_types', {}).get('items'),
            'permutations': f.get('array_data', {}).get('shader_permutations', {}).get('items'),
            'shader_object_offsets': f.get('shader_object_offsets'),
            'ptr_table_types': s['ptr_types'],
            'shader_class_names': sorted({d['name']['text'] for d in s['type_deps'][1:]}),
            'resource_hash_sha1': s['resource_hash'],
            'platform_name': s['platform_name']['text'],
        })
    return rows


def main(argv):
    if len(argv) >= 3 and argv[0] == 'roundtrip':
        data = open(argv[1], 'rb').read()
        model = decode(data)
        # encoder consumes only the JSON-serialised model, never the input bytes
        model2 = json.loads(json.dumps(model))
        rebuilt = encode(model2)
        a, b = hashlib.sha256(data).hexdigest(), hashlib.sha256(rebuilt).hexdigest()
        rec = {'label': 'Mac-only', 'input': argv[1], 'input_size': len(data), 'input_sha256': a,
               'rebuilt_size': len(rebuilt), 'rebuilt_sha256': b, 'byte_identical': data == rebuilt}
        if data != rebuilt:
            off = first_diff(data, rebuilt)
            rec['first_diff_offset'] = off
            rec['first_diff_field'] = field_at(model, off)
        rec['proofs'] = proofs(data, model)
        with open(argv[2], 'w') as fh:
            json.dump(rec, fh, indent=1)
        if '--summary' in argv:
            with open(argv[argv.index('--summary') + 1], 'w') as fh:
                json.dump({'label': 'Mac-only', 'input_sha256': a, 'sections': summary(model)}, fh)
        print(json.dumps({k: rec[k] for k in ('input_sha256', 'rebuilt_sha256', 'byte_identical')}))
        return 0 if data == rebuilt else 1
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
