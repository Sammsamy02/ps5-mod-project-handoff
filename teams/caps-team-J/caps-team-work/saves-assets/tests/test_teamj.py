#!/usr/bin/env python3
"""Host tests for Team J's tools (Mac-only). Run: python3 -I tests/test_teamj.py
Real-data tests use the game's own files and unchanged PC mod files on this Mac (read-only) and are
skipped when those files are absent; synthetic tests always run."""
import json
import os
import struct
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.join(os.path.dirname(HERE), 'tools')
sys.dont_write_bytecode = True
sys.path.insert(0, TOOLS)
import asset_rules as AR  # noqa: E402
import classify_mod  # noqa: E402
import gvas  # noqa: E402
import jlib  # noqa: E402
import make_sample_plan  # noqa: E402
import pair_compare  # noqa: E402

W = jlib.WHAT
P = W + '/outputs/2026-10-08-l25-prep'
M = P + '/pak-mirror-townfall-20261013'
G = P + '/evidence/console-pulls-20261013-paks2'
F = P + '/evidence/cap08-base-fetched-20261013'
LEON = W + '/.superpowers/native-observations/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P'
REG = W + ('/.superpowers/native-dependency-spike/unreal-config-pak-composition-1/trial-1/independent-extraction/'
           'Townfall/AssetRegistry.bin')
TOCS = [f'{M}/pakchunk{n}-ps5.utoc' for n in (0, 1, 10, 11, 12)]
Q2 = W + '/.superpowers/native-dependency-spike/iostore-three-archive-composition-1/quarantine-2'
HDRS = [W + '/.superpowers/native-observations/townfall-original-container-header-1/container-header.bin',
        F + '/pakchunk1-ps5/87608d0ea4cfc0f400000006.bin', F + '/pakchunk10-ps5/5b160bf69b8b988d00000006.bin',
        F + '/pakchunk11-ps5/f1635ac7b843b33400000006.bin', F + '/pakchunk12-ps5/d656cdde5c61998000000006.bin']
HAVE_REAL = all(os.path.exists(p) for p in [LEON + '.utoc', LEON + '.ucas', G + '/global.utoc', REG] + TOCS + HDRS)

_CACHE = {}
SK_SAME = {'/Script/Engine/SkeletalMesh': {'shape': 'same-shape', 'ps5_samples': 25, 'reasons': [],
                                           'console_bulk_signatures': [['0x0']]}}
SP_SAME = {'/Script/Engine/SubsurfaceProfile': {'shape': 'same-shape', 'ps5_samples': 2, 'reasons': [],
                                                'console_bulk_signatures': [[]]}}


def real():
    if 'R' not in _CACHE:
        R = jlib.Readers()
        _CACHE['R'] = R
        _CACHE['so'] = jlib.script_objects(R, G + '/global.utoc', G + '/global.ucas')
        _CACHE['base'] = jlib.base_index(R, TOCS)
        _CACHE['store'] = jlib.base_store(R, HDRS)
    return _CACHE['R'], _CACHE['so'], _CACHE['base'], _CACHE['store']


def leon_package(R, suffix):
    C = jlib.Container(R, LEON + '.utoc', LEON + '.ucas')
    for row, vpath, pid, data in C.packages():
        if vpath.endswith(suffix):
            return pid, vpath, data
    raise AssertionError(suffix)


def ctx(base, pair_ev=None, registry=None, mod_pids=()):
    return {'targets': {'SF_PS5', 'SP_PS5'}, 'mod_pids': set(mod_pids), 'base_ids': base, 'mat_by_pkg': {},
            'registry': registry, 'pair_ev': pair_ev}


