#!/usr/bin/env python3
"""Tests for the Q1 tools (layout_fetch_plan.py, texture_tiling_probe.py). Mac-only.
Prints how many tests ran; exits non-zero on any failure or when zero tests ran."""
import json
import os
import re
import struct
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.environ.get('CAP12B_TOOLS', os.path.join(HERE, '..', 'PATCH', 'tools')))
import layout_fetch_plan as lfp  # noqa: E402
import texture_tiling_probe as tp  # noqa: E402

WHAT = '/WORKSPACE/.codex/worktrees/translation-core/what'
FORMATS = WHAT + '/outputs/2026-10-08-l25-prep/pak-intake'
MIRROR = WHAT + '/outputs/2026-10-08-l25-prep/pak-mirror-townfall-20261013'
BASE_UTOC = WHAT + '/.superpowers/native-observations/townfall-main-utoc-2026-10-01/pakchunk0-ps5.utoc'
LEON = WHAT + '/.superpowers/native-observations/leon-real-mod-intake-2026-10-01/extracted'
ADDRLIB = os.environ.get('CAP12B_ADDRLIB', os.path.join(HERE, '..', 'evidence', 'public-addrlib'))


def parse_public_s_equations(path):
    """Re-derive the S-mode equations straight from the public addrlib header text."""
    s = open(path).read()
    tabs = {}
    for m in re.finditer(r'const UINT_64 (GFX10_SW_PATTERN_NIBBLE\w+)\[\]\[\d\]\s*=\s*\{(.*?)\n\};', s, re.S):
        tabs[m.group(1)] = [[x.strip() for x in mm.group(1).split(',') if x.strip()]
                            for mm in re.finditer(r'\{(.*)\}\s*,\s*//\s*\d+', m.group(2))]
    out = {}
    for mode, table, blk in ((1, 'GFX10_SW_256_S_PATINFO', 8), (5, 'GFX10_SW_4K_S_PATINFO', 12),
                             (9, 'GFX10_SW_64K_S_PATINFO', 16)):
        body = re.search(r'const ADDR_SW_PATINFO %s\[\]\s*=\s*\{(.*?)\n\};' % table, s, re.S).group(1)
        for eb, el in ((8, 3), (16, 4)):
            seen = set()
            for mm in re.finditer(r'\{\s*\d+,\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*\}\s*,\s*//[^\n]* %d bpe @' % eb, body):
                i01, i2, i3, i4 = map(int, mm.groups())
                eq = (tabs['GFX10_SW_PATTERN_NIBBLE01'][i01] + tabs['GFX10_SW_PATTERN_NIBBLE2'][i2] +
                      tabs['GFX10_SW_PATTERN_NIBBLE3'][i3] + tabs['GFX10_SW_PATTERN_NIBBLE4'][i4])[el:blk]
                seen.add(' '.join(eq))
            out[(mode, eb)] = seen
    return out


