#!/usr/bin/env python3
"""Plan and check read-only fetches of BASE-GAME package headers (Mac-only).

Why: choosing a base material whose PS5 shaders a mod material can reuse needs
the base materials' parameter names, which live in each package's zen header
(name map). Headers sit at the start of each package's export chunk, so only
the first bytes of the chunk are needed.

  plan  --toc T.utoc --packages /Game/A /Game/B ...  [--header-bytes 65536]
        -> JSON rows {remote_file, offset, length, package, package_id, whole_chunk, expected_iohash20}
  check --plan P.json --dir FETCHED --script-utoc G.utoc --script-ucas G.ucas
        [--base-header CH.bin ...]   (pass EVERY container header you have)
        -> parses every fetched header; verifies the package name inside hashes
           (CityHash law) to the requested package id (second source), and full
           chunks additionally by BLAKE3-20 == TOC IoHash; records each package's
           top-level export names + classes (global ScriptObjects) so the chooser
           can check the export class; lists imported packages (with 'has target
           shader maps' from the game's own store, or 'store unknown' when no
           loaded container header lists that package) for the next round.
Both fail (exit 1) when zero rows were planned/checked or any check fails.
"""
import argparse
import json
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import blake3  # noqa: E402
import cityhash  # noqa: E402
import container_header  # noqa: E402
import mod_material_audit as mma  # noqa: E402
import utoc  # noqa: E402
import zen  # noqa: E402


def _rb(p):
    with open(p, 'rb') as fh:
        return fh.read()


def _jl(p):
    with open(p) as fh:
        return json.load(fh)


def _jd(obj, p):
    with open(p, 'w') as fh:
        json.dump(obj, fh, indent=1)


def plan(tocs, packages, header_bytes):
    rows, missing = [], []
    index = {}
    for path in tocs:
        t = utoc.load(path)
        for r in t.rows_of_type(1):
            index[int.from_bytes(t.chunk_ids[r][:8], 'little')] = (path, t, r)
    for name in packages:
        pid = cityhash.ue_name_hash(name)
        if pid not in index:
            missing.append(name)
            continue
        path, t, r = index[pid]
        ranges = t.physical_ranges(r)
        total = t.offlen[r][1]
        want = min(total, header_bytes)
        got, pieces = 0, []
        for o, n in ranges:
            if got >= want:
                break
            take = min(n, want - got)
            pieces.append([o, take])
            got += take
        rows.append({'remote_file': os.path.basename(path)[:-5] + '.ucas', 'package': name,
                     'package_id': f'{pid:016x}', 'pieces': pieces, 'length': want,
                     'whole_chunk': want == total, 'chunk_length': total,
                     'expected_iohash20': t.metas[r][0] if want == total else None,
                     'local_name': f'{pid:016x}.hdr.bin'})
    return rows, missing


def check(rows, fetched_dir, store, script=None):
    out, fails = [], []
    for row in rows:
        p = os.path.join(fetched_dir, row['local_name'])
        rec = {'package': row['package']}
        out.append(rec)
        if not os.path.exists(p):
            fails.append(f"{row['package']}: not fetched")
            continue
        data = _rb(p)
        if len(data) != row['length']:
            fails.append(f"{row['package']}: length {len(data)} != {row['length']}")
            continue
        if row['whole_chunk'] and blake3.iohash20(data) != row['expected_iohash20']:
            fails.append(f"{row['package']}: IoHash mismatch")
            continue
        try:
            pk = zen.ZenPackage(data) if row['whole_chunk'] else header_only(data)
        except (zen.ZenError, ValueError, IndexError, struct.error) as e:
            fails.append(f"{row['package']}: header parse failed: {e}")
            continue
        if f'{cityhash.ue_name_hash(pk.name):016x}' != row['package_id']:
            fails.append(f"{row['package']}: header names {pk.name}, not the requested package")
            continue
        rec.update({'verified': 'iohash' if row['whole_chunk'] else 'name-hash',
                    'names': list(pk.names), 'imports': list(pk.imported_packages),
                    'exports': pk.top_level_exports(script) if script is not None else None,
                    'imports_with_target_shader_maps': [i for i in pk.imported_packages
                                                        if store.get(cityhash.ue_name_hash(i), {}).get('shader_maps')],
                    'imports_store_unknown': [i for i in pk.imported_packages
                                              if cityhash.ue_name_hash(i) not in store]})
    return out, fails


def header_only(data):
    """Parse a zen header (incl. export map) from a prefix that may stop before
    the export data. Same structural checks as a whole package."""
    zen.need(len(data) >= 64, 'fetched prefix too short')
    hs = struct.unpack_from('<I', data, 4)[0]
    zen.need(hs <= len(data), 'fetched prefix shorter than header')
    return zen.ZenPackage(data[:hs], header_only=True)


def main(argv=None):
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest='cmd', required=True)
    a1 = sub.add_parser('plan')
    a1.add_argument('--toc', nargs='+', required=True)
    a1.add_argument('--packages', nargs='+', required=True)
    a1.add_argument('--header-bytes', type=int, default=65536)
    a1.add_argument('--out', required=True)
    a2 = sub.add_parser('check')
    a2.add_argument('--plan', required=True)
    a2.add_argument('--dir', required=True)
    a2.add_argument('--base-header', nargs='*', default=[])
    a2.add_argument('--script-utoc', required=True, help='global.utoc (ScriptObjects, for export classes)')
    a2.add_argument('--script-ucas', required=True)
    a2.add_argument('--out', required=True)
    a = ap.parse_args(argv)
    if a.cmd == 'plan':
        rows, missing = plan(a.toc, a.packages, a.header_bytes)
        _jd({'label': 'Mac-only', 'rows': rows, 'missing': missing}, a.out)
        print(f'package_fetch_plan: planned {len(rows)} rows ({sum(r["length"] for r in rows)} bytes), '
              f'{len(missing)} not in the given TOCs')
        return 0 if rows and not missing else 1
    store = {}
    for hp in a.base_header:
        store.update(container_header.parse(_rb(hp))[1])
    so = mma.load_script_objects(a.script_utoc, a.script_ucas)
    rows = _jl(a.plan)['rows']
    out, fails = check(rows, a.dir, store, so)
    _jd({'label': 'Mac-only', 'checked': out, 'failures': fails}, a.out)
    n = len([r for r in out if 'verified' in r])
    unk = sorted({i for r in out for i in r.get('imports_store_unknown', [])})
    print(f'package_fetch_plan check: {n} of {len(rows)} headers verified, {len(fails)} failures, '
          f'{len(a.base_header)} container headers loaded, {len(unk)} imported packages not in any loaded store')
    for f in fails:
        print('  FAIL', f)
    return 0 if n and not fails else 1


if __name__ == '__main__':
    sys.exit(main())
