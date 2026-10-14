#!/usr/bin/env python3
"""Can a refused special-model mod instance (e.g. hair) be routed to its only
same-model game parent through a DERIVED packed mask? (Mac-only, read-only,
generic, fail closed.)

The same-model route (reparent_plan.py) finds the game parent but refuses it
when the parent cuts its shape from a separate packed mask (e.g. a slot named
'<LETTERS> Mask') while the mod keeps its cut-out in its colour texture's alpha.
A derived mask built from the mod's alpha is allowed ONLY if every channel of
the game's mask is proven from the game's own data by TWO sources:
  source 1  names: the parent's slot name and the names of every game texture
            bound to it (and of the slot's own default texture) give the same
            letters; letters map to R, G, B, A by position; the registry must
            agree that the textures have as many channels as letters; and the
            letter of the cut-out channel must be unambiguous in the game's own
            vocabulary (a letter the game also uses for another meaning in
            another packed name, e.g. 'O' = occlusion in ORM / MRO, is not).
  source 2  texel statistics of mip 0 of EVERY game texture bound to that slot
            (fetched read-only, IoHash-checked against the base TOC, located by the
            two-source layout reader): the cut-out channel must look like a cut-out
            (enough clear AND enough solid AND >= 95% near-binary texels) in all of
            them; every other channel must be the SAME
            constant in all of them (then that constant is the game's own
            neutral value); otherwise its neutral value is unknown.
A channel meaning that is not proven by both sources -> REFUSED, and the
verdict lists exactly what is missing. Shader code is not used (the cooked
material graph is stripped; PS5 shader binaries are not read).

A derived mask, if ever proven, must also ship in the game's PS5 tile layout
(texture_package.derive_ps5); nothing here builds one.

Also writes a read-only fetch plan (ftp_range_fetch.py format, whole chunks,
IoHash-checkable) for the game mask textures whose texels are not on the Mac.
Exit 0 when the check ran (verdict in the JSON), 1 on zero rows checked or bad input.
"""
import argparse
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import asset_registry  # noqa: E402
import bcn_decode  # noqa: E402
import blake3  # noqa: E402
import cityhash  # noqa: E402
import material_class as mc  # noqa: E402
import material_defaults as mdef  # noqa: E402
import param_roles as pr  # noqa: E402
import reparent_plan as rp  # noqa: E402
import utoc  # noqa: E402
import zen  # noqa: E402

CHANNELS = 'RGBA'
WORD = re.compile(r'[A-Za-z0-9]+')
CUT_CLEAR, CUT_SOLID, CUT_BINARY = 0.10, 0.05, 0.95   # cut-out: enough clear AND solid AND near-binary


def mask_letters(name):
    """'IDRO Mask' -> 'IDRO'; a texture '/X/T_Hair_IDRO' -> 'IDRO'; else None."""
    if name.startswith('/'):
        last = name.rsplit('/', 1)[-1].rsplit('_', 1)[-1]
        return last.upper() if 3 <= len(last) <= 4 and last.isalpha() and len(set(last.lower())) == len(last) \
            else None
    words = WORD.findall(name)
    cand = [w for w in words if 3 <= len(w) <= 4 and w.isalpha() and w.isupper() and len(set(w)) == len(w)]
    return cand[0] if len(cand) == 1 else None


def letter_vocabulary(packed_names):
    """{letter: set of packed names using it} from the game's own packed texture/slot names."""
    voc = {}
    for n in packed_names:
        for ch in n:
            voc.setdefault(ch, set()).add(n)
    return voc


def channel_stats(fmt, data, bb):
    """Per channel: mean, clear (< 32), solid (>= 96), distinct, constant value or None."""
    vals = [[], [], [], []]
    for o in range(0, len(data), bb):
        t = bcn_decode.decode_block(fmt, data[o:o + bb])
        for c in range(4):
            vals[c].extend(t[c::4])
    out = {}
    for c in range(4):
        v = vals[c]
        d = set(v)
        out[CHANNELS[c]] = {'texels': len(v), 'mean': round(sum(v) / len(v), 2) if v else None,
                            'clear': round(sum(x < 32 for x in v) / len(v), 4) if v else None,
                            'binary': round(sum(x < 32 or x > 223 for x in v) / len(v), 4) if v else None,
                            'solid': round(sum(x >= 96 for x in v) / len(v), 4) if v else None,
                            'distinct': len(d), 'constant': next(iter(d)) if len(d) == 1 else None}
    return out