class Rules(unittest.TestCase):
    def test_worst_order(self):
        self.assertEqual(AR.worst([AR.AS_IS, AR.CONVERT, AR.LIKELY_AS_IS]), AR.CONVERT)
        self.assertEqual(AR.worst([AR.CONVERT, AR.UNKNOWN]), AR.UNKNOWN)
        self.assertEqual(AR.worst([AR.UNKNOWN, AR.UNSUPPORTED]), AR.UNSUPPORTED)
        self.assertEqual(AR.worst([]), AR.UNKNOWN)

    def test_rule_table_consistent(self):
        self.assertGreater(len(AR.RULES), 50)
        for c, r in AR.RULES.items():
            self.assertTrue(c.startswith('/Script/'), c)
            self.assertIn(r['verdict'], AR.SEVERITY)
            if r['converter']:
                self.assertIn(r['converter'], AR.CONVERTERS)
        for c, (v, _why) in AR.SUB_RULES.items():
            self.assertIn(v, AR.SEVERITY)

    def test_registry_class(self):
        self.assertEqual(classify_mod.registry_class('/Script/Engine.Texture2D'), '/Script/Engine/Texture2D')
        self.assertEqual(classify_mod.registry_class('/Script/Engine.Blueprint'), '/Script/Engine/BlueprintGeneratedClass')
        self.assertIsNone(classify_mod.registry_class('/Game/A/B.B_C'))


class Tokens(unittest.TestCase):
    def test_shader_tokens(self):
        blob = b'\x0a\x00\x00\x00PCD3D_SM6\x00junk\x07\x00\x00\x00SP_PS5\x00 SP_Leon_Skin\x00 XYZ_ABC\x00'
        self.assertEqual(jlib.shader_tokens(blob), ['PCD3D_SM6', 'SP_PS5'])

    def test_pak_footer_synthetic(self):
        mount = b'../../../\0'
        index = struct.pack('<i', len(mount)) + mount + struct.pack('<i', 2) + b'\0' * 16
        foot = b'\0' * 16 + b'\0' + struct.pack('<IIqq', 0x5A6F12E1, 11, 0, len(index)) + b'\0' * 20 + b'\0' * 160
        with tempfile.TemporaryDirectory() as d:
            p = os.path.join(d, 'x.pak')
            with open(p, 'wb') as fh:
                fh.write(index + foot)
            self.assertEqual(jlib.read_pak_entry_count(p), (11, 2, '../../../'))
            with open(p, 'wb') as fh:
                fh.write(index + foot.replace(struct.pack('<I', 0x5A6F12E1), b'\0\0\0\0'))
            with self.assertRaises(jlib.JError):
                jlib.read_pak_entry_count(p)


class Shape(unittest.TestCase):
    def fp(self, **k):
        d = {'flags': '0x80002200', 'bulk_flags': [], 'bulk_entries': 0, 'shader_tokens': [],
             'export_slices': 'tiled', 'unresolved_script_imports': [], 'package': 'p'}
        d.update(k)
        return d

    def test_same_and_one_side(self):
        t = {'SF_PS5', 'SP_PS5'}
        self.assertEqual(pair_compare.shape([self.fp()], [self.fp()], t)[0], 'same-shape')
        self.assertEqual(pair_compare.shape([self.fp()], [], t)[0], 'one-side-only')

    def test_platform_layers(self):
        t = {'SF_PS5', 'SP_PS5'}
        v, why = pair_compare.shape([self.fp(texture_form='linear')], [self.fp(texture_form='console-tiled')], t)
        self.assertEqual(v, 'platform-specific')
        v, _ = pair_compare.shape([self.fp(shader_tokens=['PCD3D_SM6'])], [self.fp(shader_tokens=['SP_PS5'])], t)
        self.assertEqual(v, 'platform-specific')

    def test_per_sample_signatures_not_unions(self):
        t = {'SF_PS5', 'SP_PS5'}
        pcs = [self.fp(bulk_flags=['0x48', '0x10501'])]
        ps5 = [self.fp(bulk_flags=['0x48']), self.fp(bulk_flags=['0x10501'])]
        self.assertEqual(pair_compare.shape(pcs, ps5, t)[0], 'differs')

    def test_unreadable_structure_is_not_same_shape(self):
        t = {'SF_PS5', 'SP_PS5'}
        self.assertEqual(pair_compare.shape([self.fp(bulk_entries=None)], [self.fp()], t)[0], 'differs')
        self.assertEqual(pair_compare.shape([self.fp()], [self.fp(export_slices='error: x')], t)[0], 'differs')

    def test_decoder_required(self):
        t = {'SF_PS5', 'SP_PS5'}
        ok, bad = {'ok': True}, {'ok': False, 'why': 'x'}
        self.assertEqual(pair_compare.shape([self.fp(skeletal_decode=ok)], [self.fp(skeletal_decode=ok)], t,
                                            need_decoder=True)[0], 'same-shape')
        self.assertEqual(pair_compare.shape([self.fp(skeletal_decode=bad)], [self.fp(skeletal_decode=ok)], t,
                                            need_decoder=True)[0], 'differs')
        self.assertEqual(pair_compare.shape([self.fp()], [self.fp()], t, need_decoder=True)[0], 'differs')

    def test_differs(self):
        t = {'SF_PS5', 'SP_PS5'}
        v, why = pair_compare.shape([self.fp(flags='0x80000200')], [self.fp()], t)
        self.assertEqual(v, 'differs')
        v, why = pair_compare.shape([self.fp(bulk_flags=['0x0'])], [self.fp()], t)
        self.assertEqual(v, 'differs')


