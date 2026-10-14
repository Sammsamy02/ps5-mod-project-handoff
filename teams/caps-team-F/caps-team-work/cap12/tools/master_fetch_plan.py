#!/usr/bin/env python3
"""Plan (fetch plan ONLY, nothing is fetched) the next read-only reads that widen
the reparent candidate pool generically (Mac-only).

Which game materials: every asset-registry Material whose class reads the same
from two sources (material_class.base_props) AND passes the parent gate
(surface, lit, no special shading model, opaque or masked), has target shader
maps in the game's own store, is not overridden by the mod and is not fetched
yet. Rows (package_fetch_plan.py row shape: remote_file, pieces, local_name,
length, whole_chunk, expected_iohash20 when whole-chunk):
  priority 1  masters WITH skeletal-mesh use evidence now (fetched base
              SkeletalMesh headers -> imports -> registry 'Parent' chains);
              whole chunk (IoHash-checkable; names for parameters, export data).
  priority 2  (a) every registry SkeletalMesh header not fetched yet (prefix of
              --skeletal-header-bytes; whole chunk when shorter), to widen the
              usage evidence; (b) masters without usage evidence yet (whole chunk);
              (c) up to --child-instances smallest registry instances of each
              planned or already-candidate master (whole chunk), whose
              TextureParameterValues give the second source for role mapping.
Fetch: harvest/ftp_range_fetch.py --plan <this> --out <dir>. Also written:
<out>.p1.json / <out>.p2.json (priority 1 / 2 only), because ftp_range_fetch's
#12 row path does not carry 'priority' into --max-priority.
Check: package_fetch_plan.py check --plan <this> --dir <dir> ... (as rounds 1/2).
Exit 1 when zero rows were planned.
"""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import asset_registry  # noqa: E402
import cityhash  # noqa: E402
import container_header  # noqa: E402
import material_class as mc  # noqa: E402
import mod_material_audit as mma  # noqa: E402
import package_fetch_plan as pfp  # noqa: E402
import reparent_plan as rp  # noqa: E402
import utoc  # noqa: E402

WHOLE = 1 << 40
AR_SKELETAL = '/Script/Engine.SkeletalMesh'


def _rb(p):
    with open(p, 'rb') as fh:
        return fh.read()


def _jl(p):
    with open(p) as fh:
        return json.load(fh)


def chunk_sizes(tocs):
    out = {}
    for path in tocs:
        t = utoc.load(path)
        for r in t.rows_of_type(1):
            out[int.from_bytes(t.chunk_ids[r][:8], 'little')] = t.offlen[r][1]
    return out


