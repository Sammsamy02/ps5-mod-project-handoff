"""Table-free derivation for 8-byte blocks: every monotone interleaving of 7 X and 6 Y bits (1716)
of a 64 KiB tile (128x64 blocks), ranked by mip-1 -> mip-2 consistency of a game DXT1 texture.
Usage: q1_search_8bpe.py <fetched chunk dir> <package suffix>"""
import sys, os, itertools, random, struct
sys.dont_write_bytecode = True
sys.path.insert(0, '/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/pak-intake')
import ue_formats as uf
FD, WANT = sys.argv[1:3]
UTOC = '/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/townfall-main-utoc-2026-10-01/pakchunk0-ps5.utoc'
t = uf.parse_toc(open(UTOC, 'rb').read())
def get(pid, typ):
    cid = pid.to_bytes(8, 'little') + bytes([0, 0, 0, typ])
    d = open(os.path.join(FD, cid.hex() + '.bin'), 'rb').read()
    assert uf.io_hash(d) == t.hashes[t.row_of[cid]]
    return d
pid = None
for row, cid in enumerate(t.chunk_ids):
    if cid[11] == 1 and os.path.isfile(os.path.join(FD, cid.hex() + '.bin')):
        d = open(os.path.join(FD, cid.hex() + '.bin'), 'rb').read()
        z = uf.parse_zen_package(d)
        if z.name.endswith('/' + WANT):
            pid = int.from_bytes(cid[:8], 'little'); break
bulk = get(pid, 2)
def lum(b):
    c0, c1 = struct.unpack_from('<HH', b, 0)
    f = lambda c: ((c >> 11) & 31) * 8 + ((c >> 5) & 63) * 4 + (c & 31) * 8
    return (f(c0) + f(c1)) / 2
so1, _, ss1, _ = z.bulk_data[1]; so2, _, ss2, _ = z.bulk_data[2]
v1 = [lum(bulk[so1 + i:so1 + i + 8]) for i in range(0, ss1, 8)]   # mip 1: 256x256 blocks = 2x4 tiles
v2 = [lum(bulk[so2 + i:so2 + i + 8]) for i in range(0, ss2, 8)]   # mip 2: 128x128 blocks = 1x2 tiles
rnd = random.Random(7)
samples = [(rnd.randrange(128), rnd.randrange(128)) for _ in range(3000)]
def corr(a, b):
    n = len(a); ma = sum(a) / n; mb = sum(b) / n
    sab = sum((p - ma) * (q - mb) for p, q in zip(a, b)); saa = sum((p - ma) ** 2 for p in a); sbb = sum((q - mb) ** 2 for q in b)
    return sab / (saa * sbb) ** 0.5 if saa and sbb else 0.0
res = []
for pos in itertools.combinations(range(13), 7):
    ox = [0] * 128; oy = [0] * 64
    xi = yi = 0
    xb, yb = [], []
    for k in range(13):
        if k in pos: xb.append(k)
        else: yb.append(k)
    for x in range(128):
        ox[x] = sum(((x >> j) & 1) << k for j, k in enumerate(xb))
    for y in range(64):
        oy[y] = sum(((y >> j) & 1) << k for j, k in enumerate(yb))
    def idx(x, y, pitch):
        return ((y >> 6) * pitch + (x >> 7)) * 8192 + ox[x & 127] + oy[y & 63]
    a = []; b = []
    for x, y in samples:
        b.append(v2[idx(x, y, 1)])
        a.append((v1[idx(2 * x, 2 * y, 2)] + v1[idx(2 * x + 1, 2 * y, 2)] + v1[idx(2 * x, 2 * y + 1, 2)] + v1[idx(2 * x + 1, 2 * y + 1, 2)]) / 4)
    seq = ' '.join(('X%d' % xb.index(k)) if k in xb else ('Y%d' % yb.index(k)) for k in range(13))
    res.append((corr(a, b), seq))
res.sort(reverse=True)
print('package', z.name, '| candidates', len(res))
for r in res[:5]: print('  %.4f  %s' % r)
print('  median %.4f' % res[len(res) // 2][0])
print('public SW_64KB_S 8-byte equation: X0 Y0 Y1 X1 X2 Y2 X3 Y3 X4 Y4 X5 Y5 X6')
