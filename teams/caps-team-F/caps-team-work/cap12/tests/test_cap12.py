#!/usr/bin/env python3
"""Host tests for capability #12 research tools (Mac-only, read-only inputs).

Real-data tests read the game's/mod's own files already on the Mac (paths below);
they FAIL (never skip) if those inputs are missing. Every test asserts a non-zero
count of checked items. Writes only to a temp dir.
"""
import hashlib
import json
import os
import struct
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = HERE if os.path.exists(os.path.join(HERE, 'zen.py')) else os.path.join(os.path.dirname(HERE), 'tools')
sys.path.insert(0, TOOLS)
import asset_registry  # noqa: E402
import blake3  # noqa: E402
import cityhash  # noqa: E402
import container_header  # noqa: E402
import material_class as mc  # noqa: E402
import master_fetch_plan  # noqa: E402
import mic_reparent  # noqa: E402
import mod_material_audit as mma  # noqa: E402
import package_fetch_plan as pfp  # noqa: E402
import param_roles as pr  # noqa: E402
import reparent_plan  # noqa: E402
import shader_library as sl  # noqa: E402
import utoc  # noqa: E402
import zen  # noqa: E402

WHAT = '/WORKSPACE/.codex/worktrees/translation-core/what'
OBS = WHAT + '/.superpowers/native-observations'
LEON_TOC = OBS + '/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P.utoc'
LEON_CAS = OBS + '/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P.ucas'
BASE = [OBS + f'/townfall-main-utoc-2026-10-01/pakchunk{n}-ps5.utoc' for n in (0, 1, 10, 11, 12)]
BASE_HDR = OBS + '/townfall-original-container-header-1/container-header.bin'
G_TOC = OBS + '/townfall-manifest-capture-2026-10-01/global.utoc'
G_CAS = OBS + '/townfall-global-scriptobjects-1/global.ucas'
PAK_EXTRACT = WHAT + '/.superpowers/native-dependency-spike/unreal-config-main-pak-trial-1/trial-1/extracted'
# round-2 real inputs (read-only): the game's own registry + shader type info (from its pak),
# the integrator's verified fetches of 2026-10-13
AR_BIN = PAK_EXTRACT + '/Townfall/AssetRegistry.bin'
STINFO = [PAK_EXTRACT + '/Townfall/Content/ShaderTypeInfo-Townfall_Chunk0-SF_PS5-SP_PS5.stinfo',
          PAK_EXTRACT + '/Townfall/Content/ShaderTypeInfo-Global-SF_PS5-SP_PS5.stinfo']
EV = WHAT + '/outputs/2026-10-08-l25-prep/evidence'
FETCHED = EV + '/cap12-fetched-20261013'
LIBS = [FETCHED + '/manifests/Townfall_Chunk0.type8.bin', FETCHED + '/manifests/Global.type8.bin']
ALL_CH = [FETCHED + f'/manifests/pakchunk{n}-ps5.container-header.bin' for n in (0, 1, 10, 11, 12)]
CHECKED = [EV + '/cap12-20261012/round1-check.json', EV + '/cap12-20261012/round2-check.json']
AUDIT5 = EV + '/cap12-20261012/leon-material-audit-5hdr.json'
CLOTH = '/Game/Townfall/Characters/Materials/M_MRO_Cloth_NonVT'
LIQUID = '/Game/Townfall/Characters/Simon/Materials/M_Simon_Medical_Liquid_Inside_Tube'
TUBES = '/Game/Townfall/Characters/Simon/Materials/M_Medical_Tubes'
PY = [sys.executable, '-I']

_cache = {}


def wfile(p, data, mode='wb'):
    with open(p, mode) as fh:
        fh.write(data)


def rfile(p):
    with open(p, 'rb') as fh:
        return fh.read()


def jload(p):
    with open(p) as fh:
        return json.load(fh)


def jdump(o, p):
    with open(p, 'w') as fh:
        json.dump(o, fh)


def standin_header(src, new_package, new_export):
    """HOST-TEST STAND-IN for a fetched base header (the real base headers are on
    the console): a real Leon Material package renamed to new_package/new_export,
    with its top-level export public hash set by the hash law."""
    p = zen.ZenPackage(src)
    top = [e for e in p.exports if e['outer'][0] == 'Null']
    out, _ = mic_reparent.rename_names(src, {p.name: new_package, top[0]['name']: new_export}, _allow_structural=True)
    out = bytearray(out)
    exp = struct.unpack_from('<9i', out, 24)[2]
    struct.pack_into('<Q', out, exp + 72 * top[0]['index'] + 56, cityhash.ue_name_hash(new_export))
    return bytes(out)


def container_header_bytes(entries, cid=0x1234):
    """Synthetic container header v5: entries = [(package_id, [shader map hash 20B])]."""
    n = len(entries)
    body, tail = b'', b''
    for i, (_pid, maps) in enumerate(entries):
        e = 16 * i
        so = (16 * n + len(tail)) - (e + 8)
        body += struct.pack('<4I', 0, 0, len(maps), so if maps else 0)
        tail += b''.join(maps)
    store = body + tail
    return (struct.pack('<IIQI', 0x496F436E, 5, cid, n) + b''.join(struct.pack('<Q', p) for p, _ in entries)
            + struct.pack('<I', len(store)) + store)


def leon():
    if 'leon' not in _cache:
        _cache['leon'] = mma.load_mod(LEON_TOC, LEON_CAS)
    return _cache['leon']


def script():
    if 'so' not in _cache:
        _cache['so'] = mma.load_script_objects(G_TOC, G_CAS)
    return _cache['so']


def registry():
    if 'ar' not in _cache:
        _cache['ar'] = asset_registry.load(AR_BIN)
    return _cache['ar']


def classes():
    """(store, TypeIndex, Peers) from the real game files."""
    if 'cls' not in _cache:
        store = {}
        for h in ALL_CH:
            store.update(container_header.parse(rfile(h))[1])
        ti = reparent_plan.load_type_index(LIBS, STINFO, store)
        _cache['cls'] = (store, ti, mc.Peers(registry(), ti))
    return _cache['cls']


def fetched_file(pkg):
    name = f'{cityhash.ue_name_hash(pkg):016x}.hdr.bin'
    for d in ('headers-r1', 'headers-r2'):
        if os.path.exists(os.path.join(FETCHED, d, name)):
            return os.path.join(FETCHED, d, name)
    raise AssertionError(f'real input missing: fetched header of {pkg}')


def checked_rows():
    return [r for p in CHECKED for r in jload(p)['checked'] if 'verified' in r]


def base():
    if 'base' not in _cache:
        _cache['base'] = mma.load_base(BASE, [BASE_HDR])
    return _cache['base']


class T01CityHash(unittest.TestCase):
    def test_package_ids_leon_and_base(self):
        n = 0
        for path in [LEON_TOC] + BASE:
            t = utoc.load(path)
            for row, f in t.files.items():
                if not f.endswith('.uasset') or '/Content/' not in f:
                    continue
                proj = f.replace('../../../', '').split('/')[0]
                if proj not in ('Townfall', 'Engine'):
                    continue
                if f.count('/Content/') != 1 or '/Plugins/' in f:
                    continue
                name = ('/Game/' if proj == 'Townfall' else '/Engine/') + f.split('/Content/', 1)[1][:-7]
                self.assertEqual(int.from_bytes(t.chunk_ids[row][:8], 'little'), cityhash.ue_name_hash(name), name)
                n += 1
        self.assertGreater(n, 30000)

    def test_lengths_cover_all_branches(self):
        # internal consistency: every input length 0..200 hashes and is deterministic
        seen = {cityhash.cityhash64(bytes(range(i % 256)) * 1 + b'x' * i) for i in range(200)}
        self.assertEqual(len(seen), 200)


