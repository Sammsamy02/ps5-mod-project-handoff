#!/usr/bin/env python3
"""Census of the GAME's own PS5 texture layouts (Mac-only, read-only, generic).

Input: game texture export chunks fetched read-only (<chunk id>.bin files, e.g. by
ftp_range_fetch.py), each checked here against the pristine base TOC (length + BLAKE3-20
IoHash), plus Team B's texture_tiling_probe outputs for the per-texture PIXEL verdict
("header and pixels agree" = the pixels really are in the layout the header claims).
Optional extra rows: probe rows of game textures whose chunks are not in the folder
(header fields as that probe read them, marked by source).

Per texture, read from the export with struct only: header size (SizeX, SizeY), pixel
format, PackedData (bit 30 HasOptData), the 8-byte block {ExtData, NumMipsInTail}, the mip
count and the stored mip 0 size (mip table + bulk map). Output: ps5_layout_census.json, the
second source ps5_tiling.choose() checks its rule against (every row must agree).
Exit 1 on zero rows or any chunk that does not match the TOC.
"""
import argparse
import hashlib
import json
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import blake3  # noqa: E402
import utoc  # noqa: E402
import zen  # noqa: E402

# bytes per element, pixels per element side
FORMATS = {'PF_DXT1': (8, 4), 'PF_BC4': (8, 4), 'PF_DXT3': (16, 4), 'PF_DXT5': (16, 4), 'PF_BC5': (16, 4),
           'PF_BC7': (16, 4), 'PF_B8G8R8A8': (4, 1), 'PF_R8G8B8A8': (4, 1), 'PF_FloatRGBA': (8, 1),
           'PF_G8': (1, 1)}


