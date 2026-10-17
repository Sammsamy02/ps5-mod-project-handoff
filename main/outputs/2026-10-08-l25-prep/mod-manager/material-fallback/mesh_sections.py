#!/usr/bin/env python3
"""Read-only decoder for cooked UE5 SkeletalMesh packages (Mac-only, generic, capability #12 round 6).

What it reads from ONE zen package (no names of games, mods or assets in the logic):
  * the material slots: TArray<FSkeletalMaterial> = i32 N, N x {FPackageIndex material, FName slot,
    ubool bSerializeImportedSlotName (must be 0), FMeshUVChannelInfo {ubool bInitialized,
    ubool bOverrideDensities, f32 LocalUVDensities[4]}, FPackageIndex (must be null)} (44 bytes each);
  * the reference skeleton right after it: i32 B, B x {FName bone, i32 parent} (parent < own index);
  * the render LODs: i32 NumLODs, per LOD {strip flags (2 B), ubool cooked-out, ubool inlined,
    TArray<u16> required bones, TArray<FSkelMeshRenderSection>, TArray<u16> active bones,
    u32 buffers size, [inline] strip (2 B), u8 index size, bulk {i32 elem size, i32 count, data},
    position buffer {u32 stride 12, u32 count, bulk {i32 12, i32 count, f32x3 data}} ...}.
    FSkelMeshRenderSection = strip (2 B), u16 material index, u32 base index, u32 triangles,
    ubool recompute tangent, u8 mask channel, ubool cast shadow, ubool visible in ray tracing,
    u32 base vertex, TArray<TArray<..>> cloth mapping (all empty), TArray<u16> bone map,
    u32 vertices, i32 max bone influences, i16 cloth asset, FGuid, i32 cloth LOD, ubool disabled.

Nothing is trusted on its own. Every decode is self-checked and fails closed (MeshError):
  - export data offsets come from each export's CookedSerialOffset, and the exports must tile the data
    region exactly (the shared zen.py reader assumes bundle order, which is wrong for packages whose
    first serialized export is not the first in the data region);
  - exactly ONE material table + skeleton match;
  - per LOD: base indices chain (b[k+1] = b[k] + 3 t[k]), base vertices chain, index count == 3 x sum of
    triangles, vertex count == sum of section vertices == position count, every index of a section lies
    inside that section's own vertex range, bone maps index the skeleton;
  - a LOD whose geometry is not inline (cooked out / streamed) is reported as such, never guessed.

Placeholder test (Option B evidence for capability #12, three independent sources per section):
  S1 section table: at most PH_MAX_TRIS triangles;
  S2 geometry: total area <= PH_MAX_AREA_CM2, extent <= PH_MAX_EXTENT_CM, and the section is ENCLOSED by
     the same LOD's other geometry (all 6 axis rays from its centre hit another section's triangle);
  S3 the engine's own build data: the slot's LocalUVDensities are all 0 (the editor computes them from
     the slot's real UV area; a slot with visible geometry has non-zero densities).
"""
import argparse
import json
import math
import os
import struct
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import zen  # noqa: E402

TOOL = 'cap12-mesh-sections-1'
PH_MAX_TRIS = 2
PH_MAX_AREA_CM2 = 1.0
PH_MAX_EXTENT_CM = 1.0
MAT_STRIDE = 44
PACKAGE_FILE_TAG = struct.pack('<I', 0x9E2A83C1)
MAX_SLOTS = 256
MAX_LODS = 16
MAX_SECTIONS = 256
SCAN_LIMIT = 1 << 20          # material table must start in the first 1 MiB of the export


class MeshError(ValueError):
    pass


def need(ok, why):
    if not ok:
        raise MeshError(why)


