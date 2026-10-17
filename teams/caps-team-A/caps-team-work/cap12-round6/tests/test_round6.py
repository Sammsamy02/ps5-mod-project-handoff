#!/usr/bin/env python3
"""Round-6 tests (Mac-only): mesh_sections, base_material_route, invisible_route v3 static-mesh proof,
mesh_slot_repoint. Rule tests on fact dicts plus real-data tests on Leon's unchanged mod meshes (copies in
memory; the mod files are only read). Prints how many tests ran; exits non-zero on any failure, any skip,
or zero tests."""
import collections
import copy
import os
import struct
import sys
import unittest

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.environ.get('R6_TOOLS', os.path.join(HERE, '..', 'work', 'tools'))
sys.path.insert(0, TOOLS)
import base_material_route as bmr  # noqa: E402
import invisible_route as ir  # noqa: E402
import mesh_sections as ms  # noqa: E402
import mesh_slot_repoint as msr  # noqa: E402
import utoc  # noqa: E402
import zen  # noqa: E402

WHAT = '/WORKSPACE/.codex/worktrees/translation-core/what'
MOD = WHAT + '/.superpowers/native-observations/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P'
HIDE_HDR = WHAT + '/outputs/2026-10-08-l25-prep/evidence/cap12-fetched-20261013/headers-r2/ad7b3742feeb3d06.hdr.bin'
HIDE = '/Game/MetaHumans/Common/Materials/M_Hide'
BODY = '/Game/Townfall/Characters/Simon/Meshes/SK_Simon_FirstStage'
FACE = '/Game/MetaHumans/Simon_Head_5_6_Optimised/Face/SKM_Simon_Head_FaceMesh'
REAL = os.path.exists(MOD + '.utoc') and os.path.exists(HIDE_HDR)
_CACHE = {}


def mod_package(name):
    if name not in _CACHE:
        t = utoc.load(MOD + '.utoc')
        with open(MOD + '.ucas', 'rb') as fh:
            def rd(o, n):
                fh.seek(o)
                return fh.read(n)
            for row in t.rows_of_type(1):
                d = t.read_chunk(row, rd)
                if zen.ZenPackage(d).name == name:
                    _CACHE[name] = d
                    break
    return _CACHE[name]


def decoded(name):
    k = ('dec', name)
    if k not in _CACHE:
        _CACHE[k] = ms.decode(mod_package(name))
    return _CACHE[k]


