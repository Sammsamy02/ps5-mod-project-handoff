#!/usr/bin/env python3
"""Read-only fetch plans for console samples of the asset classes that still lack evidence (Mac-only).

  plan        --toc T.utoc [--toc ...] --registry AssetRegistry.bin [--per-class N] [--max-chunk B]
              [--max-total B] [--must /Game/Pkg ...] --out F1.json
              Picks, per class (and per variant: Nanite on/off, GPU emitter yes/no), the SMALLEST packages of
              the game's own TOC whose registry class matches, plus every --must package (e.g. the game's own
              copy of an asset a PC mod replaces). Rows are whole chunks (export chunk + its bulk chunk when
              small enough) in the shape harvest/ftp_range_fetch.py reads ('chunks' with ucas,
              physical_ranges, save_as), with the TOC's own IoHash for checking.
  bank-plan   --list LIST.txt --remote-name-prefix '' [--bytes 65536] --out F2.json
              From a read-only FTP LIST of the game's audio-bank folder, plans the first bytes (format header)
              of the two smallest .bank files.
  verify      --plan F.json --dir FETCHED
              Every planned file: exact length, BLAKE3-20 == the TOC's IoHash (second source), and for
              export chunks the package name inside hashes to the planned package id (third source).
All three fail (exit 1) when zero rows were planned/checked or any check fails. Nothing is written
except --out; nothing is fetched here (the integrator runs ftp_range_fetch.py with the plan).
"""
import argparse
import json
import os
import re
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import jlib  # noqa: E402

TOOL = 'teamJ-sample-plan-1'
# (registry class, variant tag, variant value or None, priority, why)
DEFAULT_CLASSES = [
    ('/Script/Engine.StaticMesh', 'NaniteEnabled', 'True', 1, 'static mesh, Nanite'),
    ('/Script/Engine.StaticMesh', 'NaniteEnabled', 'False', 1, 'static mesh, no Nanite'),
    ('/Script/Engine.PhysicsAsset', None, None, 1, 'physics asset'),
    ('/Script/Engine.DataTable', None, None, 1, 'data table'),
    ('/Script/Engine.BlueprintGeneratedClass', None, None, 1, 'Blueprint class'),
    ('/Script/UMG.WidgetBlueprintGeneratedClass', None, None, 1, 'widget Blueprint class'),
    ('/Script/Engine.AnimSequence', None, None, 1, 'animation sequence'),
    ('/Script/Engine.AnimMontage', None, None, 1, 'animation montage'),
    ('/Script/Engine.AnimBoneCompressionSettings', None, None, 1, 'bone compression codec settings'),
    ('/Script/Niagara.NiagaraSystem', 'HasGPUEmitter', 'True', 1, 'Niagara system with a GPU emitter'),
    ('/Script/Niagara.NiagaraSystem', 'HasGPUEmitter', 'False', 1, 'Niagara system, CPU emitters only'),
    ('/Script/Engine.SoundWave', None, None, 1, 'sound wave'),
    ('/Script/Engine.SoundCue', None, None, 2, 'sound cue'),
    ('/Script/FMODStudio.FMODEvent', None, None, 2, 'FMOD event stub'),
    ('/Script/FMODStudio.FMODBank', None, None, 2, 'FMOD bank stub'),
    ('/Script/Engine.BlendSpace', None, None, 2, 'blend space'),
    ('/Script/Engine.PoseAsset', None, None, 2, 'pose asset'),
    ('/Script/Engine.AnimCurveCompressionSettings', None, None, 2, 'curve compression settings'),
    ('/Script/Engine.CurveFloat', None, None, 2, 'float curve'),
    ('/Script/CoreUObject.UserDefinedStruct', None, None, 2, 'user struct'),
    ('/Script/Engine.UserDefinedEnum', None, None, 2, 'user enum'),
    ('/Script/PhysicsCore.PhysicalMaterial', None, None, 2, 'physical material'),
    ('/Script/GeometryCollectionEngine.GeometryCollection', None, None, 2, 'geometry collection'),
    ('/Script/HairStrandsCore.GroomAsset', None, None, 2, 'groom'),
    ('/Script/LevelSequence.LevelSequence', None, None, 2, 'level sequence'),
    ('/Script/Engine.TextureCube', None, None, 2, 'cube texture'),
    ('/Script/Engine.FontFace', None, None, 2, 'font face'),
    ('/Script/Engine.MaterialFunction', None, None, 2, 'material function'),
]


