#!/usr/bin/env python3
"""Tests for PATCH/tools/invisible_route.py (Mac-only). ROUND 6 COPY (Team A): Team B's suite, unchanged
except the real-data expectations (round-6 static-mesh proof + R3b + review fixes on the round-5 console plan:
10 reparent, 4 reparent-partial, 14 invisible rows) and the derive step (the integrated round-5
derive_substitutes.py). Positive, negative and one-source
cases on fact dicts, plus an integration run on the real Leon + game files when present.
Prints how many tests ran; exits non-zero on any failure or when zero tests ran."""
import collections
import json
import os
import subprocess
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
PATCH_TOOLS = os.environ.get('CAP12B_TOOLS', os.path.join(HERE, '..', 'PATCH', 'tools'))
sys.path.insert(0, PATCH_TOOLS)
import invisible_route as ir  # noqa: E402

WHAT = '/WORKSPACE/.codex/worktrees/translation-core/what'
TEAM_A_TOOLS = os.environ.get('CAP12_TOOLS',
                              '/WORKSPACE/.codex/worktrees/translation-core/caps-team-A/caps-team-work/'
                              'cap12-fulllook/PATCH/tools')
TEAM_A_PLAN = os.environ.get('R6_PLAN', WHAT + '/outputs/2026-10-08-l25-prep/evidence/cap12-launch-prep-20261013/'
                                       'reparent-plan-r5-textures.json')


def mod_parent(**kw):
    f = {'package': '/Game/X/Mats/M_Invisible', 'asset_name': 'M_Invisible', 'audit_class': 'Material',
         'audit_verdict': 'NO_TARGET_SHADERS', 'overrides_base': None,
         'reader_c': {'readable': True, 'blend': 'BLEND_Masked', 'models': ['MSM_Unlit']},
         'readers_ab': {'readable': False, 'why': 'reader B: none', 'reader_a_classes': ['MD_Surface/masked']},
         'names': ['M_Invisible', '/Game/X/Mats/M_Invisible'], 'own_names': ['M_Invisible', '/Game/X/Mats/M_Invisible'],
         'imports': []}
    f.update(kw)
    return f


def game_target(**kw):
    f = {'package': '/Game/C/M_Hide', 'asset_name': 'M_Hide', 'registry_class': 'Material', 'shader_maps': 1,
         'overridden_by_mod': False, 'header_whole_verified': True,
         'two_source_class': {'readable': True, 'domain': 'MD_Surface', 'blend': 'BLEND_Masked',
                              'models': ['MSM_Unlit'], 'sources': {'asset_registry': 'MD_Surface/masked/unlit',
                                                                   'shader_type_peers': 'MD_Surface/masked/unlit'}},
         'reader_c': {'readable': True, 'blend': 'BLEND_Masked', 'models': ['MSM_Unlit']},
         'names': ['M_Hide', '/Game/C/M_Hide'], 'own_names': ['M_Hide', '/Game/C/M_Hide'], 'imports': [],
         'skeletal_users': ['/Game/A/SK_Face'], 'usage_flag': {'index': 53, 'present': True, 'trained': 19}}
    f.update(kw)
    return f


def plan_row(**kw):
    r = {'mod_package': '/Game/X/MI_Glass', 'class': 'MaterialInstanceConstant', 'verdict': 'INHERITS_MISSING',
         'old_parent': '/Game/X/Mats/M_Invisible', 'params': [], 'overrides_base': 'base.utoc',
         'action': 'known-checker', 'why': 'no own parameters', 'parameterless': True,
         '_ids': {'pkg': '1111111111111111', 'from': '2222222222222222', 'to': '3333333333333333'}}
    r.update(kw)
    return r


def proven():
    p = ir.decide_mod_parent(mod_parent())
    t = ir.decide_game_target(game_target())
    t['export'] = 'M_Hide'
    return {p['package']: p}, t


class Names(unittest.TestCase):
    def test_tokens(self):
        self.assertEqual(ir.name_says_invisible('M_Invisible'), 'invisible')
        self.assertEqual(ir.name_says_invisible('M_Hide'), 'hide')
        self.assertEqual(ir.name_says_invisible('NaniteHiddenSectionMaterial'), 'hidden')
        self.assertEqual(ir.name_says_invisible('MI_InvisMask'), 'invis')

    def test_no_false_tokens(self):
        for n in ('M_HideoutWall', 'Simon_Slot0', 'M_Leon_Body', 'M_InvisbleMesh', 'M_Visible', 'M_Cartilage',
                  'M_TF_Widget_Transparent', 'M_NotHidden', 'M_Un_Hide', 'M_NonInvisible', 'M_Unhide'):
            self.assertIsNone(ir.name_says_invisible(n), n)