# ---------------------------------------------------------------- mesh_sections on real data
@unittest.skipUnless(REAL, 'real Leon / game files not on this Mac')
class MeshSectionsReal(unittest.TestCase):
    def test_body_mesh_slots_and_sections(self):
        m = decoded(BODY)
        self.assertEqual(len(m['slots']), 19)
        self.assertEqual(len(m['lods']), 1)
        lod = m['lods'][0]
        self.assertEqual(len(lod['sections']), 14)
        ph = [ms.section_placeholder(m, lod, r)[0] for r in lod['sections']]
        self.assertEqual(ph, [True] * 8 + [False] * 6)
        names = [r['slot_name'] for r in lod['sections'][:8]]
        self.assertEqual(names, ['Simon_Slot%d' % k for k in range(8)])
        for r in lod['sections'][:8]:
            self.assertEqual(r['triangles'], 1)
            self.assertLess(r['area_cm2'], 0.01)
            self.assertEqual(r['enclosed_rays'], 6)
            self.assertEqual(r['bones'], ['pelvis'])

    def test_face_mesh_placeholders_not_proven_enclosed(self):
        m = decoded(FACE)
        lod = m['lods'][0]
        fs = [r for r in lod['sections'] if r['slot_name'].startswith('Simon_FSlot')]
        self.assertEqual(len(fs), 9)
        for r in fs:
            ok, why = ms.section_placeholder(m, lod, r)
            self.assertFalse(ok)
            self.assertIn('not enclosed', why)

    def _mutate(self, fn):
        d = bytearray(mod_package(BODY))
        fn(d, decoded(BODY))
        return bytes(d)

    def test_placeholder_that_is_actually_drawn_is_refused(self):
        def big(d, m):
            o = m['export_data_offset'] + m['lods'][0]['positions_offset'] + 12 * 1   # slot-0 vertex 1
            x, y, z = struct.unpack_from('<3f', d, o)
            struct.pack_into('<3f', d, o, x + 0.05, y, z + 50.0)                     # a 50 cm sliver
        m = ms.decode(self._mutate(big))
        lod = m['lods'][0]
        ok, why = ms.section_placeholder(m, lod, lod['sections'][0])
        self.assertFalse(ok)
        self.assertIn('S2', why)
        self.assertTrue(ms.section_placeholder(m, lod, lod['sections'][1])[0])     # the others still are

    def test_tiny_triangle_outside_the_body_is_not_a_placeholder(self):
        def out(d, m):
            base = m['export_data_offset'] + m['lods'][0]['positions_offset']
            for v in range(3):
                x, y, z = struct.unpack_from('<3f', d, base + 12 * v)
                struct.pack_into('<3f', d, base + 12 * v, x + 200.0, y, z)
        m = ms.decode(self._mutate(out))
        lod = m['lods'][0]
        ok, why = ms.section_placeholder(m, lod, lod['sections'][0])
        self.assertFalse(ok)
        self.assertIn('not enclosed', why)

    def test_uv_density_is_the_third_source(self):
        def dens(d, m):
            o = m['export_data_offset'] + m['material_table_offset'] + 4 + 24      # slot 0 density[0]
            struct.pack_into('<f', d, o, 3.0)
        m = ms.decode(self._mutate(dens))
        lod = m['lods'][0]
        ok, why = ms.section_placeholder(m, lod, lod['sections'][0])
        self.assertFalse(ok)
        self.assertIn('S3', why)

    def test_tampered_buffer_header_fails_closed(self):
        def tri(d, m):
            # section 8 (real) triangle count: search its u32 after the section table offset is not exposed;
            # change the index-buffer element count instead (count must equal 3 x triangles)
            base = m['export_data_offset'] + m['lods'][0]['positions_offset']
            struct.pack_into('<I', d, base - 16, 13)            # position stride field -> 13
        with self.assertRaises(ms.MeshError):
            ms.decode(self._mutate(tri))

    def test_export_tiling_gap_fails_closed(self):
        d = bytearray(mod_package(BODY))
        z = zen.ZenPackage(bytes(d))
        exp = struct.unpack_from('<i', d, 24 + 8)[0]
        soff = struct.unpack_from('<Q', d, exp + 72 * 1)[0]
        struct.pack_into('<Q', d, exp + 72 * 1, soff + 2)       # mesh export starts 2 bytes later: a gap
        with self.assertRaises(ms.MeshError):
            ms.export_slices(zen.ZenPackage(bytes(d)))
        self.assertTrue(z.exports)

    def test_package_tail_rule(self):
        d = mod_package(BODY)
        self.assertEqual(d[-4:], ms.PACKAGE_FILE_TAG)
        ms.export_slices(zen.ZenPackage(d))                                          # tag: ok
        with self.assertRaises(ms.MeshError):
            ms.export_slices(zen.ZenPackage(d[:-4] + b'\0\0\0\0'))               # not the tag
        with self.assertRaises(ms.MeshError):
            ms.export_slices(zen.ZenPackage(d + b'\0' * 4))                         # 8 extra bytes

    def test_cso_bytes_differ_from_bundle_order_bytes(self):
        z = zen.ZenPackage(mod_package(BODY))
        top = [e for e in z.exports if e['outer'][0] == 'Null'][0]
        cso = [b for e, b in ms.export_slices(z) if e['outer'][0] == 'Null'][0]
        self.assertNotEqual(z.export_bytes(top), cso)                                # zen.py is off by 6 bytes here
        self.assertEqual(cso[:len(cso) - 6], z.data[z.header_size + 6:z.header_size + len(cso)])
        tex = zen.ZenPackage(mod_package('/Game/Leon/Textures/T_Leon_Eye_D'))
        t0 = tex.exports[0]
        self.assertEqual(tex.export_bytes(t0), ms.export_slices(tex)[0][1])           # single export: equal

    def test_material_usage_counts(self):
        u = ms.material_usage({BODY: decoded(BODY)})
        slot0 = u['/Game/Townfall/Characters/Simon/Meshes/Simon_Slot0']
        self.assertEqual(len(slot0), 1)
        self.assertTrue(slot0[0][3])
        self.assertFalse(u['/Game/Leon/MI_Leon_UpperBody'][0][3])