# ---------- GVAS synthetic files (written to the public layout the reader implements) ----------
def fstr(s):
    b = s.encode() + b'\0'
    return struct.pack('<i', len(b)) + b


def header(ue5=1014, cls='/Script/Game.TestSave', v=3, magic=gvas.MAGIC):
    out = struct.pack('<Ii', magic, v)
    out += struct.pack('<ii', 522, ue5) if v >= 3 else struct.pack('<i', 522)
    out += struct.pack('<HHHI', 5, 6, 1, 0) + fstr('++UE5+Release-5.6')
    if v >= 2:
        out += struct.pack('<ii', 3, 1) + b'\x11' * 16 + struct.pack('<i', 7)
    return out + fstr(cls)


def tree_bytes(t):
    return fstr(t[0]) + struct.pack('<i', len(t[1])) + b''.join(tree_bytes(k) for k in t[1])


def tag_new(name, tree, value, flags=0):
    return fstr(name) + tree_bytes(tree) + struct.pack('<i', len(value)) + bytes([flags]) + value


def tag_old(name, typ, value, extras=b'', ue5=1010):
    return (fstr(name) + fstr(typ) + struct.pack('<ii', len(value), 0) + extras + b'\0'
            + (b'\0' if ue5 >= gvas.UE5_EXTENSIONS else b'') + value)


END = fstr('None')


