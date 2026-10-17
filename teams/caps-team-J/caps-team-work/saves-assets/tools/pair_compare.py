#!/usr/bin/env python3
"""PC-cooked vs console-cooked packages of the SAME class: what differs (Mac-only, read-only).

Inputs:
  --pc UTOC UCAS            (repeatable) PC mod containers, read unchanged
  --ps5-dir DIR             (repeatable) directories of game chunks fetched read-only from the console
                            (files named <24-hex chunk id>.bin; export chunks type 01, bulk type 02)
  --base-toc T.utoc ...     the game's own TOCs (virtual paths + IoHash for the fetched files)
  --script-utoc/--script-ucas  the game's global container (ScriptObjects)
  --mesh-decoder DIR        optional: directory holding an independent SkeletalMesh decoder
                            (mesh_sections.py, Team A) used as an extra structural source
Output: JSON with, per top-level class, the fingerprints of both forms and a 'shape' verdict:
  same-shape        every PC sample's package flags and bulk-flag signature are seen on a console sample,
                    no unreadable structure, no PC shader code on either side (+ for skeletal meshes: the
                    independent decoder reads every PC sample and at least one console sample)
  platform-specific the two forms differ in a known platform layer (texture tiling / shader maps)
  differs           something else differs (listed)
  one-side-only     no sample of that class on the other side (no verdict)
Every fetched console file is re-identified two ways before use: the package name inside it must hash
(CityHash, lower-case UTF-16) to the chunk id's package id, and when the game's TOC lists the chunk,
the game's TOC must list the chunk (else refused), its length must match the TOC, and BLAKE3-20 must equal
the TOC IoHash (every file; verified ones are cached in --hash-receipt by sha256).
Exit 1 when zero package pairs were compared.
"""
import argparse
import collections
import hashlib
import json
import os
import re
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import jlib  # noqa: E402

CHUNK_FILE = re.compile(r'^([0-9a-f]{24})\.bin$')


def load_ps5_samples(R, dirs, base_tocs, receipt_path):
    """Console chunk files -> {pid: (path, bytes)}, {pid: bulk bytes}, refused. A file is used only when the
    game's TOC lists its chunk id, its length equals the TOC's, and BLAKE3-20 equals the TOC IoHash. BLAKE3 is
    slow in pure Python, so verified files are remembered in a receipt keyed by their sha256."""
    toc_rows = {}
    for p in base_tocs:
        t = R.utoc.load(p)
        for row, cid in enumerate(t.chunk_ids):
            toc_rows[cid.hex()] = (t, row, os.path.basename(p))
    receipt = {}
    if receipt_path and os.path.isfile(receipt_path):
        with open(receipt_path) as fh:
            receipt = json.load(fh)
    exports, bulks, refused = {}, {}, []
    for d in dirs:
        for root, _dirs, files in os.walk(d):
            for fn in sorted(files):
                m = CHUNK_FILE.match(fn)
                if not m:
                    continue
                cid = m.group(1)
                ctype = int(cid[22:24], 16) & 0x3f
                if ctype not in (1, 2) or cid[16:20] != '0000':
                    continue
                path = os.path.join(root, fn)
                with open(path, 'rb') as fh:
                    data = fh.read()
                tr = toc_rows.get(cid)
                why = None
                if not tr:
                    why = 'chunk id not in any given game TOC'
                else:
                    t, row, tname = tr
                    sha = hashlib.sha256(data).hexdigest()
                    if t.offlen[row][1] != len(data):
                        why = f'length {len(data)} != TOC {t.offlen[row][1]}'
                    elif receipt.get(sha) != t.metas[row][0]:
                        if R.blake3.iohash20(data) != t.metas[row][0]:
                            why = 'BLAKE3-20 != TOC IoHash'
                        else:
                            receipt[sha] = t.metas[row][0]
                if why:
                    refused.append({'file': path, 'why': why})
                    continue
                pid = int.from_bytes(bytes.fromhex(cid[:16]), 'little')
                if ctype == 2:
                    bulks[pid] = data
                    continue
                exports.setdefault(pid, (path, data))
    if receipt_path:
        with open(receipt_path, 'w') as fh:
            json.dump(receipt, fh, indent=0, sort_keys=True)
    return exports, bulks, refused


def skeletal_decode(dec, data):
    if dec is None:
        return None
    try:
        m = dec.decode(data)
        return {'ok': True, 'lods': len(m['lods']), 'slots': len(m['slots']), 'bones': m['bones'],
                'inline': [lod['inline_geometry'] for lod in m['lods']]}
    except Exception as ex:  # fail closed: recorded, never guessed
        return {'ok': False, 'why': str(ex)[:120]}