# ---------------------------------------------------------------- base_material_route (pure rules)
def usage(ph=True, n=8, real=0):
    row = {'section': 0, 'triangles': 1}
    out = [('/M/A', 0, dict(row, section=k), True if ph else False, 'S1 1 triangle(s); S2 ...') for k in range(n)]
    out += [('/M/A', 0, {'section': 100 + k, 'triangles': 5000}, False, 'S1: 5000 triangles') for k in range(real)]
    return out


def facts(**kw):
    f = {'package': '/Game/X/M_Base', 'audit_class': 'Material', 'audit_verdict': 'NO_TARGET_SHADERS',
         'overrides_base': None, 'instance_children': [], 'other_importers': {}, 'meshes': {'/M/A': 'ok'},
         'usage': usage(), 'parent_proof': {'proven': False, 'why': 'name'}, 'textures': [],
         'reader_c': {'readable': True, 'blend': 'BLEND_Opaque'}, 'readers_ab': {'readable': True, 'blend_group': 'opaque'}}
    f.update(kw)
    return f


class BaseMaterialRoute(unittest.TestCase):
    def test_placeholder(self):
        r = bmr.decide(facts())
        self.assertEqual((r['result'], r['option'], r['ship']), ('placeholder', 'B', False))

    def test_placeholder_that_is_actually_drawn(self):
        r = bmr.decide(facts(usage=usage(real=1)))
        self.assertEqual(r['option'], 'A')
        self.assertEqual(r['result'], 'refused')
        self.assertIn('references no texture', r['why'])

    def test_no_sections_at_all(self):
        r = bmr.decide(facts(usage=[]))
        self.assertEqual(r['result'], 'refused')
        self.assertIn('not provably unused', r['why'])

    def test_roles_cannot_be_proven(self):
        drawn = usage(real=1)
        r = bmr.decide(facts(usage=drawn, textures=[('/Game/X/T_Body_Grunge', 'PF_DXT1')]))
        self.assertEqual(r['result'], 'refused')
        self.assertIn('no name role', r['why'])
        r = bmr.decide(facts(usage=drawn, textures=[('/Game/X/T_Body_N', 'PF_DXT1')]))   # normal must be BC5
        self.assertIn('source 2', r['why'])
        r = bmr.decide(facts(usage=drawn, textures=[('/Game/X/T_Body_ORM', 'PF_BC5')]))   # mask cannot be BC5
        self.assertIn('source 2', r['why'])
        r = bmr.decide(facts(usage=drawn, textures=[('/Game/X/T_A_D', 'PF_DXT1'), ('/Game/X/T_B_D', 'PF_DXT1')]))
        self.assertIn('ambiguous', r['why'])

    def test_candidate_two_sources(self):
        r = bmr.decide(facts(usage=usage(real=2), textures=[('/Game/X/T_Body_D', 'PF_DXT1'),
                                                            ('/Game/X/T_Body_N', 'PF_BC5'),
                                                            ('/Game/X/T_Body_ORM', 'PF_DXT1')]))
        self.assertEqual(r['result'], 'base-instance-candidate')
        self.assertEqual(sorted(r['roles'].values()), ['base_color', 'normal', 'packed:ORM'])
        self.assertFalse(r['ship'])

    def test_blend_needs_two_readers(self):
        tex = [('/Game/X/T_Body_D', 'PF_DXT1')]
        r = bmr.decide(facts(usage=usage(real=1), textures=tex, readers_ab={'readable': True, 'blend_group': 'masked'}))
        self.assertEqual(r['result'], 'refused')
        self.assertIn('two readers', r['why'])
        r = bmr.decide(facts(usage=usage(real=1), textures=tex, readers_ab={'readable': False}))
        self.assertEqual(r['result'], 'refused')

    def test_translucent_never_goes_to_opaque(self):
        tex = [('/Game/X/T_Glass_D', 'PF_DXT5')]
        r = bmr.decide(facts(usage=usage(real=1), textures=tex,
                             reader_c={'readable': True, 'blend': 'BLEND_Translucent'},
                             readers_ab={'readable': True, 'blend_group': 'translucent'}))
        self.assertEqual((r['result'], r['blend_group']), ('base-instance-candidate', 'translucent'))
        self.assertIn('translucent game material', r['why'])
        self.assertNotIn('opaque', r['why'])
        r = bmr.decide(facts(usage=usage(real=1), textures=tex,
                             reader_c={'readable': True, 'blend': 'BLEND_Translucent'},
                             readers_ab={'readable': True, 'blend_group': 'opaque'}))
        self.assertEqual(r['result'], 'refused')

    def test_scope_and_fail_closed(self):
        self.assertEqual(bmr.decide(facts(audit_class='MaterialInstanceConstant'))['result'], 'not-this-route')
        self.assertEqual(bmr.decide(facts(overrides_base='/Game/G'))['result'], 'refused')
        self.assertEqual(bmr.decide(facts(meshes={}, instance_children=['/X/MI']))['result'], 'parent-only')
        self.assertEqual(bmr.decide(facts(meshes={}))['result'], 'no-user')
        self.assertEqual(bmr.decide(facts(meshes={'/M/A': 'bad table'}))['result'], 'refused')
        r = bmr.decide(facts(other_importers={'Blueprint': ['/B/BP']}))
        self.assertEqual(r['result'], 'refused')
        self.assertIn('Blueprint', r['why'])

    def test_draws_nothing_option_b(self):
        r = bmr.decide(facts(usage=usage(real=1), parent_proof={'proven': True, 'sources': {'name': 'invisible'}}))
        self.assertEqual((r['result'], r['option']), ('draws-nothing', 'B'))

    def test_texture_name_roles(self):
        self.assertEqual(bmr.texture_name_role('/G/T_X_D'), 'base_color')
        self.assertEqual(bmr.texture_name_role('/G/T_X_Normal'), 'normal')
        self.assertEqual(bmr.texture_name_role('/G/T_X_MRO'), 'packed:MRO')
        self.assertIsNone(bmr.texture_name_role('/G/T_X_Mask'))
        self.assertIsNone(bmr.texture_name_role('/G/T_X_OOR'))        # repeated letters