class ModParent(unittest.TestCase):
    def test_positive(self):
        d = ir.decide_mod_parent(mod_parent())
        self.assertTrue(d['proven'], d)
        self.assertIn('name', d['sources'])
        self.assertIn('own_property_block', d['sources'])

    def test_blend_whitelist(self):
        for lab in ('BLEND_Additive', 'BLEND_Modulate', 'BLEND_Weird'):
            d = ir.decide_mod_parent(mod_parent(reader_c={'readable': True, 'blend': lab}))
            self.assertFalse(d['proven'], lab)
        self.assertIsNone(ir.blend_group('BLEND_Additive'))

    def test_translucent_ok(self):
        self.assertTrue(ir.decide_mod_parent(mod_parent(reader_c={'readable': True, 'blend': 'BLEND_Translucent'}))['proven'])

    def test_one_source_name_only_opaque(self):
        d = ir.decide_mod_parent(mod_parent(reader_c={'readable': True, 'blend': 'BLEND_Opaque'}))
        self.assertFalse(d['proven'])
        self.assertIn('opaque', d['why'].lower())

    def test_one_source_blend_only(self):
        d = ir.decide_mod_parent(mod_parent(asset_name='Simon_Slot0', package='/Game/X/Simon_Slot0'))
        self.assertFalse(d['proven'])
        self.assertIn('source 1', d['why'])

    def test_blend_unreadable(self):
        d = ir.decide_mod_parent(mod_parent(reader_c={'readable': False, 'why': 'x'}))
        self.assertFalse(d['proven'])
        self.assertIn('source 2', d['why'])

    def test_readers_ab_contradict(self):
        d = ir.decide_mod_parent(mod_parent(readers_ab={'readable': True, 'blend_group': 'opaque'}))
        self.assertFalse(d['proven'])

    def test_reader_a_only_opaque(self):
        d = ir.decide_mod_parent(mod_parent(readers_ab={'readable': False, 'reader_a_classes': ['MD_Surface/opaque']}))
        self.assertFalse(d['proven'])

    def test_texture_import_refused(self):
        d = ir.decide_mod_parent(mod_parent(imports=['/Game/X/T_Mask']))
        self.assertFalse(d['proven'])
        self.assertIn('constant graph', d['why'])

    def test_parameter_name_refused(self):
        d = ir.decide_mod_parent(mod_parent(names=['M_Invisible', '/Game/X/Mats/M_Invisible', 'Opacity']))
        self.assertFalse(d['proven'])

    def test_overrides_base_refused(self):
        self.assertFalse(ir.decide_mod_parent(mod_parent(overrides_base='base.utoc'))['proven'])

    def test_instance_or_has_shaders_refused(self):
        self.assertFalse(ir.decide_mod_parent(mod_parent(audit_class='MaterialInstanceConstant'))['proven'])
        self.assertFalse(ir.decide_mod_parent(mod_parent(audit_verdict='OK'))['proven'])