def header_fields(d):
    p = zen.ZenPackage(d)
    pf = [n for n in p.names if n.startswith('PF_')]
    if len(pf) != 1:
        return None, 'not exactly one pixel-format name'
    top = [e for e in p.exports if e['outer'][0] == 'Null']
    if len(top) != 1:
        return None, 'not exactly one top-level export'
    x = p.export_bytes(top[0])
    s = x.find(pf[0].encode() + b'\0')
    if s < 48 or x.count(b'PF_') != 1:
        return None, 'pixel-format string not found once'
    ln = struct.unpack_from('<i', x, s - 4)[0]
    sx, sy, packed = struct.unpack_from('<iiI', x, s - 16)
    q = s + ln
    if not packed & (1 << 30):
        return None, 'no layout block (HasOptData clear)'
    ext, ntail = struct.unpack_from('<II', x, q)
    q += 8
    first, n = struct.unpack_from('<ii', x, q)
    q += 8
    names, pos = zen.read_name_batch(d, 60)
    pad = struct.unpack_from('<Q', d, pos)[0]
    bpos = pos + 8 + pad
    bsz = struct.unpack_from('<q', d, bpos)[0]
    ents = [struct.unpack_from('<qqqII', d, bpos + 8 + 32 * i) for i in range(bsz // 32)]
    mips = []
    for _k in range(n):
        bi = struct.unpack_from('<I', x, q)[0]
        q += 4
        if bi >= len(ents):
            return None, 'mip index outside the bulk map'
        if not ents[bi][3] & 0x100:
            q += ents[bi][2]
        mx, my, _mz = struct.unpack_from('<III', x, q)
        q += 12
        mips.append([mx, my])
    return {'package': p.name, 'format': pf[0], 'size': [sx, sy], 'packed': packed, 'ext_data': ext,
            'num_mips_in_tail': ntail, 'first_mip': first, 'mips': n, 'mip0': mips[0] if mips else None}, None


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument('--toc', required=True, help='pristine base .utoc the chunks come from')
    ap.add_argument('--chunks', required=True, help='folder of fetched <chunk id>.bin files')
    ap.add_argument('--local', nargs=2, action='append', default=[], metavar=('UTOC', 'UCAS'),
                    help='a base container on the Mac (texture chunks read and IoHash-checked against its TOC)')
    ap.add_argument('--probe', nargs='+', required=True, help='Team B texture_tiling_probe JSON outputs (verdicts)')
    ap.add_argument('--extra-probe-rows', nargs='*', default=[],
                    help='probe JSONs whose game rows are added as rows (chunks not in --chunks)')
    ap.add_argument('--out', required=True)
    a = ap.parse_args(argv)
    toc = utoc.load(a.toc)
    rowof = {c.hex(): i for i, c in enumerate(toc.chunk_ids)}
    verdict = {}
    for pth in a.probe:
        for r in json.load(open(pth))['textures']:
            verdict.setdefault(r['package'], set()).add(r.get('verdict') or '')
    rows, skipped, bad = [], [], 0
    sources = []
    for fn in sorted(os.listdir(a.chunks)):
        if fn.endswith('00000001.bin'):
            sources.append((fn[:-4], toc, rowof, lambda fn=fn: open(os.path.join(a.chunks, fn), 'rb').read(),
                            f'chunk {fn[:-4]}'))
    for lutoc, lucas in a.local:
        lt = utoc.load(lutoc)
        lrow = {c.hex(): i for i, c in enumerate(lt.chunk_ids)}

        def rd(o, n, path=lucas):
            with open(path, 'rb') as fh:
                fh.seek(o)
                return fh.read(n)
        for cid, i in sorted(lrow.items()):
            if cid.endswith('00000001'):
                sources.append((cid, lt, lrow, lambda i=i, lt=lt, rd=rd: lt.read_chunk(i, rd),
                                f'{os.path.basename(lucas)} chunk {cid}'))
    for cid, t_, rows_of, get, label in sources:
        d = get()
        i = rows_of.get(cid)
        if i is None or t_.offlen[i][1] != len(d) or blake3.iohash20(d) != t_.metas[i][0]:
            print(f'game_layout_census: {label} does not match its base TOC (length/IoHash)')
            bad += 1
            continue
        try:
            h, why = header_fields(d)
        except (zen.ZenError, struct.error, ValueError) as e:
            h, why = None, str(e)
        if h is None:
            skipped.append({'chunk': cid, 'why': why})
            continue
        if h['format'] not in FORMATS:
            skipped.append({'chunk': cid, 'why': f"format {h['format']} not in the census"})
            continue
        if any(r['package'] == h['package'] for r in rows):
            continue
        v = verdict.get(h['package'], set())
        h.update(element_bytes=FORMATS[h['format']][0], pixels_per_element=FORMATS[h['format']][1],
                 pixel_verdicts=sorted(v), verified=True, source=f'{label} (IoHash == base TOC)')
        rows.append(h)
    seen = {r['package'] for r in rows}
    for pth in a.extra_probe_rows:
        for r in json.load(open(pth))['textures']:
            if not r.get('has_opt_data') or r['package'] in seen or r['format'] not in FORMATS:
                continue
            if r.get('first_mip', 0) != 0:
                continue
            rows.append({'package': r['package'], 'format': r['format'], 'size': r['size'], 'packed': r['packed'],
                         'ext_data': r['ext_data'], 'num_mips_in_tail': r['num_mips_in_tail'], 'first_mip': 0,
                         'mips': r['num_mips'], 'mip0': r['size'], 'element_bytes': FORMATS[r['format']][0],
                         'pixels_per_element': FORMATS[r['format']][1],
                         'pixel_verdicts': sorted(verdict.get(r['package'], {r.get('verdict') or ''})),
                         'verified': False,
                         'source': f'{os.path.basename(pth)} (probe row, chunk NOT verified here: cannot settle)'})
            seen.add(r['package'])
    rows.sort(key=lambda r: r['package'])
    doc = {'label': 'Mac-only', 'tool': 'game_layout_census.py', 'toc_sha256': hashlib.sha256(open(a.toc, 'rb').read()).hexdigest(),
           'rows': rows, 'skipped': skipped,
           'summary': {'rows': len(rows), 'skipped': len(skipped), 'bad_chunks': bad,
                       'verified_rows': sum(r['verified'] for r in rows),
                       'pixel_agree_rows': sum(any('header and pixels agree' in v for v in r['pixel_verdicts'])
                                               for r in rows)}}
    with open(a.out, 'w') as fh:
        json.dump(doc, fh, indent=1, sort_keys=True)
        fh.write('\n')
    print(f"game_layout_census: {len(rows)} game texture rows ({doc['summary']['verified_rows']} IoHash-verified), "
          f"{doc['summary']['pixel_agree_rows']} with pixel agreement, {len(skipped)} skipped, {bad} bad chunks")
    return 0 if rows and not bad else 1


if __name__ == '__main__':
    sys.exit(main())