# ---------------------------------------------------------------- invisible_route v3: static meshes
LOCAL, SKIN, ISM = ir.VF_PARAMS['StaticMesh'], ir.VF_PARAMS['SkeletalMesh'], 'FInstancedStaticMeshVertexFactoryShaderParameters'


def vf_train():
    t = []
    for i in range(12):
        t.append(('/G/S%d' % i, {LOCAL, ISM} if i % 2 else {LOCAL}, {'StaticMesh'}))
    for i in range(11):
        t.append(('/G/K%d' % i, {LOCAL, SKIN}, {'SkeletalMesh'}))
    return t


def target(**kw):
    tr = vf_train()
    f = {'package': '/Game/C/M_Hide', 'asset_name': 'M_Hide', 'registry_class': 'Material', 'shader_maps': 1,
         'overridden_by_mod': False, 'header_whole_verified': True,
         'two_source_class': {'readable': True, 'domain': 'MD_Surface', 'blend': 'BLEND_Masked', 'models': ['MSM_Unlit'],
                              'sources': {'asset_registry': 'x', 'shader_type_peers': 'y'}},
         'reader_c': {'readable': True, 'blend': 'BLEND_Masked'}, 'names': ['M_Hide', '/Game/C/M_Hide'],
         'own_names': ['M_Hide', '/Game/C/M_Hide'], 'imports': [], 'skeletal_users': ['/G/SK_Face'],
         'usage_flag': {'index': 53, 'global_index': 53, 'present': True, 'trained': 19},
         'vf': {'tokens': sorted({LOCAL, SKIN}), 'static': ir.learn_vf_token(tr, 'StaticMesh', exclude={'/Game/C/M_Hide'}),
                'static_global': ir.learn_vf_token(tr, 'StaticMesh'),
                'skeletal': ir.learn_vf_token(tr, 'SkeletalMesh', need_absent_in_static_only=True)}}
    f.update(kw)
    d = ir.decide_game_target(f)
    d['export'] = 'M_Hide'
    return d


def parents():
    return {'/Game/X/M_Invisible': {'package': '/Game/X/M_Invisible', 'proven': True, 'blend': 'BLEND_Masked',
                                    'models': ['MSM_Unlit'], 'sources': {'name': 'invisible'}}}


def row(**kw):
    r = {'mod_package': '/Game/X/MI_Glass', 'action': 'known-checker', 'class': 'MaterialInstanceConstant',
         'verdict': 'INHERITS_MISSING', 'old_parent': '/Game/X/M_Invisible', 'params': [], 'why': 'x',
         '_ids': {'pkg': 'a', 'from': 'b', 'to': 'c'}}
    r.update(kw)
    return r


