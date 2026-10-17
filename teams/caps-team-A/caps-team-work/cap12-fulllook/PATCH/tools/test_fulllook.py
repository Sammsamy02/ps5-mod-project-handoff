#!/usr/bin/env python3
"""Host tests for capability #12 round 4 'full look' rules (Mac-only, read-only
inputs). Real-data tests FAIL (never skip) when an input is missing. Every test
asserts a non-zero count of checked items and prints it. Writes only to a temp dir.

Rules covered: reader C (material_props), plain-mip / cut-out proof
(texture_layout), slot gate / packed same-order / colour+alpha (param_roles),
part roles (part_roles), same-model special route + role/model ranking
(reparent_plan), --same-model-special fetch plan (master_fetch_plan).
"""
import json
import os
import struct
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import material_class as mc  # noqa: E402
import material_props as mp  # noqa: E402
import mic_reparent  # noqa: E402
import mod_material_audit as mma  # noqa: E402
import param_roles as pr  # noqa: E402
import part_roles  # noqa: E402
import reparent_plan as rp  # noqa: E402
import test_cap12 as T  # noqa: E402   (shared real-input paths and caches)
import texture_layout as tl  # noqa: E402
import utoc  # noqa: E402
import zen  # noqa: E402

HAIR = '/Game/Townfall/Characters/Materials/M_Hair'
HAIR_BEARD = '/Game/Townfall/Characters/Materials/M_Hair_Beard'
SKIN_HANDS = '/Game/Townfall/Characters/Materials/M_SkinBasic_Hands'
NONVT = '/Game/Townfall/Characters/Materials/M_MRO_Cloth_NonVT'
LEON_MI = ['Eye', 'Face', 'Hair', 'Knife', 'Lash', 'LowerBody', 'Mouth', 'Pistol', 'Props', 'UpperBody', 'Watches']
BITS = {0: 'MSM_Unlit', 1: 'MSM_DefaultLit', 5: 'MSM_SubsurfaceProfile', 7: 'MSM_Hair', 8: 'MSM_Cloth'}
_c = {}


def gate(name, n):
    print(f'  [gate] {name}: checked {n}')
    assert n > 0, f'{name}: zero items checked'


# ---------------- synthetic builders ----------------
def uv_blob(items, tail=b''):
    """items: sorted [(schema index, value bytes or None for a zero-masked value)] -> unversioned block."""
    runs, prev = [], None
    for i, v in items:
        if runs and i == prev + 1:
            runs[-1][1].append(v)
        else:
            runs.append([i, [v]])
        prev = i
    frags, pos, zbits = [], 0, []
    for k, (start, vals) in enumerate(runs):
        skip = start - pos
        while skip > 127:
            frags.append(127)
            skip -= 127
        hz = any(v is None for v in vals)
        frags.append(skip | (hz << 7) | (len(vals) << 9))
        if hz:
            zbits += [v is None for v in vals]
        pos = start + len(vals)
    frags[-1] |= 1 << 8
    out = b''.join(struct.pack('<H', f) for f in frags)
    if zbits:
        word = sum(1 << k for k, z in enumerate(zbits) if z)
        out += bytes([word]) if len(zbits) <= 8 else struct.pack('<H', word)
    return out + b''.join(v for _i, v in items if v is not None) + tail


B, M, F, X = 5, 9, 11, 2             # synthetic schema: blend, model, field, a 4-byte property before them
BLEND_CODE = {'BLEND_Masked': 1, 'BLEND_Translucent': 2}
MODEL_CODE = {'MSM_Unlit': 0, 'MSM_SubsurfaceProfile': 5, 'MSM_Hair': 7, 'MSM_Cloth': 8}
FIELD_OF = {'MSM_DefaultLit': 2, 'MSM_Unlit': 1, 'MSM_SubsurfaceProfile': 32, 'MSM_Hair': 128, 'MSM_Cloth': 256}


def mat_blob(blend, model, extra=False, field=None, zero_model=False):
    items = []
    if extra:
        items.append((X, b'\xc1\xff\xff\xff'))
    if blend != 'BLEND_Opaque':
        items.append((B, bytes([BLEND_CODE[blend]])))
    if model != 'MSM_DefaultLit':
        items.append((M, None if (zero_model or MODEL_CODE[model] == 0) else bytes([MODEL_CODE[model]])))
        f = FIELD_OF[model] if field is None else field
        items.append((F, struct.pack('<HH', 0x0300, f)))
    elif field is not None:
        items.append((F, struct.pack('<HH', 0x0300, field)))
    items.append((40, b'\x01'))
    return uv_blob(items, b'\x00' * 32)


def tags(blend, model):
    return {'BlendMode': blend, 'ShadingModel': model, 'ShadingModels': f'(ShadingModelField={FIELD_OF[model]})'}


def train_set(n=30):
    combos = [('BLEND_Opaque', 'MSM_DefaultLit'), ('BLEND_Masked', 'MSM_Hair'), ('BLEND_Opaque', 'MSM_SubsurfaceProfile'),
              ('BLEND_Translucent', 'MSM_DefaultLit'), ('BLEND_Masked', 'MSM_Unlit'), ('BLEND_Opaque', 'MSM_Cloth')]
    out = []
    for k in range(n):
        bl, mo = combos[k % len(combos)]
        out.append((f'/G/M{k}', mat_blob(bl, mo, extra=(k % 4 == 0)), tags(bl, mo)))
    return out


class FakePkg:
    def __init__(self, blobs, name='/L/T'):
        self.exports = [{'outer': ('Null',)} for _ in blobs]
        self._b = blobs
        self.name = name

    def export_bytes(self, e):
        return self._b[self.exports.index(e)]