class Gvas(unittest.TestCase):
    def new_save(self):
        inner = tag_new('X', ['IntProperty', []], struct.pack('<i', 3)) + END
        body = (tag_new('Level', ['IntProperty', []], struct.pack('<i', 7))
                + tag_new('Name', ['StrProperty', []], fstr('Simon'))
                + tag_new('Alive', ['BoolProperty', []], b'', flags=0x10)
                + tag_new('Items', ['ArrayProperty', [['IntProperty', []]]], struct.pack('<ii', 1, 5))
                + tag_new('Pos', ['StructProperty', [['Vector', []], ['/Script/CoreUObject', []]]], b'\0' * 24,
                          flags=0x08)
                + tag_new('Inner', ['StructProperty', [['S_Inner', []]]], inner))
        return header() + body + END + struct.pack('<I', 0)

    def test_new_format_roundtrip(self):
        s = gvas.read_save(self.new_save())
        self.assertEqual(s['header']['save_game_class'], '/Script/Game.TestSave')
        self.assertEqual(s['header']['engine']['minor'], 6)
        props = {p['name']: p for p in s['properties']}
        self.assertEqual(props['Level']['value'], 7)
        self.assertEqual(props['Name']['value'], 'Simon')
        self.assertIs(props['Alive']['value'], True)
        self.assertEqual(props['Items']['type_str'], 'ArrayProperty(IntProperty)')
        self.assertEqual(props['Pos']['type_str'], 'StructProperty(Vector,/Script/CoreUObject)')
        self.assertEqual(props['Inner']['fields'][0]['value'], 3)

    def test_old_format(self):
        ue5 = 1010
        body = (tag_old('Level', 'IntProperty', struct.pack('<i', 9), ue5=ue5)
                + tag_old('On', 'BoolProperty', b'', extras=b'\x01', ue5=ue5)
                + tag_old('Pos', 'StructProperty', b'\0' * 24, extras=fstr('Vector') + b'\0' * 16, ue5=ue5))
        s = gvas.read_save(header(ue5=ue5) + body + END + struct.pack('<I', 0))
        props = {p['name']: p for p in s['properties']}
        self.assertEqual(props['Level']['value'], 9)
        self.assertIs(props['On']['value'], True)
        self.assertEqual(props['Pos']['type_str'], 'StructProperty(Vector)')

    def test_refusals(self):
        good = self.new_save()
        for bad, why in ((header(magic=0x12345678) + END + b'\0' * 4, 'magic'),
                         (header(v=4) + END + b'\0' * 4, 'version'),
                         (good[:-9], 'truncated'),
                         (good + b'\0', 'trailing'),
                         (good[:-4] + struct.pack('<I', 1), 'guid')):
            with self.assertRaises(gvas.SaveError, msg=why):
                gvas.read_save(bad)
        ext = header() + fstr('A') + tree_bytes(['IntProperty', []]) + struct.pack('<i', 4) + bytes([0x04, 0x02]) \
            + struct.pack('<i', 1) + END + struct.pack('<I', 0)
        with self.assertRaises(gvas.SaveError):
            gvas.read_save(ext)
        skipped = header() + tag_new('A', ['IntProperty', []], b'', flags=0x20) + END + struct.pack('<I', 0)
        with self.assertRaises(gvas.SaveError):
            gvas.read_save(skipped)
        fatbool = header() + tag_new('B', ['BoolProperty', []], b'\x01', flags=0x10) + END + struct.pack('<I', 0)
        with self.assertRaises(gvas.SaveError):
            gvas.read_save(fatbool)

    def test_schema_check(self):
        s = gvas.read_save(self.new_save())
        schema = {'class': '/Script/Game.TestSave',
                  'properties': {p['name']: p['type_str'] for p in s['properties']}}
        self.assertTrue(gvas.schema_check(s, schema)['ok'])
        schema['properties']['Level'] = 'FloatProperty'
        r = gvas.schema_check(s, schema)
        self.assertFalse(r['ok'])
        self.assertEqual(r['type_diff'][0]['name'], 'Level')
        del schema['properties']['Name']
        self.assertIn('Name', gvas.schema_check(s, schema)['missing'])

    def test_cli_refuses_engine_mismatch(self):
        with tempfile.TemporaryDirectory() as d:
            p = os.path.join(d, 's.sav')
            with open(p, 'wb') as fh:
                fh.write(self.new_save())
            self.assertEqual(gvas.main([p, '--expect-engine', '5.6.1']), 0)
            self.assertEqual(gvas.main([p, '--expect-engine', '5.5']), 1)


class BankPlan(unittest.TestCase):
    def test_list_parse(self):
        with tempfile.TemporaryDirectory() as d:
            p = os.path.join(d, 'list.txt')
            with open(p, 'w') as fh:
                fh.write('-rwxrwxrwx 1 0 0 900000 Oct 01 16:54 Music.bank\n'
                         '-rwxrwxrwx 1 0 0 1200 Oct 01 16:54 UI.bank\n'
                         'drwxrwxrwx 2 0 0 65536 Oct 01 16:54 sub\n'
                         'lrwxrwxrwx 1 0 0 10 Oct 01 16:54 Link.bank -> /etc/x.bank\n'
                         '-rwxrwxrwx 1 0 0 10 Oct 01 16:54 ../up.bank\n'
                         '-rwxrwxrwx 1 0 0 50 Oct 01 16:54 notes.txt\n')
            plan = make_sample_plan.bank_plan(p, 65536)
            self.assertEqual(plan['banks_listed'], 2)
            self.assertEqual([c['ucas'] for c in plan['chunks']], ['UI.bank', 'Music.bank'])
            self.assertEqual(plan['chunks'][0]['physical_ranges'], [{'offset': 0, 'length': 1200}])