class StaticMeshProof(unittest.TestCase):
    def test_learn_tokens(self):
        tr = vf_train()
        self.assertEqual(ir.learn_vf_token(tr, 'StaticMesh')['token'], LOCAL)
        self.assertEqual(ir.learn_vf_token(tr, 'SkeletalMesh', need_absent_in_static_only=True)['token'], SKIN)
        self.assertIsNone(ir.learn_vf_token(tr, 'SkeletalMesh')['token'])      # LOCAL and SKIN both fit
        self.assertIsNone(ir.learn_vf_token(tr[:5], 'StaticMesh')['token'])    # too few
        bad = [(p, t | {ISM}, u) for p, t, u in tr]                            # ISM everywhere: 2 fit
        self.assertIsNone(ir.learn_vf_token(bad, 'StaticMesh')['token'])

    def test_target_proven_for_both(self):
        t = target()
        self.assertTrue(t['proven'])
        self.assertEqual(t['proven_user_classes'], ['SkeletalMesh', 'StaticMesh'])

    def test_static_one_source_missing(self):
        t = target(vf={'tokens': [SKIN], 'static': {'token': LOCAL, 'trained': 12}, 'static_global': {'token': LOCAL},
                       'skeletal': {'token': SKIN}})
        self.assertEqual(t['proven_user_classes'], ['SkeletalMesh'])          # own map lacks the static token
        self.assertIn('lacks', t['static_mesh']['why'])
        t = target(vf={'tokens': [LOCAL, SKIN], 'static': {'token': ISM, 'trained': 12}, 'static_global': {'token': ISM},
                       'skeletal': {'token': SKIN}})
        self.assertEqual(t['proven_user_classes'], ['SkeletalMesh'])          # learned token is not the engine's
        t = target(vf={'tokens': [LOCAL, SKIN], 'static': {'token': LOCAL, 'trained': 12},
                       'static_global': {'token': ISM}, 'skeletal': {'token': SKIN}})
        self.assertEqual(t['proven_user_classes'], ['SkeletalMesh'])          # not stable without it
        t = target(vf={'tokens': [LOCAL], 'static': {'token': LOCAL, 'trained': 12}, 'static_global': {'token': LOCAL},
                       'skeletal': {'token': SKIN}})
        self.assertEqual(t['proven_user_classes'], ['SkeletalMesh'])          # method check: skin token absent
        t = target(vf={'tokens': None})
        self.assertEqual(t['proven_user_classes'], ['SkeletalMesh'])

    def test_rows(self):
        tg = [target()]
        new, why = ir.decide_row(row(), {'users': {'SkeletalMesh': 1, 'StaticMesh': 3}}, parents(), tg)
        self.assertEqual(new['action'], 'reparent', why)
        self.assertEqual(new['user_coverage'], 1.0)
        new, why = ir.decide_row(row(), {'users': {'Blueprint': 4, 'StaticMesh': 1}}, parents(), tg)
        self.assertEqual(new['action'], 'reparent-partial')
        self.assertEqual(new['user_coverage'], 0.2)
        self.assertIn('allow-partial', new['partial_reason'])
        new, why = ir.decide_row(row(), {'users': {'Blueprint': 2, 'NiagaraSystem': 1}}, parents(), tg)
        self.assertIsNone(new)
        self.assertIn('no SkeletalMesh/StaticMesh user', why)

    def test_static_users_do_not_count_without_the_static_proof(self):
        tg = [target(vf={'tokens': [SKIN], 'static': {'token': LOCAL, 'trained': 12}, 'static_global': {'token': LOCAL},
                         'skeletal': {'token': SKIN}})]
        new, why = ir.decide_row(row(), {'users': {'StaticMesh': 1}}, parents(), tg)
        self.assertIsNone(new)
        self.assertIn('no SkeletalMesh user', why)

    def test_translucent_mod_parent_never_goes_to_the_masked_target(self):
        p = parents()
        p['/Game/X/M_Invisible'].update(blend='BLEND_Translucent')
        new, why = ir.decide_row(row(), {'users': {'SkeletalMesh': 1}}, p, [target()])
        self.assertIsNone(new)
        self.assertIn('same blend group', why)

    def test_r3b_full_coverage_through_the_game_original(self):
        tg = [target()]
        om = {'master': '/G/M_Skin', 'tokens': sorted({LOCAL, SKIN})}
        new, why = ir.decide_row(row(), {'users': {'LevelSequence': 1}, 'original_master': om,
                                         'mod_user_classes': []}, parents(), tg)
        self.assertEqual(new['action'], 'reparent', why)
        self.assertEqual(new['users_proven_by'], 'original-master-paths')
        self.assertEqual(new['user_coverage'], 1.0)
        new, why = ir.decide_row(row(), {'users': {'Blueprint': 1, 'SkeletalMesh': 3}, 'original_master': om,
                                         'mod_user_classes': ['SkeletalMesh']}, parents(), tg)
        self.assertEqual(new['action'], 'reparent', why)
        new, why = ir.decide_row(row(), {'users': {'LevelSequence': 1}, 'original_master': om}, parents(), tg)
        self.assertIsNone(new)                                            # mod users not known: no R3b
        self.assertIn('mod users not known', why)

    def test_r3b_refuses_a_vertex_factory_the_target_lacks(self):
        tg = [target()]
        om = {'master': '/G/M_Liquid', 'tokens': sorted({LOCAL, 'FNiagaraMeshVertexFactoryShaderParametersVS'})}
        new, why = ir.decide_row(row(), {'users': {'Blueprint': 5, 'NiagaraSystem': 2}, 'original_master': om},
                                 parents(), tg)
        self.assertIsNone(new)
        self.assertIn('FNiagaraMeshVertexFactoryShaderParametersVS', why)
        om = {'master': '/G/M_Cloth', 'tokens': sorted({LOCAL, SKIN, ISM})}
        new, why = ir.decide_row(row(), {'users': {'Blueprint': 4, 'StaticMesh': 1}, 'original_master': om},
                                 parents(), tg)
        self.assertEqual(new['action'], 'reparent-partial')               # direct static user still proven
        self.assertIn(ISM, new['partial_reason'])

    def test_r3b_needs_the_master_and_proven_mod_users(self):
        tg = [target()]
        new, why = ir.decide_row(row(), {'users': {'LevelSequence': 1}, 'original_master': {'why': 'not fetched'}},
                                 parents(), tg)
        self.assertIsNone(new)
        self.assertIn('not fetched', why)
        om = {'master': '/G/M_Skin', 'tokens': sorted({LOCAL, SKIN})}
        new, why = ir.decide_row(row(), {'users': {'LevelSequence': 1}, 'original_master': om,
                                         'mod_user_classes': ['NiagaraSystem']}, parents(), tg)
        self.assertIsNone(new)
        self.assertIn('mod users of unproven classes', why)
        tg2 = [target(vf={'tokens': [SKIN], 'static': {'token': LOCAL, 'trained': 12},
                          'static_global': {'token': LOCAL}, 'skeletal': {'token': SKIN}})]   # skeletal only
        new, why = ir.decide_row(row(), {'users': {'LevelSequence': 1}, 'original_master': om,
                                         'mod_user_classes': []}, parents(), tg2)
        self.assertIsNone(new)                                            # LOCAL not proven for this target
        self.assertIn(LOCAL, why)

    def test_vf_tokens_reader(self):
        lp = lambda n: struct.pack('<i', len(n) + 1) + n.encode() + b'\0'
        blob = b'\x07' + lp(LOCAL) + b'xx' + lp('TGPUSkinAPEXClothVertexFactoryShaderParameters') + lp('FDepthOnlyPS')
        self.assertEqual(ir.vf_tokens(blob), {LOCAL, 'TGPUSkinAPEXClothVertexFactoryShaderParameters'})
        self.assertEqual(ir.vf_tokens(b'..' + LOCAL.encode() + b'\0'), set())          # no length prefix
        self.assertEqual(ir.vf_tokens(b'\0\0\0\0FooF' + lp(LOCAL)[4:]), set())       # wrong prefix
        self.assertEqual(ir.vf_tokens(None), set())
        self.assertEqual(ir.drawing_paths(lp(LOCAL) + lp('TBasePassCSFNoLightMapPolicy')), {LOCAL, ir.NANITE_PATH})

    def test_r3b_cloth_layout_and_nanite_are_missing_paths(self):
        tg = [target()]
        for extra in ('TGPUSkinAPEXClothVertexFactoryShaderParameters', ir.NANITE_PATH):
            om = {'master': '/G/M', 'tokens': sorted({LOCAL, SKIN, extra})}
            new, why = ir.decide_row(row(), {'users': {'Blueprint': 1}, 'original_master': om,
                                             'mod_user_classes': []}, parents(), tg)
            self.assertIsNone(new)
            self.assertIn(extra, why)

    def test_strict_partial_when_the_original_has_other_paths(self):
        tg = [target()]
        om = {'master': '/G/M', 'tokens': sorted({LOCAL, SKIN, ISM})}
        new, why = ir.decide_row(row(), {'users': {'SkeletalMesh': 1, 'StaticMesh': 3}, 'original_master': om,
                                         'mod_user_classes': []}, parents(), tg)
        self.assertEqual(new['action'], 'reparent-partial', why)          # all users proven, master has ISM
        self.assertIn(ISM, new['partial_reason'])
        new, why = ir.decide_row(row(), {'users': {'SkeletalMesh': 1, 'StaticMesh': 3},
                                         'original_master': {'why': 'not fetched'}}, parents(), tg)
        self.assertEqual(new['action'], 'reparent', why)                  # master unknown: Team B's standard
        self.assertTrue(any('original master not checked' in n for n in new['look_notes']))

    def test_nanite_users_are_not_proven(self):
        tg = [target()]
        new, why = ir.decide_row(row(), {'users': {'StaticMesh(Nanite)': 1}}, parents(), tg)
        self.assertIsNone(new)
        new, why = ir.decide_row(row(), {'users': {'StaticMesh(Nanite)': 1, 'StaticMesh': 1}}, parents(), tg)
        self.assertEqual(new['action'], 'reparent-partial')
        self.assertEqual(new['user_coverage'], 0.5)

    def test_r3b_zero_users_and_truncation(self):
        tg = [target()]
        om = {'master': '/G/M', 'tokens': sorted({LOCAL, SKIN})}
        new, why = ir.decide_row(row(), {'users': {}, 'original_master': om, 'mod_user_classes': []}, parents(), tg)
        self.assertIsNone(new)
        self.assertIn('no run-time user', why)
        new, why = ir.decide_row(row(), {'users': {'Blueprint': 1}, 'users_truncated': True, 'original_master': om,
                                         'mod_user_classes': []}, parents(), tg)
        self.assertIsNone(new)
        self.assertIn('deeper', why)
        new, why = ir.decide_row(row(), {'users': {'unknown': 1}, 'original_master': om,
                                         'mod_user_classes': ['unknown']}, parents(), tg)
        self.assertIsNone(new)
        self.assertIn('unproven classes', why)

    def test_static_proof_needs_the_surface_domain(self):
        t = target(two_source_class={'readable': True, 'domain': 'MD_Surface', 'blend': 'BLEND_Masked',
                                     'models': ['MSM_Unlit'], 'sources': {'asset_registry': 'x', 'shader_type_peers': 'y'}})
        self.assertIn('engine_rule', t['static_mesh'])
        f = {'vf': {'tokens': [LOCAL, SKIN], 'static': {'token': LOCAL, 'trained': 12}, 'static_global': {'token': LOCAL},
                    'skeletal': {'token': SKIN}}, 'two_source_class': {'domain': 'MD_Volume'}}
        ok, why = ir.static_proof(f)
        self.assertFalse(ok)
        self.assertIn('MD_Surface', why)