class T02ShaderLibraryIds(unittest.TestCase):
    def test_all_type8_ids_from_game_names(self):
        out = os.path.join(tempfile.mkdtemp(), 'si.json')
        r = subprocess.run(PY + [os.path.join(TOOLS, 'shader_inputs.py'), '--toc'] + BASE +
                           ['--names', PAK_EXTRACT, '--out', out], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        d = jload(out)
        libs = {l['library']: l for t in d['tocs'] for l in t['libraries']}
        self.assertEqual(set(libs), {'Global', 'Townfall_Chunk0', 'Townfall_Chunk11', 'Townfall_Chunk12'})
        self.assertEqual({l['hash_input'].split('-', 1)[1] for l in libs.values()}, {'SF_PS5-SP_PS5'})
        g = libs['Townfall_Chunk0']
        self.assertEqual(g['physical_ranges'][0][0], 367536)   # physical, not logical 393216
        self.assertEqual(g['length'], 1428008)
        self.assertEqual(len(d['fetch_plan']), 4)
        self.assertTrue(all(sum(n for _o, n in r['pieces']) == r['length'] for r in d['fetch_plan']))
        # F6: every TOC's container header is planned; the local pakchunk0 copy matches its row
        chp = {r['toc']: r for r in d['container_header_plan']}
        self.assertEqual(sorted(chp), sorted(os.path.basename(b) for b in BASE))
        hdr = rfile(BASE_HDR)
        self.assertEqual(chp['pakchunk0-ps5.utoc']['length'], len(hdr))
        self.assertEqual(chp['pakchunk0-ps5.utoc']['expected_iohash20'], blake3.iohash20(hdr))
        self.assertEqual(chp['pakchunk10-ps5.utoc']['length'], 416)

    def test_fails_without_names(self):
        tmp = tempfile.mkdtemp()
        empty = os.path.join(tmp, 'names.txt')
        wfile(empty, 'nothing-here.txt\n', 'w')
        r = subprocess.run(PY + [os.path.join(TOOLS, 'shader_inputs.py'), '--toc', BASE[0], '--names', empty,
                                 '--out', os.path.join(tmp, 'o.json')], capture_output=True, text=True)
        self.assertEqual(r.returncode, 1)

    def test_fails_with_zero_type8(self):
        tmp = tempfile.mkdtemp()
        r = subprocess.run(PY + [os.path.join(TOOLS, 'shader_inputs.py'), '--toc', LEON_TOC, '--names', PAK_EXTRACT,
                                 '--out', os.path.join(tmp, 'o.json')], capture_output=True, text=True)
        self.assertEqual(r.returncode, 1)
        self.assertIn('0 type-8 rows', r.stdout)


class T03Blake3(unittest.TestCase):
    def test_spec_empty_vector(self):
        self.assertEqual(blake3.blake3(b'').hex(),
                         'af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262')

    def test_toc_iohashes_leon_small_chunks(self):
        t = utoc.load(LEON_TOC)
        rd = mma.reader(LEON_CAS)
        n = 0
        for row in range(t.entry_count):
            if t.offlen[row][1] <= 80000:
                self.assertEqual(blake3.iohash20(t.read_chunk(row, rd)), t.metas[row][0])
                n += 1
        self.assertGreater(n, 50)

    def test_base_container_header_iohash(self):
        t = utoc.load(BASE[0])
        row = t.rows_of_type(6)[0]
        self.assertEqual(t.physical_ranges(row)[0][0], 128238104864)  # = earlier capture receipt offset
        data = rfile(BASE_HDR)
        self.assertEqual(len(data), t.offlen[row][1])
        self.assertEqual(blake3.iohash20(data), t.metas[row][0])


class T04ContainerHeader(unittest.TestCase):
    def test_base_and_mod(self):
        _cid, pk = container_header.parse(rfile(BASE_HDR))
        self.assertEqual(len(pk), 38439)
        refs = [h for v in pk.values() for h in v['shader_maps']]
        self.assertEqual((len(refs), len(set(refs))), (5387, 1500))
        t = utoc.load(LEON_TOC)
        _c, mp = container_header.parse(t.read_chunk(t.rows_of_type(6)[0], mma.reader(LEON_CAS)))
        self.assertEqual(len(mp), 127)
        self.assertEqual(sum(len(v['shader_maps']) for v in mp.values()), 0)

    def test_synthetic_writer_roundtrip(self):
        mh = [hashlib.sha1(b'a').digest(), hashlib.sha1(b'b').digest()]
        _cid, pk = container_header.parse(container_header_bytes([(7, mh), (9, [])]))
        self.assertEqual(pk[7]['shader_maps'], tuple(mh))
        self.assertEqual(pk[9]['shader_maps'], ())

    def test_rejects_bad_magic(self):
        with self.assertRaises(container_header.HeaderError):
            container_header.parse(b'\0' * 64)


class T05Zen(unittest.TestCase):
    def test_leon_census_and_public_hashes(self):
        pk, libs = leon()
        self.assertEqual((len(pk), libs), (127, []))
        so = script()
        classes, n = {}, 0
        for p in pk.values():
            for e in p.exports:
                c = p.describe_ref(e['class'], so)['path'].rsplit('/', 1)[-1]
                classes[c] = classes.get(c, 0) + 1
                if e['outer'][0] == 'Null':
                    self.assertEqual(e['public_hash'], cityhash.ue_name_hash(e['name']))
                    n += 1
        self.assertEqual(n, 127)
        self.assertEqual(classes['Material'], 13)
        self.assertEqual(classes['MaterialInstanceConstant'], 54)
        self.assertEqual(classes['Texture2D'], 34)

    def test_name_batch_writer_reproduces_real_bytes(self):
        pk, _ = leon()
        n = 0
        for p in pk.values():
            d = p.data
            nb = mic_reparent.name_batch(p.names)
            self.assertEqual(d[60:60 + len(nb)], nb, p.name)
            ipn = struct.unpack_from('<i', d, 48)[0]
            pairs = [mic_reparent.split_number(x) for x in p.imported_packages]
            tail = mic_reparent.name_batch([b for b, _ in pairs]) + b''.join(struct.pack('<i', k) for _, k in pairs)
            self.assertEqual(d[ipn:p.header_size], tail, p.name)
            n += 1
        self.assertEqual(n, 127)


def _synthetic_library(fmt=b'SP_PS5'):
    sh = [hashlib.sha1(bytes([i])).digest() for i in range(5)]
    groups = [[(sh[0], 0, 100), (sh[1], 5, 50)], [(sh[2], 0, 10)], [(sh[3], 1, 7), (sh[4], 5, 9)]]
    maps = [(hashlib.sha1(b'mapA').digest(), [0, 1, 2]), (hashlib.sha1(b'mapB').digest(), [3, 4, 0])]
    return sl.build(maps, groups, fmt), sh, maps


class T06ShaderLibrary(unittest.TestCase):
    def test_roundtrip_and_lookup(self):
        data, sh, maps = _synthetic_library()
        lib = sl.Library(data)
        c = lib.validate()
        self.assertEqual(c, {'shader_maps': 2, 'shaders': 5, 'groups': 3, 'indices': 11})
        self.assertEqual(lib.check_group_ids([b'SF_PS5', b'SP_PS5']), b'SP_PS5')
        m = lib.shader_map(maps[1][0])
        self.assertEqual([x['shader_hash'] for x in m], [sh[3].hex(), sh[4].hex(), sh[0].hex()])
        self.assertEqual(m[1]['offset_in_group'], 7)
        self.assertIsNone(lib.shader_map(b'\0' * 20))

    def test_wrong_format_fails_closed(self):
        data, _, _ = _synthetic_library(b'SP_PS5')
        self.assertIsNone(sl.Library(data).check_group_ids([b'PCD3D_SM6', b'SF_PS5']))

    def test_rejects_version_trailing_truncation(self):
        data, _, _ = _synthetic_library()
        with self.assertRaises(sl.LibError):
            sl.Library(struct.pack('<I', 2) + data[4:])
        with self.assertRaises(sl.LibError):
            sl.Library(data + b'\0')
        with self.assertRaises(sl.LibError):
            sl.Library(data[:-3])

    def test_rejects_membership_mismatch(self):
        data, _, _ = _synthetic_library()
        lib = sl.Library(data)
        lib.shader_entries[0] = (lib.shader_entries[0] & ~(((1 << 30) - 1) << 4)) | (2 << 4)
        with self.assertRaises(sl.LibError):
            lib.validate()


class T07VerifyFetch(unittest.TestCase):
    def _setup(self, tamper=False, drop=False, extra_map=False, empty_lib=False):
        tmp = tempfile.mkdtemp()
        data, _, maps = _synthetic_library()
        if empty_lib:
            data = sl.build([], [], b'SP_PS5')
        refs = [m for m, _ in maps] + ([hashlib.sha1(b'not-in-any-library').digest()] if extra_map else [])
        ch = container_header_bytes([(11, refs[:1]), (12, refs[1:])])
        wfile(os.path.join(tmp, 'ch.bin'), ch)
        plan = {'tocs': [{'libraries': [{'chunk_id': 'aa' * 12, 'hash_input': 'Lib-SF_PS5-SP_PS5'}]}],
                'fetch_plan': [{'chunk_id': 'aa' * 12, 'library': 'Lib', 'length': len(data),
                                'expected_iohash20': blake3.iohash20(data), 'local_name': 'Lib.type8.bin'}],
                'container_header_plan': [{'toc': 'c.utoc', 'length': len(ch), 'expected_iohash20': blake3.iohash20(ch),
                                           'local_name': 'c.container-header.bin'}]}
        jdump(plan, os.path.join(tmp, 'plan.json'))
        if not drop:
            body = bytearray(data)
            if tamper:
                body[10] ^= 1
            wfile(os.path.join(tmp, 'Lib.type8.bin'), body)
        return tmp

    def _run(self, tmp, headers=('ch.bin',)):
        return subprocess.run(PY + [os.path.join(TOOLS, 'verify_fetch.py'), '--plan', os.path.join(tmp, 'plan.json'),
                                    '--dir', tmp, '--out', os.path.join(tmp, 'r.json'), '--container-header']
                              + [os.path.join(tmp, h) for h in headers], capture_output=True, text=True)

    def test_pass(self):
        tmp = self._setup()
        r = self._run(tmp)
        self.assertEqual(r.returncode, 0, r.stdout)
        rep = jload(os.path.join(tmp, 'r.json'))
        self.assertEqual(rep['rows'][0]['group_id_format'], 'SP_PS5')
        self.assertEqual(rep['rows'][0]['group_ids_checked'], 3)
        self.assertEqual(rep['container_headers'][0]['missing'], 0)

    def test_tampered_fails(self):
        self.assertEqual(self._run(self._setup(tamper=True)).returncode, 1)

    def test_missing_fails(self):
        r = self._run(self._setup(drop=True))
        self.assertEqual(r.returncode, 1)
        self.assertIn('checked 0 of 1', r.stdout)

    def test_store_map_missing_from_libraries_fails(self):   # N1: PASS needs missing == 0
        r = self._run(self._setup(extra_map=True))
        self.assertEqual(r.returncode, 1, r.stdout)
        self.assertIn('not in the fetched libraries', r.stdout)

    def test_planned_container_header_not_given_fails(self):   # F6
        tmp = self._setup()
        r = self._run(tmp, headers=())
        self.assertEqual(r.returncode, 1, r.stdout)
        self.assertIn('not given', r.stdout)

    def test_unplanned_container_header_refused(self):
        tmp = self._setup()
        wfile(os.path.join(tmp, 'other.bin'), container_header_bytes([(1, [])]))
        r = self._run(tmp, headers=('ch.bin', 'other.bin'))
        self.assertEqual(r.returncode, 1, r.stdout)
        self.assertIn('not a planned container header', r.stdout)

    def test_zero_group_library_fails(self):   # N1: nothing recomputed is not a pass
        r = self._run(self._setup(empty_lib=True))
        self.assertEqual(r.returncode, 1, r.stdout)
        self.assertIn('zero shader groups', r.stdout)


class T08MaterialAudit(unittest.TestCase):
    def test_leon_all_materials_lack_ps5_shaders(self):
        pk, libs = leon()
        ids, store = base()
        rep = mma.audit_packages(pk, libs, script(), ids, store, 'SF_PS5-SP_PS5')
        self.assertEqual(rep['material_exports_checked'], 67)
        self.assertEqual(rep['verdict_counts'], {'INHERITS_MISSING': 54, 'NO_TARGET_SHADERS': 13})
        by = {m['package']: m for m in rep['materials']}
        self.assertEqual(by['/Game/Leon/MI_Leon_UpperBody']['parent'], '/Game/Leon/M_Leon_Body')
        self.assertEqual(by['/Game/Leon/M_Leon_Body']['inline_shader_formats'], ['PCD3D_SM5', 'PCD3D_SM6'])
        ovr = [m for m in rep['materials'] if m['overrides_base']]
        self.assertEqual(len(ovr), 43)
        self.assertEqual(sum(1 for m in ovr if m['base_had_target_shader_maps']), 24)
        # F6: pakchunk10's container header is not loaded here -> unknown, not False
        unk = [m['package'] for m in ovr if m['base_had_target_shader_maps'] is None]
        self.assertEqual(len(unk), 1)
        self.assertEqual([by[u]['overrides_base'] for u in unk], ['pakchunk10-ps5.utoc'])
        self.assertEqual(rep['overrides_with_container_header_not_loaded'], unk)
        self.assertEqual(sum(1 for m in ovr if m['base_had_target_shader_maps'] is False), 18)

    def test_reparented_mic_becomes_ok(self):
        pk, libs = leon()
        ids, store = base()
        pk = dict(pk)
        target = '/Game/Townfall/Characters/Materials/M_MRO_Basic'
        self.assertTrue(store[cityhash.ue_name_hash(target)]['shader_maps'])
        new, _ = mic_reparent.reparent(pk['/Game/Leon/MI_Leon_UpperBody'].data, '/Game/Leon/M_Leon_Body',
                                       target, 'M_MRO_Basic',
                                       base_header=standin_header(pk['/Game/Leon/M_Leon_Skin'].data, target, 'M_MRO_Basic'))
        pk['/Game/Leon/MI_Leon_UpperBody'] = zen.ZenPackage(new)
        rep = mma.audit_packages(pk, libs, script(), ids, store, 'SF_PS5-SP_PS5')
        by = {m['package']: m for m in rep['materials']}
        self.assertEqual(by['/Game/Leon/MI_Leon_UpperBody']['verdict'], 'OK_BASE_SHADERS')
        self.assertEqual(rep['verdict_counts']['INHERITS_MISSING'], 53)

    def test_cli_fails_on_zero_materials(self):
        tmp = tempfile.mkdtemp()
        # a mod container with packages but no materials: use the base pakchunk11 TOC as 'mod'
        # is not possible without its ucas; instead run on the global container (0 packages)
        r = subprocess.run(PY + [os.path.join(TOOLS, 'mod_material_audit.py'), '--mod-utoc', G_TOC, '--mod-ucas', G_CAS,
                                 '--script-utoc', G_TOC, '--script-ucas', G_CAS, '--base-toc', BASE[1],
                                 '--base-header', BASE_HDR, '--target', 'SF_PS5-SP_PS5',
                                 '--out', os.path.join(tmp, 'a.json')], capture_output=True, text=True)
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)