def export_slices(z):
    """[(export, bytes)] from CookedSerialOffset; the exports must tile [HeaderSize, len) exactly."""
    d = z.data
    exp = struct.unpack_from('<i', d, 24 + 8)[0]
    rows = []
    for i, e in enumerate(z.exports):
        soff, ssize = struct.unpack_from('<QQ', d, exp + 72 * i)
        need(ssize == e['serial_size'], 'export size mismatch')
        rows.append((soff, ssize, e))
    pos = 0
    for soff, ssize, _e in sorted(rows, key=lambda r: r[0]):
        need(soff == pos, 'exports do not tile the data region (gap or overlap at %d)' % soff)
        pos += ssize
    tail = len(d) - z.header_size - pos
    # cooked packages end with the 4-byte package file tag (0x9E2A83C1); nothing else may follow
    need(tail == 0 or (tail == 4 and d[-4:] == PACKAGE_FILE_TAG),
         'export data does not end at the package end (%d trailing bytes)' % tail)
    return [(e, d[z.header_size + soff:z.header_size + soff + ssize]) for soff, ssize, e in rows]


class _R:
    def __init__(self, b):
        self.b = b
        self.n = len(b)

    def u8(self, o):
        need(0 <= o < self.n, 'read past end')
        return self.b[o]

    def u16(self, o):
        need(0 <= o and o + 2 <= self.n, 'read past end')
        return struct.unpack_from('<H', self.b, o)[0]

    def i16(self, o):
        need(0 <= o and o + 2 <= self.n, 'read past end')
        return struct.unpack_from('<h', self.b, o)[0]

    def u32(self, o):
        need(0 <= o and o + 4 <= self.n, 'read past end')
        return struct.unpack_from('<I', self.b, o)[0]

    def i32(self, o):
        need(0 <= o and o + 4 <= self.n, 'read past end')
        return struct.unpack_from('<i', self.b, o)[0]

    def f32(self, o):
        need(0 <= o and o + 4 <= self.n, 'read past end')
        return struct.unpack_from('<f', self.b, o)[0]


def _fname(z, r, o):
    idx, num = r.u32(o), r.u32(o + 4)
    if idx >= len(z.names) or num >= (1 << 16):
        return None
    return z.names[idx] + ('_%d' % (num - 1) if num else '')


def _material_ref(z, raw):
    """FPackageIndex -> (package path, public hash) | (None, None) for null | False if invalid."""
    if raw == 0:
        return None, None
    if raw > 0 or -raw - 1 >= len(z.imports):
        return False
    kind, v = z.imports[-raw - 1]
    if kind != 'PackageImport':
        return False
    pi, hi = v
    if pi >= len(z.imported_packages) or hi >= len(z.import_hashes):
        return False
    return z.imported_packages[pi], '%016x' % z.import_hashes[hi]


def _try_materials(z, r, o):
    n = r.i32(o)
    if not 1 <= n <= MAX_SLOTS or o + 4 + n * MAT_STRIDE + 4 > r.n:
        return None
    slots = []
    p = o + 4
    for k in range(n):
        ref = _material_ref(z, r.i32(p))
        if ref is False:
            return None
        slot = _fname(z, r, p + 4)
        if slot is None or r.u32(p + 12) != 0 or r.u32(p + 16) not in (0, 1) or r.u32(p + 20) not in (0, 1):
            return None
        dens = [r.f32(p + 24 + 4 * j) for j in range(4)]
        if not all(math.isfinite(x) and x >= 0 for x in dens) or r.u32(p + 40) != 0:
            return None
        slots.append({'index': k, 'material': ref[0], 'material_export_hash': ref[1], 'slot_name': slot,
                      'uv_initialized': bool(r.u32(p + 16)), 'uv_densities': dens, 'raw_ref': r.i32(p),
                      'ref_offset': p})
        p += MAT_STRIDE
    nb = r.i32(p)
    if not 1 <= nb <= 65535 or p + 4 + 12 * nb > r.n:
        return None
    bones = []
    for k in range(nb):
        q = p + 4 + 12 * k
        nm = _fname(z, r, q)
        par = r.i32(q + 8)
        if nm is None or not (-1 <= par < k) or (k == 0) != (par == -1):
            return None
        bones.append((nm, par))
    return slots, bones, p + 4 + 12 * nb