class OriginalMaster(unittest.TestCase):
    def setUp(self):
        self.cls = {'/G/MI': 'MaterialInstanceConstant', '/G/MI2': 'MaterialInstanceConstant', '/G/M': 'Material',
                    '/G/T': 'Texture2D', '/G/M2': 'Material'}.get
        self.paths = lambda m: (sorted({LOCAL, SKIN}), None)

    def find(self, imports, pkg='/G/MI', overridden=('/G/MI',), paths=None, depth=ir.MAX_DEPTH):
        return ir.find_original_master(pkg, set(overridden), imports, self.cls, paths or self.paths, depth=depth)

    def test_chain(self):
        r = self.find({'/G/MI': {'/G/MI2', '/G/T'}, '/G/MI2': {'/G/M'}})
        self.assertEqual((r['master'], r['tokens']), ('/G/M', sorted({LOCAL, SKIN})))

    def test_fail_closed(self):
        self.assertIn('not an override', self.find({}, overridden=())['why'])
        self.assertIn('2 material imports', self.find({'/G/MI': {'/G/M', '/G/M2'}})['why'])
        self.assertIn('0 material imports', self.find({'/G/MI': {'/G/T'}})['why'])
        self.assertIn('cyclic', self.find({'/G/MI': {'/G/MI2'}, '/G/MI2': {'/G/MI'}})['why'])
        self.assertIn('deeper', self.find({'/G/MI': {'/G/MI2'}, '/G/MI2': {'/G/M'}}, depth=1)['why'])
        r = self.find({'/G/MI': {'/G/M'}}, paths=lambda m: (None, 'master not fetched whole + IoHash-verified'))
        self.assertNotIn('tokens', r)
        self.assertIn('not fetched', r['why'])
        r = self.find({'/G/MI': {'/G/M'}}, paths=lambda m: ([ir.NANITE_PATH], None))
        self.assertIn('no vertex-factory layout', r['why'])