class PublicRules(unittest.TestCase):
    @unittest.skipUnless(os.path.exists(os.path.join(ADDRLIB, 'gfx10SwizzlePattern.h')), 'public addrlib copy missing')
    def test_embedded_equations_equal_public_header_for_every_pipe_config(self):
        pub = parse_public_s_equations(os.path.join(ADDRLIB, 'gfx10SwizzlePattern.h'))
        self.assertEqual(len(pub), 6)
        for k, eq in tp.S_EQ.items():
            self.assertEqual(pub[k], {eq}, k)     # one equation only: no pipe/bank XOR in S modes

    @unittest.skipUnless(os.path.exists(os.path.join(ADDRLIB, 'addrtypes.h')), 'public addrlib copy missing')
    def test_mode_numbers_are_the_public_enum(self):
        s = open(os.path.join(ADDRLIB, 'addrtypes.h')).read()
        for n, name in ((0, 'ADDR_SW_LINEAR'), (1, 'ADDR_SW_256B_S'), (5, 'ADDR_SW_4KB_S'), (9, 'ADDR_SW_64KB_S')):
            self.assertRegex(s, r'%s\s*=\s*%d,' % (name, n))

    def test_chain_sizes_equal_the_games_bulk_sizes(self):
        # bulk sizes the game's own TOC lists for many textures (see evidence/q1-toc-bulk-size-census.txt)
        self.assertEqual(lfp.chain_bytes_64k_s(1024, 1024, 16, 64, 64), 22347776)   # 4096^2 16-byte blocks
        self.assertEqual(lfp.chain_bytes_64k_s(512, 512, 16, 64, 64), 5570560)
        self.assertEqual(lfp.chain_bytes_64k_s(256, 256, 16, 64, 64), 1376256)
        self.assertEqual(lfp.chain_bytes_64k_s(512, 512, 8, 128, 64), 2752512)      # 2048^2 8-byte blocks
        self.assertEqual(lfp.chain_bytes_64k_s(1024, 1024, 8, 128, 64), 11141120)
        self.assertEqual(lfp.chain_bytes_64k_s(256, 256, 8, 128, 64), 655360)

    def test_tail_rules_match_the_games_headers(self):
        # game headers: 4096^2 BC7/BC5 -> 13 mips, NumMipsInTail 8; 256^2 BC5 -> 9 mips, NumMipsInTail 8
        first, tail = tp.tail_layout(9, 16, 1024, 1024, 13)
        self.assertEqual((first, 13 - first), (5, 8))
        self.assertEqual(tail[5], (32, 0))
        first, _t = tp.tail_layout(9, 16, 64, 64, 9)
        self.assertEqual(9 - first, 8)

    def test_linear_mode_has_no_tail(self):
        self.assertEqual(tp.tail_layout(0, 16, 64, 64, 7), (None, {}))


def _load(utoc, ucas, want):
    sys.path.insert(0, FORMATS)
    import ue_formats as uf  # noqa: E402
    t = uf.parse_toc(open(utoc, 'rb').read())
    rd = uf.ChunkReader(t, ucas)
    rows = {}
    for row, cid in enumerate(t.chunk_ids):
        rows.setdefault(cid[:8], {})[cid[11]] = row
    for k in rows.values():
        if 1 not in k:
            continue
        d = rd.read_row(k[1])
        z = uf.parse_zen_package(d)
        if z.name.endswith('/' + want):
            x = uf.export_data(d, z, [e for e in z.exports if 'chunk_offset' in e][-1])
            h = tp.platform_header(x, z.names)
            return h, tp.mip_payloads(z, x, rd.read_row(k[2]) if 2 in k else None)
    raise AssertionError(want + ' not found')


@unittest.skipUnless(os.path.exists(MIRROR + '/pakchunk10-ps5.ucas') and os.path.isdir(FORMATS), 'game files missing')
class GamePixels(unittest.TestCase):
    def test_game_bc5_is_64kb_s_and_header_agrees(self):
        h, mips = _load(MIRROR + '/pakchunk10-ps5.utoc', MIRROR + '/pakchunk10-ps5.ucas',
                        'T_BodywarmerBeanieTag_Deluxe_Deluxe_N')
        self.assertEqual((h['has_opt_data'], h['ext_data'], h['num_mips_in_tail']), (True, 9, 8))
        j = tp.judge(h, mips)
        self.assertTrue(j['verdict'].startswith('SW_64KB_S (header and pixels agree)'), j)
        self.assertGreater(j['scores']['SW_64KB_S']['match'], 0.99)
        self.assertLess(j['scores']['LINEAR']['match'], 0.3)


@unittest.skipUnless(os.path.exists(LEON + '/zzz_Leon_Townfall_2K_P.ucas') and os.path.isdir(FORMATS), 'mod files missing')
class ModPixels(unittest.TestCase):
    def test_mod_bc5_is_linear_without_layout_block(self):
        h, mips = _load(LEON + '/zzz_Leon_Townfall_2K_P.utoc', LEON + '/zzz_Leon_Townfall_2K_P.ucas', 'T_Leon_UpperBody_N')
        self.assertFalse(h['has_opt_data'])
        j = tp.judge(h, mips)
        self.assertTrue(j['verdict'].startswith('LINEAR (header and pixels agree'), j)

    def test_contradiction_is_reported(self):
        h, mips = _load(LEON + '/zzz_Leon_Townfall_2K_P.utoc', LEON + '/zzz_Leon_Townfall_2K_P.ucas', 'T_Leon_UpperBody_N')
        fake = dict(h, has_opt_data=True, ext_data=9)       # a header that lies about the layout
        self.assertIn('CONTRADICTS', tp.judge(fake, mips)['verdict'])