class GameTarget(unittest.TestCase):
    def test_positive(self):
        d = ir.decide_game_target(game_target())
        self.assertTrue(d['proven'], d)
        for k in ('name', 'asset_registry', 'own_property_block', 'skeletal_use', 'skeletal_usage_flag'):
            self.assertIn(k, d['sources'])

    def test_one_source_name_only_opaque(self):
        t = game_target()
        t['two_source_class'] = dict(t['two_source_class'], blend='BLEND_Opaque')
        t['reader_c'] = dict(t['reader_c'], blend='BLEND_Opaque')
        self.assertFalse(ir.decide_game_target(t)['proven'])

    def test_one_source_blend_only(self):
        d = ir.decide_game_target(game_target(asset_name='M_Cartilage', package='/Game/C/M_Cartilage'))
        self.assertFalse(d['proven'])
        self.assertIn('source 1', d['why'])

    def test_sources_disagree(self):
        d = ir.decide_game_target(game_target(reader_c={'readable': True, 'blend': 'BLEND_Opaque'}))
        self.assertFalse(d['proven'])
        self.assertIn('disagree', d['why'])

    def test_two_source_class_unreadable(self):
        self.assertFalse(ir.decide_game_target(game_target(two_source_class={'readable': False, 'why': 'x'}))['proven'])

    def test_reader_c_unreadable(self):
        self.assertFalse(ir.decide_game_target(game_target(reader_c={'readable': False, 'why': 'x'}))['proven'])

    def test_store_and_header_gates(self):
        self.assertFalse(ir.decide_game_target(game_target(shader_maps=0))['proven'])
        self.assertFalse(ir.decide_game_target(game_target(overridden_by_mod=True))['proven'])
        self.assertFalse(ir.decide_game_target(game_target(header_whole_verified=False))['proven'])
        self.assertFalse(ir.decide_game_target(game_target(registry_class='MaterialInstanceConstant'))['proven'])

    def test_not_surface(self):
        t = game_target()
        t['two_source_class'] = dict(t['two_source_class'], domain='MD_PostProcess')
        self.assertFalse(ir.decide_game_target(t)['proven'])

    def test_skeletal_one_source(self):
        self.assertFalse(ir.decide_game_target(game_target(skeletal_users=[]))['proven'])
        self.assertFalse(ir.decide_game_target(game_target(usage_flag={'index': 53, 'present': False}))['proven'])
        self.assertFalse(ir.decide_game_target(game_target(usage_flag={'index': None, 'why': 'ambiguous'}))['proven'])

    def test_texture_import_refused(self):
        self.assertFalse(ir.decide_game_target(game_target(imports=['/Game/C/T_Mask']))['proven'])

    def test_flag_unstable_refused(self):
        d = ir.decide_game_target(game_target(usage_flag={'index': 53, 'present': True, 'global_index': 54}))
        self.assertFalse(d['proven'])
        self.assertIn('not stable', d['why'])

    def test_flag_unreadable_block_refused(self):
        d = ir.decide_game_target(game_target(usage_flag={'index': 53, 'present': False,
                                                          'present_why': 'property header unreadable'}))
        self.assertFalse(d['proven'])
        self.assertIn('unreadable', d['why'])

    def test_additive_target_refused(self):
        t = game_target()
        t['two_source_class'] = dict(t['two_source_class'], blend='BLEND_Additive')
        t['reader_c'] = dict(t['reader_c'], blend='BLEND_Additive')
        self.assertFalse(ir.decide_game_target(t)['proven'])

    def test_rank(self):
        a = dict(ir.decide_game_target(game_target()), export='M_Hide')
        b = dict(ir.decide_game_target(game_target(package='/Game/B/M_Hidden', asset_name='M_Hidden',
                                                   skeletal_users=['/a', '/b'])), export='M_Hidden')
        self.assertEqual(ir.rank_targets([a, b])[0]['package'], '/Game/B/M_Hidden')
        self.assertEqual(ir.rank_targets([dict(a, proven=False)]), [])


class UsageFlag(unittest.TestCase):
    def train(self):
        rows = [('sk%d' % i, {12, 53, 140}, {'SkeletalMesh'}) for i in range(12)]
        rows += [('st%d' % i, {12, 140}, {'StaticMesh'}) for i in range(8)]
        rows += [('st_flag', {53, 140}, {'StaticMesh'})]
        return rows

    def test_learns_one_index(self):
        self.assertEqual(ir.learn_usage_flag(self.train())['index'], 53)

    def test_ambiguous_refused(self):
        rows = [(p, s | {99} if 'SkeletalMesh' in u else s, u) for p, s, u in self.train()]
        self.assertIsNone(ir.learn_usage_flag(rows)['index'])

    def test_one_below_min_absent(self):
        rows = [('sk%d' % i, {12, 53, 140}, {'SkeletalMesh'}) for i in range(12)]
        rows += [('st%d' % i, {12, 140}, {'StaticMesh'}) for i in range(ir.MIN_ST_ABSENT - 1)]
        rows += [('stf%d' % i, {12, 53, 140}, {'StaticMesh'}) for i in range(6)]
        self.assertIsNone(ir.learn_usage_flag(rows)['index'])
        rows.append(('st_last', {12, 140}, {'StaticMesh'}))
        r = ir.learn_usage_flag(rows)
        self.assertEqual((r['index'], r['static_only_absent']), (53, ir.MIN_ST_ABSENT))

    def test_too_few(self):
        self.assertIsNone(ir.learn_usage_flag(self.train()[3:12] + self.train()[12:])['index'])

    def test_leave_one_out(self):
        rows = self.train()
        rows[0] = ('cand', {12, 140}, {'SkeletalMesh'})     # candidate lacks the flag: would break learning
        self.assertIsNone(ir.learn_usage_flag(rows)['index'])
        self.assertEqual(ir.learn_usage_flag(rows, exclude={'cand'})['index'], 53)

    def test_flag_present_everywhere_is_not_a_flag(self):
        rows = [(p, s | {53}, u) for p, s, u in self.train()]
        self.assertIsNone(ir.learn_usage_flag(rows)['index'])