class T09Reparent(unittest.TestCase):
    def test_reparent_changes_only_intended_fields(self):
        pk, _ = leon()
        p = pk['/Game/Leon/MI_Leon_UpperBody']
        tgt = '/Game/Townfall/Characters/Materials/M_MRO_Basic'
        new, rec = mic_reparent.reparent(p.data, '/Game/Leon/M_Leon_Body', tgt, 'M_MRO_Basic',
                                         base_header=standin_header(pk['/Game/Leon/M_Leon_Skin'].data, tgt, 'M_MRO_Basic'))
        self.assertIn('two sources', rec['new_public_hash_source'])
        q = zen.ZenPackage(new)
        self.assertEqual(q.names, p.names)
        self.assertEqual(q.exports[0]['serial_size'], p.exports[0]['serial_size'])
        self.assertEqual(q.export_bytes(q.exports[0]), p.export_bytes(p.exports[0]))
        self.assertEqual(q.imports, p.imports)
        self.assertEqual(q.import_hashes[rec['public_hash_slot']], cityhash.ue_name_hash('M_MRO_Basic'))
        others = [i for i in range(len(p.import_hashes)) if i != rec['public_hash_slot']]
        self.assertTrue(others)
        for i in others:
            self.assertEqual(q.import_hashes[i], p.import_hashes[i])
        self.assertEqual(rec['new_header_size'] - rec['old_header_size'], len(new) - len(p.data))
        self.assertEqual(struct.unpack_from('<I', new, 20), struct.unpack_from('<I', p.data, 20))  # cooked hdr size

    def test_rename_roundtrip_all_packages(self):
        pk, _ = leon()
        n = 0
        for p in pk.values():
            structural = {p.name} | {e['name'] for e in p.exports}
            free = [x for x in p.names if x not in structural and not x.startswith('/')]
            if not free:
                continue
            a, _ = mic_reparent.rename_names(p.data, {free[0]: free[0] + '_LongerTmp'})
            b, _ = mic_reparent.rename_names(a, {free[0] + '_LongerTmp': free[0]})
            self.assertEqual(b, p.data, p.name)
            n += 1
        self.assertGreater(n, 20)
        # structural relayout (stand-in path) round-trips on all 127 too
        m = 0
        for p in pk.values():
            a, _ = mic_reparent.rename_names(p.data, {p.names[0]: p.names[0] + '_LongerTmp'}, _allow_structural=True)
            b, _ = mic_reparent.rename_names(a, {p.names[0] + '_LongerTmp': p.names[0]}, _allow_structural=True)
            self.assertEqual(b, p.data, p.name)
            m += 1
        self.assertEqual(m, 127)

    def test_rename_then_reparent(self):
        pk, _ = leon()
        p = pk['/Game/Leon/MI_Leon_UpperBody']
        new, rec = mic_reparent.reparent(p.data, '/Game/Leon/M_Leon_Body', '/Game/X/M_Y', 'M_Y',
                                         {'ORM': 'OcclusionRoughnessMetallic'},
                                         base_header=standin_header(pk['/Game/Leon/M_Leon_Skin'].data, '/Game/X/M_Y', 'M_Y'))
        q = zen.ZenPackage(new)
        self.assertIn('OcclusionRoughnessMetallic', q.names)
        self.assertNotIn('ORM', q.names)
        self.assertEqual(q.export_bytes(q.exports[0]), p.export_bytes(p.exports[0]))
        self.assertEqual(rec['renamed_parameters'], [['ORM', 'OcclusionRoughnessMetallic']])
        self.assertEqual(struct.unpack_from('<i', new, 24)[0] % 8, 0)  # import hashes stay 8-aligned

    def test_refusals(self):
        pk, _ = leon()
        d = pk['/Game/Leon/MI_Leon_UpperBody'].data
        bh = standin_header(pk['/Game/Leon/M_Leon_Skin'].data, '/Game/X/M', 'M')
        with self.assertRaises(zen.ZenError):
            mic_reparent.reparent(d, '/Game/Nope', '/Game/X/M', 'M', base_header=bh)
        with self.assertRaises(zen.ZenError):
            mic_reparent.reparent(d, '/Game/Leon/M_Leon_Body', '/Game/Leon/Textures/T_Leon_UpperBody_N', 'X',
                                  base_header=bh)
        with self.assertRaises(zen.ZenError):
            mic_reparent.reparent(d, '/Game/Leon/M_Leon_Body', '/Game/X/M', 'M', {'ORM': 'Normal'}, base_header=bh)
        with self.assertRaises(zen.ZenError):   # F3: no second source -> nothing written
            mic_reparent.reparent(d, '/Game/Leon/M_Leon_Body', '/Game/X/M', 'M')
        with self.assertRaises(zen.ZenError):   # N3: export / path names are not parameters
            mic_reparent.rename_names(d, {'MI_Leon_UpperBody': 'MI_X'})
        with self.assertRaises(zen.ZenError):
            mic_reparent.rename_names(d, {'/Game/Leon/MI_Leon_UpperBody': '/Game/X'})

    def test_cli_requires_base_header(self):   # F3: exit 1 without --base-header, nothing written
        pk, _ = leon()
        tmp = tempfile.mkdtemp()
        inp, out, hdr = (os.path.join(tmp, x) for x in ('in.bin', 'out.bin', 'hdr.bin'))
        wfile(inp, pk['/Game/Leon/MI_Leon_UpperBody'].data)
        M = os.path.join(TOOLS, 'mic_reparent.py')
        args = PY + [M, '--in', inp, '--old-parent', '/Game/Leon/M_Leon_Body', '--new-parent', '/Game/X/M',
                     '--new-export', 'M', '--out', out]
        r = subprocess.run(args, capture_output=True, text=True)
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('mandatory', r.stdout)
        self.assertFalse(os.path.exists(out))
        wfile(hdr, standin_header(pk['/Game/Leon/M_Leon_Skin'].data, '/Game/X/Other', 'M'))   # wrong package
        r = subprocess.run(args + ['--base-header', hdr], capture_output=True, text=True)
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertFalse(os.path.exists(out))
        wfile(hdr, standin_header(pk['/Game/Leon/M_Leon_Skin'].data, '/Game/X/M', 'M'))
        r = subprocess.run(args + ['--base-header', hdr], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn('1 package rewritten', r.stdout)
        self.assertTrue(os.path.exists(out))

    def test_base_header_second_source(self):
        pk, _ = leon()
        body = pk['/Game/Leon/M_Leon_Body'].data   # stand-in "base" header with a real export map
        d = pk['/Game/Leon/MI_Leon_UpperBody'].data
        with self.assertRaises(zen.ZenError):   # header names a different package -> refuse
            mic_reparent.reparent(d, '/Game/Leon/M_Leon_Body', '/Game/Leon/M_Other', 'M_Leon_Body', base_header=body)
        skin = pk['/Game/Leon/M_Leon_Skin'].data   # positive: real export map agrees with the hash law
        _new, rec = mic_reparent.reparent(d, '/Game/Leon/M_Leon_Body', '/Game/Leon/M_Leon_Skin', 'M_Leon_Skin',
                                          base_header=skin)
        self.assertIn('two sources', rec['new_public_hash_source'])
        hs = struct.unpack_from('<I', skin, 4)[0]   # a header-only prefix works as the second source too
        _new, rec = mic_reparent.reparent(d, '/Game/Leon/M_Leon_Body', '/Game/Leon/M_Leon_Skin', 'M_Leon_Skin',
                                          base_header=skin[:hs])
        self.assertIn('two sources', rec['new_public_hash_source'])


class T10FetchPlanAndChoice(unittest.TestCase):
    def test_plan_and_check_with_local_mod_as_stand_in(self):
        # Stand-in: plan against the MOD TOC (bytes are local) to exercise both
        # whole-chunk (IoHash) and header-only (name-hash) verification paths.
        tmp = tempfile.mkdtemp()
        rows, missing = pfp.plan([LEON_TOC], ['/Game/Leon/MI_Leon_UpperBody',
                                              '/Game/Townfall/Characters/Simon/Meshes/SK_Simon_FirstStage'], 65536)
        self.assertEqual((len(rows), missing), (2, []))
        self.assertEqual([r['whole_chunk'] for r in rows], [True, False])
        rd = mma.reader(LEON_CAS)
        for r in rows:
            wfile(os.path.join(tmp, r['local_name']), b''.join(rd(o, n) for o, n in r['pieces']))
        out, fails = pfp.check(rows, tmp, {}, script())
        self.assertEqual(fails, [])
        self.assertEqual([o['verified'] for o in out], ['iohash', 'name-hash'])
        self.assertIn('/Game/Leon/MI_Leon_Face', out[1]['imports'])
        # export classes come from the header (also from a header-only prefix)
        self.assertEqual([e['class'] for e in out[0]['exports']], ['/Script/Engine/MaterialInstanceConstant'])
        self.assertEqual([e['class'] for e in out[1]['exports']], ['/Script/Engine/SkeletalMesh'])
        self.assertIn('/Game/Leon/MI_Leon_Face', out[1]['imports_store_unknown'])   # empty store -> unknown
        # corrupt one byte of the whole chunk -> refused
        p = os.path.join(tmp, rows[0]['local_name'])
        b = bytearray(rfile(p)); b[-1] ^= 1; wfile(p, b)
        _out, fails = pfp.check(rows, tmp, {}, script())
        self.assertEqual(len(fails), 1)

    def test_choose_rule(self):
        o = lambda m, e, mic, n: {'mapped': m, 'exact': e, 'is_mic': mic, 'names': n}   # noqa: E731
        P = ['BaseColor', 'Normal', 'ORM']
        opts = {'/Game/B/MI_Full': o(3, 3, True, 2), '/Game/B/M_Full': o(3, 3, False, 9), '/Game/B/M_Half': o(1, 1, False, 1)}
        self.assertEqual(reparent_plan.choose(P, opts, False), ('/Game/B/M_Full', 1.0))   # Material before instance
        opts = {'/Game/B/M_Renamed': o(2, 0, False, 1), '/Game/B/M_Exact': o(2, 2, False, 50)}
        self.assertEqual(reparent_plan.choose(P, opts, False), (None, 2 / 3))      # partial refused by default
        self.assertEqual(reparent_plan.choose(P, opts, True)[0], '/Game/B/M_Exact')  # exact beats renamed
        self.assertEqual(reparent_plan.choose(P, {'/Game/B/M_Half': o(1, 1, False, 1)}, True)[0], None)  # < 0.5
        self.assertEqual(reparent_plan.choose(P, {}, True), (None, 0.0))
        self.assertEqual(reparent_plan.choose([], opts, True), (None, 0.0))

    def _hdr(self, pkg, cls, imports=(), names=('BaseColor', 'Normal', 'ORM'), export=None):
        return {'package': pkg, 'verified': 'iohash', 'names': list(names), 'imports': list(imports),
                'exports': [{'name': export or pkg.rsplit('/', 1)[-1], 'class': cls, 'public_hash': '0'}]}

    @staticmethod
    def _props(group='opaque', **kw):
        p = {'readable': True, 'domain': 'MD_Surface', 'blend': {'opaque': 'BLEND_Opaque', 'masked': 'BLEND_Masked'}.get(
            group, 'BLEND_Translucent'), 'blend_group': group, 'lit': 'lit', 'models': ['MSM_DefaultLit'],
             'special': [], 'sources': {'asset_registry': f'MD_Surface/{group}/lit',
                                        'shader_type_peers': f'MD_Surface/{group}/lit', 'peers': 9}}
        p.update(kw)
        return p

    def test_candidates_need_class_store_usage(self):   # F2 + F6 + round 2 class gate
        M, I, SK = mma.MATERIAL, mma.MIC, reparent_plan.SKELETAL_MESH
        extra = ['/Game/B/M_Glass', '/Game/B/M_Unread', '/Game/B/M_Unlit', '/Game/B/M_HairSM', '/Game/B/M_Decal']
        checked = [self._hdr('/Game/B/SK_Body', SK, ['/Game/B/MI_Body', '/Game/B/M_Direct', '/Game/B/NS_Fx'] + extra),
                   self._hdr('/Game/B/MI_Body', I, ['/Game/B/M_Master']),
                   self._hdr('/Game/B/M_Master', M),
                   self._hdr('/Game/B/M_Direct', M, export='M_Direct_Export'),
                   self._hdr('/Game/B/NS_Fx', '/Script/Niagara/NiagaraSystem'),
                   self._hdr('/Game/B/M_Static', M),                       # no skeletal-mesh use
                   self._hdr('/Game/B/M_Unknown', M),                      # not in any loaded store
                   self._hdr('/Game/B/M_Overridden', M)] + [self._hdr(x, M) for x in extra]
        checked[0]['imports'] += ['/Game/B/M_Unknown', '/Game/B/M_Overridden']
        store = {cityhash.ue_name_hash(r['package']): {'shader_maps': (b'x' * 20,)} for r in checked
                 if r['package'] != '/Game/B/M_Unknown'}
        special = {'/Game/B/M_Glass': self._props('translucent'),
                   '/Game/B/M_Unread': {'readable': False, 'why': 'sources disagree: x vs y'},
                   '/Game/B/M_Unlit': self._props(lit='unlit'),
                   '/Game/B/M_HairSM': self._props('masked', special=['MSM_Hair']),
                   '/Game/B/M_Decal': self._props(domain='MD_DeferredDecal')}
        cands, excl, det = reparent_plan.candidates(checked, store, {'/Game/B/M_Overridden'}, None,
                                                    lambda p: special.get(p, self._props()))
        self.assertEqual(sorted(cands), ['/Game/B/MI_Body', '/Game/B/M_Direct', '/Game/B/M_Master'])
        self.assertEqual(cands['/Game/B/M_Direct']['export'], 'M_Direct_Export')   # from header, not basename
        self.assertEqual(cands['/Game/B/M_Master']['skeletal_mesh_evidence'], ['/Game/B/SK_Body'])
        self.assertEqual(sorted(excl['not exactly one Material/MaterialInstanceConstant export']),
                         ['/Game/B/NS_Fx', '/Game/B/SK_Body'])   # wrong class (Niagara, the mesh itself)
        self.assertEqual(excl['no skeletal-mesh use evidence'], ['/Game/B/M_Static'])
        self.assertEqual(excl['store unknown (its container header not loaded)'], ['/Game/B/M_Unknown'])
        self.assertEqual(excl['overridden by the mod'], ['/Game/B/M_Overridden'])
        # round 2: translucent candidate refused, and every other class refusal says why
        self.assertIn('translucent family', det['/Game/B/M_Glass'])
        self.assertIn('sources disagree', det['/Game/B/M_Unread'])
        self.assertIn('unlit', det['/Game/B/M_Unlit'])
        self.assertIn('MSM_Hair', det['/Game/B/M_HairSM'])
        self.assertIn('not a surface', det['/Game/B/M_Decal'])
        self.assertEqual(len(det), 5)
        # without a class reader nothing is a candidate (fail closed)
        cands, excl, _ = reparent_plan.candidates(checked, store, set(), None, None)
        self.assertEqual(cands, {})
        self.assertIn('no material class reader (asset registry + shader types) given', excl)

    def _ctx(self, classes_by_master, accept=False):
        bound = {n: {'/T/' + n} for n in ('BaseColor', 'Normal', 'ORM', 'HairAO')}   # texture slots in the base
        return reparent_plan.Context(lambda m: classes_by_master.get(m, {'readable': False, 'why': 'test'}),
                                     lambda c: bound, lambda t: None, {}, accept)

    def test_plan_rules_on_real_leon_audit(self):   # F4 + round 2 blend matching
        pk, libs = leon()
        ids, store = base()
        audit = mma.audit_packages(pk, libs, script(), ids, store, 'SF_PS5-SP_PS5')
        names = ['BaseColor', 'Normal', 'ORM', 'HairAO']
        cands = {'/Game/B/M_Char_Pkg': {'names': names, 'export': 'M_Char', 'class': mma.MATERIAL, 'verified': 'iohash',
                                        'skeletal_mesh_evidence': ['/Game/B/SK'], 'props': self._props()},
                 '/Game/B/M_Masked_Pkg': {'names': names, 'export': 'M_Masked', 'class': mma.MATERIAL,
                                          'verified': 'iohash', 'skeletal_mesh_evidence': ['/Game/B/SK'],
                                          'props': self._props('masked')}}
        op, ma = {'readable': True, 'blend_group': 'opaque'}, {'readable': True, 'blend_group': 'masked'}
        ctx = self._ctx({'/Game/Leon/M_Leon_Body': op, '/Game/Leon/M_Leon_Skin': op, '/Game/Leon/M_Leon_Hair': ma})
        with self.assertRaises(ValueError):           # no context -> refuse to plan
            reparent_plan.plan(audit, pk, cands, False)
        rows = reparent_plan.plan(audit, pk, cands, False, ctx=ctx)
        acts = {}
        for r in rows:
            acts[r['action']] = acts.get(r['action'], 0) + 1
        self.assertEqual(len(rows), 67)
        self.assertEqual(acts, {'reparent': 11, 'known-checker': 56})
        by = {r['mod_package']: r for r in rows}
        up = by['/Game/Leon/MI_Leon_UpperBody']
        self.assertEqual((up['new_parent'], up['new_export']), ('/Game/B/M_Char_Pkg', 'M_Char'))
        self.assertIn('/Game/B/M_Masked_Pkg', up['refused_parents'])           # masked refused for opaque
        hair = by['/Game/Leon/MI_Leon_Hair']
        self.assertEqual(hair['new_parent'], '/Game/B/M_Masked_Pkg')           # masked mod -> masked parent
        self.assertIn('/Game/B/M_Char_Pkg', hair['refused_parents'])
        self.assertTrue(any('cut-out' in n for n in hair['look_notes']))
        inv = [r for r in rows if r['old_parent'] and r['old_parent'].endswith('/M_Invisible')]
        self.assertEqual(len(inv), 43)
        self.assertTrue(all(r['action'] == 'known-checker' and r['parameterless'] for r in inv))
        mats = [r for r in rows if r['class'] == 'Material']
        self.assertEqual(len(mats), 13)
        self.assertTrue(all(r['action'] == 'known-checker' for r in mats))
        # only the opaque candidate: the masked hair instance stays known-checker, and says why
        rows = reparent_plan.plan(audit, pk, {'/Game/B/M_Char_Pkg': cands['/Game/B/M_Char_Pkg']}, True, ctx=ctx)
        hair = {r['mod_package']: r for r in rows}['/Game/Leon/MI_Leon_Hair']
        self.assertEqual(hair['action'], 'known-checker')
        self.assertIn('opaque parent for a masked mod instance', hair['refused_parents']['/Game/B/M_Char_Pkg'])
        # unreadable / translucent mod class -> known-checker with the reason
        ctx2 = self._ctx({'/Game/Leon/M_Leon_Body': {'readable': True, 'blend_group': 'translucent'}})
        rows = reparent_plan.plan(audit, pk, cands, True, ctx=ctx2)
        by = {r['mod_package']: r for r in rows}
        self.assertEqual(by['/Game/Leon/MI_Leon_UpperBody']['action'], 'known-checker')
        self.assertIn('translucent mod material', by['/Game/Leon/MI_Leon_UpperBody']['why'])
        self.assertIn('not readable', by['/Game/Leon/MI_Leon_Face']['why'])
        # opaque not provable against masked: known-checker, unless the user decides to accept it
        un = {'readable': False, 'unproven_opaque': True, 'why': 'opaque vs masked not provable'}
        cls = {'/Game/Leon/M_Leon_Body': un, '/Game/Leon/M_Leon_Skin': un, '/Game/Leon/M_Leon_Hair': ma}
        rows = reparent_plan.plan(audit, pk, cands, False, ctx=self._ctx(cls))
        up = {r['mod_package']: r for r in rows}['/Game/Leon/MI_Leon_UpperBody']
        self.assertEqual(up['action'], 'known-checker')
        self.assertIn('not provable', up['why'])
        rows = reparent_plan.plan(audit, pk, cands, False, ctx=self._ctx(cls, accept=True))
        up = {r['mod_package']: r for r in rows}['/Game/Leon/MI_Leon_UpperBody']
        self.assertEqual((up['action'], up['new_parent']), ('reparent', '/Game/B/M_Char_Pkg'))
        self.assertTrue(any('USER DECISION' in n for n in up['look_notes']))
        # an exact name that no base instance binds to a texture is not counted (type unverified)
        ctx3 = reparent_plan.Context(lambda m: op if m != '/Game/Leon/M_Leon_Hair' else ma, lambda c: {},
                                     lambda t: None, {})
        rows = reparent_plan.plan(audit, pk, cands, True, ctx=ctx3)
        up = {r['mod_package']: r for r in rows}['/Game/Leon/MI_Leon_UpperBody']
        self.assertEqual(up['action'], 'known-checker')
        # restore-base only on request, and only where base shader maps are proven (True, not None/False)
        rows2 = reparent_plan.plan(audit, pk, {}, False, allow_restore_base=True, ctx=ctx)
        rb = [r for r in rows2 if r['action'] == 'restore-base']
        self.assertEqual(len(rb), 24)
        self.assertTrue(all(r['base_had_target_shader_maps'] is True for r in rb))

    def test_param_names_from_real_mic(self):
        pk, _ = leon()
        p = pk['/Game/Leon/MI_Leon_UpperBody']
        self.assertEqual(reparent_plan.param_names(p.names, 'MI_Leon_UpperBody', p.name), ['BaseColor', 'Normal', 'ORM'])


class T11Harvest(unittest.TestCase):
    def test_join(self):
        tmp = tempfile.mkdtemp()
        audit = {'materials': [{'package': '/Game/Leon/M_Leon_Body', 'verdict': 'NO_TARGET_SHADERS'}]}
        jdump(audit, os.path.join(tmp, 'a.json'))
        before = ('[x] [Lua] [mat12] [S] c=1 i=0 mat=MaterialInstanceConstant /Game/Leon/MI_Leon_UpperBody.MI_Leon_UpperBody '
                  'base=Material /Game/Leon/M_Leon_Body.M_Leon_Body\n')
        after = before.replace('Material /Game/Leon/M_Leon_Body.M_Leon_Body',
                               'Material /Game/Townfall/Characters/Materials/M_MRO_Basic.M_MRO_Basic')
        audit['materials'] += [{'package': '/Game/Leon/MI_Leon_UpperBody', 'verdict': 'INHERITS_MISSING'},
                               {'package': '/Game/S/Slot0', 'verdict': 'NO_TARGET_SHADERS'}]
        jdump(audit, os.path.join(tmp, 'a.json'))
        jdump({'rows': [{'mod_package': '/Game/S/Slot0', 'action': 'known-checker'},
                        {'mod_package': '/Game/Leon/MI_Leon_UpperBody', 'action': 'reparent'}]},
              os.path.join(tmp, 'p.json'))
        # F1 repro from the review: only a base-game slot was probed -> must not PASS
        base_only = ('[mat12] [S] c=1 i=0 mat=MaterialInstanceConstant /Game/Townfall/UI/MI_MenuChar.MI_MenuChar '
                     'base=Material /Game/Townfall/Characters/Materials/M_X.M_X\n')
        slot0 = '[mat12] [S] c=2 i=0 mat=Material /Game/S/Slot0.Slot0 base=Material /Game/S/Slot0.Slot0\n'
        H = os.path.join(TOOLS, 'harvest_matdiag.py')
        P = ['--plan', os.path.join(tmp, 'p.json')]
        for text, flag, code, word in [(before, [], 1, 'FAIL'), (before, ['--expect-checker'], 0, 'PASS'),
                                       (after, [], 0, 'PASS'), ('', [], 1, 'FAIL'),
                                       (base_only, [], 1, 'INCONCLUSIVE'), (base_only + after, [], 0, 'PASS'),
                                       (after + slot0, [], 1, 'FAIL'), (after + slot0, P, 0, 'known-checker'),
                                       (slot0, P, 1, 'FAIL'), (before + slot0, P, 1, 'FAIL')]:
            wfile(os.path.join(tmp, 'log.txt'), text, 'w')
            r = subprocess.run(PY + [H, '--log', os.path.join(tmp, 'log.txt'), '--audit', os.path.join(tmp, 'a.json')]
                               + flag, capture_output=True, text=True)
            self.assertEqual(r.returncode, code, (text, flag, r.stdout))
            self.assertIn(word, r.stdout.splitlines()[-1], (text, flag, r.stdout))



class FakeRegistry:
    """Synthetic stand-in for asset_registry.Registry (assets with class + tags)."""

    def __init__(self, assets):
        self.assets = assets
        self.by_package = {a['package']: a for a in assets}

    def asset(self, p):
        return self.by_package.get(p)

    def parent_package(self, p):
        v = (self.asset(p) or {}).get('tags', {}).get('Parent')
        return v.split("'")[1].rsplit('.', 1)[0] if v else None


class FakeTypes:
    def __init__(self, sigs):
        self.sigs = sigs

    def signature(self, p):
        return (self.sigs[p], None) if p in self.sigs else (None, 'no target shader maps')


def mat(pkg, blend='BLEND_Opaque', sm='MSM_DefaultLit', field=2, domain='MD_Surface'):
    return {'package': pkg, 'class': mc.MATERIAL_AR_CLASS, 'tags': {
        'BlendMode': blend, 'MaterialDomain': domain, 'ShadingModel': sm,
        'ShadingModels': f'(ShadingModelField={field})'}}


def mic(pkg, parent):
    return {'package': pkg, 'class': reparent_plan.AR_MIC,
            'tags': {'Parent': f"/Script/Engine.Material'{parent}.{parent.rsplit('/', 1)[-1]}'"}}


class T12AssetRegistry(unittest.TestCase):
    def test_real_registry_parses(self):
        reg = registry()
        self.assertEqual(reg.counts['assets'], 41565)
        self.assertGreater(reg.counts['names'], 0)
        self.assertLessEqual(reg.tail_bytes, 64)
        mats = [a for a in reg.assets if a['class'] == mc.MATERIAL_AR_CLASS]
        self.assertEqual(len(mats), 418)
        self.assertTrue(all('BlendMode' in a['tags'] and 'MaterialDomain' in a['tags'] for a in mats))
        t = reg.asset(CLOTH)['tags']
        self.assertEqual((t['BlendMode'], t['MaterialDomain']), ('BLEND_Opaque', 'MD_Surface'))
        self.assertEqual(reg.parent_package('/Game/Townfall/Characters/Simon/Materials/MI_Simon_Shirt'), CLOTH)
        self.assertEqual(reg.asset('/Game/Townfall/Characters/Simon/Textures/T_Shirt_N')['tags']['CompressionSettings'],
                         'TC_Normalmap')
        # the same bytes were extracted twice from the game's pak by independent extractions
        twin = WHAT + ('/.superpowers/native-dependency-spike/unreal-archive-config-pak-composition-2/trial-1/'
                       'independent-extraction/Townfall/AssetRegistry.bin')
        self.assertEqual(hashlib.sha256(rfile(AR_BIN)).digest(), hashlib.sha256(rfile(twin)).digest())

    def test_corrupt_registry_fails_closed(self):
        d = bytearray(rfile(AR_BIN))
        _names, pos = zen.read_name_batch(bytes(d), 24)
        bad = bytearray(d); bad[pos] ^= 1                       # begin magic
        with self.assertRaises(asset_registry.RegistryError):
            asset_registry.Registry(bytes(bad))
        bad = bytearray(d); struct.pack_into('<I', bad, pos + 4 + 40, struct.unpack_from('<I', d, pos + 44)[0] + 1)
        with self.assertRaises((asset_registry.RegistryError, zen.ZenError)):   # pair count off by one
            asset_registry.Registry(bytes(bad))
        with self.assertRaises((asset_registry.RegistryError, zen.ZenError, struct.error)):
            asset_registry.Registry(bytes(d[:len(d) // 2]))       # truncated
        with self.assertRaises(asset_registry.RegistryError):
            asset_registry.Registry(b'\0' * 32)


class T13MaterialClass(unittest.TestCase):
    def test_stinfo_pairs_with_its_library(self):
        st = mc.load_stinfo(rfile(STINFO[0]))
        lib = sl.Library(rfile(LIBS[0]))
        self.assertEqual(len(st), len(lib.shader_hashes))
        self.assertGreater(len(st), 0)
        with self.assertRaises(mc.ClassError):                  # wrong pairing refused
            mc.TypeIndex([(lib, mc.load_stinfo(rfile(STINFO[1])))], {})
        with self.assertRaises(mc.ClassError):
            mc.load_stinfo(rfile(STINFO[0]) + b'\0')            # trailing bytes
        with self.assertRaises(ValueError):
            reparent_plan.load_type_index(LIBS, STINFO[:1], {})

    def test_shading_bits_are_data_derived(self):
        bits = classes()[2].bits
        self.assertEqual((bits[0], bits[1], bits[7], bits[8]), ('MSM_Unlit', 'MSM_DefaultLit', 'MSM_Hair', 'MSM_Cloth'))

    def test_two_sources_on_real_candidates(self):
        _store, ti, pe = classes()
        reg, hd = registry(), {r['package']: r for r in checked_rows()}
        self.assertEqual(len(pe.pop), 418)
        c = mc.base_props(CLOTH, reg, ti, pe, hd)
        self.assertTrue(c['readable'], c)
        self.assertEqual((c['domain'], c['blend_group'], c['lit']), ('MD_Surface', 'opaque', 'lit'))
        self.assertGreaterEqual(c['sources']['peers'], mc.MIN_PEERS)
        self.assertIsNone(reparent_plan.base_gate(c))
        shirt = mc.base_props('/Game/Townfall/Characters/Simon/Materials/MI_Simon_Shirt', reg, ti, pe, hd)
        self.assertEqual((shirt['readable'], shirt['master']), (True, CLOTH))
        liq = mc.base_props(LIQUID, reg, ti, pe, hd)               # round-1 pick: MASKED by both sources
        self.assertEqual((liq['readable'], liq['blend_group']), (True, 'masked'))
        tub = mc.base_props(TUBES, reg, ti, pe, hd)                # translucent candidate refused
        self.assertEqual(tub['blend_group'], 'translucent')
        self.assertIn('translucent family', reparent_plan.base_gate(tub))

    def _peers(self, assets, sigs):
        reg = FakeRegistry(assets)
        ti = FakeTypes(sigs)
        return reg, ti, mc.Peers(reg, ti)

    def test_fail_closed_cases(self):
        S, T = frozenset({(1, 0), (2, 3)}), frozenset({(1, 0), (9, 3)})
        peers = [mat(f'/G/P{i}') for i in range(3)] + [mat(f'/G/T{i}', 'BLEND_Translucent') for i in range(3)]
        sigs = {f'/G/P{i}': S for i in range(3)}
        sigs.update({f'/G/T{i}': T for i in range(3)})
        # (a) agree with >= MIN_PEERS -> readable
        reg, ti, pe = self._peers(peers + [mat('/G/C')], dict(sigs, **{'/G/C': S}))
        self.assertTrue(mc.base_props('/G/C', reg, ti, pe, {})['readable'])
        # (b) registry says opaque but the compiled types match translucent materials -> sources disagree
        reg, ti, pe = self._peers(peers + [mat('/G/C')], dict(sigs, **{'/G/C': T}))
        r = mc.base_props('/G/C', reg, ti, pe, {})
        self.assertFalse(r['readable'])
        self.assertIn('sources disagree', r['why'])
        # (c) too few peers
        reg, ti, pe = self._peers(peers[:2] + [mat('/G/C')], {'/G/P0': S, '/G/P1': S, '/G/C': S})
        self.assertIn('only 2 other', mc.base_props('/G/C', reg, ti, pe, {})['why'])
        # (d) mixed peers
        mixed = peers[:2] + [mat('/G/M0', 'BLEND_Masked'), mat('/G/C')]
        reg, ti, pe = self._peers(mixed, {'/G/P0': S, '/G/P1': S, '/G/M0': S, '/G/C': S})
        self.assertIn('differ', mc.base_props('/G/C', reg, ti, pe, {})['why'])
        # (e) no compiled signature -> source 2 missing
        reg, ti, pe = self._peers(peers + [mat('/G/C')], sigs)
        self.assertIn('source 2', mc.base_props('/G/C', reg, ti, pe, {})['why'])
        # (f) registry without class tags -> source 1 missing
        bare = {'package': '/G/C', 'class': mc.MATERIAL_AR_CLASS, 'tags': {}}
        reg, ti, pe = self._peers(peers + [bare], dict(sigs, **{'/G/C': S}))
        self.assertIn('source 1', mc.base_props('/G/C', reg, ti, pe, {})['why'])
        # (g) instance whose fetched header does not import the registry parent -> fail closed
        reg, ti, pe = self._peers(peers + [mat('/G/C'), mic('/G/MI', '/G/C')], dict(sigs, **{'/G/MI': S}))
        self.assertTrue(mc.base_props('/G/MI', reg, ti, pe, {})['readable'])
        r = mc.base_props('/G/MI', reg, ti, pe, {'/G/MI': {'imports': ['/G/Other']}})
        self.assertFalse(r['readable'])
        self.assertIn('not imported by its fetched header', r['why'])
        # (h) the candidate itself is never its own peer
        reg, ti, pe = self._peers(peers[:2] + [mat('/G/C')], {'/G/P0': S, '/G/P1': S, '/G/C': S})
        self.assertFalse(mc.base_props('/G/C', reg, ti, pe, {})['readable'])
        # (i) zero population -> refuse
        with self.assertRaises(mc.ClassError):
            mc.Peers(FakeRegistry([]), FakeTypes({}))

    def test_mod_class_two_readers_real(self):
        pk, _ = leon()
        pe = classes()[2]

        def blob(n):
            p = pk[n]
            return p.export_bytes([e for e in p.exports if e['outer'][0] == 'Null'][0])
        body, hair = mc.mod_props(blob('/Game/Leon/M_Leon_Body'), pe), mc.mod_props(blob('/Game/Leon/M_Leon_Hair'), pe)
        # review R2-1: opaque vs masked has no second reader -> fail closed, flagged for a user decision
        self.assertEqual((body['readable'], body.get('unproven_opaque')), (False, True))
        self.assertIn('not provable', body['why'])
        self.assertEqual((hair['readable'], hair['blend_group']), (True, 'masked'))   # masked IS provable
        self.assertTrue(mc.mod_props(blob('/Game/Leon/M_Leon_Skin'), pe).get('unproven_opaque'))
        self.assertGreater(body['type_hashes_found'], 0)
        b = blob('/Game/Leon/M_Leon_Body')
        self.assertFalse(mc.mod_props(b'OpaqueBasePass' + b'\0' * 64, pe)['readable'])        # no type hashes
        r = mc.mod_props(b.replace(b'OpaqueBasePass', b'TranslucentBasePass'), pe)
        self.assertFalse(r['readable'])                                                       # readers disagree
        self.assertIn('disagree', r['why'])
        self.assertFalse(mc.mod_props(b + b'TranslucentBasePass', pe)['readable'])           # both buffers
        self.assertFalse(mc.mod_props(b.replace(b'OpaqueBasePass', b'XpaqueBasePass'), pe)['readable'])
        # review R2-2: a blend group with too few game materials cannot be ruled out
        S = frozenset({(11, 0), (12, 3)})
        few = [mat(f'/G/O{i}') for i in range(12)] + [mat(f'/G/M{i}', 'BLEND_Masked') for i in range(12)] + \
              [mat('/G/T0', 'BLEND_Translucent')]
        fp = mc.Peers(FakeRegistry(few), FakeTypes({a['package']: S for a in few}))
        r = mc.mod_props(struct.pack('<QQ', 11, 12) + b'OpaqueBasePass', fp)
        self.assertFalse(r['readable'])
        self.assertIn('fewer than', r['why'])


class T14ParamRoles(unittest.TestCase):
    def test_role_of(self):
        cases = {'BaseColor': 'base_color', 'BaseColour': 'base_color', 'Base Color - Texture': 'base_color',
                 'Albedo': 'base_color', 'Normal': 'normal', 'Normal - Texture': 'normal', 'ORM': 'packed:ORM',
                 'MRO - Texture': 'packed:MRO', 'RMA': 'packed:RMA',
                 'Base Color - Tint': None, 'Diffuse Brightness': None, 'Normal Strenght': None,
                 'Wet Normal Overlay': None, 'Detail Normals': None, 'HairAO': None, 'AO': None, 'Mask': None,
                 'MRO - Occlusion Intensity': None}
        for n, want in cases.items():
            self.assertEqual(pr.role_of(n), want, n)

    def test_bindings_and_formats_real(self):
        pk, _ = leon()
        b, why = pr.texture_bindings(pk['/Game/Leon/MI_Leon_UpperBody'])
        self.assertEqual(b, {'BaseColor': '/Game/Leon/Textures/T_Leon_UpperBody_D',
                             'Normal': '/Game/Leon/Textures/T_Leon_UpperBody_N',
                             'ORM': '/Game/Leon/Textures/T_Leon_UpperBody_ORM'})
        n = 0
        for name, p in pk.items():
            if name.startswith('/Game/Leon/MI_'):
                b, why = pr.texture_bindings(p)
                self.assertTrue(b, (name, why))
                n += 1
        self.assertEqual(n, 11)
        self.assertEqual(pr.pixel_format(pk['/Game/Leon/Textures/T_Leon_UpperBody_N']), 'PF_BC5')
        self.assertEqual(pr.pixel_format(pk['/Game/Leon/Textures/T_Leon_UpperBody_D']), 'PF_DXT1')
        self.assertIsNone(pr.pixel_format(pk['/Game/Leon/MI_Leon_UpperBody']))
        # a base PS5 instance decodes the same way (second cook, same layout)
        shirt = zen.ZenPackage(rfile(fetched_file('/Game/Townfall/Characters/Simon/Materials/MI_Simon_Shirt')))
        b, _ = pr.texture_bindings(shirt)
        self.assertEqual(sorted(b), ['BaseColour', 'MRO', 'Normal'])
        # fail closed: header-only prefix has no export data; the material itself has no array
        hs = struct.unpack_from('<I', shirt.data, 4)[0]
        self.assertIsNone(pr.texture_bindings(zen.ZenPackage(shirt.data[:hs], header_only=True))[0])
        self.assertIsNone(pr.texture_bindings(pk['/Game/Leon/M_Leon_Body'])[0])

    def test_map_params(self):
        tags = {'/T/D': {'SRGB': 'True', 'CompressionSettings': 'TC_BC7'},
                '/T/N': {'SRGB': 'False', 'CompressionSettings': 'TC_Normalmap'},
                '/T/M': {'SRGB': 'False', 'CompressionSettings': 'TC_BC7'}}
        mod_bind = {'BaseColor': '/L/D', 'Nrm': '/L/N', 'ORM': '/L/M'}
        fmts = {'/L/D': 'PF_DXT1', '/L/N': 'PF_BC5', '/L/M': 'PF_DXT1'}
        base_names = ['BaseColour', 'Normal - Texture', 'MRO']
        base_bind = {'BaseColour': {'/T/D'}, 'Normal - Texture': {'/T/N'}, 'MRO': {'/T/M'}}
        m, lost = pr.map_params(['BaseColor', 'ORM'], mod_bind, fmts, base_names, base_bind, tags.get, set())
        self.assertEqual(m['BaseColor']['to'], 'BaseColour')
        self.assertEqual(m['BaseColor']['how'], 'role:base_color')
        self.assertIn('channel order has one source', lost['ORM'])          # packed never remapped
        m, lost = pr.map_params(['Normal - Texture'], {}, {}, base_names, base_bind, tags.get, set())
        self.assertEqual(m['Normal - Texture']['how'], 'exact')            # exact: no layout question
        m, lost = pr.map_params(['Normal - Texture'], {}, {}, base_names, {}, tags.get, set())
        self.assertIn('no fetched base instance binds', lost['Normal - Texture'])   # R2-4: slot type unverified
        mb = dict(mod_bind, Normal='/L/N')
        m, lost = pr.map_params(['Normal'], mb, fmts, base_names, base_bind, tags.get, set())
        self.assertEqual(m['Normal']['to'], 'Normal - Texture')
        # negatives: missing / wrong second source
        m, lost = pr.map_params(['BaseColor'], mod_bind, fmts, base_names, {}, tags.get, set())
        self.assertIn('second source', lost['BaseColor'])                  # no bound base texture
        m, lost = pr.map_params(['BaseColor'], mod_bind, {'/L/D': 'PF_BC5'}, base_names, base_bind, tags.get, set())
        self.assertIn('does not confirm', lost['BaseColor'])               # mod texture is a normal format
        bad = dict(tags, **{'/T/D': {'SRGB': 'False', 'CompressionSettings': 'TC_Masks'}})
        m, lost = pr.map_params(['BaseColor'], mod_bind, fmts, base_names, base_bind, bad.get, set())
        self.assertIn('BaseColor', lost)                                    # base texture is not colour
        two = {'BaseColour': {'/T/D'}, 'Albedo': {'/T/D'}}
        m, lost = pr.map_params(['BaseColor'], mod_bind, fmts, ['BaseColour', 'Albedo'], two, tags.get, set())
        self.assertIn('ambiguous', lost['BaseColor'])                       # two confirmed targets
        m, lost = pr.map_params(['BaseColor'], mod_bind, fmts, base_names, base_bind, tags.get, {'BaseColour'})
        self.assertIn('BaseColor', lost)                                    # rename would collide
        mb2 = dict(mod_bind, Albedo='/L/D')                                 # R2-3: two mod params, one target
        m, lost = pr.map_params(['BaseColor', 'Albedo'], mb2, fmts, base_names, base_bind, tags.get, set())
        self.assertEqual(m, {})
        self.assertIn('same base parameter', lost['Albedo'])
        m, lost = pr.map_params(['Wobble'], mod_bind, fmts, base_names, base_bind, tags.get, set())
        self.assertIn('no known role', lost['Wobble'])


def _plan_args(tmp, extra=()):
    L = LEON_TOC[:-5]
    return PY + [os.path.join(TOOLS, 'reparent_plan.py'), '--audit', AUDIT5, '--mod-utoc', LEON_TOC,
                 '--mod-ucas', L + '.ucas', '--checked-headers'] + CHECKED + [
        '--fetched-dir', FETCHED + '/headers-r1', FETCHED + '/headers-r2', '--base-header'] + ALL_CH + [
        '--asset-registry', AR_BIN, '--library'] + LIBS + ['--stinfo'] + STINFO + list(extra) + [
        '--out', os.path.join(tmp, 'plan.json')]


class T15RealDataRound2(unittest.TestCase):
    def test_reparent_plan_real_default_and_partial(self):
        tmp = tempfile.mkdtemp()
        r = subprocess.run(_plan_args(tmp), capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn('418 game materials in the class population', r.stdout)
        plan = jload(os.path.join(tmp, 'plan.json'))
        self.assertGreater(len(plan['candidates']), 0)
        self.assertNotIn(TUBES, plan['candidates'])                                  # translucent refused
        self.assertIn('translucent family', plan['excluded_class_detail'][TUBES])
        self.assertTrue(all(v['props']['readable'] for v in plan['candidates'].values()))
        self.assertEqual({r['action'] for r in plan['rows']}, {'known-checker'})    # partial needs the flag
        r = subprocess.run(_plan_args(tmp, ['--allow-partial']), capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        plan = jload(os.path.join(tmp, 'plan.json'))
        self.assertEqual({r['action'] for r in plan['rows']}, {'known-checker'})    # opaque not provable
        up = {x['mod_package']: x for x in plan['rows']}['/Game/Leon/MI_Leon_UpperBody']
        self.assertIn('not provable', up['why'])
        self.assertTrue(up['mod_class']['unproven_opaque'])
        r = subprocess.run(_plan_args(tmp, ['--allow-partial', '--accept-unproven-opaque']),
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        plan = jload(os.path.join(tmp, 'plan.json'))
        rows = {x['mod_package']: x for x in plan['rows']}
        fixed = [x for x in plan['rows'] if x['action'].startswith('reparent')]
        self.assertEqual(len(fixed), 10)
        for x in fixed:
            c = plan['candidates'][x['new_parent']]['props']
            self.assertEqual((c['domain'], c['blend_group'], c['lit']), ('MD_Surface', 'opaque', 'lit'))
            self.assertEqual(c['sources']['asset_registry'], c['sources']['shader_type_peers'])
            self.assertEqual(x['mod_class']['blend_group'], 'opaque')
            self.assertNotEqual(x['new_parent'], LIQUID)
            self.assertIn(LIQUID, x['refused_parents'])                             # masked refused for opaque
            self.assertEqual(x['param_renames'], {'BaseColor': 'BaseColour'})
            self.assertEqual(x['param_map']['Normal']['how'], 'exact')
            self.assertIn('ORM', x['lost_params'])
            self.assertEqual(x['coverage'], 0.667)
            self.assertTrue(any('USER DECISION' in n for n in x['look_notes']))
        hair = rows['/Game/Leon/MI_Leon_Hair']                                       # 1/4 < 0.5 -> stays
        self.assertEqual(hair['action'], 'known-checker')
        self.assertEqual(hair['mod_class']['blend_group'], 'masked')
        # the planned row is applicable with the REAL fetched parent header as second source
        pk, _ = leon()
        up = rows['/Game/Leon/MI_Leon_UpperBody']
        out, rec = mic_reparent.reparent(pk[up['mod_package']].data, up['old_parent'], up['new_parent'],
                                         up['new_export'], up['param_renames'],
                                         base_header=rfile(fetched_file(up['new_parent'])))
        self.assertIn('two sources', rec['new_public_hash_source'])
        self.assertTrue(rec['export_bytes_identical'])
        self.assertEqual(rec['store_entry_import_change']['new_package_id'], up['store_entry_repoint']['to_package_id'])
        b, _ = pr.texture_bindings(zen.ZenPackage(out))
        self.assertEqual(sorted(b), ['BaseColour', 'Normal', 'ORM'])

    def test_master_fetch_plan_real(self):
        tmp = tempfile.mkdtemp()
        out = os.path.join(tmp, 'r3.json')
        r = subprocess.run(PY + [os.path.join(TOOLS, 'master_fetch_plan.py'), '--asset-registry', AR_BIN,
                                 '--library'] + LIBS + ['--stinfo'] + STINFO + ['--base-header'] + ALL_CH +
                           ['--toc'] + BASE + ['--checked-headers'] + CHECKED +
                           ['--mod-utoc', LEON_TOC, '--mod-ucas', LEON_CAS, '--out', out],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        plan, p1, p2 = jload(out), jload(out[:-5] + '.p1.json'), jload(out[:-5] + '.p2.json')
        rows = plan['rows']
        self.assertGreater(len(rows), 0)
        self.assertEqual(len(p1['rows']) + len(p2['rows']), len(rows))
        self.assertGreater(len(p1['rows']), 0)
        fetched = {x['package'] for x in checked_rows()}
        pk, _ = leon()
        for x in rows:
            for k in ('remote_file', 'pieces', 'local_name', 'length', 'whole_chunk'):
                self.assertIn(k, x)
            self.assertEqual(sum(n for _o, n in x['pieces']), x['length'])
            self.assertEqual(x['expected_iohash20'] is not None, x['whole_chunk'])
            self.assertNotIn(x['package'], fetched)
            self.assertNotIn(x['package'], pk)
            if 'class' in x:
                self.assertIn(x['class']['blend'], ('BLEND_Opaque', 'BLEND_Masked'))
                self.assertEqual(x['class']['sources']['asset_registry'], x['class']['sources']['shader_type_peers'])
        self.assertTrue(all(x['skeletal_mesh_evidence_now'] for x in p1['rows']))
        self.assertTrue(all(x['whole_chunk'] for x in rows if 'master' in x['purpose']))
        self.assertEqual(len({x['local_name'] for x in rows}), len(rows))
        self.assertNotIn(TUBES, {x['package'] for x in rows})


if __name__ == '__main__':
    unittest.main(verbosity=2)