# ---------------------------------------------------------------- mesh_slot_repoint (real meshes)
@unittest.skipUnless(REAL, 'real Leon / game files not on this Mac')
class SlotRepoint(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.hdr = open(HIDE_HDR, 'rb').read()

    def test_body_placeholder_slots(self):
        out, rec = msr.repoint(mod_package(BODY), range(8), HIDE, 'M_Hide', self.hdr)
        self.assertEqual(rec['changed_export_bytes'], 7)
        self.assertFalse(rec['export_bytes_identical'])
        m = ms.decode(out)
        self.assertEqual([s['material'] for s in m['slots'][:8]], [HIDE] * 8)
        self.assertEqual([s['material'] for s in m['slots'][8:]], [s['material'] for s in decoded(BODY)['slots'][8:]])
        self.assertEqual(rec['store_entry_import_change']['new_package_id'], 'ad7b3742feeb3d06')

    def test_rewrite_that_would_change_a_non_target_slot_is_refused(self):
        with self.assertRaises(msr.RepointError) as cm:
            msr.repoint(mod_package(FACE), [0], HIDE, 'M_Hide', self.hdr)     # FSlot0..8 share one import
        self.assertIn('non-target', str(cm.exception))

    def test_guard_catches_a_stray_patch(self):
        m0 = decoded(BODY)
        out, _rec = msr.repoint(mod_package(BODY), range(8), HIDE, 'M_Hide', self.hdr)
        m1 = ms.decode(out)
        d = bytearray(out)
        o = m1['export_data_offset'] + m1['slot_ref_offsets'][9]
        struct.pack_into('<i', d, o, m1['slots'][0]['raw_ref'])                 # a stray write into slot 9
        m2 = ms.decode(bytes(d))
        with self.assertRaises(msr.RepointError) as cm:
            msr.verify(m0, m2, list(range(8)), HIDE, m1['slots'][0]['material_export_hash'])
        self.assertIn('non-target slot 9 changed', str(cm.exception))

    def test_carrier_referenced_elsewhere_is_refused(self):
        m = decoded(BODY)
        d = bytearray(mod_package(BODY))
        carrier = m['slots'][0]['raw_ref']
        struct.pack_into('<i', d, m['export_data_offset'] + m['material_table_offset'] - 8, carrier)
        with self.assertRaises(msr.RepointError) as cm:
            msr.repoint(bytes(d), range(8), HIDE, 'M_Hide', self.hdr)
        self.assertIn('referenced', str(cm.exception))

    def test_new_package_already_imported_is_refused(self):
        with self.assertRaises(msr.RepointError):
            msr.repoint(mod_package(BODY), [0], '/Game/Leon/MI_Leon_UpperBody', 'MI_Leon_UpperBody', self.hdr)

    def test_bad_slots(self):
        with self.assertRaises(msr.RepointError):
            msr.repoint(mod_package(BODY), [], HIDE, 'M_Hide', self.hdr)
        with self.assertRaises(msr.RepointError):
            msr.repoint(mod_package(BODY), [99], HIDE, 'M_Hide', self.hdr)


if __name__ == '__main__':
    suite = unittest.defaultTestLoader.loadTestsFromModule(sys.modules[__name__])
    res = unittest.TextTestRunner(verbosity=1).run(suite)
    ran = res.testsRun - len(res.skipped)
    print('test_round6: %d tests ran, %d failed, %d skipped' % (ran, len(res.failures) + len(res.errors),
                                                               len(res.skipped)))
    sys.exit(0 if res.wasSuccessful() and ran > 0 and not res.skipped else 1)