class Users(unittest.TestCase):
    def cls(self, n):
        return {'SK_A': 'SkeletalMesh', 'SK_B': 'SkeletalMesh', 'MI_X': 'MaterialInstanceConstant',
                'SM_C': 'StaticMesh', 'BP_D': 'Blueprint'}.get(n)

    def test_overridden_importer_dropped(self):
        u = ir.effective_users('MI_T', {'MI_T': {'SK_A'}}, {'SK_A': set()}, self.cls, {'SK_A'})
        self.assertEqual(sum(u.values()), 0)

    def test_mod_importer_counted_and_chains_walked(self):
        names = {}
        u = ir.effective_users('MI_T', {'MI_T': {'MI_X'}, 'MI_X': {'SM_C'}}, {'SK_B': {'MI_T'}}, self.cls, set(),
                               names=names)
        self.assertEqual(u, collections.Counter({'SkeletalMesh': 1, 'StaticMesh': 1}))
        self.assertEqual(names['StaticMesh'], ['SM_C'])

    def test_unknown_class_counted(self):
        u = ir.effective_users('MI_T', {}, {'NEW_MESH': {'MI_T'}}, self.cls, set())
        self.assertEqual(u, collections.Counter({'unknown': 1}))

    def test_cycle_terminates(self):
        cls = lambda n: 'MaterialInstanceConstant' if n.startswith('MI') else 'SkeletalMesh'  # noqa: E731
        st = {}
        u = ir.effective_users('MI_T', {'MI_T': {'MI_A'}, 'MI_A': {'MI_B'}, 'MI_B': {'MI_A', 'SK'}}, {}, cls, set(),
                               state=st)
        self.assertEqual(u, collections.Counter({'SkeletalMesh': 1}))
        self.assertFalse(st['truncated'])

    def test_depth_truncation_reported(self):
        cls = lambda n: 'MaterialInstanceConstant' if n.startswith('MI') else 'SkeletalMesh'  # noqa: E731
        chain = {'MI_%d' % i: {'MI_%d' % (i + 1)} for i in range(12)}
        chain['MI_12'] = {'SK'}
        st = {}
        ir.effective_users('MI_0', chain, {}, cls, set(), state=st)
        self.assertTrue(st['truncated'])

    def test_overridden_importer_that_still_imports_counts_once(self):
        u = ir.effective_users('MI_T', {'MI_T': {'SK_A'}}, {'SK_A': {'MI_T'}}, self.cls, {'SK_A'})
        self.assertEqual(u, collections.Counter({'SkeletalMesh': 1}))