def _parse_section(r, o, nmat, nbones):
    s = {'strip': r.b[o:o + 2].hex()}
    o += 2
    s['material_index'] = r.u16(o)
    if s['material_index'] >= nmat:
        return None
    s['base_index'], s['triangles'] = r.u32(o + 2), r.u32(o + 6)
    o += 10
    if r.u32(o) not in (0, 1):
        return None
    o += 4
    if r.u8(o) > 8:
        return None
    o += 1
    s['cast_shadow'] = r.u32(o)
    s['visible_in_ray_tracing'] = r.u32(o + 4)
    if s['cast_shadow'] not in (0, 1) or s['visible_in_ray_tracing'] not in (0, 1):
        return None
    o += 8
    s['base_vertex'] = r.u32(o)
    o += 4
    nc = r.i32(o)
    o += 4
    if not 0 <= nc < 16:
        return None
    for _ in range(nc):
        if r.i32(o) != 0:
            return None                       # cloth mapping data: not handled (fail closed)
        o += 4
    nbm = r.i32(o)
    o += 4
    if not 0 < nbm <= 65535 or o + 2 * nbm > r.n:
        return None
    bm = list(struct.unpack_from('<%dH' % nbm, r.b, o))
    if any(b >= nbones for b in bm):
        return None
    s['bone_map'] = bm
    o += 2 * nbm
    s['vertices'] = r.u32(o)
    s['max_influences'] = r.i32(o + 4)
    if not 1 <= s['max_influences'] <= 32:
        return None
    s['cloth_asset'] = r.i16(o + 8)
    o += 10 + 16 + 4
    s['disabled'] = r.u32(o)
    if s['disabled'] not in (0, 1):
        return None
    s['end'] = o + 4
    return s


def _parse_lod_at(r, p, nmat, nbones):
    """LOD header at p (strip, cooked-out, inlined, required bones, sections). -> dict or None."""
    if p + 18 > r.n:
        return None
    cooked_out, inlined = r.u32(p + 2), r.u32(p + 6)
    if cooked_out not in (0, 1) or inlined not in (0, 1):
        return None
    nreq = r.i32(p + 10)
    if not 1 <= nreq <= nbones or p + 14 + 2 * nreq + 4 > r.n:
        return None
    req = struct.unpack_from('<%dH' % nreq, r.b, p + 14)
    if any(req[k] >= req[k + 1] for k in range(nreq - 1)) or req[-1] >= nbones:
        return None
    o = p + 14 + 2 * nreq
    ns = r.i32(o)
    if not 1 <= ns <= MAX_SECTIONS:
        return None
    o += 4
    secs = []
    for _ in range(ns):
        s = _parse_section(r, o, nmat, nbones)
        if s is None:
            return None
        secs.append(s)
        o = s['end']
    if secs[0]['base_index'] != 0 or secs[0]['base_vertex'] != 0:
        return None
    for a, b in zip(secs, secs[1:]):
        if b['base_index'] != a['base_index'] + 3 * a['triangles'] or b['base_vertex'] != a['base_vertex'] + a['vertices']:
            return None
    return {'cooked_out': bool(cooked_out), 'inlined': bool(inlined), 'required_bones': len(req),
            'sections': secs, 'sections_end': o}


