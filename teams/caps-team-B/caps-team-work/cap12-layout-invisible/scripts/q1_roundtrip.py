"""Round-trip proofs for the conversion spec (Mac-only).
A) game: detile(stored game bytes) then tile() must give the stored bytes back, byte-equal, every mip
   (incl. padding and the tail block), for every fetched/local game texture with all stored mips.
B) Leon: tile(Leon linear) then detile() must give Leon's bytes back, byte-equal; sizes = the rule.
C) DXT1 2048^2 game chains: every de-tiled mip is a coherent half-size copy of the one above.
Usage: q1_roundtrip.py <addrlib dir> <fetched chunk dir> <out.json>"""
import sys, os, re, json, struct, collections, time
sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'PATCH', 'tools'))
sys.path.insert(0, '/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/pak-intake')
import ue_formats as uf
import texture_tiling_probe as tp
import tile_reference as tr
AL, FD, OUT = sys.argv[1:4]
WHAT = '/WORKSPACE/.codex/worktrees/translation-core/what'
UTOC = WHAT + '/.superpowers/native-observations/townfall-main-utoc-2026-10-01/pakchunk0-ps5.utoc'
M = WHAT + '/outputs/2026-10-08-l25-prep/pak-mirror-townfall-20261013/'
L = WHAT + '/.superpowers/native-observations/leon-real-mod-intake-2026-10-01/extracted/'
# public equations for 1/2/4-byte elements (others are embedded in the probe and tested against the header)
s = open(os.path.join(AL, 'gfx10SwizzlePattern.h')).read()
tabs = {m.group(1): [[x.strip() for x in mm.group(1).split(',') if x.strip()]
                     for mm in re.finditer(r'\{(.*)\}\s*,\s*//\s*\d+', m.group(2))]
        for m in re.finditer(r'const UINT_64 (GFX10_SW_PATTERN_NIBBLE\w+)\[\]\[\d\]\s*=\s*\{(.*?)\n\};', s, re.S)}
for mode, table, blk in ((1, 'GFX10_SW_256_S_PATINFO', 8), (5, 'GFX10_SW_4K_S_PATINFO', 12), (9, 'GFX10_SW_64K_S_PATINFO', 16)):
    body = re.search(r'const ADDR_SW_PATINFO %s\[\]\s*=\s*\{(.*?)\n\};' % table, s, re.S).group(1)
    for eb in (1, 2, 4):
        mm = re.search(r'\{\s*\d+,\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*\}\s*,\s*//\s*1 pipes %d bpe @' % eb, body)
        i01, i2, i3, i4 = map(int, mm.groups())
        tr.EXTRA_EQ[(mode, eb)] = ' '.join((tabs['GFX10_SW_PATTERN_NIBBLE01'][i01] + tabs['GFX10_SW_PATTERN_NIBBLE2'][i2] +
                                           tabs['GFX10_SW_PATTERN_NIBBLE3'][i3] + tabs['GFX10_SW_PATTERN_NIBBLE4'][i4])[eb.bit_length() - 1:blk])

def textures(utoc, chunk):
    t = uf.parse_toc(open(utoc, 'rb').read())
    rows = {}
    for row, cid in enumerate(t.chunk_ids):
        rows.setdefault(cid[:8], {})[cid[11]] = row
    for kinds in rows.values():
        if 1 not in kinds:
            continue
        d = chunk(t, kinds[1])
        if d is None:
            continue
        try:
            z = uf.parse_zen_package(d)
        except uf.FormatError:
            continue
        if not any(n.startswith('PF_') for n in z.names):
            continue
        x = uf.export_data(d, z, [e for e in z.exports if 'chunk_offset' in e][-1])
        h = tp.platform_header(x, z.names)
        if h is None:
            continue
        bulk = chunk(t, kinds[2]) if 2 in kinds else None
        stored = []
        for so, dso, ss, fl in z.bulk_data:
            if ss == 0:
                stored.append(b'')
            elif fl & 0x100:
                stored.append(bulk[so:so + ss] if bulk is not None and so + ss <= len(bulk) else None)
            else:
                stored.append(x[so:so + ss])
        yield z.name, h, stored

def fetched_chunk(t, row):
    p = os.path.join(FD, t.chunk_ids[row].hex() + '.bin')
    if not os.path.isfile(p):
        return None
    d = open(p, 'rb').read()
    return d if uf.io_hash(d) == t.hashes[row] else None

def local_chunk(ucas):
    rd = {}
    def f(t, row):
        if id(t) not in rd:
            rd[id(t)] = uf.ChunkReader(t, ucas)
        d = rd[id(t)].read_row(row)
        return d if uf.io_hash(d) == t.hashes[row] else None
    return f

