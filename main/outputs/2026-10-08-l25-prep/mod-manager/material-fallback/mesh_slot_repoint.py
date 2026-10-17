#!/usr/bin/env python3
"""Re-point chosen material slots of a cooked UE5 SkeletalMesh package to ONE game material (Mac-only).

Builder for capability #12 round 6 (Option A/B of base_material_route.py). NOT wired into the shipping
overlay (paktx.py): it writes a derived package for inspection only. The mod's file is never modified.

How (data-only, two edits, nothing else):
  1. the carrier import (the import the LOWEST target slot uses) is re-pointed to the new package with
     mic_reparent.reparent (unchanged, pinned by the caller): ImportedPackageNames entry + public export
     hash slot, the hash from two sources (hash law + the new package's own fetched header);
  2. every OTHER target slot's FPackageIndex (inside the FSkeletalMaterial table, offsets from
     mesh_sections.decode) is set to the carrier import's index.
Guards (fail closed, nothing written):
  - the carrier import must be used by target slots only, and by no other import entry;
  - every target slot must currently reference a package import;
  - re-decoding the derived package: target slots -> the new package + hash; EVERY non-target slot is
    unchanged (material, hash, slot name, UV data); every LOD section table is unchanged;
  - the export bytes differ from the original exactly at the patched 4-byte fields (and the other
    exports are byte-identical).
The caller must repoint the package's container-header store entry import as the receipt says.
"""
import argparse
import json
import os
import struct
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mesh_sections  # noqa: E402
import mic_reparent  # noqa: E402
import zen  # noqa: E402

TOOL = 'cap12-mesh-slot-repoint-1'


class RepointError(ValueError):
    pass


def need(ok, why):
    if not ok:
        raise RepointError(why)


def _top(z):
    tops = [(e, b) for e, b in mesh_sections.export_slices(z) if e['outer'][0] == 'Null']
    need(len(tops) == 1, 'need exactly one top-level export')
    return tops[0]


def _find_all(b, pat):
    out, i = [], b.find(pat)
    while i != -1:
        out.append(i)
        i = b.find(pat, i + 1)
    return out


def _in_buffers(i, m):
    """Is export offset i inside a decoded LOD's index/position (or later bulk vertex) buffers? Those hold
    numbers, not object references."""
    for l in m['lods']:
        if l.get('buffers_offset') is not None and l['buffers_offset'] <= i < l['buffers_end']:
            return True
    pr = m.get('ref_pose_range')
    return bool(pr) and pr[0] <= i < pr[1]          # reference pose: doubles, not references


