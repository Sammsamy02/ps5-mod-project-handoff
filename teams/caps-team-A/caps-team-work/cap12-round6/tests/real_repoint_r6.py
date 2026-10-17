#!/usr/bin/env python3
"""Builder check (Mac-only, not shipped): re-point the placeholder slots of every mod SkeletalMesh whose
placeholder slots each have their own import, to the game's hide material; verify with the builder's guards.
Writes receipts + sha256 of the derived bytes (not the bytes). Prints counts; exits 1 on zero or any failure."""
import hashlib
import json
import os
import sys

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
T = os.path.join(HERE, '..', 'work', 'tools')
sys.path.insert(0, T)
sys.path.insert(0, HERE)
import mesh_sections as ms  # noqa: E402
import mesh_slot_repoint as msr  # noqa: E402
import test_round6 as t6  # noqa: E402  (mod reader helpers)
import utoc, zen  # noqa: E402,E401

out = sys.argv[1]
os.makedirs(out)
hdr = open(t6.HIDE_HDR, 'rb').read()
t = utoc.load(t6.MOD + '.utoc')
done, refused = 0, 0
with open(t6.MOD + '.ucas', 'rb') as fh:
    def rd(o, n):
        fh.seek(o)
        return fh.read(n)
    for row in t.rows_of_type(1):
        d = t.read_chunk(row, rd)
        try:
            m = ms.decode(d)
        except ms.MeshError:
            continue
        lod = m['lods'][0]
        tgt = sorted({r['material_index'] for r in lod['sections'] if ms.section_placeholder(m, lod, r)[0]})
        if not tgt:
            continue
        try:
            der, rec = msr.repoint(d, tgt, t6.HIDE, 'M_Hide', hdr)
        except msr.RepointError as e:
            refused += 1
            print('  refused %s: %s' % (m['package'], e))
            continue
        rec['derived_sha256'] = hashlib.sha256(der).hexdigest()
        rec['source_sha256'] = hashlib.sha256(d).hexdigest()
        name = m['package'].rsplit('/', 1)[-1]
        json.dump(rec, open(os.path.join(out, name + '.receipt.json'), 'w'), indent=1)
        done += 1
        print('  %s: %d placeholder slot(s) -> M_Hide, %d untouched, %d export bytes changed' % (
            name, len(tgt), len(rec['untouched_slots']), rec['changed_export_bytes']))
print('real_repoint_r6: %d meshes re-pointed (builder check only, nothing shipped), %d refused' % (done, refused))
sys.exit(0 if done == 11 and refused == 0 else 1)