res = {'A_game': [], 'B_leon': [], 'C_dxt1_chain': []}
cnt = collections.Counter()
t0 = time.time()
sources = [(UTOC, fetched_chunk), (M + 'pakchunk10-ps5.utoc', local_chunk(M + 'pakchunk10-ps5.ucas'))]
for utoc, ch in sources:
    for name, h, stored in textures(utoc, ch):
        row = {'package': name, 'format': h['format'], 'size': h['size'], 'ext': h['ext_data'], 'first_mip': h['first_mip']}
        if None in stored or len(stored) != h['num_mips']:
            row['result'] = 'skipped: not every stored mip is on the Mac'
        else:
            try:
                lin = tr.detile_chain(stored, h['format'], h['size'][0], h['size'][1], h['first_mip'], h['ext_data'])
                back = tr.tile_chain(lin, h['format'], h['size'][0], h['size'][1], h['first_mip'], h['ext_data'])
                same = [a == b for a, b in zip(back, stored)]
                row['result'] = 'BYTE-EQUAL' if all(same) else 'DIFFERS at mips %s' % [i for i, v in enumerate(same) if not v]
                row['bytes'] = sum(len(x) for x in stored)
                if h['format'] == 'PF_DXT1' and h['ext_data'] == 9:
                    vf = tp.block_value('PF_DXT1')
                    L_ = tr.layout(h['format'], h['size'][0], h['size'][1], h['first_mip'], len(stored), 9)
                    chain = []
                    for i in range(len(lin) - 1):
                        (wa, ha), (wb, hb) = L_['dims'][i], L_['dims'][i + 1]
                        if wb < 2 or hb < 2:
                            break
                        ga = [[lin[i][(y * wa + x) * 8:(y * wa + x + 1) * 8] for x in range(wa)] for y in range(ha)]
                        gb = [[lin[i + 1][(y * wb + x) * 8:(y * wb + x + 1) * 8] for x in range(wb)] for y in range(hb)]
                        chain.append((i, i + 1, 'tail' if L_['first_tail'] is not None and i + 1 >= L_['first_tail'] else 'bulk',
                                      tp.corr(tp._down(tp._values(ga, vf)), tp._values(gb, vf))))
                    res['C_dxt1_chain'].append({'package': name, 'first_tail': L_['first_tail'], 'pairs': chain})
            except tr.TileError as e:
                row['result'] = 'refused: %s' % e
        cnt['A ' + row['result'].split(':')[0].split(' at')[0]] += 1
        res['A_game'].append(row)
print('A game round trip (detile -> tile == stored):', dict(cnt), '%.0fs' % (time.time() - t0))
for r in res['A_game']:
    if not r['result'].startswith(('BYTE-EQUAL', 'skipped')):
        print('   ', r)
print('A by (format, ExtData):', dict(collections.Counter((r['format'], r['ext']) for r in res['A_game'] if r['result'] == 'BYTE-EQUAL')))
for c in res['C_dxt1_chain']:
    print('C', c['package'].split('/')[-1], 'first tail mip', c['first_tail'], [(a, b, k, v) for a, b, k, v in c['pairs']])
# B: Leon
t1 = time.time()
cb = collections.Counter()
for name, h, stored in textures(L + 'zzz_Leon_Townfall_2K_P.utoc', local_chunk(L + 'zzz_Leon_Townfall_2K_P.ucas')):
    fm = h['first_mip']
    eb, w0, h0 = tr.elements(h['format'], h['size'][0], h['size'][1], fm)
    mode = 9 if max(1, h['size'][0] >> fm) >= 256 and max(1, h['size'][1] >> fm) >= 256 else 5
    row = {'package': name, 'format': h['format'], 'size': h['size'], 'mode': mode}
    lin_ok = [len(m) == max(1, w0 >> i) * max(1, h0 >> i) * eb for i, m in enumerate(stored)]
    if not all(lin_ok) or len(stored) != h['num_mips']:
        row['result'] = 'refused: Leon mip sizes are not the linear sizes'
    else:
        til = tr.tile_chain(stored, h['format'], h['size'][0], h['size'][1], fm, mode)
        lay = tr.layout(h['format'], h['size'][0], h['size'][1], fm, len(stored), mode)
        back = tr.detile_chain(til, h['format'], h['size'][0], h['size'][1], fm, mode)
        row.update(result='BYTE-EQUAL' if back == stored else 'DIFFERS', num_mips_in_tail=lay['num_mips_in_tail'],
                   first_tail=lay['first_tail'], tiled_sizes=[len(x) for x in til], sizes_follow_rule=[len(x) for x in til] == lay['sizes'],
                   packed_and_optdata=[hex(tr.header_fields(1, mode, lay['num_mips_in_tail'])[0]),
                                       tr.header_fields(1, mode, lay['num_mips_in_tail'])[1].hex()])
    cb[row['result']] += 1
    res['B_leon'].append(row)
print('B Leon round trip (tile -> detile == linear):', dict(cb), '%.0fs' % (time.time() - t1))
print('B Leon modes/tails:', dict(collections.Counter((r['format'], tuple(r['size']), r.get('mode'), r.get('num_mips_in_tail')) for r in res['B_leon'])))
json.dump(res, open(OUT, 'w'), indent=1)