def tex_blob(fmt, w, h, n_stream, halve=True):
    out = b'\x01\x02' + fmt.encode() + b'\x00' + struct.pack('<II', 0, n_stream + 3)
    for k in range(n_stream):
        x, y = (max(1, w >> k), max(1, h >> k)) if halve else (w, h)
        out += struct.pack('<IIII', k, x, y, 1)
    return out + struct.pack('<I', n_stream) + b'\xaa' * 40      # inline tail (not a mip row)


def dxt5_blocks(n, clear_ratio):
    """n DXT5 blocks; a fraction of blocks fully clear (alpha 0), the rest alpha 200."""
    out = b''
    for k in range(n):
        a = 0 if k < n * clear_ratio else 200
        out += bytes([a, a]) + b'\x00' * 6 + b'\x00' * 8
    return out


# ---------------- tests ----------------
class Counted(unittest.TestCase):
    """Counts the assertions a test really makes, so its gate line reports them (never a fixed number)."""

    def setUp(self):
        self.n = 0

    def _c(name):  # noqa: N805
        def f(self, *a, **k):
            self.n = getattr(self, 'n', 0) + 1
            return getattr(unittest.TestCase, name)(self, *a, **k)
        return f
    assertEqual, assertIn, assertTrue, assertFalse, assertIsNone, assertGreater, assertGreaterEqual, assertNotIn = (
        _c('assertEqual'), _c('assertIn'), _c('assertTrue'), _c('assertFalse'), _c('assertIsNone'),
        _c('assertGreater'), _c('assertGreaterEqual'), _c('assertNotIn'))


class R01ReaderC(Counted):
    def test_parse_header_and_zero_mask(self):
        b = uv_blob([(3, b'\x07'), (4, None), (130, b'\x01')])
        entries, p = mp.parse_header(b)
        self.assertEqual(entries, [(3, False), (4, True), (130, False)])
        self.assertEqual(b[p:p + 2], b'\x07\x01')
        with self.assertRaises(mp.PropsError):
            mp.parse_header(b'\x05')                                   # truncated
        gate('header entries', len(entries))

    def test_learned_decoder_synthetic(self):
        tr = train_set()
        d = mp.Decoder(tr)
        self.assertTrue(d.ok, d.why)
        self.assertEqual({k: v[0] for k, v in d.found.items()}, {mp.BLEND: B, mp.MODEL: M, mp.FIELD: F})
        self.assertEqual(d.raw_learned, {X: {'bytes': 4, 'rows': 5}})  # 4-byte size, 5 rows really test it
        r = d.read(mat_blob('BLEND_Masked', 'MSM_Hair', extra=True), BITS)
        self.assertEqual((r['blend'], r['models']), ('BLEND_Masked', ['MSM_Hair']))
        r = d.read(mat_blob('BLEND_Masked', 'MSM_Unlit'), BITS)     # zero-masked enum value = byte 0
        self.assertEqual(r['models'], ['MSM_Unlit'])
        gate('synthetic training materials', d.n_train)

    def test_decoder_negatives(self):
        tr = train_set()
        bad = list(tr)
        bad[1] = (bad[1][0], bad[1][1], tags('BLEND_Translucent', 'MSM_Hair'))   # registry contradicts the bytes
        self.assertFalse(mp.Decoder(bad).ok)
        few = mp.Decoder(tr[:10])
        self.assertFalse(few.ok)
        self.assertIn('need 20', few.why)
        d = mp.Decoder(tr)
        r = d.read(mat_blob('BLEND_Masked', 'MSM_Hair', field=2), BITS)  # enum Hair, bit field DefaultLit
        self.assertFalse(r['readable'])
        self.assertIn('disagrees', r['why'])
        r = d.read(uv_blob([(3, b'\x00\x00'), (B, b'\x01'), (40, b'\x01')]), BITS)   # unknown size before ours
        self.assertFalse(r['readable'])
        self.assertIn('no learned size', r['why'])
        r = d.read(uv_blob([(B, b'\x09'), (40, b'\x01')]), BITS)       # never-seen blend byte
        self.assertFalse(r['readable'])
        self.assertFalse(mp.Decoder([]).read(b'\x00\x01', BITS)['readable'])
        # review #4: a row whose bytes contradict the assignment (bad nested header) is not silently skipped
        bad = list(tr)
        blob = bytearray(bad[1][1])
        k = blob.find(struct.pack('<HH', 0x0300, 128))
        blob[k:k + 2] = b'\x00\x02'
        bad[1] = (bad[1][0], bytes(blob), bad[1][2])
        self.assertFalse(mp.Decoder(bad).ok)
        # review #5: a raw size tested by fewer than MIN_RAW_ROWS rows is not learned -> those rows unreadable
        few = [t for t in tr if not t[0].endswith(('M4', 'M8', 'M16', 'M20'))]     # leaves 1 testing row (M28)
        d2 = mp.Decoder(few)
        self.assertTrue(d2.ok, d2.why)
        self.assertEqual(d2.raw_learned, {})
        self.assertFalse(d2.read(mat_blob('BLEND_Masked', 'MSM_Hair', extra=True), BITS)['readable'])

        gate('negative decoder cases (assertions)', self.n)

    def test_real_game_and_leon(self):
        ctx = real()
        rc = ctx['rc']
        self.assertTrue(rc.full.ok, rc.full.why)
        self.assertEqual({k: v[0] for k, v in rc.full.found.items()}, {mp.BLEND: 12, mp.MODEL: 18, mp.FIELD: 20})
        n_read = n_bad = 0
        for pkg, blob, tg in ctx['train']:
            r = rc.base(pkg, blob, {pkg})                               # leave-one-out
            if not r['readable']:
                continue
            n_read += 1
            c, _ = mc.ar_class(tg, ctx['peers'].bits)
            n_bad += (r['blend'] != tg['BlendMode'] or r['models'] != c['models'])
        self.assertEqual(n_bad, 0)
        self.assertGreaterEqual(n_read, 125)
        gate('game materials read leave-one-out (0 disagreements with the registry)', n_read)
        got = {n: rc.mod(mp.top_export_bytes(ctx['pk']['/Game/Leon/' + n])) for n in
               ('M_Leon_Body', 'M_Leon_Skin', 'M_Leon_Hair')}
        self.assertEqual((got['M_Leon_Body']['blend'], got['M_Leon_Body']['models']), ('BLEND_Opaque', ['MSM_DefaultLit']))
        self.assertEqual((got['M_Leon_Skin']['blend'], got['M_Leon_Skin']['models']),
                         ('BLEND_Opaque', ['MSM_SubsurfaceProfile']))
        self.assertEqual((got['M_Leon_Hair']['blend'], got['M_Leon_Hair']['models']), ('BLEND_Masked', ['MSM_Hair']))