def repoint(data, target_slots, new_package, new_export, base_header):
    """-> (derived bytes, receipt). target_slots: iterable of slot indices."""
    try:
        m0 = mesh_sections.decode(data)
    except mesh_sections.MeshError as e:
        raise RepointError('mesh not decodable: %s' % e)
    slots = m0['slots']
    tgt = sorted(set(int(k) for k in target_slots))
    need(tgt, 'no target slots')
    need(all(0 <= k < len(slots) for k in tgt), 'target slot out of range')
    need(all(slots[k]['raw_ref'] < 0 for k in tgt), 'a target slot does not reference an import')
    carrier = slots[tgt[0]]['raw_ref']
    users = [s['index'] for s in slots if s['raw_ref'] == carrier]
    need(set(users) <= set(tgt), 'the carrier import is also used by non-target slot(s) %s: re-pointing it '
         'would change them' % sorted(set(users) - set(tgt)))
    z0 = zen.ZenPackage(data)
    # any other reference to the carrier import inside the export (outside the slot table and the bulk
    # geometry buffers) would be re-pointed too: refuse
    _e0, b0 = _top(z0)
    pat = struct.pack('<i', carrier)
    slot_offs = set(m0['slot_ref_offsets'])
    hits = [i for i in _find_all(b0, pat) if i not in slot_offs and not _in_buffers(i, m0)]
    need(not hits, 'the carrier import is referenced %d more time(s) in the export (offsets %s): re-pointing it '
         'would change more than the target slots' % (len(hits), hits[:5]))
    kind, (pkg_slot, _hash_slot) = z0.imports[-carrier - 1]
    need(kind == 'PackageImport', 'carrier is not a package import')
    old_package = z0.imported_packages[pkg_slot]
    try:
        out, rec = mic_reparent.reparent(data, old_package, new_package, new_export, base_header=base_header)
    except zen.ZenError as e:
        raise RepointError('import re-point refused: %s' % e)
    z1 = zen.ZenPackage(out)
    e1, b1 = _top(z1)
    exp = struct.unpack_from('<i', out, 24 + 8)[0]
    soff = struct.unpack_from('<Q', out, exp + 72 * e1['index'])[0]
    base = z1.header_size + soff
    buf = bytearray(out)
    patched = []
    for k in tgt[1:]:
        off = m0['slot_ref_offsets'][k]
        struct.pack_into('<i', buf, base + off, carrier)
        patched.append(off)
    out = bytes(buf)
    # ---- verification (independent re-decode)
    try:
        m1 = mesh_sections.decode(out)
    except mesh_sections.MeshError as e:
        raise RepointError('derived mesh not decodable: %s' % e)
    verify(m0, m1, tgt, new_package, rec['new_public_hash'])
    _e0, b0 = _top(z0)
    _e1, b1 = _top(zen.ZenPackage(out))
    need(len(b0) == len(b1), 'export size changed')
    diff = [i for i in range(len(b0)) if b0[i] != b1[i]]
    allowed = {o + j for o in patched for j in range(4)}
    need(set(diff) <= allowed, 'export bytes changed outside the patched slot fields')
    o0 = [b for e, b in mesh_sections.export_slices(z0) if e['outer'][0] != 'Null']
    o1 = [b for e, b in mesh_sections.export_slices(zen.ZenPackage(out)) if e['outer'][0] != 'Null']
    need(o0 == o1, 'another export changed')
    rec = dict(rec, tool=TOOL, export_bytes_identical=not diff, target_slots=tgt, carrier_ref=carrier, carrier_old_package=old_package,
               patched_slot_offsets=patched, changed_export_bytes=len(diff),
               untouched_slots=[s['index'] for s in slots if s['index'] not in tgt])
    return out, rec


def verify(m0, m1, tgt, new_package, new_hash):
    need(len(m0['slots']) == len(m1['slots']), 'slot count changed')
    for a, b in zip(m0['slots'], m1['slots']):
        if a['index'] in tgt:
            need(b['material'] == new_package and b['material_export_hash'] == new_hash,
                 'target slot %d not re-pointed' % a['index'])
            need(b['slot_name'] == a['slot_name'] and b['uv_densities'] == a['uv_densities'],
                 'target slot %d changed besides its material' % a['index'])
        else:
            keys = ('material', 'material_export_hash', 'slot_name', 'uv_initialized', 'uv_densities', 'raw_ref')
            need(all(a[k] == b[k] for k in keys), 'non-target slot %d changed' % a['index'])
    strip = lambda l: [{k: v for k, v in s.items() if k not in ('material',)} for s in l['sections']]
    need([strip(l) for l in m0['lods']] == [strip(l) for l in m1['lods']], 'LOD sections changed')


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--in', dest='inp', required=True)
    ap.add_argument('--slots', required=True, help='comma-separated slot indices')
    ap.add_argument('--new-package', required=True)
    ap.add_argument('--new-export', required=True)
    ap.add_argument('--base-header', required=True)
    ap.add_argument('--out', required=True)
    a = ap.parse_args(argv)
    if os.path.abspath(a.out) == os.path.abspath(a.inp):
        print('mesh_slot_repoint: FAIL: refusing to overwrite the input; 0 written')
        return 1
    data = open(a.inp, 'rb').read()
    try:
        out, rec = repoint(data, [int(x) for x in a.slots.split(',')], a.new_package, a.new_export,
                           open(a.base_header, 'rb').read())
    except RepointError as e:
        print('mesh_slot_repoint: FAIL: %s; 0 written' % e)
        return 1
    with open(a.out, 'xb') as fh:
        fh.write(out)
    with open(a.out + '.receipt.json', 'x') as fh:
        json.dump(rec, fh, indent=1)
    print('mesh_slot_repoint: %d slot(s) re-pointed, %d untouched, %d export bytes changed' % (
        len(rec['target_slots']), len(rec['untouched_slots']), rec['changed_export_bytes']))
    return 0


if __name__ == '__main__':
    sys.exit(main())