def texture_form(R, data, bulk_len):
    try:
        lay = R.texture_package.layout(data, bulk_len, allow_ps5=True)
        return 'console-tiled' if lay['ps5'] else 'linear'
    except Exception as ex:
        return 'unread: ' + str(ex)[:80]


def describe(R, so, data, bulk_len, dec, side, path):
    z = R.zen.ZenPackage(data)
    fp = jlib.fingerprint(R, z, data, so)
    fp['side'] = side
    fp['path'] = path
    tops = [c for c in fp['top_level_classes'] if not c.startswith('export:')]
    fp['class'] = tops[0] if len(tops) == 1 else ('|'.join(tops) if tops else 'none')
    if fp['class'].endswith('/SkeletalMesh'):
        fp['skeletal_decode'] = skeletal_decode(dec, data)
    if fp['class'].endswith('/Texture2D'):
        fp['texture_form'] = texture_form(R, data, bulk_len)
    return z.name, fp


def _bulk_sig(f):
    return tuple(sorted(f.get('bulk_flags') or []))


def shape(pcs, ps5s, target_tokens, need_decoder=False):
    if not pcs or not ps5s:
        return 'one-side-only', []
    reasons, platform, info = [], [], []
    broken = [f['package'] for f in pcs + ps5s
              if f.get('bulk_entries') is None or f.get('shader_tokens') is None or f.get('export_slices') != 'tiled']
    if broken:
        reasons.append(f'unreadable structure (bulk map / export tiling) in {broken[:4]}')
    fl_ps = {f['flags'] for f in ps5s}
    odd = sorted({f['flags'] for f in pcs if f['flags'] not in fl_ps})
    if odd:
        reasons.append(f'PC package flags {odd} not seen on any console sample {sorted(fl_ps)}')
    elif {f['flags'] for f in pcs} != fl_ps:
        info.append(f"console also has package flags {sorted(fl_ps - {f['flags'] for f in pcs})} (per-package)")
    # every PC sample's bulk-flag signature must match at least one console sample's (not just the unions)
    sig_ps = {_bulk_sig(f) for f in ps5s}
    unmatched = sorted({_bulk_sig(f) for f in pcs if _bulk_sig(f) not in sig_ps})
    if unmatched:
        reasons.append(f'PC bulk-flag signatures {unmatched[:3]} not seen on console {sorted(sig_ps)[:3]}')
    tk_pc = {x for f in pcs for x in (f.get('shader_tokens') or [])}
    tk_ps = {x for f in ps5s for x in (f.get('shader_tokens') or [])}
    if tk_pc or tk_ps:
        if tk_ps <= target_tokens and not (tk_pc & target_tokens):
            platform.append(f'shader maps: PC {sorted(tk_pc)} vs console {sorted(tk_ps)}')
        else:
            reasons.append(f'shader tokens PC {sorted(tk_pc)} vs console {sorted(tk_ps)}')
    # textures whose bulk chunk was not fetched cannot be read: counted, not compared
    tf_pc = {f['texture_form'] for f in pcs if not f.get('texture_form', 'unread').startswith('unread')}
    tf_ps = {f['texture_form'] for f in ps5s if not f.get('texture_form', 'unread').startswith('unread')}
    if tf_pc or tf_ps:
        if tf_pc == {'linear'} and tf_ps == {'console-tiled'}:
            platform.append('texture texel layout: PC linear vs console GPU-tiled')
        elif tf_pc != tf_ps:
            reasons.append(f'texture form PC {sorted(map(str, tf_pc))} vs console {sorted(map(str, tf_ps))}')
    if need_decoder:
        sd_pc = [f.get('skeletal_decode') for f in pcs]
        sd_ps = [f.get('skeletal_decode') for f in ps5s]
        ok_pc = sum(1 for s in sd_pc if s and s['ok'])
        ok_ps = sum(1 for s in sd_ps if s and s['ok'])
        info.append(f'independent decoder read PC {ok_pc}/{len(sd_pc)}, console {ok_ps}/{len(sd_ps)}; '
                    f'console misses: {sorted({s["why"] for s in sd_ps if s and not s["ok"]})}')
        if ok_pc != len(sd_pc) or not ok_ps:
            reasons.append(f'independent decoder must read every PC sample and at least one console sample '
                           f'(PC {ok_pc}/{len(sd_pc)}, console {ok_ps}/{len(sd_ps)})')
    unres = [f['package'] for f in pcs + ps5s if f['unresolved_script_imports']]
    if unres:
        reasons.append(f'unresolved script imports in {unres[:4]}')
    if reasons:
        return 'differs', reasons + platform + info
    if platform:
        return 'platform-specific', platform + info
    return 'same-shape', info


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument('--pc', nargs=2, action='append', metavar=('UTOC', 'UCAS'), required=True)
    ap.add_argument('--ps5-dir', action='append', required=True)
    ap.add_argument('--base-toc', nargs='+', required=True)
    ap.add_argument('--script-utoc', required=True)
    ap.add_argument('--script-ucas', required=True)
    ap.add_argument('--target', required=True,
                    help='shader format-platform of the console build, as in the game\'s own files')
    ap.add_argument('--mesh-decoder', default=None)
    ap.add_argument('--readers', default=jlib.READERS_DEFAULT)
    ap.add_argument('--hash-receipt', default=None,
                    help='JSON cache {sha256: TOC IoHash} of console files already BLAKE3-verified')
    ap.add_argument('--out', required=True)
    a = ap.parse_args(argv)
    R = jlib.Readers(a.readers)
    so = jlib.script_objects(R, a.script_utoc, a.script_ucas)
    dec = None
    if a.mesh_decoder:
        sys.path.insert(1, os.path.abspath(a.mesh_decoder))
        import mesh_sections as dec  # noqa: E402
    target = set(a.target.split('-'))
    by_class = collections.defaultdict(lambda: {'pc': [], 'ps5': []})
    same_path = []
    pc_by_pid = {}
    for utoc, ucas in a.pc:
        C = jlib.Container(R, utoc, ucas)
        for row, vpath, pid, data in C.packages():
            name, fp = describe(R, so, data, C.bulk_len.get(pid), dec, 'pc', vpath)
            by_class[fp['class']]['pc'].append(fp)
            pc_by_pid[pid] = fp
    ex, bulks, refused = load_ps5_samples(R, a.ps5_dir, a.base_toc, a.hash_receipt)
    misnamed = []
    for pid, (path, data) in sorted(ex.items()):
        try:
            b = bulks.get(pid)
            name, fp = describe(R, so, data, len(b) if b is not None else None, dec, 'ps5', path)
        except Exception as e:
            refused.append({'file': path, 'why': f'unreadable: {e}'})
            continue
        if R.cityhash.ue_name_hash(name) != pid:
            misnamed.append(path)
            continue
        fp['toc_checked'] = True
        by_class[fp['class']]['ps5'].append(fp)
        if pid in pc_by_pid:
            same_path.append({'package': name, 'class': fp['class'],
                              'pc_bytes': pc_by_pid[pid]['bytes'], 'ps5_bytes': fp['bytes'],
                              'pc_decode': pc_by_pid[pid].get('skeletal_decode'),
                              'ps5_decode': fp.get('skeletal_decode')})
    classes = {}
    pairs = 0
    for cls, sides in sorted(by_class.items()):
        verdict, reasons = shape(sides['pc'], sides['ps5'], target, need_decoder=cls.endswith('/SkeletalMesh'))
        if sides['pc'] and sides['ps5']:
            pairs += len(sides['pc'])
        forms = {side: dict(collections.Counter(f['texture_form'][:48] for f in sides[side] if 'texture_form' in f))
                 for side in ('pc', 'ps5')}
        classes[cls] = {'pc_samples': len(sides['pc']), 'ps5_samples': len(sides['ps5']), 'shape': verdict,
                        'reasons': reasons, 'texture_forms': forms if any(forms.values()) else None,
                        'console_bulk_signatures': sorted({_bulk_sig(f) for f in sides['ps5']}),
                        'pc': sides['pc'][:40], 'ps5': sides['ps5'][:40]}
    rep = {'tool': 'teamJ-pair-compare-1', 'label': 'Mac-only', 'readers_sha256': R.sha256,
           'inputs': {'pc': [{'utoc': u, 'utoc_sha256': jlib.sha256_file(u), 'ucas_sha256': jlib.sha256_file(c)}
                             for u, c in a.pc],
                      'ps5_dirs': a.ps5_dir, 'mesh_decoder': a.mesh_decoder,
                      'mesh_decoder_sha256': jlib.sha256_file(os.path.join(a.mesh_decoder, 'mesh_sections.py'))
                      if a.mesh_decoder else None},
           'target_tokens': sorted(target), 'refused_ps5_files': refused, 'misnamed_ps5_files': misnamed,
           'pairs_compared': pairs, 'same_path_pairs': same_path,
           'classes': classes}
    with open(a.out, 'w') as fh:
        json.dump(rep, fh, indent=1)
    print(f'pair_compare: {len(classes)} classes, {pairs} PC packages with a console same-class sample, '
          f'{len(same_path)} same-path pairs, {len(refused)} console files refused, {len(misnamed)} misnamed')
    for cls, c in classes.items():
        print(f"  {c['shape']:18s} {cls}  (PC {c['pc_samples']}, console {c['ps5_samples']})"
              + (f"  {'; '.join(c['reasons'])}" if c['reasons'] else ''))
    if pairs == 0:
        print('pair_compare: FAIL (nothing compared)')
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