class R02TextureLayout(Counted):
    def test_plain_mips_synthetic(self):
        p = FakePkg([tex_blob('PF_DXT1', 256, 256, 2)])
        need = 256 * 256 // 2 + 128 * 128 // 2
        self.assertTrue(tl.plain_mips(p, [need])['proven'])
        self.assertFalse(tl.plain_mips(p, [need + 16])['proven'])                     # size mismatch
        self.assertFalse(tl.plain_mips(p, [need // 2, need // 2])['proven'])         # two bulk chunks
        self.assertFalse(tl.plain_mips(p, [])['proven'])                             # no bulk (virtual or inline)
        self.assertIn('halve', tl.plain_mips(FakePkg([tex_blob('PF_DXT1', 256, 256, 2, halve=False)]), [need])['why'])
        self.assertFalse(tl.plain_mips(FakePkg([b'no format here']), [need])['proven'])
        self.assertFalse(tl.plain_mips(FakePkg([tex_blob('PF_XYZ', 64, 64, 1)]), [2048])['proven'])
        gate('synthetic layout cases (assertions)', self.n)

    def test_cutout_synthetic(self):
        w = 64
        mip0, mip1 = dxt5_blocks(256, 0.5), dxt5_blocks(64, 0.5)
        p = FakePkg([tex_blob('PF_DXT5', w, w, 2)])
        r = tl.cutout_alpha(p, [mip0 + mip1])
        self.assertTrue(r['cutout'], r)
        self.assertFalse(tl.cutout_alpha(p, [dxt5_blocks(256, 0) + dxt5_blocks(64, 0)])['cutout'])   # solid alpha
        self.assertFalse(tl.cutout_alpha(p, [dxt5_blocks(256, 1) + dxt5_blocks(64, 1)])['cutout'])   # all clear
        q = FakePkg([tex_blob('PF_DXT1', w, w, 2)])
        self.assertIn('no alpha', tl.cutout_alpha(q, [b'\x00' * (2048 + 512)])['why'])
        gate('synthetic cut-out cases (assertions)', self.n)

    def test_real_leon_textures(self):
        ctx = real()
        toc, pk = ctx['toc'], ctx['pk']
        bi = tl.mod_bulk_index(toc)
        texs = [n for n in pk if n.startswith('/Game/Leon/Textures/')]
        ok = [n for n in texs if tl.plain_mips(pk[n], [toc.offlen[r][1] for r in bi.get(T.cityhash.ue_name_hash(n), [])])['proven']]
        self.assertEqual(len(ok), len(texs))
        self.assertEqual(len(texs), 34)
        gate('Leon textures proven plain mip chains', len(ok))
        hd = '/Game/Leon/Textures/T_Leon_Hair_D'
        rows = bi[T.cityhash.ue_name_hash(hd)]
        r = tl.cutout_alpha(pk[hd], [toc.read_chunk(x, ctx['rd']) for x in rows])
        self.assertTrue(r['cutout'], r)
        self.assertGreater(r['clear'], 0.5)
        ub = '/Game/Leon/Textures/T_Leon_UpperBody_D'
        r = tl.cutout_alpha(pk[ub], [b'\x00' * toc.offlen[x][1] for x in bi[T.cityhash.ue_name_hash(ub)]])
        self.assertFalse(r['cutout'])
        self.assertIn('no alpha', r['why'])


NV = {'VirtualTextureStreaming': 'False'}
TG = {'/T/D': dict(NV, SRGB='True', CompressionSettings='TC_BC7'),
      '/T/DA': dict(NV, SRGB='True', CompressionSettings='TC_BC7', HasAlphaChannel='True'),
      '/T/DnoA': dict(NV, SRGB='True', CompressionSettings='TC_BC7', HasAlphaChannel='False'),
      '/T/VT_D': dict(SRGB='True', CompressionSettings='TC_BC7', VirtualTextureStreaming='True'),
      '/T/N': dict(NV, SRGB='False', CompressionSettings='TC_Normalmap'),
      '/T/X_ORM': dict(NV, SRGB='False', CompressionSettings='TC_BC7'),
      '/T/S/srgb_X_ORM': dict(NV, SRGB='True', CompressionSettings='TC_BC7'),
      '/T/X_MRO': dict(NV, SRGB='False', CompressionSettings='TC_BC7'),
      '/T/NoVT': dict(SRGB='True', CompressionSettings='TC_BC7')}
DEF = [TG['/T/D'], TG['/T/N'], TG['/T/X_MRO']]
MB = {'BaseColor': '/L/T_D', 'Normal': '/L/T_N', 'ORM': '/L/T_ORM'}
FM = {'/L/T_D': 'PF_DXT5', '/L/T_N': 'PF_BC5', '/L/T_ORM': 'PF_DXT1'}
PLAIN = {'proven': True}
CUT = {'proven': True, 'cutout': {'cutout': True}}


def mpar(params, names, bind, mod_tex=lambda t: PLAIN, defaults=DEF, mod_masked=False, mb=MB):
    return pr.map_params(params, mb, FM, names, bind, TG.get, set(), mod_tex=mod_tex, defaults=defaults,
                         mod_masked=mod_masked)


class R03ParamRules(Counted):
    def test_roles(self):
        cases = {'Hair Diffuse + Alpha': 'base_color_alpha', 'BaseColor': 'base_color', 'Diffuse Alpha Strength': None,
                 'ORM': 'packed:ORM', 'ORM - Texture': 'packed:ORM', 'IDRO Mask': None, 'Packed MRO Map': None,
                 'Opacity': None, 'Hair Diffuse': 'base_color'}
        for k, v in cases.items():
            self.assertEqual(pr.role_of(k), v, k)
        self.assertEqual(pr.name_letters('/a/T_Leon_Body_ORM'), 'ORM')
        self.assertIsNone(pr.name_letters('/a/T_Leon_Body_N'))
        gate('role cases', len(cases))

    def test_slot_gate_negatives(self):
        ok, lost = mpar(['BaseColor'], ['Albedo'], {'Albedo': {'/T/D'}})
        self.assertEqual(ok['BaseColor']['to'], 'Albedo')                                     # positive control
        _, lost = mpar(['BaseColor'], ['Albedo'], {'Albedo': {'/T/VT_D'}})
        self.assertIn('VIRTUAL', str(lost['BaseColor']))                                     # virtual slot
        _, lost = mpar(['BaseColor'], ['Albedo'], {'Albedo': {'/T/D', '/T/VT_D'}})
        self.assertIn('BaseColor', lost)                                                     # one virtual binding
        _, lost = mpar(['BaseColor'], ['Albedo'], {'Albedo': {'/T/NoVT'}})
        self.assertIn('unknown', str(lost['BaseColor']))                                     # VT flag missing
        _, lost = mpar(['BaseColor'], ['Albedo'], {'Albedo': {'/T/D'}}, defaults=[TG['/T/N']])
        self.assertIn('second source', str(lost['BaseColor']))                               # no plain colour default
        _, lost = mpar(['BaseColor'], ['Albedo'], {'Albedo': {'/T/D'}}, mod_tex=lambda t: {'proven': False, 'why': 'x'})
        self.assertIn('not proven plain', str(lost['BaseColor']))                            # mod texture unproven
        _, lost = mpar(['BaseColor'], ['Albedo'], {'Albedo': {'/T/D'}}, mod_tex=None)
        self.assertIn('BaseColor', lost)                                                     # no layout reader
        _, lost = mpar(['Normal'], ['Normal'], {'Normal': {'/T/D'}})
        self.assertIn('class mismatch', lost['Normal'])                                      # BC5 into a colour slot
        _, lost = mpar(['BaseColor'], ['Albedo'], {'Albedo': {'/T/D'}}, defaults=DEF + [TG['/T/VT_D']])
        self.assertIn('VIRTUAL colour default', lost['BaseColor'])
        self.assertIn('cannot be proven plain', str(lost['BaseColor']))                       # review #6
        ok, _ = mpar(['BaseColor'], ['Albedo'], {'Albedo': {'/T/D'}}, defaults=DEF + [dict(TG['/T/VT_D'], SRGB='False')])
        self.assertIn('BaseColor', ok)                                                       # a virtual LINEAR default
        # review #3: an exact name cannot read an ORM texture through an MRO-named game texture
        _, lost = mpar(['Mask'], ['Mask'], {'Mask': {'/T/X_MRO'}}, mb={'Mask': '/L/T_Body_ORM'})
        self.assertIn('disagree on packed channel letters', lost['Mask'])
        ok, _ = mpar(['Mask'], ['Mask'], {'Mask': {'/T/X_ORM'}}, mb={'Mask': '/L/T_Body_ORM'})
        self.assertIn('Mask', ok)                                                            # same letters: kept
        gate('slot gate cases (assertions)', self.n)

    def test_packed_same_order_and_swizzle(self):
        ok, lost = mpar(['ORM'], ['ORM - Texture'], {'ORM - Texture': {'/T/X_ORM'}})
        self.assertEqual(ok['ORM']['to'], 'ORM - Texture')                                   # same order, renamed
        self.assertEqual(ok['ORM']['how'], 'role:packed:ORM')
        _, lost = mpar(['ORM'], ['MRO'], {'MRO': {'/T/X_MRO'}})
        self.assertIn('swizzle', lost['ORM'])                                                # ORM vs MRO
        _, lost = mpar(['ORM'], ['ORM - Texture'], {'ORM - Texture': {'/T/X_MRO'}})
        self.assertIn('does not name the letters', str(lost['ORM']))                         # one source only
        _, lost = mpar(['ORM'], ['ORM - Texture'], {'ORM - Texture': {'/T/X_ORM'}},
                       mb=dict(MB, ORM='/L/T_Mask'))
        self.assertIn('mod texture name', str(lost['ORM']))                                  # mod side one source
        _, lost = mpar(['ORM'], ['ORM'], {'ORM': {'/T/X_MRO'}})
        self.assertIn('same name', lost['ORM'])                                              # exact name, wrong order
        _, lost = mpar(['ORM'], ['ORM'], {'ORM': {'/T/S/srgb_X_ORM'}})
        self.assertIn('not linear', lost['ORM'])
        gate('packed cases (assertions)', self.n)

    def test_colour_alpha(self):
        names, bind = ['Hair Diffuse + Alpha'], {'Hair Diffuse + Alpha': {'/T/DA'}}
        ok, lost = mpar(['BaseColor'], names, bind, mod_tex=lambda t: CUT, mod_masked=True)
        self.assertEqual(ok['BaseColor']['to'], 'Hair Diffuse + Alpha')                      # positive control
        _, lost = mpar(['BaseColor'], names, {'Hair Diffuse + Alpha': {'/T/DnoA'}}, mod_tex=lambda t: CUT,
                       mod_masked=True)
        self.assertIn('WITHOUT an alpha channel', str(lost['BaseColor']))                    # the real Townfall case
        _, lost = mpar(['BaseColor'], names, bind, mod_tex=lambda t: CUT, mod_masked=False)
        self.assertIn('not masked', lost['BaseColor'])
        _, lost = mpar(['BaseColor'], names, bind, mod_tex=lambda t: dict(CUT, cutout={'cutout': False, 'why': 'solid'}),
                       mod_masked=True)
        self.assertIn('not a proven cut-out', lost['BaseColor'])
        # review #2: a mod parameter that is itself named colour+alpha gets the same gate
        mb = {'Diffuse Alpha': '/L/T_D'}
        _, lost = mpar(['Diffuse Alpha'], names, bind, mod_tex=lambda t: dict(CUT, cutout={'cutout': False}),
                       mod_masked=True, mb=mb)
        self.assertIn('not a proven cut-out', lost['Diffuse Alpha'])
        _, lost = pr.map_params(['Diffuse Alpha'], mb, {'/L/T_D': 'PF_DXT1'}, names, bind, TG.get, set(),
                                mod_tex=lambda t: CUT, defaults=DEF, mod_masked=True)
        self.assertIn('PF_DXT1', lost['Diffuse Alpha'])                                       # DXT1 has no alpha
        ok, _ = mpar(['Diffuse Alpha'], names, bind, mod_tex=lambda t: CUT, mod_masked=True, mb=mb)
        self.assertEqual(ok['Diffuse Alpha']['to'], 'Hair Diffuse + Alpha')
        # review #7: another bound texture named opacity/mask -> the colour alpha is not trusted as the cut-out
        _, lost = mpar(['BaseColor', 'OpacityMask'], names, bind, mod_tex=lambda t: CUT, mod_masked=True,
                       mb=dict(MB, OpacityMask='/L/T_O'))
        self.assertIn('another texture named', lost['BaseColor'])
        gate('colour+alpha cases (assertions)', self.n)


class R04PartRoles(Counted):
    def test_roles_and_scores(self):
        r = part_roles.role(['MSM_SubsurfaceProfile'], '/Game/Leon/MI_Leon_Face', '/Game/Leon/M_Leon_Skin')
        self.assertEqual((r['role'], r['strength']), ('skin', 2))
        r = part_roles.role(['MSM_DefaultLit'], '/Game/Leon/MI_Leon_UpperBody', '/Game/Leon/M_Leon_Body')
        self.assertIsNone(r['role'])                                                          # 'upper body': ambiguous
        r = part_roles.role(['MSM_DefaultLit'], '/Game/Leon/MI_Leon_Pistol')
        self.assertEqual((r['role'], r['strength']), ('hard', 1))
        r = part_roles.role(['MSM_Hair'], '/X/MI_Jacket')
        self.assertIsNone(r['role'])                                                          # sources disagree
        self.assertIn('names say cloth', r['note'])
        cloth = part_roles.role(['MSM_Cloth', 'MSM_DefaultLit'], NONVT)
        skin = part_roles.role(['MSM_SubsurfaceProfile'], SKIN_HANDS)
        face = part_roles.role(['MSM_SubsurfaceProfile'], '/Game/Leon/MI_Leon_Face')
        self.assertEqual(part_roles.score(face, skin), 0)
        self.assertEqual(part_roles.score(face, cloth), 3)
        self.assertEqual(part_roles.score(part_roles.role(None, '/X/MI_A'), cloth), 2)
        gate('role cases (assertions)', self.n)


def _cand(names, group='opaque', models=('MSM_DefaultLit',), two=True, cls=mma.MATERIAL, special=None):
    p = {'readable': True, 'domain': 'MD_Surface', 'blend': {'opaque': 'BLEND_Opaque', 'masked': 'BLEND_Masked'}.get(
        group, 'BLEND_Translucent'), 'blend_group': group, 'lit': 'lit', 'models': list(models),
         'special': list(special if special is not None else [m for m in models if m in mc.SPECIAL_MODELS]),
         'models_two_source': two, 'sources': {'asset_registry': f'MD_Surface/{group}/lit',
                                               'shader_type_peers': f'MD_Surface/{group}/lit', 'peers': 9}}
    return {'names': list(names), 'export': 'E', 'class': cls, 'verified': 'iohash', 'skeletal_mesh_evidence': ['/S'],
            'props': p}


class R05PlanRules(Counted):
    LIN = {'SRGB': 'False', 'CompressionSettings': 'TC_Default', 'VirtualTextureStreaming': 'False'}
    NAMES = ['BaseColor', 'Normal', 'ORM', 'HairAO']

    @classmethod
    def setUpClass(cls):
        pk, libs = T.leon()
        ids, store = T.base()
        cls.pk = pk
        cls.audit = mma.audit_packages(pk, libs, T.script(), ids, store, 'SF_PS5-SP_PS5')

    def ctx(self, models=None, classes=None, accept=False, field_only=False):
        op, ma = {'readable': True, 'blend_group': 'opaque'}, {'readable': True, 'blend_group': 'masked'}
        classes = classes or {'/Game/Leon/M_Leon_Body': op, '/Game/Leon/M_Leon_Skin': op, '/Game/Leon/M_Leon_Hair': ma}
        models = models if models is not None else {
            '/Game/Leon/M_Leon_Body': ('BLEND_Opaque', ['MSM_DefaultLit']),
            '/Game/Leon/M_Leon_Skin': ('BLEND_Opaque', ['MSM_SubsurfaceProfile']),
            '/Game/Leon/M_Leon_Hair': ('BLEND_Masked', ['MSM_Hair'])}

        def mod_c(m):
            if m not in models:
                return {'readable': False, 'why': 'test'}
            out = {'readable': True, 'blend': models[m][0], 'models': models[m][1], 'model_enum': models[m][1][0]}
            if field_only:
                out.update(field_only=True, model_enum='MSM_FromMaterialExpression')
            return out
        bound = {n: {'/T/' + n} for n in ('BaseColor', 'Normal', 'HairAO')}
        return rp.Context(lambda m: classes.get(m, {'readable': False, 'why': 'test'}), lambda c: bound,
                          lambda t: self.LIN, {}, accept, mod_c=mod_c, mod_tex=lambda t: {'proven': True},
                          defaults=lambda c: [self.LIN])

    def rows(self, cands, **kw):
        return {r['mod_package'].rsplit('/', 1)[-1]: r for r in rp.plan(self.audit, self.pk, cands, True, ctx=self.ctx(**kw))}

    def test_cross_model_hair_refused(self):
        by = self.rows({'/B/M_HairDefaultLit': _cand(self.NAMES, 'masked')})
        h = by['MI_Leon_Hair']
        self.assertEqual(h['action'], 'known-checker')
        self.assertIn('never cross', h['refused_parents']['/B/M_HairDefaultLit'])
        self.assertIn('same', h['why'])
        gate('plan rows', len(by))

    def test_same_model_hair_and_its_negatives(self):
        same = _cand(self.NAMES, 'masked', ('MSM_Hair',))
        by = self.rows({'/B/M_Hair': same})
        self.assertEqual((by['MI_Leon_Hair']['action'], by['MI_Leon_Hair']['new_parent']), ('reparent-partial', '/B/M_Hair'))
        self.assertTrue(any('same shading model' in n for n in by['MI_Leon_Hair']['look_notes']))
        self.assertIn('masked parent for a opaque', by['MI_Leon_UpperBody']['refused_parents']['/B/M_Hair'])
        one = _cand(self.NAMES, 'masked', ('MSM_Hair',), two=False)                     # one-source model only
        h = self.rows({'/B/M_Hair': one})['MI_Leon_Hair']
        self.assertEqual(h['action'], 'known-checker')
        self.assertIn('two sources', h['refused_parents']['/B/M_Hair'])
        mic = _cand(self.NAMES, 'masked', ('MSM_Hair',), cls=mma.MIC)                    # instance parent
        self.assertEqual(self.rows({'/B/MI_Hair': mic})['MI_Leon_Hair']['action'], 'known-checker')
        both = _cand(self.NAMES, 'masked', ('MSM_Hair', 'MSM_DefaultLit'))               # superset, not the same
        h = self.rows({'/B/M_HairMixed': both})['MI_Leon_Hair']
        self.assertEqual(h['action'], 'known-checker')
        # review #1: an unreadable mod model refuses EVERY parent (A/B cannot exclude a special model)
        for cands in ({'/B/M_Hair': same}, {'/B/M_HairDefaultLit': _cand(self.NAMES, 'masked')}):
            h = self.rows(cands, models={})['MI_Leon_Hair']
            self.assertEqual(h['action'], 'known-checker')
            self.assertIn('mod shading model unreadable', h['why'])
        # review #8: a special model named only by the bit field (per-pixel enum) refuses the same-model route
        h = self.rows({'/B/M_Hair': same}, field_only=True)['MI_Leon_Hair']
        self.assertEqual(h['action'], 'known-checker')
        self.assertIn('bit field', h['why'])
        # a non-special (opaque DefaultLit) instance never takes an opaque special parent
        eye = _cand(self.NAMES, 'opaque', ('MSM_Eye',))
        up = self.rows({'/B/M_Eye': eye})['MI_Leon_UpperBody']
        self.assertEqual(up['action'], 'known-checker')
        self.assertIn('never cross', up['refused_parents']['/B/M_Eye'])
        gate('same-model cases (assertions)', self.n)

    def test_translucent_never_for_opaque(self):
        by = self.rows({'/B/M_Glass': _cand(self.NAMES, 'translucent')})
        self.assertEqual(by['MI_Leon_UpperBody']['action'], 'known-checker')
        self.assertIn('translucent parent for a opaque', by['MI_Leon_UpperBody']['refused_parents']['/B/M_Glass'])
        gate('translucent cases (assertions)', self.n)

    def test_role_and_model_ranking(self):
        cands = {'/B/M_Skin': _cand(self.NAMES, 'opaque', ('MSM_SubsurfaceProfile',)),
                 '/B/M_Cloth': _cand(self.NAMES, 'opaque', ('MSM_Cloth', 'MSM_DefaultLit'))}
        by = self.rows(cands)
        self.assertEqual(by['MI_Leon_Face']['new_parent'], '/B/M_Skin')                     # skin part -> skin parent
        self.assertEqual(by['MI_Leon_Face']['ranking']['role_score'], 0)
        self.assertEqual(by['MI_Leon_UpperBody']['new_parent'], '/B/M_Cloth')               # no role: model decides
        self.assertEqual(by['MI_Leon_UpperBody']['ranking']['model_score'], 1)
        self.assertEqual(by['MI_Leon_Pistol']['new_parent'], '/B/M_Cloth')
        self.assertTrue(any('role mismatch' in n for n in by['MI_Leon_Pistol']['look_notes']))
        # coverage first: a skin part goes to the cloth parent when that maps MORE (role only breaks ties)
        cands['/B/M_Skin'] = _cand(['BaseColor', 'ORM'], 'opaque', ('MSM_SubsurfaceProfile',))     # maps 1
        cands['/B/M_Cloth'] = _cand(['BaseColor', 'Normal', 'ORM'], 'opaque', ('MSM_Cloth', 'MSM_DefaultLit'))  # 2
        face = self.rows(cands)['MI_Leon_Face']
        self.assertEqual(face['new_parent'], '/B/M_Cloth')
        self.assertEqual(face['ranking']['role_score'], 3)
        self.assertTrue(any('role mismatch' in n for n in face['look_notes']))
        gate('ranking cases (assertions)', self.n)

    def test_reader_c_contradictions(self):
        cand = {'/B/M': _cand(self.NAMES)}
        models = {'/Game/Leon/M_Leon_Body': ('BLEND_Masked', ['MSM_DefaultLit'])}
        up = self.rows(cand, models=models)['MI_Leon_UpperBody']
        self.assertEqual(up['action'], 'known-checker')
        self.assertIn('mod readers disagree', up['why'])
        un = {'readable': False, 'unproven_opaque': True, 'why': 'opaque vs masked not provable'}
        cls = {'/Game/Leon/M_Leon_Body': un}
        up = self.rows(cand, models=models, classes=cls, accept=True)['MI_Leon_UpperBody']
        self.assertEqual(up['action'], 'known-checker')
        self.assertIn('user decision refused', up['why'])
        up = self.rows(cand, classes=cls, accept=True)['MI_Leon_UpperBody']                  # C agrees: accepted
        self.assertEqual(up['action'], 'reparent-partial')
        self.assertTrue(any('also reads opaque' in n for n in up['look_notes']))
        gate('reader C contradiction cases (assertions)', self.n)

    def test_rank_backward_compatible(self):
        o = {'mapped': 2, 'exact': 1, 'is_mic': False, 'names': 3}
        self.assertEqual(rp.rank('/x', o)[:3], (-2, 2, 2))
        gate('rank keys (assertions)', self.n)


def real():
    if 'real' in _c:
        return _c['real']
    store = {}
    for hp in T.ALL_CH:
        store.update(T.container_header.parse(T.rfile(hp))[1])
    checked = [r for p in T.CHECKED3 for r in T.jload(p)['checked'] if 'verified' in r]
    headers = {r['package']: r for r in checked}
    reg = T.registry()
    ti = rp.load_type_index(T.LIBS, T.STINFO, store)
    peers = mc.Peers(reg, ti)
    whole = rp.fetched_packages([T.FETCHED + d for d in ('/headers-r1', '/headers-r2', '/headers-r3')], checked)
    train = mp.training_set(whole, reg, mc.MATERIAL_AR_CLASS)
    pk, _ = T.leon()
    toc = utoc.load(T.LEON_TOC)
    rd = mma.reader(T.LEON_CAS)
    rc = mp.ReaderC(train, peers.bits)
    ctx = rp.build_context(reg, peers, pk, whole, headers, True, reader_c=rc, mod_toc=toc, mod_read=rd)
    _c['real'] = dict(store=store, checked=checked, headers=headers, reg=reg, peers=peers, whole=whole, train=train,
                      pk=pk, toc=toc, rd=rd, rc=rc, ctx=ctx)
    return _c['real']


class R06RealPlan(Counted):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp()
        cls.out = {}
        for key, flags in (('full', ['--allow-partial', '--accept-unproven-opaque']), ('default', [])):
            r = subprocess.run(T._plan_args(cls.tmp, flags), capture_output=True, text=True)
            assert r.returncode == 0, r.stdout + r.stderr
            cls.out[key] = (T.jload(os.path.join(cls.tmp, 'plan.json')), r.stdout)

    def test_per_part_outcome(self):
        plan, stdout = self.out['full']
        self.assertIn('reader C trained on', stdout)
        self.assertIn('candidates have a two-source shading model', stdout)
        by = {r['mod_package'].rsplit('/', 1)[-1]: r for r in plan['rows']}
        fixed = [n for n in LEON_MI if by['MI_Leon_' + n]['action'] == 'reparent-partial']
        self.assertEqual(sorted(fixed), sorted(n for n in LEON_MI if n != 'Hair'))
        reg = T.registry()
        bind = real()['ctx'].base_bind(NONVT)
        n_slots = 0
        for n in fixed:
            r = by['MI_Leon_' + n]
            self.assertEqual(r['new_parent'], NONVT)
            self.assertEqual(r['param_renames'], {'BaseColor': 'BaseColour'})
            self.assertIn('swizzle', r['lost_params']['ORM'])
            self.assertTrue(r['parent_class']['models_two_source'])
            self.assertTrue(r['mod_model']['readable'])
            for _p, v in r['param_map'].items():                       # no Leon texture lands in a virtual slot
                self.assertTrue(bind[v['to']])
                self.assertTrue(all(reg.asset(t)['tags']['VirtualTextureStreaming'] == 'False' for t in bind[v['to']]))
                n_slots += 1
        self.assertEqual(n_slots, 20)
        gate('mapped Leon slots on non-virtual game slots', n_slots)
        for n in ('Face', 'Mouth'):
            self.assertEqual(by['MI_Leon_' + n]['part_role']['role'], 'skin')
            self.assertTrue(any('role mismatch' in x for x in by['MI_Leon_' + n]['look_notes']))

    def test_hair_same_model_route_fails_closed_with_reason(self):
        plan, _ = self.out['full']
        h = {r['mod_package']: r for r in plan['rows']}['/Game/Leon/MI_Leon_Hair']
        self.assertEqual(h['action'], 'known-checker')
        self.assertEqual(h['mod_model']['models'], ['MSM_Hair'])
        self.assertIn(HAIR, h['why'])
        self.assertIn('WITHOUT an alpha channel', h['why'])
        c = {x['parent']: x for x in h['considered']}
        self.assertEqual(list(c), [HAIR])                                   # the ONLY allowed parent: same model
        self.assertEqual((c[HAIR]['model_score'], c[HAIR]['role_score']), (0, 0))
        self.assertIn('never cross', h['refused_parents'][HAIR_BEARD])
        cand = plan['candidates'][HAIR]['props']
        self.assertEqual(cand['reader_c']['models'], ['MSM_Hair'])
        self.assertTrue(cand['models_two_source'])
        gate('hair parents considered', len(c))

    def test_skin_hands_lost_on_virtual_slots(self):
        R = real()
        ctx = R['ctx']
        pk = R['pk']['/Game/Leon/MI_Leon_Face']
        bind, _ = pr.texture_bindings(pk)
        names = [r for r in R['checked'] if r['package'] == SKIN_HANDS][0]['names']
        m, lost = pr.map_params(['BaseColor', 'Normal', 'ORM'], bind, ctx.mod_formats, names, ctx.base_bind(SKIN_HANDS),
                                ctx.tex_tags, set(pk.names), mod_tex=ctx.mod_tex, defaults=ctx.defaults(SKIN_HANDS))
        self.assertEqual(m, {})
        self.assertIn('VIRTUAL', str(lost['BaseColor']))
        self.assertIn('VIRTUAL', lost['Normal'])
        gate('round-3 parent lost on virtual slots', len(lost))

    def test_default_flags_fail_closed(self):
        plan, _ = self.out['default']
        acts = {r['action'] for r in plan['rows']}
        self.assertEqual(acts, {'known-checker'})
        gate('default-flag rows', len(plan['rows']))

    def test_derived_package_applies_to_new_parent(self):
        plan, _ = self.out['full']
        up = {r['mod_package']: r for r in plan['rows']}['/Game/Leon/MI_Leon_UpperBody']
        pk = real()['pk']
        out, rec = mic_reparent.reparent(pk[up['mod_package']].data, up['old_parent'], up['new_parent'],
                                         up['new_export'], up['param_renames'],
                                         base_header=T.rfile(T.fetched_file(up['new_parent'])))
        self.assertTrue(rec['export_bytes_identical'])
        self.assertIn('two sources', rec['new_public_hash_source'])
        b, _ = pr.texture_bindings(zen.ZenPackage(out))
        self.assertEqual(sorted(b), ['BaseColour', 'Normal', 'ORM'])
        self.assertEqual(b['BaseColour'], '/Game/Leon/Textures/T_Leon_UpperBody_D')      # Leon's own texture kept
        gate('derived package bindings', len(b))


class R07FetchPlan(Counted):
    def _run(self, extra):
        tmp = tempfile.mkdtemp()
        out = os.path.join(tmp, 'r4.json')
        r = subprocess.run(T.PY + [os.path.join(HERE, 'master_fetch_plan.py'), '--asset-registry', T.AR_BIN,
                                   '--library'] + T.LIBS + ['--stinfo'] + T.STINFO + ['--base-header'] + T.ALL_CH +
                           ['--toc'] + T.BASE + ['--checked-headers'] + T.CHECKED3 +
                           ['--mod-utoc', T.LEON_TOC, '--mod-ucas', T.LEON_CAS, '--child-instances', '4', '--out', out]
                           + extra, capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        return T.jload(out)

    def test_same_model_special_rows(self):
        with_sp, without = self._run(['--same-model-special']), self._run([])
        pk = {r['package'] for r in with_sp['rows']}
        hair_kids = {'/Game/Townfall/Characters/Richard/Materials/MI_Richard_Eyebrows',
                     '/Game/Townfall/Characters/Zoe/Materials/MI_Zoe_Eyelashes'}
        self.assertTrue(hair_kids <= pk)
        self.assertFalse(hair_kids & {r['package'] for r in without['rows']})
        fetched = {r['package'] for r in real()['checked']}
        for x in with_sp['rows']:
            for k in ('remote_file', 'pieces', 'local_name', 'priority', 'length', 'whole_chunk'):
                self.assertIn(k, x)
            self.assertEqual(sum(n for _o, n in x['pieces']), x['length'])
            self.assertTrue(x['whole_chunk'])
            self.assertNotIn(x['package'], fetched)
        self.assertEqual(len({x['local_name'] for x in with_sp['rows']}), len(with_sp['rows']))
        self.assertEqual(sum(x['priority'] == 1 for x in without['rows']), 0)   # no skinned master left unfetched
        gate('round-4 fetch rows', len(with_sp['rows']))


if __name__ == '__main__':
    unittest.main(verbosity=2)
