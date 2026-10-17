#!/usr/bin/env python3
"""Verify and decode shader-library manifests the integrator fetched (Mac-only).

Input: the fetch plan from shader_inputs.py, a directory with the fetched files
(named by each row's local_name), and EVERY container header (type-6 chunk) the
plan lists (container_header_plan) to join package -> shader map -> library.
Checks per row: exact length, BLAKE3-20 == the TOC's own IoHash (second source),
manifest parse + structural validation, and recomputation of EVERY type-9 group
chunk id from member hashes/sizes + format string (third, internal source).
Each container header given is matched to its plan row by IoHash.
Fails (exit 1) on: zero rows checked, any mismatch, a library with zero groups
(nothing to recompute), a planned container header not given, or any store
shader map not found in the fetched libraries (missing != 0).
Read-only except --out.
"""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import blake3  # noqa: E402
import container_header  # noqa: E402
import shader_library  # noqa: E402


def format_candidates(hash_input):
    """Group-hash format string candidates derived from the library hash input
    '<Lib>-<Fmt>-<Plat>' (retoc shader_library.rs:1086-1097 notes UnrealPak
    splits at the LAST '-'). Tried in this order; the manifest decides."""
    parts = hash_input.split('-')
    cands = []
    for s in (parts[-1], '-'.join(parts[1:]), parts[1] if len(parts) > 2 else parts[-1]):
        for enc in ('utf-8', 'utf-16-le'):
            b = s.encode(enc)
            if b not in cands:
                cands.append(b)
    return cands


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument('--plan', required=True)
    ap.add_argument('--dir', required=True)
    ap.add_argument('--container-header', nargs='*', default=[])
    ap.add_argument('--out', required=True)
    a = ap.parse_args(argv)
    with open(a.plan) as fh:
        plan = json.load(fh)
    rows = plan['fetch_plan']
    libs = {}
    for lib in (lib for t in plan['tocs'] for lib in t['libraries']):
        libs[lib['chunk_id']] = lib
    report = {'label': 'Mac-only', 'rows': [], 'failures': []}
    all_maps = {}
    for r in rows:
        p = os.path.join(a.dir, r['local_name'])
        res = {'library': r['library'], 'file': r['local_name']}
        report['rows'].append(res)
        if not os.path.exists(p):
            report['failures'].append(f"{r['local_name']}: missing")
            continue
        with open(p, 'rb') as fh:
            data = fh.read()
        res['bytes'] = len(data)
        if len(data) != r['length']:
            report['failures'].append(f"{r['local_name']}: length {len(data)} != {r['length']}")
            continue
        h = blake3.iohash20(data)
        res['iohash20'] = h
        res['iohash_ok'] = h == r['expected_iohash20']
        if not res['iohash_ok']:
            report['failures'].append(f"{r['local_name']}: IoHash mismatch")
            continue
        try:
            lib = shader_library.Library(data)
            res['counts'] = lib.validate()
        except shader_library.LibError as e:
            report['failures'].append(f"{r['local_name']}: {e}")
            continue
        hin = libs[r['chunk_id']]['hash_input']
        if not lib.group_entries:
            res['group_id_format'] = None
            res['group_ids_recomputed'] = False
            report['failures'].append(f"{r['local_name']}: library has zero shader groups (nothing recomputed)")
            continue
        fmt = lib.check_group_ids(format_candidates(hin))
        res['group_id_format'] = fmt.decode('latin1') if fmt is not None else None
        res['group_ids_recomputed'] = fmt is not None
        res['group_ids_checked'] = len(lib.group_entries) if fmt is not None else 0
        if fmt is None:
            report['failures'].append(f"{r['local_name']}: no format string reproduces all group ids")
        for mh in lib.map_hashes:
            all_maps.setdefault(mh, []).append(r['library'])
    ch_plan = {row['expected_iohash20']: row for row in plan.get('container_header_plan', [])}
    if not ch_plan:
        report['failures'].append('plan lists no container headers (re-run shader_inputs.py)')
    matched = set()
    report['container_headers'] = []
    for chp in a.container_header:
        with open(chp, 'rb') as fh:
            data = fh.read()
        h = blake3.iohash20(data)
        row = ch_plan.get(h)
        if row is None or len(data) != row['length']:
            report['failures'].append(f'{os.path.basename(chp)}: not a planned container header (IoHash/length)')
            continue
        matched.add(h)
        try:
            _cid, pk = container_header.parse(data)
        except container_header.HeaderError as e:
            report['failures'].append(f'{os.path.basename(chp)}: {e}')
            continue
        refs = {x for v in pk.values() for x in v['shader_maps']}
        found = {x for x in refs if x in all_maps}
        report['container_headers'].append(
            {'file': os.path.basename(chp), 'toc': row.get('toc'), 'packages': len(pk), 'unique_shader_maps': len(refs),
             'found_in_fetched_libraries': len(found), 'missing': len(refs) - len(found)})
        if len(refs) != len(found):
            report['failures'].append(f"{os.path.basename(chp)}: {len(refs) - len(found)} store shader maps "
                                      f"not in the fetched libraries")
    for h, row in ch_plan.items():
        if h not in matched:
            report['failures'].append(f"container header of {row.get('toc')} not given: its shader maps are not joined")
    report['items_checked'] = len([x for x in report['rows'] if 'bytes' in x])
    with open(a.out, 'w') as fh:
        json.dump(report, fh, indent=1)
    print(f"verify_fetch: checked {report['items_checked']} of {len(rows)} rows, "
          f"{len(report['container_headers'])} of {len(ch_plan)} container headers, "
          f"{len(report['failures'])} failures")
    for f in report['failures']:
        print('  FAIL', f)
    ok = report['items_checked'] > 0 and not report['failures']
    print('verify_fetch:', 'PASS' if ok else 'FAIL')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