@unittest.skipUnless(HAVE_REAL, 'game/mod files not on this Mac')
class RealData(unittest.TestCase):
    def test_leon_classification(self):
        R, so, base, store = real()
        reg = R.asset_registry.load(REG)
        mods = [(LEON + '.utoc', LEON + '.ucas', LEON + '.pak')]
        rep = classify_mod.classify(R, so, mods, base, store, 'SF_PS5-SP_PS5', reg, None)
        self.assertEqual(rep['by_class']['/Script/Engine/SkeletalMesh'], {'UNKNOWN': 25})   # no pair evidence
        rep = classify_mod.classify(R, so, mods, base, store, 'SF_PS5-SP_PS5', reg, SK_SAME)
        self.assertEqual(rep['packages_checked'], 127)
        bc = rep['by_class']
        self.assertEqual(bc['/Script/Engine/SkeletalMesh'], {'LIKELY_AS_IS': 25})
        self.assertEqual(bc['/Script/Engine/Texture2D'], {'CONVERT': 34})
        self.assertEqual(bc['/Script/Engine/MaterialInstanceConstant'], {'CONVERT': 54})
        self.assertEqual(sum(bc['/Script/Engine/Material'].values()), 13)
        self.assertNotIn('UNSUPPORTED', rep['summary'])
        self.assertEqual(rep['converters_needed']['texture-ps5-tiling']['packages'], 34)
        self.assertEqual(rep['containers'][0]['pak']['entries'], 0)

    def _sp(self):
        R, so, base, store = real()
        pid, vpath, data = leon_package(R, 'SP_Leon_Skin.uasset')
        return R, so, base, pid, vpath, data

    def test_pure_data_needs_pair_evidence(self):
        R, so, base, pid, vpath, data = self._sp()
        z = R.zen.ZenPackage(data)
        rec = classify_mod.judge_package(R, so, z, data, None, vpath, ctx(base), pid)
        self.assertEqual(rec['verdict'], AR.UNKNOWN)
        rec = classify_mod.judge_package(R, so, z, data, None, vpath, ctx(base, SP_SAME), pid)
        self.assertEqual(rec['verdict'], AR.LIKELY_AS_IS)
        bulk_mismatch = {'/Script/Engine/SubsurfaceProfile': dict(SP_SAME['/Script/Engine/SubsurfaceProfile'],
                                                                   console_bulk_signatures=[['0x48']])}
        rec = classify_mod.judge_package(R, so, z, data, None, vpath, ctx(base, bulk_mismatch), pid)
        self.assertEqual(rec['verdict'], AR.UNKNOWN)
        diff = {'/Script/Engine/SubsurfaceProfile': {'shape': 'differs', 'ps5_samples': 2, 'reasons': ['x']}}
        rec = classify_mod.judge_package(R, so, z, data, None, vpath, ctx(base, diff), pid)
        self.assertEqual(rec['verdict'], AR.UNKNOWN)

    def test_mutations_fail_closed(self):
        R, so, base, pid, vpath, data = self._sp()
        same = SP_SAME
        # editor-only package flag
        d = bytearray(data)
        struct.pack_into('<I', d, 16, struct.unpack_from('<I', d, 16)[0] | jlib.PKG_EDITOR_ONLY)
        z = R.zen.ZenPackage(bytes(d))
        rec = classify_mod.judge_package(R, so, z, bytes(d), None, vpath, ctx(base, same), pid)
        self.assertEqual(rec['verdict'], AR.UNSUPPORTED)
        # class that the console build does not have (script import index not in ScriptObjects)
        d = bytearray(data)
        exp = struct.unpack_from('<i', d, 32)[0]
        struct.pack_into('<Q', d, exp + 32, (1 << 62) | 0x0123456789AB)
        z = R.zen.ZenPackage(bytes(d))
        rec = classify_mod.judge_package(R, so, z, bytes(d), None, vpath, ctx(base, same), pid)
        self.assertEqual(rec['verdict'], AR.UNSUPPORTED)
        self.assertTrue(any(f['check'] == 'G2' for f in rec['findings']))
        # wrong chunk id for the package name
        z = R.zen.ZenPackage(data)
        rec = classify_mod.judge_package(R, so, z, data, None, vpath, ctx(base, same), pid ^ 1)
        self.assertEqual(rec['verdict'], AR.UNSUPPORTED)
        # PC shader code injected into a data export (same length, so the package still parses)
        d = bytearray(data)
        z = R.zen.ZenPackage(data)
        e, blob = jlib.export_slices(R, z)[0]
        off = z.header_size + struct.unpack_from('<Q', d, exp)[0]
        tok = b'\0PCD3D_SM6\0'
        self.assertGreater(len(blob), len(tok) + 8)
        d[off + 4:off + 4 + len(tok)] = tok
        z = R.zen.ZenPackage(bytes(d))
        rec = classify_mod.judge_package(R, so, z, bytes(d), None, vpath, ctx(base, same), pid)
        self.assertEqual(rec['verdict'], AR.UNSUPPORTED)
        self.assertTrue(any(f['check'] == 'G5' for f in rec['findings']))

    def test_class_change_against_registry(self):
        R, so, base, pid, vpath, data = self._sp()
        same = SP_SAME

        class FakeReg:
            def asset(self, name):
                return {'class': '/Script/Engine.StaticMesh'}
        b2 = dict(base)
        b2[pid] = ('pakchunk0-ps5.utoc', 'x')
        z = R.zen.ZenPackage(data)
        rec = classify_mod.judge_package(R, so, z, data, None, vpath, ctx(b2, same, FakeReg()), pid)
        self.assertEqual(rec['verdict'], AR.UNKNOWN)
        self.assertTrue(any(f['check'] == 'G7' for f in rec['findings']))
        # overriding a game asset without a registry: the game's class is not provable -> UNKNOWN
        rec = classify_mod.judge_package(R, so, z, data, None, vpath, ctx(b2, same, None), pid)
        self.assertEqual(rec['verdict'], AR.UNKNOWN)
        self.assertTrue(any(f['check'] == 'G7' for f in rec['findings']))

    def test_uncovered_sub_object_fails_closed(self):
        R, so, base, store = real()
        pid, vpath, data = leon_package(R, 'Meshes/SK_Simon_FirstStage.uasset')
        z = R.zen.ZenPackage(data)
        leon_pids = {p for _r, _v, p, _d in jlib.Container(R, LEON + '.utoc', LEON + '.ucas').packages()}
        c = ctx(base, SK_SAME, R.asset_registry.load(REG), leon_pids)
        rec = classify_mod.judge_package(R, so, z, data, None, vpath, c, pid)
        self.assertEqual(rec['verdict'], AR.LIKELY_AS_IS)
        sm = next(k for k, v in so.objs.items() if so.path(k) == '/Script/Engine/StaticMesh')
        d = bytearray(data)
        exp = struct.unpack_from('<i', d, 32)[0]
        sub = next(i for i, e in enumerate(z.exports) if e['outer'][0] != 'Null' or
                   jlib.class_path(z, e['class'], so) != '/Script/Engine/SkeletalMesh')
        struct.pack_into('<Q', d, exp + 72 * sub + 32, sm)
        z2 = R.zen.ZenPackage(bytes(d))
        rec = classify_mod.judge_package(R, so, z2, bytes(d), None, vpath, c, pid)
        self.assertEqual(rec['verdict'], AR.UNKNOWN)
        self.assertTrue(any(f['check'] == 'G6-sub' for f in rec['findings']))

    def test_generic_save_function_flags(self):
        R, so, base, store = real()

        class FakeZ:
            def __init__(self, paths):
                idx = {so.path(k): k for k in so.objs}
                self.imports = [('ScriptImport', idx[p]) for p in paths]
                self.exports = []
        f = jlib.save_flags(FakeZ(['/Script/Engine/GameplayStatics/DeleteGameInSlot']), so)
        self.assertEqual(f, ['deletes-save-slots'])
        save_cls = sorted(p for p in (so.path(k) for k in so.objs)
                          if p and p.count('/') == 4 and 'save' in p.split('/')[3].lower()
                          and not p.split('/')[4].startswith('Default__'))[0]
        f = jlib.save_flags(FakeZ([save_cls]), so)
        self.assertTrue(f and f[0].startswith('calls-save-functions:'), f)
        self.assertEqual(jlib.save_flags(FakeZ(['/Script/Engine/GameplayStatics/GetPlayerPawn']), so), [])

    def test_control_game_container_not_flagged(self):
        R, so, base, store = real()
        mods = [(M + '/pakchunk10-ps5.utoc', M + '/pakchunk10-ps5.ucas', None)]
        rep = classify_mod.classify(R, so, mods, base, store, 'SF_PS5-SP_PS5', None, None)
        # without the registry the game's own packages are 'overrides' of unprovable class: fail closed
        self.assertEqual(rep['summary'], {'UNKNOWN': 10})
        rep = classify_mod.classify(R, so, mods, base, store, 'SF_PS5-SP_PS5', R.asset_registry.load(REG), None)
        self.assertEqual(rep['packages_checked'], 10)
        self.assertNotIn('UNSUPPORTED', rep['summary'])
        self.assertEqual(rep['by_class']['/Script/Engine/Texture2D'], {'AS_IS': 5})  # already console layout

    @unittest.skipUnless(os.path.exists(Q2 + '/member-00000001.bin'), 'third-person sample absent')
    def test_save_safety_flags(self):
        R, so, base, store = real()
        C = jlib.Container(R, Q2 + '/member-00000001.bin', Q2 + '/member-00000000.bin')
        flags = {}
        for row, vpath, pid, data in C.packages():
            z = R.zen.ZenPackage(data)
            f = jlib.save_flags(z, so)
            if f:
                flags[vpath.rsplit('/', 1)[-1]] = f
        # a PC Blueprint that saves its own slot, and its save-game class
        self.assertEqual(flags, {'BP_star.uasset': ['writes-save-slots'], 'BP_SAVE.uasset': ['defines-save-class']})

    def test_sample_plan(self):
        R, so, base, store = real()
        p = make_sample_plan.plan(R, [TOCS[0]], REG, 1, 4 << 20, 16 << 20,
                                  ['/Game/Townfall/Animation/Data/Paces_DT'], make_sample_plan.DEFAULT_CLASSES[:4])
        self.assertTrue(p['chunks'])
        self.assertEqual(p['chunks'][0]['package'], '/Game/Townfall/Animation/Data/Paces_DT')
        for c in p['chunks']:
            self.assertEqual(sum(r['length'] for r in c['physical_ranges']), c['length'])
            self.assertEqual(len(c['io_hash20']), 40)
            self.assertTrue(c['save_as'].endswith(c['chunk_id'] + '.bin'))
        self.assertLessEqual(p['total_bytes'], 16 << 20)
        with tempfile.TemporaryDirectory() as d:
            pp = os.path.join(d, 'plan.json')
            with open(pp, 'w') as fh:
                json.dump(p, fh)
            ok, bad, rows = make_sample_plan.verify(R, pp, d)   # nothing fetched: every row fails
            self.assertEqual((ok, bad), (0, len(p['chunks'])))


if __name__ == '__main__':
    unittest.main(verbosity=1)