def mip0(chunk, bulk):
    """(format, mip 0 bytes) located by the two-source texture layout reader (header bulk map +
    export mip table + block / PS5 size rule). Block order does not matter for statistics."""
    import texture_package as tpkg
    try:
        lay = tpkg.layout(chunk, None if bulk is None else len(bulk), allow_ps5=True)
    except tpkg.LayoutError as e:
        return None, f'texture layout not proven: {e}'
    fmt = lay['format']
    if fmt not in bcn_decode.BLOCK:
        return None, f'no decoder for {fmt}'
    pay = tpkg.payload(lay, chunk, bulk)
    if lay['ps5']:                     # game PS5 form: de-tile first (padding and tail neighbours dropped)
        import ps5_tiling
        eb = ps5_tiling.ELEMENT_BYTES.get(fmt)
        if eb is None or lay['ps5']['ext_data'] not in ps5_tiling.BLOCK_LOG2:
            return None, f"{fmt} ExtData {lay['ps5']['ext_data']}: no de-tiler"
        w, h = lay['size']
        ch = {'mode': lay['ps5']['ext_data'], 'element_bytes': eb,
              'chain': ps5_tiling.chain_layout(lay['ps5']['ext_data'], eb, -(-w // 4), -(-h // 4), len(pay))}
        try:
            pay = ps5_tiling.detile(fmt, w, h, pay, ch)
        except ps5_tiling.TilingRefused as e:
            return None, f'de-tile: {e}'
    return (fmt, pay[0]), None


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument('--plan', required=True, help='reparent plan (known-checker special-model rows are checked)')
    ap.add_argument('--checked-headers', nargs='+', required=True)
    ap.add_argument('--fetched-dir', nargs='+', required=True)
    ap.add_argument('--asset-registry', required=True)
    ap.add_argument('--mirror', required=True, help="folder with the base game's .utoc files")
    ap.add_argument('--mask-chunks', help='folder with fetched <pid>.export.bin / <pid>.bulk.bin (optional)')
    ap.add_argument('--fetch-plan-out', required=True)
    ap.add_argument('--out', required=True)
    a = ap.parse_args(argv)
    plan = json.load(open(a.plan))
    checked = [r for p in a.checked_headers for r in json.load(open(p))['checked'] if 'verified' in r]
    headers = {r['package']: r for r in checked}
    registry = asset_registry.load(a.asset_registry)
    whole = rp.fetched_packages(a.fetched_dir, checked)
    tocs = {fn[:-5]: utoc.load(os.path.join(a.mirror, fn)) for fn in sorted(os.listdir(a.mirror))
            if fn.endswith('.utoc') and not fn.startswith('global')}

    def tags(t):
        x = registry.asset(t)
        return x['tags'] if x else None
    binds = {}
    packed_names = set()
    for pkg, z in whole.items():
        b, _ = pr.texture_bindings(z)
        if not b:
            continue
        m, chain = mc.master_of(pkg, registry, headers)
        for node in (chain if m else [pkg]):
            d = binds.setdefault(node, {})
            for q, t in b.items():
                d.setdefault(q, set()).add(t)
        for q, t in b.items():
            # a packed mask of the game = slot name and bound texture name carry the same letters
            lq = mask_letters(q) or ((pr.role_of(q) or '')[7:] if (pr.role_of(q) or '').startswith('packed:') else None)
            lt = pr.name_letters(t) or mask_letters(t)
            if lq and lq == lt:
                packed_names.add(lq)
    voc = letter_vocabulary(packed_names)
    rows_out, fetch_rows, n_checked = [], [], 0
    for r in plan['rows']:
        mm = r.get('mod_model') or {}
        special = sorted(set(mm.get('models') or ()) & set(mc.SPECIAL_MODELS))
        if r.get('action') != 'known-checker' or not special or len(r.get('considered') or []) != 1:
            continue
        n_checked += 1
        parent = r['considered'][0]['parent']
        z = whole.get(parent)
        row = {'mod_package': r['mod_package'], 'mod_model': special, 'parent': parent, 'missing': []}
        if z is None:
            row['missing'].append('parent package not fetched whole')
            rows_out.append(row)
            continue
        pairs, why = mdef.slot_defaults(z)
        nval, bad = mdef.validate(pairs or {}, binds.get(parent, {}), lambda t: pr.tex_class(tags(t)))
        row['slot_defaults'] = {'pairs': {q: t.rsplit('/', 1)[-1] for q, t in (pairs or {}).items()},
                                'cross_checked_slots': nval, 'problem': why or bad}
        slots = [q for q in sorted(set(binds.get(parent, {})) | set(pairs or {})) if mask_letters(q)]
        row['mask_slots'] = slots
        if len(slots) != 1:
            row['missing'].append(f'{len(slots)} packed mask slots on the parent (need exactly 1)')
            rows_out.append(row)
            continue
        q = slots[0]
        L = mask_letters(q)
        texs = sorted(binds.get(parent, {}).get(q, set()) | ({pairs[q]} if pairs and q in pairs else set()))
        # ---- source 1: names + registry
        s1 = {'slot': q, 'letters': L, 'textures': {}}
        names_agree = all(pr.name_letters(t) == L or mask_letters(t) == L for t in texs) and bool(texs)
        s1['texture_names_agree'] = names_agree
        for t in texs:
            tg = tags(t) or {}
            s1['textures'][t] = {k: tg.get(k) for k in ('HasAlphaChannel', 'SRGB', 'CompressionSettings',
                                                        'SourceFormat', 'VirtualTextureStreaming', 'Dimensions')}
        alpha = {v['HasAlphaChannel'] for v in s1['textures'].values()}
        s1['channels_by_position'] = {L[i]: CHANNELS[i] for i in range(len(L))}
        if not names_agree:
            row['missing'].append(f'names: not every texture of {q} names the letters {L}')
        if len(L) == 4 and alpha != {'True'}:
            row['missing'].append(f"names vs registry: '{L}' has 4 letters, but the registry says the game's "
                                  f"{q} textures have NO alpha channel (HasAlphaChannel {sorted(map(str, alpha))}); "
                                  f"the letter {L[3]} has no channel by position")
        cut_letters = [ch for ch in L if ch == 'O']
        if not cut_letters:
            row['missing'].append(f'names: no letter of {L} names the cut-out')
        else:
            other = sorted(n for n in voc.get('O', set()) if n != L)
            if other:
                row['missing'].append(f"names: the letter O also appears in the game's other packed masks "
                                      f"({', '.join(other)}; there it is read as occlusion by the usual convention); "
                                      'names alone cannot tell opacity from occlusion')
            if any('ao' in w.lower() for w in z.names if not w.startswith('/') and
                   re.fullmatch(r'AO[A-Za-z]*', w)):
                row['missing'].append('names: the parent also has an ambient-occlusion parameter '
                                      f"({', '.join(w for w in z.names if re.fullmatch(r'AO[A-Za-z]*', w))}), "
                                      'so O may be occlusion')
        row['source_1_names'] = s1
        # ---- source 2: texel statistics of every game mask texture (fetched read-only)
        s2 = {'textures': {}}
        have_all = True
        for t in texs:
            pid = cityhash.ue_name_hash(t)
            loc = []
            for stem, toc in tocs.items():
                for i, cid in enumerate(toc.chunk_ids):
                    if int.from_bytes(cid[:8], 'little') == pid and cid[8:11] == bytes(3) and (cid[11] & 0x3f) in (1, 2):
                        loc.append((stem, i, cid[11] & 0x3f))
            kinds = sorted(k for _s, _i, k in loc)
            if kinds != [1, 2]:
                s2['textures'][t] = {'error': f'base chunks {kinds} (need export + bulk)'}
                have_all = False
                continue
            files = {}
            for stem, i, k in loc:
                ln = toc_len = tocs[stem].offlen[i][1]
                name = '%016x.%s.bin' % (pid, 'export' if k == 1 else 'bulk')
                fetch_rows.append({'remote_file': stem + '.ucas', 'package': t, 'package_id': '%016x' % pid,
                                   'pieces': [list(x) for x in tocs[stem].physical_ranges(i)], 'length': ln,
                                   'whole_chunk': True, 'chunk_length': toc_len,
                                   'expected_iohash20': tocs[stem].metas[i][0], 'local_name': name,
                                   'priority': 1, 'purpose': f'hair mask statistics ({q})'})
                p = os.path.join(a.mask_chunks, name) if a.mask_chunks else None
                if p and os.path.isfile(p):
                    data = open(p, 'rb').read()
                    if len(data) != ln or blake3.iohash20(data) != tocs[stem].metas[i][0]:
                        s2['textures'][t] = {'error': f'{name}: length/IoHash differs from the base TOC'}
                        have_all = False
                        files = None
                        break
                    files[k] = data
            if files is None:
                continue
            if sorted(files) != [1, 2]:
                s2['textures'][t] = {'error': 'texels not on the Mac (fetch needed)'}
                have_all = False
                continue
            mip, why = mip0(files[1], files[2])
            if mip is None:
                s2['textures'][t] = {'error': why}
                have_all = False
                continue
            s2['textures'][t] = {'format': mip[0], 'stats': channel_stats(mip[0], mip[1], bcn_decode.BLOCK[mip[0]])}
        if not have_all:
            row['missing'].append('statistics: the texels of the game mask textures are not on the Mac '
                                  f'(read-only fetch plan written: {len(texs)} textures)')
        else:
            cut = []
            for c in CHANNELS[:len(L)]:
                if all(v['stats'][c]['clear'] >= CUT_CLEAR and v['stats'][c]['solid'] >= CUT_SOLID and
                       v['stats'][c]['binary'] >= CUT_BINARY for v in s2['textures'].values()):
                    cut.append(c)
            s2['cut_out_like_channels'] = cut
            neutral = {}
            for c in CHANNELS[:len(L)]:
                consts = {v['stats'][c]['constant'] for v in s2['textures'].values()}
                neutral[c] = consts.pop() if len(consts) == 1 and None not in consts else None
            s2['constant_channels'] = neutral
            if len(cut) != 1:
                row['missing'].append(f'statistics: {len(cut)} cut-out-like channels {cut} (need exactly 1)')
            pos = {CHANNELS[i]: L[i] for i in range(len(L))}
            if cut_letters and len(cut) == 1 and pos.get(cut[0]) != 'O':
                row['missing'].append(f'sources disagree: names put O in {s1["channels_by_position"].get("O")}, '
                                      f'statistics find the cut-out in {cut[0]}')
            for c in CHANNELS[:len(L)]:
                if cut and c == cut[0]:
                    continue
                if neutral[c] is None:
                    row['missing'].append(f'neutral value of channel {c} ({pos[c]}) unknown: it differs between '
                                          "the game's mask textures and the material graph is not in the cooked files")
        row['source_2_statistics'] = s2
        row['verdict'] = 'proven' if not row['missing'] else 'REFUSED'
        rows_out.append(row)
    seen = set()
    fetch_rows = [x for x in fetch_rows if not (x['local_name'] in seen or seen.add(x['local_name']))]
    fp = {'label': 'Mac-only fetch plan (read-only FTP range reads; integrator only)', 'tool': 'hair_mask_check.py',
          'summary': {'rows': len(fetch_rows), 'bytes': sum(x['length'] for x in fetch_rows),
                      'whole_chunk_rows': len(fetch_rows)}, 'rows': fetch_rows}
    with open(a.fetch_plan_out, 'w') as fh:
        json.dump(fp, fh, indent=1)
    res = {'label': 'Mac-only', 'rows_checked': n_checked, 'rows': rows_out,
           'verdicts': {r['mod_package']: r.get('verdict', 'REFUSED') for r in rows_out}}
    with open(a.out, 'w') as fh:
        json.dump(res, fh, indent=1)
    for r in rows_out:
        print(f"hair_mask_check: {r['mod_package']} -> {r.get('parent')}: {r.get('verdict', 'REFUSED')}")
        for m in r['missing']:
            print('   missing:', m)
    print(f'hair_mask_check: {n_checked} special-model rows checked; fetch plan {len(fetch_rows)} chunks, '
          f"{fp['summary']['bytes']} bytes")
    return 0 if n_checked else 1


if __name__ == '__main__':
    sys.exit(main())
