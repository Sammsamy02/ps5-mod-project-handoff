#!/usr/bin/env python3
"""Locate a UE5 game's own shader code inputs from its own files (Mac-only, read-only).

Generic for UE5 IoStore titles: no game names or values are hard-coded.
Inputs:
  --toc   one or more .utoc files of the game (e.g. pakchunk*-<plat>.utoc)
  --names text file(s) or directories whose file NAMES list the game's shader
          side files (ShaderTypeInfo-*.stinfo, ShaderArchive-*.ushaderbytecode,
          ShaderAssetInfo-*.assetinfo.json, GlobalShaderCache-*.bin). Typically the
          game's own classic .pak listing or an extracted copy of it.
Output (JSON): for every ShaderCodeLibrary (type 8) chunk: the library name that
reproduces its chunk id (CityHash64 law, see cityhash.py), its exact physical
byte ranges in the matching .ucas and the TOC's own IoHash; plus ShaderCode
(type 9) group counts/bytes per TOC; plus a ready fetch plan for the integrator
(fetch_plan = type-8 manifests; container_header_plan = every TOC's type-6
container header, so shader-map refs of all containers can be joined).
Fails (exit 1) when it checked zero TOCs, zero type-8 rows, or any type-8 row
stays unidentified.
"""
import argparse
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cityhash  # noqa: E402
import utoc  # noqa: E402

SIDE = re.compile(r'(?:ShaderTypeInfo|ShaderArchive|ShaderAssetInfo)-(?P<lib>[^-/\\]+)-(?P<fmt>[^-/\\]+)-(?P<plat>[^-/\\.]+)')
GLOBAL = re.compile(r'GlobalShaderCache-(?P<plat>[A-Za-z0-9_]+)\.bin')
CHUNKNUM = re.compile(r'pakchunk(\d+)', re.I)


def collect_names(paths):
    names = []
    for p in paths:
        if os.path.isdir(p):
            for root, _d, files in os.walk(p):
                names.extend(files)
        else:
            with open(p, 'r', errors='replace') as fh:
                names.extend(fh.read().split())
    return names


def candidates(names, toc_paths):
    libs, suffixes, plats = set(), set(), set()
    for n in names:
        m = SIDE.search(n)
        if m:
            libs.add(m.group('lib'))
            suffixes.add(f"{m.group('fmt')}-{m.group('plat')}")
        g = GLOBAL.search(n)
        if g:
            plats.add(g.group('plat'))
    # chunked libraries follow "<Project>_Chunk<N>" with N from the container names
    projects = {l.rsplit('_Chunk', 1)[0] for l in libs if '_Chunk' in l}
    nums = {int(m.group(1)) for p in toc_paths for m in [CHUNKNUM.search(os.path.basename(p))] if m}
    for proj in projects:
        for n in nums:
            libs.add(f'{proj}_Chunk{n}')
    return sorted(libs), sorted(suffixes), sorted(plats)


def fetch_row(ucas, ranges, cid, iohash, local_name, **extra):
    """One read-only fetch row. 'pieces' are the exact physical ranges; the local
    file is their concatenation in order (= the chunk), so its BLAKE3-20 must equal
    the TOC IoHash. 'offset'/'length' keep the single-range form when contiguous."""
    contiguous = all(ranges[i][0] + ranges[i][1] == ranges[i + 1][0] for i in range(len(ranges) - 1))
    row = {'remote_file': ucas, 'offset': ranges[0][0], 'length': sum(n for _o, n in ranges),
           'contiguous': contiguous, 'pieces': [list(r) for r in ranges], 'chunk_id': cid,
           'expected_iohash20': iohash, 'local_name': local_name}
    row.update(extra)
    return row


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument('--toc', nargs='+', required=True)
    ap.add_argument('--names', nargs='+', required=True)
    ap.add_argument('--out', required=True)
    a = ap.parse_args(argv)
    names = collect_names(a.names)
    libs, suffixes, plats = candidates(names, a.toc)
    table = {}
    for lib in libs:
        for suf in suffixes:
            table[cityhash.shader_library_chunk_id(lib, suf)[:16]] = (lib, suf)
    out = {'label': 'Mac-only', 'tool': 'shader_inputs.py', 'candidate_libraries': libs,
           'candidate_format_platform': suffixes, 'global_shader_cache_platforms': plats,
           'tocs': [], 'fetch_plan': [], 'container_header_plan': []}
    n_type8 = unresolved = 0
    for path in a.toc:
        t = utoc.load(path)
        ucas = os.path.basename(path)[:-5] + '.ucas'
        entry = {'toc': os.path.basename(path), 'ucas': ucas, 'chunks': t.entry_count,
                 'compression_methods': t.methods, 'libraries': []}
        g = t.rows_of_type(9)
        entry['shader_code_groups'] = len(g)
        entry['shader_code_bytes'] = sum(t.offlen[r][1] for r in g)
        for row in t.rows_of_type(8):
            n_type8 += 1
            cid = t.chunk_ids[row].hex()
            ident = table.get(cid[:16])
            if ident is None:
                unresolved += 1
            ranges = t.physical_ranges(row)
            lib = {'chunk_id': cid, 'library': ident[0] if ident else None,
                   'format_platform': ident[1] if ident else None,
                   'hash_input': f'{ident[0]}-{ident[1]}' if ident else None,
                   'logical_offset': t.offlen[row][0], 'length': t.offlen[row][1],
                   'physical_ranges': ranges, 'iohash20': t.metas[row][0]}
            entry['libraries'].append(lib)
            if ranges:
                out['fetch_plan'].append(fetch_row(ucas, ranges, cid, lib['iohash20'],
                                                   f"{lib['library'] or cid}.type8.bin", library=lib['library']))
        # every container header (type 6): needed so that shader-map refs of ALL
        # containers are joined, not only pakchunk0's (small, IoHash-verified)
        for row in t.rows_of_type(6):
            ranges = t.physical_ranges(row)
            if ranges:
                out['container_header_plan'].append(
                    fetch_row(ucas, ranges, t.chunk_ids[row].hex(), t.metas[row][0],
                              f"{os.path.basename(path)[:-5]}.container-header.bin", toc=os.path.basename(path)))
        out['tocs'].append(entry)
    out['items_checked'] = {'tocs': len(a.toc), 'type8_rows': n_type8, 'unresolved_type8': unresolved,
                            'name_inputs': len(names), 'container_headers': len(out['container_header_plan'])}
    with open(a.out, 'w') as fh:
        json.dump(out, fh, indent=1)
    print(f'shader_inputs: checked {len(a.toc)} TOCs, {len(names)} names, {n_type8} type-8 rows, '
          f'{unresolved} unresolved, {len(out["container_header_plan"])} container headers planned')
    if not a.toc or n_type8 == 0 or unresolved:
        print('shader_inputs: FAIL')
        return 1
    print('shader_inputs: PASS')
    return 0


if __name__ == '__main__':
    sys.exit(main())