FETCHED = WHAT + '/outputs/2026-10-08-l25-prep/evidence/cap12-layout-fetched-20261013/pakchunk0-ps5'


def _stored(utoc, want, fetched=None, ucas=None):
    """(header, [stored bytes per mip]) of one texture package; chunks IoHash-checked."""
    sys.path.insert(0, FORMATS)
    import ue_formats as uf  # noqa: E402
    t = uf.parse_toc(open(utoc, 'rb').read())
    rd = uf.ChunkReader(t, ucas) if ucas else None

    def chunk(cid):
        row = t.row_of.get(cid)
        if row is None:
            return None
        d = rd.read_row(row) if rd else open(os.path.join(fetched, cid.hex() + '.bin'), 'rb').read()
        assert uf.io_hash(d) == t.hashes[row]
        return d
    for row, cid in enumerate(t.chunk_ids):
        if cid[11] != 1 or (fetched and not os.path.isfile(os.path.join(fetched, cid.hex() + '.bin'))):
            continue
        d = chunk(cid)
        z = uf.parse_zen_package(d)
        if not z.name.endswith('/' + want):
            continue
        x = uf.export_data(d, z, [e for e in z.exports if 'chunk_offset' in e][-1])
        h = tp.platform_header(x, z.names)
        bulk = chunk(cid[:11] + bytes([2]))
        return h, [b'' if ss == 0 else (bulk[so:so + ss] if fl & 0x100 else x[so:so + ss])
                   for so, _d, ss, fl in z.bulk_data]
    raise AssertionError(want)


@unittest.skipUnless(os.path.isdir(FETCHED) and os.path.exists(BASE_UTOC), 'fetched game textures missing')
class ReferenceOnGameBytes(unittest.TestCase):
    """detile(game) -> tile() must reproduce the game's stored bytes exactly; the de-tiled pixels must be coherent."""

    def check(self, want, mode):
        import tile_reference as tr
        h, st = _stored(BASE_UTOC, want, fetched=FETCHED)
        self.assertEqual(h['ext_data'], mode)
        lin = tr.detile_chain(st, h['format'], h['size'][0], h['size'][1], h['first_mip'], mode)
        self.assertEqual(tr.tile_chain(lin, h['format'], h['size'][0], h['size'][1], h['first_mip'], mode), st)
        lay = tr.layout(h['format'], h['size'][0], h['size'][1], h['first_mip'], len(st), mode)
        self.assertEqual(lay['num_mips_in_tail'], h['num_mips_in_tail'])
        return h, lin, lay

    def test_dxt1_64kb_s_chain_and_tail(self):
        h, lin, lay = self.check('T_Sky_Blue', 9)
        self.assertEqual((lay['first_tail'], lay['num_mips_in_tail']), (3, 9))
        vf = tp.block_value('PF_DXT1')
        for i in (1, 2, 3):                      # bulk -> bulk, bulk -> tail, tail -> tail
            (wa, ha), (wb, hb) = lay['dims'][i], lay['dims'][i + 1]
            ga = [[lin[i][(y * wa + x) * 8:(y * wa + x + 1) * 8] for x in range(wa)] for y in range(ha)]
            gb = [[lin[i + 1][(y * wb + x) * 8:(y * wb + x + 1) * 8] for x in range(wb)] for y in range(hb)]
            self.assertGreater(tp.corr(tp._down(tp._values(ga, vf)), tp._values(gb, vf)), 0.99, i)

    def test_dxt1_4kb_s_whole_chain_in_tail(self):
        h, lin, lay = self.check('T_RTBrder', 5)
        self.assertEqual((h['first_mip'], lay['first_tail']), (1, 0))

    def test_bc5_4kb_s(self):
        self.check('T_Gray_N', 5)