def build(registry, typeidx, peers, checked, mod_names, tocs, skel_bytes, child_n):
    headers = {r['package']: r for r in checked}
    fetched = set(headers)
    used = rp.skeletal_use(checked, registry)
    sizes = chunk_sizes(tocs)
    p1, p2m, gated, refused = [], [], {}, {}
    for a in registry.assets:
        if a['class'] != mc.MATERIAL_AR_CLASS or a['package'] in mod_names:
            continue
        pkg = a['package']
        props = mc.base_props(pkg, registry, typeidx, peers, headers)
        why = rp.base_gate(props)
        if why:
            refused[why.split(':')[0]] = refused.get(why.split(':')[0], 0) + 1
            continue
        gated[pkg] = props
        if pkg in fetched:
            continue
        (p1 if pkg in used else p2m).append(pkg)
    skel = sorted(a['package'] for a in registry.assets
                  if a['class'] == AR_SKELETAL and a['package'] not in fetched and a['package'] not in mod_names)
    children = {}
    for a in registry.assets:
        if a['class'] != rp.AR_MIC or a['package'] in mod_names or a['package'] in fetched:
            continue
        m, _chain = mc.master_of(a['package'], registry, headers)
        if m in gated:
            sz = sizes.get(cityhash.ue_name_hash(a['package']))
            if sz:
                children.setdefault(m, []).append((sz, a['package']))
    kids = sorted({p for m in gated for _sz, p in sorted(children.get(m, []))[:child_n]})
    groups = [(1, 'master material (usage evidence now)', sorted(p1), WHOLE),
              (2, 'skeletal mesh header (usage evidence)', skel, skel_bytes),
              (2, 'master material (usage evidence pending)', sorted(p2m), WHOLE),
              (2, 'material instance (texture-binding evidence)', kids, WHOLE)]
    rows, missing = [], []
    for prio, purpose, pkgs, nbytes in groups:
        r, miss = pfp.plan(tocs, pkgs, nbytes)
        for row in r:
            row.update({'priority': prio, 'purpose': purpose})
            if row['package'] in gated:
                g = gated[row['package']]
                row['class'] = {'blend': g['blend'], 'domain': g['domain'], 'models': g['models'],
                                'sources': g['sources']}
                row['skeletal_mesh_evidence_now'] = used.get(row['package'], [])
        rows += r
        missing += miss
    summary = {'gated_masters_in_registry': len(gated), 'refused_by_class': refused,
               'rows': len(rows), 'bytes': sum(r['length'] for r in rows),
               'priority_1_rows': sum(r['priority'] == 1 for r in rows),
               'priority_1_bytes': sum(r['length'] for r in rows if r['priority'] == 1),
               'whole_chunk_rows': sum(r['whole_chunk'] for r in rows)}
    return rows, missing, summary


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument('--asset-registry', required=True)
    ap.add_argument('--library', nargs='+', required=True)
    ap.add_argument('--stinfo', nargs='+', required=True)
    ap.add_argument('--base-header', nargs='+', required=True, help='EVERY container header of the game')
    ap.add_argument('--toc', nargs='+', required=True, help='the game base .utoc files')
    ap.add_argument('--checked-headers', nargs='+', required=True)
    ap.add_argument('--mod-utoc', required=True)
    ap.add_argument('--mod-ucas', required=True)
    ap.add_argument('--skeletal-header-bytes', type=int, default=262144)
    ap.add_argument('--child-instances', type=int, default=2)
    ap.add_argument('--out', required=True)
    a = ap.parse_args(argv)
    if a.skeletal_header_bytes < 4096 or a.child_instances < 0:
        ap.error('--skeletal-header-bytes must be >= 4096 and --child-instances >= 0')
    store = {}
    for hp in a.base_header:
        store.update(container_header.parse(_rb(hp))[1])
    registry = asset_registry.load(a.asset_registry)
    typeidx = rp.load_type_index(a.library, a.stinfo, store)
    peers = mc.Peers(registry, typeidx)
    checked = [r for p in a.checked_headers for r in _jl(p)['checked'] if 'verified' in r]
    pk, _ = mma.load_mod(a.mod_utoc, a.mod_ucas)
    rows, missing, summary = build(registry, typeidx, peers, checked, set(pk), a.toc,
                                   a.skeletal_header_bytes, a.child_instances)
    base = a.out[:-5] if a.out.endswith('.json') else a.out
    for path, sel in ((a.out, rows), (base + '.p1.json', [r for r in rows if r['priority'] == 1]),
                      (base + '.p2.json', [r for r in rows if r['priority'] == 2])):
        with open(path, 'w') as fh:
            json.dump({'label': 'Mac-only fetch plan (read-only FTP range reads; integrator only)',
                       'tool': 'master_fetch_plan.py', 'summary': summary, 'rows': sel, 'missing': missing},
                      fh, indent=1)
    print(f"master_fetch_plan: {len(registry.assets)} registry assets, {len(peers.pop)} classed materials, "
          f"{summary['gated_masters_in_registry']} pass the two-source parent gate; planned {summary['rows']} rows "
          f"({summary['bytes']} bytes; priority 1: {summary['priority_1_rows']} rows, "
          f"{summary['priority_1_bytes']} bytes), {len(missing)} not in the given TOCs")
    return 0 if rows else 1


if __name__ == '__main__':
    sys.exit(main())