def _geometry(r, lod):
    """Inline index + position buffers right after the section table; validated. Sets lod['geometry']."""
    o = lod['sections_end']
    na = r.i32(o)
    need(0 <= na <= 65535, 'active bone count implausible')
    o += 4 + 2 * na
    bsize = r.u32(o)
    o += 4                                     # buffers size
    bstart = o
    o += 2                                     # strip flags
    dts = r.u8(o)
    need(dts in (2, 4), 'index size %d' % dts)
    o += 1
    need(r.i32(o) == dts, 'index element size disagrees with the index size byte')
    n = r.i32(o + 4)
    secs = lod['sections']
    tri = sum(s['triangles'] for s in secs)
    need(n == 3 * tri, 'index count %d != 3 x %d triangles' % (n, tri))
    o += 8
    need(o + dts * n <= r.n, 'index buffer past end')
    idx = struct.unpack_from('<%d%s' % (n, 'H' if dts == 2 else 'I'), r.b, o)
    o += dts * n
    need(r.u32(o) == 12, 'position stride is not 12')
    nv = r.u32(o + 4)
    need(r.i32(o + 8) == 12 and r.i32(o + 12) == nv, 'position bulk header mismatch')
    need(nv == secs[-1]['base_vertex'] + secs[-1]['vertices'], 'position count != section vertex total')
    o += 16
    need(o + 12 * nv <= r.n, 'position buffer past end')
    pos_off = o
    pos = [struct.unpack_from('<3f', r.b, o + 12 * i) for i in range(nv)]
    need(all(math.isfinite(c) for p in pos for c in p), 'non-finite vertex position')
    o += 12 * nv
    for s in secs:
        lo, hi = s['base_vertex'], s['base_vertex'] + s['vertices']
        seg = idx[s['base_index']:s['base_index'] + 3 * s['triangles']]
        need(all(lo <= v < hi for v in seg), 'section indices leave the section vertex range')
    # the streamed buffers (index, position and the later vertex buffers) span BuffersSize bytes when that
    # fits the export; otherwise only the decoded index + position buffers are known to be numeric
    bend = bstart + bsize if o <= bstart + bsize <= r.n else o
    lod['geometry'] = {'indices': idx, 'positions': pos, 'positions_offset': pos_off, 'end': o,
                       'buffers': (bstart, bend)}
    return lod


def _tri_area(a, b, c):
    ux, uy, uz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
    vx, vy, vz = c[0] - a[0], c[1] - a[1], c[2] - a[2]
    cx, cy, cz = uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx
    return 0.5 * math.sqrt(cx * cx + cy * cy + cz * cz)


def _section_measures(lod, s):
    g = lod['geometry']
    idx, pos = g['indices'], g['positions']
    seg = idx[s['base_index']:s['base_index'] + 3 * s['triangles']]
    area, degen = 0.0, 0
    for k in range(0, len(seg), 3):
        a = _tri_area(pos[seg[k]], pos[seg[k + 1]], pos[seg[k + 2]])
        area += a
        degen += a == 0.0
    vs = [pos[v] for v in range(s['base_vertex'], s['base_vertex'] + s['vertices'])]
    lo = [min(p[j] for p in vs) for j in range(3)]
    hi = [max(p[j] for p in vs) for j in range(3)]
    return area, degen, lo, hi


def _ray_hits(origin, axis, sign, tris):
    """Does the axis ray from origin (direction sign along axis) hit any triangle (closed test)?"""
    a1, a2 = [j for j in range(3) if j != axis]
    ox = origin
    for (p, q, r) in tris:
        # quick reject on the two orthogonal coordinates
        if (ox[a1] < min(p[a1], q[a1], r[a1]) or ox[a1] > max(p[a1], q[a1], r[a1]) or
                ox[a2] < min(p[a2], q[a2], r[a2]) or ox[a2] > max(p[a2], q[a2], r[a2])):
            continue
        # barycentric test in the (a1, a2) plane, then the hit coordinate along the axis
        x1, y1 = p[a1], p[a2]
        x2, y2 = q[a1], q[a2]
        x3, y3 = r[a1], r[a2]
        den = (y2 - y3) * (x1 - x3) + (x3 - x2) * (y1 - y3)
        if den == 0:
            continue
        l1 = ((y2 - y3) * (ox[a1] - x3) + (x3 - x2) * (ox[a2] - y3)) / den
        l2 = ((y3 - y1) * (ox[a1] - x3) + (x1 - x3) * (ox[a2] - y3)) / den
        l3 = 1.0 - l1 - l2
        if l1 < 0 or l2 < 0 or l3 < 0:
            continue
        t = l1 * p[axis] + l2 * q[axis] + l3 * r[axis]
        if (t - ox[axis]) * sign > 0:
            return True
    return False


def triangles_of(lod, sections):
    g = lod['geometry']
    idx, pos = g['indices'], g['positions']
    tris = []
    for o in sections:
        seg = idx[o['base_index']:o['base_index'] + 3 * o['triangles']]
        tris.extend((pos[seg[k]], pos[seg[k + 1]], pos[seg[k + 2]]) for k in range(0, len(seg), 3))
    return tris