@unittest.skipUnless(os.path.exists(LEON + '/zzz_Leon_Townfall_2K_P.ucas'), 'mod files missing')
class ReferenceOnLeon(unittest.TestCase):
    def roundtrip(self, want, mode):
        import tile_reference as tr
        h, st = _stored(LEON + '/zzz_Leon_Townfall_2K_P.utoc', want, ucas=LEON + '/zzz_Leon_Townfall_2K_P.ucas')
        til = tr.tile_chain(st, h['format'], h['size'][0], h['size'][1], h['first_mip'], mode)
        lay = tr.layout(h['format'], h['size'][0], h['size'][1], h['first_mip'], len(st), mode)
        self.assertEqual([len(x) for x in til], lay['sizes'])
        self.assertEqual(tr.detile_chain(til, h['format'], h['size'][0], h['size'][1], h['first_mip'], mode), st)
        return h, til, lay

    def test_lossless_128_dxt1_4kb(self):
        h, til, lay = self.roundtrip('T_Leon_Lash_D', 5)
        self.assertEqual(lay['num_mips_in_tail'], 7)

    def test_lossless_512_bc5_64kb(self):
        h, til, lay = self.roundtrip('T_Leon_Eye_N', 9)
        self.assertEqual(lay['num_mips_in_tail'], 8)

    def test_header_fields(self):
        import tile_reference as tr
        self.assertEqual(tr.header_fields(1, 9, 9), (0x40000001, bytes.fromhex('0900000009000000')))
        with self.assertRaises(tr.TileError):
            tr.header_fields(0x40000001, 9, 9)

    def test_wrong_sizes_refused(self):
        import tile_reference as tr
        with self.assertRaises(tr.TileError):
            tr.tile_chain([b'\0' * 7], 'PF_DXT1', 4, 4, 0, 9)
        with self.assertRaises(tr.TileError):
            tr.detile_chain([b'\0' * 8], 'PF_DXT1', 4, 4, 0, 9)


class Gates(unittest.TestCase):
    def test_probe_zero_inputs_fails(self):
        out = os.path.join(tempfile.mkdtemp(), 'p.json')
        self.assertEqual(tp.main(['--formats-lib', FORMATS, '--out', out]), 1)

    @unittest.skipUnless(os.path.exists(BASE_UTOC) and os.path.isdir(FORMATS), 'base utoc missing')
    def test_fetch_plan_counts_and_shape(self):
        out = os.path.join(tempfile.mkdtemp(), 'plan.json')
        self.assertEqual(lfp.main(['--utoc', BASE_UTOC, '--ucas-name', 'pakchunk0-ps5.ucas', '--formats-lib', FORMATS,
                                   '--out', out]), 0)
        p = json.load(open(out))
        self.assertTrue(p['read_only'])
        self.assertEqual(sum(1 for c in p['chunks'] if c['priority'] == 1), 121)
        self.assertEqual(sum(1 for c in p['chunks'] if c['priority'] == 2), 6)
        for c in p['chunks']:
            self.assertEqual(sum(r['length'] for r in c['physical_ranges']), c['length'])
            self.assertTrue(c['save_as'].startswith('pakchunk0-ps5/'))

    @unittest.skipUnless(os.path.exists(MIRROR + '/global.utoc') and os.path.isdir(FORMATS), 'global utoc missing')
    def test_fetch_plan_zero_textures_fails(self):
        out = os.path.join(tempfile.mkdtemp(), 'plan.json')
        self.assertEqual(lfp.main(['--utoc', MIRROR + '/global.utoc', '--ucas-name', 'global.ucas',
                                   '--formats-lib', FORMATS, '--out', out]), 1)


if __name__ == '__main__':
    suite = unittest.defaultTestLoader.loadTestsFromModule(sys.modules[__name__])
    res = unittest.TextTestRunner(verbosity=1).run(suite)
    ran = res.testsRun - len(res.skipped)
    print(f'test_layout_tools: {ran} tests ran, {len(res.failures) + len(res.errors)} failed, {len(res.skipped)} skipped')
    sys.exit(0 if res.wasSuccessful() and ran > 0 else 1)