def _chunk(t, tocname, ucas, row, priority, why, package, pid):
    cid = t.chunk_ids[row].hex()
    pr = t.physical_ranges(row)
    return {'ucas': ucas, 'priority': priority, 'why': why, 'package': package, 'package_id': f'{pid:016x}',
            'save_as': f'{tocname}/{cid}.bin', 'toc_row': row, 'chunk_id': cid,
            'chunk_type': t.chunk_type(t.chunk_ids[row]), 'length': t.offlen[row][1], 'io_hash20': t.metas[row][0],
            'physical_ranges': [{'offset': o, 'length': n} for o, n in pr]}


def plan(R, tocs, registry_path, per_class, max_chunk, max_total, must, classes):
    reg = R.asset_registry.load(registry_path)
    index = {}
    for p in tocs:
        t = R.utoc.load(p)
        stem = os.path.basename(p)[:-5]
        by_pid = {}
        for row, cid in enumerate(t.chunk_ids):
            if int.from_bytes(cid[8:10], 'little') == 0 and t.chunk_type(cid) in (1, 2):
                by_pid.setdefault(int.from_bytes(cid[:8], 'little'), {})[t.chunk_type(cid)] = row
        for pid, rows in by_pid.items():
            if 1 in rows:
                index.setdefault(pid, (t, stem, rows))
    chunks, notes, total = [], [], 0
    seen = set()

    def add(package, priority, why):
        nonlocal total
        pid = jlib.package_id(R, package)
        if pid in seen or pid not in index:
            return 'not in the given TOCs' if pid not in index else 'already planned'
        t, stem, rows = index[pid]
        er, br = rows[1], rows.get(2)
        elen = t.offlen[er][1]
        blen = t.offlen[br][1] if br is not None else 0
        want = elen + (blen if blen <= max_chunk else 0)
        if elen > max_chunk or total + want > max_total:
            return f'too big ({elen} + {blen} bytes)'
        try:
            chunks.append(_chunk(t, stem, stem + '.ucas', er, priority, why, package, pid))
            if br is not None and blen <= max_chunk:
                chunks.append(_chunk(t, stem, stem + '.ucas', br, priority, why + ' (bulk)', package, pid))
        except Exception as ex:  # e.g. compressed blocks: fail closed for this package only
            return f'not plannable: {ex}'
        seen.add(pid)
        total += want
        return None

    for package in must:
        why = add(package, 1, 'game copy of an asset a PC mod replaces')
        if why:
            notes.append({'package': package, 'skipped': why})
    for cls, tag, val, prio, label in classes:
        cands = []
        for a in reg.assets:
            if a['class'] != cls or (tag and a['tags'].get(tag) != val):
                continue
            pid = jlib.package_id(R, a['package'])
            if pid in index:
                t, _s, rows = index[pid]
                cands.append((t.offlen[rows[1]][1], a['package']))
        cands.sort()
        # the smallest sample (cheap) and a median-size one (representative), within the size cap
        fit = [c for c in cands if c[0] <= max_chunk]
        order = fit[:1] + ([fit[len(fit) // 2]] if len(fit) > 2 else []) + fit[1:]
        got = 0
        for _n, package in order:
            if got >= per_class:
                break
            if add(package, prio, label) is None:
                got += 1
        notes.append({'class': cls, 'variant': f'{tag}={val}' if tag else None, 'candidates': len(cands),
                      'planned': got})
    return {'tool': TOOL, 'label': 'Mac-only plan; READ-ONLY console fetch (REST+RETR), run by the integrator',
            'read_only': True, 'source_tocs': [os.path.abspath(p) for p in tocs],
            'registry': os.path.abspath(registry_path), 'max_chunk': max_chunk, 'max_total': max_total,
            'total_bytes': sum(c['length'] for c in chunks), 'chunks': chunks, 'selection': notes}


LIST_LINE = re.compile(r'^-[rwx-]{9}\s+\d+\s+\S+\s+\S+\s+(\d+)\s+\w{3}\s+\d+\s+[\d:]+\s+(.+)$')
SAFE_NAME = re.compile(r'^[A-Za-z0-9_][A-Za-z0-9_. -]{0,127}$')


def bank_plan(list_path, nbytes):
    files = []
    with open(list_path, errors='replace') as fh:
        for line in fh:
            m = LIST_LINE.match(line.rstrip('\n'))
            # regular files only (no links); names come from the console, so only plain names are kept
            name = m.group(2) if m else ''
            if m and name.lower().endswith('.bank') and SAFE_NAME.match(name) and '..' not in name \
                    and ' -> ' not in name:
                files.append((int(m.group(1)), name))
    files.sort()
    chunks = [{'ucas': name, 'priority': 1, 'why': 'audio bank format header (codec of the console build)',
               'save_as': f'banks/{name}.head.bin',
               'physical_ranges': [{'offset': 0, 'length': min(nbytes, size)}]} for size, name in files[:2]]
    return {'tool': TOOL, 'label': 'READ-ONLY console fetch of the first bytes of two audio banks',
            'read_only': True, 'list': os.path.abspath(list_path), 'banks_listed': len(files), 'chunks': chunks,
            'remote_dir_note': 'pass the folder the LIST came from as --remote-dir'}


def verify(R, plan_path, d):
    with open(plan_path) as fh:
        p = json.load(fh)
    ok = bad = 0
    rows = []
    for c in p['chunks']:
        f = os.path.join(d, c['save_as'])
        why = None
        if not os.path.isfile(f):
            why = 'missing'
        else:
            with open(f, 'rb') as fh:
                data = fh.read()
            want = sum(r['length'] for r in c['physical_ranges'])
            if len(data) != want:
                why = f'length {len(data)} != {want}'
            elif c.get('io_hash20') and R.blake3.iohash20(data) != c['io_hash20']:
                why = 'BLAKE3-20 != TOC IoHash'
            elif c.get('chunk_type') == 1:
                try:
                    z = R.zen.ZenPackage(data)
                    if f"{jlib.package_id(R, z.name):016x}" != c['package_id']:
                        why = f'package name {z.name} does not hash to {c["package_id"]}'
                except Exception as ex:
                    why = f'unreadable package: {ex}'
        rows.append({'save_as': c['save_as'], 'ok': why is None, 'why': why})
        ok += why is None
        bad += why is not None
    return ok, bad, rows


def main(argv=None):
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest='cmd', required=True)
    a1 = sub.add_parser('plan')
    a1.add_argument('--toc', action='append', required=True)
    a1.add_argument('--registry', required=True)
    a1.add_argument('--per-class', type=int, default=2)
    a1.add_argument('--max-chunk', type=int, default=4 << 20)
    a1.add_argument('--max-total', type=int, default=48 << 20)
    a1.add_argument('--must', nargs='*', default=[],
                    help='game packages a PC sample replaces (their console copies are planned first)')
    a1.add_argument('--readers', default=jlib.READERS_DEFAULT)
    a1.add_argument('--out', required=True)
    a2 = sub.add_parser('bank-plan')
    a2.add_argument('--list', required=True)
    a2.add_argument('--bytes', type=int, default=65536)
    a2.add_argument('--out', required=True)
    a3 = sub.add_parser('verify')
    a3.add_argument('--plan', required=True)
    a3.add_argument('--dir', required=True)
    a3.add_argument('--readers', default=jlib.READERS_DEFAULT)
    a = ap.parse_args(argv)
    if a.cmd == 'plan':
        R = jlib.Readers(a.readers)
        p = plan(R, a.toc, a.registry, a.per_class, a.max_chunk, a.max_total, a.must, DEFAULT_CLASSES)
        with open(a.out, 'w') as fh:
            json.dump(p, fh, indent=1)
        print(f"make_sample_plan: {len(p['chunks'])} chunks, {p['total_bytes']} bytes planned "
              f"({sum(1 for c in p['chunks'] if c['chunk_type'] == 1)} packages)")
        for n in p['selection']:
            print('  ', n)
        return 0 if p['chunks'] else 1
    if a.cmd == 'bank-plan':
        p = bank_plan(a.list, a.bytes)
        with open(a.out, 'w') as fh:
            json.dump(p, fh, indent=1)
        print(f"bank-plan: {p['banks_listed']} banks listed, {len(p['chunks'])} planned")
        return 0 if p['chunks'] else 1
    R = jlib.Readers(a.readers)
    ok, bad, rows = verify(R, a.plan, a.dir)
    for r in rows:
        if not r['ok']:
            print('  FAIL', r['save_as'], r['why'])
    print(f'verify: {ok} ok, {bad} failed, {ok + bad} checked')
    return 0 if ok and not bad else 1


if __name__ == '__main__':
    sys.exit(main())