class Rows(unittest.TestCase):
    def test_all_skeletal_reparent(self):
        parents, t = proven()
        new, why = ir.decide_row(plan_row(), {'users': {'SkeletalMesh': 2}}, parents, t)
        self.assertIsNone(why)
        self.assertEqual(new['action'], 'reparent')
        self.assertEqual(new['coverage'], 1.0)
        for k in ('mod_package', 'old_parent', 'new_parent', 'new_export', 'param_renames', 'store_entry_repoint',
                  'mod_class'):
            self.assertIn(k, new)
        self.assertFalse(new['mod_class'].get('unproven_opaque'))
        self.assertEqual(new['store_entry_repoint'], {'package_id': '1111111111111111',
                                                      'from_package_id': '2222222222222222',
                                                      'to_package_id': '3333333333333333'})
        self.assertNotIn('_ids', new)
        self.assertEqual(new['new_parent'], '/Game/C/M_Hide')

    def test_mixed_users_partial(self):
        parents, t = proven()
        new, why = ir.decide_row(plan_row(), {'users': {'SkeletalMesh': 1, 'StaticMesh': 3}}, parents, t)
        self.assertEqual(new['action'], 'reparent-partial')
        self.assertEqual(new['coverage'], 1.0)
        self.assertEqual(new['user_coverage'], 0.25)
        self.assertIn('allow-partial', new['partial_reason'])
        self.assertTrue(any('default' in n for n in new['look_notes']))

    def test_no_users(self):
        parents, t = proven()
        new, why = ir.decide_row(plan_row(), {'users': {}}, parents, t)
        self.assertIsNone(new)
        self.assertTrue(why.startswith('no run-time user'))

    def test_non_skeletal_users_refused(self):
        parents, t = proven()
        new, why = ir.decide_row(plan_row(), {'users': {'Blueprint': 1, 'NiagaraSystem': 2}}, parents, t)
        self.assertIsNone(new)
        self.assertIn('SkeletalMesh', why)

    def test_refusals(self):
        parents, t = proven()
        sk = {'users': {'SkeletalMesh': 1}}
        cases = [plan_row(params=['Opacity']), plan_row(action='reparent'), plan_row(**{'class': 'Material'}),
                 plan_row(old_parent='/Game/X/M_Other'), plan_row(verdict='NO_TARGET_SHADERS')]
        for r in cases:
            self.assertIsNone(ir.decide_row(r, sk, parents, t)[0], r)
        self.assertIsNone(ir.decide_row(plan_row(), {'users': {'SkeletalMesh': 1}, 'own_shader_tokens': 3},
                                        parents, t)[0])
        self.assertIsNone(ir.decide_row(plan_row(), sk, parents, None)[0])
        bad = {'/Game/X/Mats/M_Invisible': ir.decide_mod_parent(mod_parent(reader_c={'readable': True,
                                                                                      'blend': 'BLEND_Opaque'}))}
        self.assertIsNone(ir.decide_row(plan_row(), sk, bad, t)[0])

    def test_missing_ids_refused(self):
        parents, t = proven()
        new, why = ir.decide_row(plan_row(_ids={}), {'users': {'SkeletalMesh': 1}}, parents, t)
        self.assertIsNone(new)
        self.assertIn('ids', why)

    def test_truncated_users_refused(self):
        parents, t = proven()
        new, why = ir.decide_row(plan_row(), {'users': {'SkeletalMesh': 1}, 'users_truncated': True}, parents, t)
        self.assertIsNone(new)

    def test_target_blend_group_must_match_parent(self):
        parents, t = proven()                       # parent masked
        tr = dict(t, package='/Game/C/M_HiddenGlow', blend='BLEND_Translucent', skeletal_users=['a', 'b', 'c'])
        new, why = ir.decide_row(plan_row(), {'users': {'SkeletalMesh': 1}}, parents, [tr])
        self.assertIsNone(new)
        self.assertIn('same blend group', why)
        new, why = ir.decide_row(plan_row(), {'users': {'SkeletalMesh': 1}}, parents, [tr, t])
        self.assertEqual(new['new_parent'], '/Game/C/M_Hide')


REAL = all(os.path.exists(p) for p in (TEAM_A_TOOLS, TEAM_A_PLAN, WHAT + '/outputs/2026-10-08-l25-prep/evidence'))


def real_args(plan, out, extra=()):
    EV = WHAT + '/outputs/2026-10-08-l25-prep/evidence'
    F = EV + '/cap12-fetched-20261013'
    X = WHAT + '/.superpowers/native-dependency-spike/unreal-config-main-pak-trial-1/trial-1/extracted/Townfall'
    L = WHAT + '/.superpowers/native-observations/leon-real-mod-intake-2026-10-01/extracted/'
    return ['--tools-dir', TEAM_A_TOOLS, '--plan', plan, '--audit', EV + '/cap12-20261012/leon-material-audit-5hdr.json',
            '--mod-utoc', L + 'zzz_Leon_Townfall_2K_P.utoc', '--mod-ucas', L + 'zzz_Leon_Townfall_2K_P.ucas',
            '--checked-headers'] + [EV + '/cap12-20261012/round%d-check.json' % i for i in (1, 2, 3)] + \
           ['--fetched-dir'] + [F + '/headers-r%d' % i for i in (1, 2, 3)] + \
           ['--base-header'] + [F + '/manifests/pakchunk%s-ps5.container-header.bin' % c for c in ('0', '1', '10', '11', '12')] + \
           ['--asset-registry', X + '/AssetRegistry.bin',
            '--library', F + '/manifests/Townfall_Chunk0.type8.bin', F + '/manifests/Global.type8.bin',
            '--stinfo', X + '/Content/ShaderTypeInfo-Townfall_Chunk0-SF_PS5-SP_PS5.stinfo',
            X + '/Content/ShaderTypeInfo-Global-SF_PS5-SP_PS5.stinfo',
            '--script-utoc', EV + '/console-pulls-20261013-paks2/global.utoc',
            '--script-ucas', EV + '/console-pulls-20261013-paks2/global.ucas', '--out', out] + list(extra)


