"""Q1 on the fetched game textures (Mac-only): public addrlib tail + size rules vs every header,
and the DXT1 SW_64KB_S chain check. Usage: q1_fetch_analysis.py <addrlib dir> <fetched chunk dir> <out.json>"""
import sys, os, re, json, struct, collections
sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'PATCH', 'tools'))
sys.path.insert(0, '/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/pak-intake')
import ue_formats as uf
import texture_tiling_probe as tp
AL, FD, OUT = sys.argv[1:4]
UTOC = '/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/townfall-main-utoc-2026-10-01/pakchunk0-ps5.utoc'

# --- public equations for every element size, straight from the header text
s = open(os.path.join(AL, 'gfx10SwizzlePattern.h')).read()
tabs = {}
for m in re.finditer(r'const UINT_64 (GFX10_SW_PATTERN_NIBBLE\w+)\[\]\[\d\]\s*=\s*\{(.*?)\n\};', s, re.S):
    tabs[m.group(1)] = [[x.strip() for x in mm.group(1).split(',') if x.strip()]
                        for mm in re.finditer(r'\{(.*)\}\s*,\s*//\s*\d+', m.group(2))]
EQ = {}
for mode, table, blk in ((1, 'GFX10_SW_256_S_PATINFO', 8), (5, 'GFX10_SW_4K_S_PATINFO', 12), (9, 'GFX10_SW_64K_S_PATINFO', 16)):
    body = re.search(r'const ADDR_SW_PATINFO %s\[\]\s*=\s*\{(.*?)\n\};' % table, s, re.S).group(1)
    for eb in (1, 2, 4, 8, 16):
        el = eb.bit_length() - 1
        mm = re.search(r'\{\s*\d+,\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*\}\s*,\s*//\s*1 pipes %d bpe @' % eb, body)
        i01, i2, i3, i4 = map(int, mm.groups())
        eq = (tabs['GFX10_SW_PATTERN_NIBBLE01'][i01] + tabs['GFX10_SW_PATTERN_NIBBLE2'][i2] +
              tabs['GFX10_SW_PATTERN_NIBBLE3'][i3] + tabs['GFX10_SW_PATTERN_NIBBLE4'][i4])[el:blk]
        EQ[(mode, eb)] = [(t[0], int(t[1:])) for t in eq]
BLK = {1: 8, 5: 12, 9: 16}
PIX = {'PF_B8G8R8A8': 4, 'PF_R8G8B8A8': 4, 'PF_G8': 1, 'PF_G16': 2, 'PF_FloatRGBA': 8, 'PF_A32B32G32R32F': 16,
       'PF_R16F': 2, 'PF_R32_FLOAT': 4}

def elem(fmt, w, h):
    if fmt in tp.BLOCK_BYTES:
        return tp.BLOCK_BYTES[fmt], max(1, -(-w // 4)), max(1, -(-h // 4))
    if fmt in PIX:
        return PIX[fmt], w, h
    return None, None, None

def predict(mode, eb, w_el, h_el, nmips):
    """public rules -> (NumMipsInTail, [bytes per mip in UE's per-mip storage])"""
    eq = EQ[(mode, eb)]
    tw, th = tp.tile_dims(eq)
    blk = BLK[mode]
    first = None
    if blk >= 12:
        tail_w, tail_h, maxin = tw // 2, th, tp.max_mips_in_tail(blk)
        for i in range(nmips):
            mw, mh = max(1, w_el >> i), max(1, h_el >> i)
            if mw <= tail_w and mh <= tail_h and nmips - i <= maxin:
                first = i
                break
    sizes = []
    for i in range(nmips):
        mw, mh = max(1, w_el >> i), max(1, h_el >> i)
        if first is not None and i >= first:
            sizes.append((1 << blk) if i == first else 0)
        else:
            sizes.append((-(-mw // tw) * tw) * (-(-mh // th) * th) * eb)
    return (nmips - first if first is not None else 0), sizes

t = uf.parse_toc(open(UTOC, 'rb').read())
rows = {}
for row, cid in enumerate(t.chunk_ids):
    rows.setdefault(cid[:8], {})[cid[11]] = row
res = []
cnt = collections.Counter()
for key, kinds in rows.items():
    if 1 not in kinds:
        continue
    p = os.path.join(FD, t.chunk_ids[kinds[1]].hex() + '.bin')
    if not os.path.isfile(p):
        continue
    d = open(p, 'rb').read()
    if uf.io_hash(d) != t.hashes[kinds[1]]:
        cnt['iohash-bad'] += 1
        continue
    z = uf.parse_zen_package(d)
    if not any(n.startswith('PF_') for n in z.names):
        continue
    x = uf.export_data(d, z, [e for e in z.exports if 'chunk_offset' in e][-1])
    h = tp.platform_header(x, z.names)
    if h is None:
        cnt['no-header'] += 1
        continue
    fm = max(0, h.get('first_mip') or 0)          # cooked chains may start below SizeX (LOD bias)
    eb, we, he = elem(h['format'], max(1, h['size'][0] >> fm), max(1, h['size'][1] >> fm))
    r = {'package': z.name, 'format': h['format'], 'size': h['size'], 'ext': h['ext_data'], 'tail': h['num_mips_in_tail'],
         'num_mips': h['num_mips'], 'stored_sizes': [b[2] for b in z.bulk_data]}
    if eb is None or h['ext_data'] not in BLK:
        r['check'] = 'format/mode not covered'
    else:
        pt, ps = predict(h['ext_data'], eb, we, he, h['num_mips'])
        r['pred_tail'], r['pred_sizes'] = pt, ps
        r['tail_ok'] = pt == h['num_mips_in_tail']
        r['sizes_ok'] = ps == r['stored_sizes'][:len(ps)] and len(r['stored_sizes']) == len(ps)
        r['check'] = 'both ok' if r['tail_ok'] and r['sizes_ok'] else ('tail ok, sizes differ' if r['tail_ok'] else 'TAIL MISMATCH')
    cnt[r['check']] += 1
    res.append(r)
print('headers checked:', len(res), dict(cnt))
by = collections.Counter((r['ext'], r['check']) for r in res)
for k, v in sorted(by.items(), key=str): print('  ExtData %s: %-24s %d' % (k[0], k[1], v))
for r in res:
    if r['check'] not in ('both ok', 'format/mode not covered'):
        print('  DIFF', r['package'].split('/')[-1], r['format'], r['size'], 'ext', r['ext'], 'tail', r['tail'], 'pred', r.get('pred_tail'), r['stored_sizes'][:8], r.get('pred_sizes', [])[:8])
json.dump(res, open(OUT, 'w'), indent=1)