def enclosure(lod, s, tris):
    """-> number of the 6 axis directions (of 6) from the section's bbox centre that hit `tris`
    (the triangles of the LOD's OTHER, non-placeholder-sized sections)."""
    _a, _d, lo, hi = _section_measures(lod, s)
    c = [(lo[j] + hi[j]) / 2 for j in range(3)]
    hits = 0
    for axis in range(3):
        for sign in (1, -1):
            hits += _ray_hits(c, axis, sign, tris)
    return hits


def decode(data, label=None):
    """-> mesh dict (slots, bones, lods with sections + measures). Raises MeshError (fail closed)."""
    try:
        z = zen.ZenPackage(data)
    except (zen.ZenError, struct.error, IndexError) as e:
        raise MeshError('package unreadable: %s' % e)
    slices = export_slices(z)
    tops = [(e, b) for e, b in slices if e['outer'][0] == 'Null']
    need(len(tops) == 1, '%d top-level exports (need 1)' % len(tops))
    e, b = tops[0]
    r = _R(b)
    hits = []
    for o in range(0, min(r.n - 8, SCAN_LIMIT)):
        m = _try_materials(z, r, o)
        if m:
            hits.append((o, m))
            if len(hits) > 1:
                break
    need(len(hits) == 1, '%d material-table matches (need exactly 1)' % len(hits))
    mat_off, (slots, bones, bones_end) = hits[0]
    nmat, nb = len(slots), len(bones)
    # reference pose right after the bone infos: i32 count == bones, then one FTransform (10 doubles) per bone
    pose = None
    if bones_end + 4 + 80 * nb <= r.n and r.i32(bones_end) == nb:
        pose = (bones_end, bones_end + 4 + 80 * nb)
    # LOD array: i32 count right before the first LOD header that parses (searched from the skeleton end)
    lod0 = None
    for p in range(bones_end, r.n - 22):
        nl = r.i32(p)
        if 1 <= nl <= MAX_LODS:
            lod = _parse_lod_at(r, p + 4, nmat, nb)
            if lod is not None:
                lod0 = (p, nl, lod)
                break
    need(lod0 is not None, 'no LOD section table found')
    p, nl, lod = lod0
    lods = [lod]
    cur = lod
    while True:
        if cur['inlined'] and not cur['cooked_out']:
            _geometry(r, cur)
            start = cur['geometry']['end']
        else:
            # without inline geometry nothing cross-checks the next LOD header: fail closed on multi-LOD
            need(nl == 1, 'LOD %d geometry not inline in a %d-LOD mesh: not decodable' % (len(lods) - 1, nl))
            cur['geometry'] = None
            start = cur['sections_end']
        if len(lods) == nl:
            break
        nxt = None
        for q in range(start, r.n - 18):
            cand = _parse_lod_at(r, q, nmat, nb)
            if cand is not None:
                nxt = cand
                break
        need(nxt is not None, 'LOD %d of %d not found' % (len(lods), nl))
        lods.append(nxt)
        cur = nxt
    out_lods = []
    for li, lod in enumerate(lods):
        secs = []
        big = [s for s in lod['sections'] if s['triangles'] > PH_MAX_TRIS]
        big_tris = None
        for si, s in enumerate(lod['sections']):
            row = {'section': si, 'material_index': s['material_index'],
                   'slot_name': slots[s['material_index']]['slot_name'],
                   'material': slots[s['material_index']]['material'],
                   'triangles': s['triangles'], 'vertices': s['vertices'], 'disabled': bool(s['disabled']),
                   'cast_shadow': bool(s['cast_shadow']),
                   'bones': [bones[k][0] for k in s['bone_map']]}
            if lod['geometry'] is not None:
                area, degen, lo, hi = _section_measures(lod, s)
                row.update(area_cm2=area, degenerate_triangles=degen, bbox_min=lo, bbox_max=hi,
                           extent_cm=max(hi[j] - lo[j] for j in range(3)))
                if s['triangles'] <= PH_MAX_TRIS:
                    if big_tris is None:
                        big_tris = triangles_of(lod, big)
                    row['enclosed_rays'] = enclosure(lod, s, big_tris)
            secs.append(row)
        out_lods.append({'lod': li, 'inline_geometry': lod['geometry'] is not None,
                         'positions_offset': lod['geometry']['positions_offset'] if lod['geometry'] else None,
                         'buffers_offset': lod['geometry']['buffers'][0] if lod['geometry'] else None,
                         'buffers_end': lod['geometry']['buffers'][1] if lod['geometry'] else None,
                         'cooked_out': lod['cooked_out'], 'sections': secs,
                         'triangles': sum(s['triangles'] for s in lod['sections']),
                         'vertices': sum(s['vertices'] for s in lod['sections'])})
    exp = struct.unpack_from('<i', z.data, 24 + 8)[0]
    data_off = z.header_size + struct.unpack_from('<Q', z.data, exp + 72 * e['index'])[0]
    return {'tool': TOOL, 'package': z.name, 'export': e['name'], 'label': label, 'export_data_offset': data_off,
            'material_table_offset': mat_off, 'slots': [{k: v for k, v in s.items() if k != 'ref_offset'}
                                                        for s in slots],
            'slot_ref_offsets': [s['ref_offset'] for s in slots], 'ref_pose_range': pose,
            'bones': len(bones), 'lods': out_lods}