@unittest.skipUnless(REAL, 'real Leon / game files not on this Mac')
class RealLeon(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix='cap12-invisible-')
        cls.out = os.path.join(cls.tmp, 'plan.json')
        cls.code = ir.main(real_args(TEAM_A_PLAN, cls.out, ['--apply']))
        cls.plan = json.load(open(cls.out))
        cls.rep = cls.plan['invisible_route']

    def test_exit_and_counts(self):
        self.assertEqual(self.code, 0)
        self.assertEqual(self.rep['game_materials_scanned'], 418)
        self.assertGreater(self.rep['usage_flag_training_rows'], 0)
        res = collections.Counter(r['result'] for r in self.rep['rows'])
        self.assertEqual((res['reparent'], res['reparent-partial']), (10, 4), res)

    def test_target_is_the_game_hide_material(self):
        self.assertEqual(self.rep['chosen_target'], '/Game/MetaHumans/Common/Materials/M_Hide')
        self.assertEqual(sum(t['proven'] for t in self.rep['targets']), 1)

    def test_mod_parents(self):
        p = {x['package'].rsplit('/', 1)[-1] + ':' + x['package'].split('/')[2]: x for x in self.rep['mod_parents']}
        self.assertTrue(p['M_Invisible:MetaHumans']['proven'])
        self.assertTrue(p['M_Invisible:Townfall']['proven'])
        for k, v in p.items():
            if k.startswith('Simon_Slot'):
                self.assertFalse(v['proven'], k)

    def test_simon_slot_rows_stay_checker(self):
        for r in self.plan['rows']:
            if '/Simon_Slot' in r['mod_package']:
                self.assertEqual(r['action'], 'known-checker')

    def test_rows_fit_the_overlay(self):
        sys.path.insert(0, TEAM_A_TOOLS)
        import cityhash  # noqa: E402
        conv = [r for r in self.plan['rows'] if r.get('route') == 'invisible']
        self.assertEqual(len(conv), 14)
        for r in conv:
            rep = r['store_entry_repoint']
            self.assertEqual(rep['package_id'], '%016x' % cityhash.ue_name_hash(r['mod_package']))
            self.assertEqual(rep['from_package_id'], '%016x' % cityhash.ue_name_hash(r['old_parent']))
            self.assertEqual(rep['to_package_id'], '%016x' % cityhash.ue_name_hash(r['new_parent']))
            self.assertIn(r['action'], ('reparent', 'reparent-partial'))
            self.assertTrue(r['old_parent'].endswith('/M_Invisible'))

    def test_mic_reparent_accepts_one_row(self):
        sys.path.insert(0, TEAM_A_TOOLS)
        import cityhash  # noqa: E402
        import mod_material_audit as mma  # noqa: E402
        import utoc  # noqa: E402
        r = [x for x in self.plan['rows'] if x.get('route') == 'invisible'][0]
        L = WHAT + '/.superpowers/native-observations/leon-real-mod-intake-2026-10-01/extracted/'
        t = utoc.load(L + 'zzz_Leon_Townfall_2K_P.utoc')
        cid = cityhash.ue_name_hash(r['mod_package']).to_bytes(8, 'little') + bytes.fromhex('00000001')
        inp = os.path.join(self.tmp, 'in.bin')
        with open(inp, 'wb') as fh:
            fh.write(t.read_chunk(t.chunk_ids.index(cid), mma.reader(L + 'zzz_Leon_Townfall_2K_P.ucas')))
        hdr = os.path.join(WHAT, 'outputs/2026-10-08-l25-prep/evidence/cap12-fetched-20261013/headers-r2',
                           '%016x.hdr.bin' % cityhash.ue_name_hash(r['new_parent']))
        out = os.path.join(self.tmp, 'derived.bin')
        pr = subprocess.run([sys.executable, '-I', os.path.join(TEAM_A_TOOLS, 'mic_reparent.py'), '--in', inp,
                             '--old-parent', r['old_parent'], '--new-parent', r['new_parent'], '--new-export',
                             r['new_export'], '--base-header', hdr, '--out', out], capture_output=True, text=True)
        self.assertEqual(pr.returncode, 0, pr.stdout + pr.stderr)
        rec = json.load(open(out + '.receipt.json'))
        self.assertTrue(rec['export_bytes_identical'])

    def test_dry_run_keeps_rows(self):
        out = os.path.join(self.tmp, 'dry.json')
        self.assertEqual(ir.main(real_args(TEAM_A_PLAN, out)), 0)
        d = json.load(open(out))
        self.assertEqual(d['rows'], json.load(open(TEAM_A_PLAN))['rows'])

    def test_zero_rows_gate_fails_and_writes_nothing(self):
        empty = os.path.join(self.tmp, 'empty-plan.json')
        with open(empty, 'w') as fh:
            json.dump({'rows': []}, fh)
        out = os.path.join(self.tmp, 'o.json')
        self.assertEqual(ir.main(real_args(empty, out)), 1)
        self.assertFalse(os.path.exists(out))

    def test_zero_mod_materials_gate_fails(self):
        audit = os.path.join(self.tmp, 'empty-audit.json')
        with open(audit, 'w') as fh:
            json.dump({'materials': []}, fh)
        args = real_args(TEAM_A_PLAN, os.path.join(self.tmp, 'o2.json'))
        args[args.index('--audit') + 1] = audit
        self.assertEqual(ir.main(args), 1)
        self.assertFalse(os.path.exists(os.path.join(self.tmp, 'o2.json')))

    def test_usage_flag_reported(self):
        self.assertEqual(self.rep['usage_flag']['index'], 53)
        self.assertTrue(self.rep['mod_classes_from_script_objects'])

    def test_derive_substitutes_accepts_the_rows(self):
        import shutil
        work = tempfile.mkdtemp(prefix='cap12-derive-')
        mf = os.path.join(work, 'mf')
        shutil.copytree(TEAM_A_TOOLS, mf, ignore=shutil.ignore_patterns('__pycache__', 'tools-orig'))
        ov = os.path.join(work, 'derive_substitutes.py')
        shutil.copy(os.path.join(TEAM_A_TOOLS, 'derive_substitutes.py'), ov)
        L25 = WHAT + '/outputs/2026-10-08-l25-prep'
        F = L25 + '/evidence/cap12-fetched-20261013'
        L = WHAT + '/.superpowers/native-observations/leon-real-mod-intake-2026-10-01/extracted/'
        out = os.path.join(work, 'out')
        cmd = [sys.executable, '-I', ov, '--plan', self.out, '--mod-utoc', L + 'zzz_Leon_Townfall_2K_P.utoc',
               '--mod-ucas', L + 'zzz_Leon_Townfall_2K_P.ucas', '--mirror', L25 + '/pak-mirror-townfall-20261013',
               '--headers'] + [F + '/headers-r%d' % i for i in (1, 2, 3)] + ['--base-store'] + \
              [F + '/manifests/pakchunk%s-ps5.container-header.bin' % c for c in ('0', '1', '10', '11', '12')] + \
              ['--material-fallback', mf, '--allow-partial', '--accept-unproven-opaque',
               '--tiling-reference', L25 + '/pak-intake', '--out', out]
        pr = subprocess.run(cmd, capture_output=True, text=True, timeout=1500)
        self.assertEqual(pr.returncode, 0, (pr.stdout + pr.stderr)[-1500:])
        man = [os.path.join(out, x) for x in os.listdir(out) if x.endswith('.json') and 'manifest' in x]
        self.assertTrue(man, os.listdir(out))
        rows = json.load(open(man[0]))['rows']
        inv = [r for r in rows if r['reparent']['new_parent'].endswith('/M_Hide')]
        self.assertEqual(len(inv), 14)
        self.assertEqual(sorted({r['action'] for r in inv}), ['reparent', 'reparent-partial'])
        shutil.rmtree(work)


if __name__ == '__main__':
    suite = unittest.defaultTestLoader.loadTestsFromModule(sys.modules[__name__])
    res = unittest.TextTestRunner(verbosity=1).run(suite)
    ran = res.testsRun - len(res.skipped)
    print(f'test_invisible_route: {ran} tests ran, {len(res.failures) + len(res.errors)} failed, '
          f'{len(res.skipped)} skipped')
    sys.exit(0 if res.wasSuccessful() and ran > 0 else 1)