def section_placeholder(mesh, lod, row):
    """Three-source placeholder verdict for one section -> (bool, why)."""
    slot = mesh['slots'][row['material_index']]
    if not lod['inline_geometry']:
        return False, 'geometry not inline: cannot measure'
    if row['disabled']:
        return True, 'section disabled by the engine flag'
    if row['triangles'] > PH_MAX_TRIS:
        return False, 'S1: %d triangles (placeholder max %d)' % (row['triangles'], PH_MAX_TRIS)
    if row['area_cm2'] > PH_MAX_AREA_CM2 or row['extent_cm'] > PH_MAX_EXTENT_CM:
        return False, 'S2: area %.4f cm2 / extent %.3f cm exceeds the placeholder size' % (
            row['area_cm2'], row['extent_cm'])
    if row.get('enclosed_rays') != 6:
        return False, 'S2: not enclosed by the mesh (%s of 6 rays hit other geometry)' % row.get('enclosed_rays')
    if any(slot['uv_densities']):
        return False, 'S3: engine UV densities of the slot are non-zero (real UV area)'
    return True, ('S1 %d triangle(s); S2 %.4f cm2, extent %.3f cm, enclosed (6/6 rays); S3 slot UV densities 0'
                  % (row['triangles'], row['area_cm2'], row['extent_cm']))


def material_usage(meshes):
    """meshes: {package: decoded mesh}. -> {material package: [(mesh, lod, section row, placeholder, why)]}."""
    out = {}
    for name, m in sorted(meshes.items()):
        for lod in m['lods']:
            for row in lod['sections']:
                if row['material'] is None:
                    continue
                ph, why = section_placeholder(m, lod, row)
                out.setdefault(row['material'], []).append((name, lod['lod'], row, ph, why))
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--package', required=True, help='zen package chunk bytes (read-only)')
    ap.add_argument('--out')
    a = ap.parse_args(argv)
    with open(a.package, 'rb') as fh:
        data = fh.read()
    try:
        m = decode(data, label=os.path.basename(a.package))
    except MeshError as e:
        print('mesh_sections: FAIL (%s)' % e)
        return 1
    n = sum(len(l['sections']) for l in m['lods'])
    print('mesh_sections: %s: %d slots, %d LOD(s), %d sections checked' % (m['package'], len(m['slots']),
                                                                           len(m['lods']), n))
    if n == 0:
        print('mesh_sections: FAIL (zero sections)')
        return 1
    if a.out:
        with open(a.out, 'w') as fh:
            json.dump(m, fh, indent=1)
    return 0


if __name__ == '__main__':
    sys.exit(main())
