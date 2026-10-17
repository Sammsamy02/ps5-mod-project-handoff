"""Tests use the existing synthetic TOC writer; no preserved input is written."""
import hashlib
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
SOURCE = ROOT / 'outputs/2026-10-08-l25-prep/pak-overlay'
spec = importlib.util.spec_from_file_location('cap7_fixture', SOURCE / 'test_overlay_builder.py')
fixture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fixture)
try:
    import discover_containers as discovery
except ModuleNotFoundError:
    discovery = None


class DiscoveryTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(discovery, 'container-discovery implementation is missing')
        self.tmp = tempfile.TemporaryDirectory(dir=HERE / 'tmp')
        self.addCleanup(self.tmp.cleanup)
        self.folder = Path(self.tmp.name)

    def toc(self, name, packages, identity=700, chunk_type=1):
        cid = struct.pack('<Q', identity)
        header = fixture.make_container_header_bytes(identity, {p: ((), ()) for p in packages})
        chunks = [(cid + bytes.fromhex('00000006'), header)]
        files = []
        for pid in packages:
            chunk = fixture.chunk_id_for(pid, chunk_type=chunk_type)
            chunks.append((chunk, b'package payload'))
            files.append(('../../../Game/Content/%x.uasset' % pid, chunk))
        archive = fixture.SyntheticArchive(cid, chunks, files)
        path = self.folder / name
        path.write_bytes(archive.utoc)
        return path

    def plan(self, bases, mods):
        return discovery.discover(bases, mods, fixture.parse_toc)

    def test_renamed_container_and_inputs_unchanged(self):
        base = self.toc('arbitrary-assets.utoc', [1, 2])
        mod = self.toc('costume.utoc', [2, 3], identity=900)
        before = {p: hashlib.sha256(p.read_bytes()).hexdigest() for p in (base, mod)}
        result = self.plan([base], [mod])
        self.assertEqual(result['result'], 'candidate_selected')
        self.assertEqual(result['targets'][0]['index_name'], 'arbitrary-assets.utoc')
        self.assertEqual(result['targets'][0]['sidecar_name'], 'arbitrary-assets_s1.ucas')
        self.assertEqual(result['targets'][0]['matched_package_count'], 1)
        self.assertEqual(result['mods'][0]['unmatched_package_count'], 1)
        self.assertFalse(result['runtime_mount_ready'])
        self.assertEqual(before, {p: hashlib.sha256(p.read_bytes()).hexdigest() for p in (base, mod)})

    def test_ambiguous_owner_refuses(self):
        a = self.toc('a.utoc', [1])
        b = self.toc('b.utoc', [1], identity=701)
        mod = self.toc('m.utoc', [1], identity=900)
        self.assertEqual(self.plan([a, b], [mod])['code'], 'ambiguous_package_owner')

    def test_multi_container_composition_refuses(self):
        a = self.toc('a.utoc', [1])
        b = self.toc('b.utoc', [2], identity=701)
        mod = self.toc('m.utoc', [1, 2], identity=900)
        result = self.plan([a, b], [mod])
        self.assertEqual(result['code'], 'multi_container_overlay_required')
        self.assertEqual(len(result['targets']), 2)

    def test_new_only_mod_has_no_arbitrary_destination(self):
        a = self.toc('a.utoc', [1])
        mod = self.toc('m.utoc', [2], identity=900)
        self.assertEqual(self.plan([a], [mod])['code'], 'no_base_package_match')

    def test_non_package_chunks_do_not_select_destination(self):
        a = self.toc('a.utoc', [1], chunk_type=9)
        mod = self.toc('m.utoc', [1], identity=900)
        self.assertEqual(self.plan([a], [mod])['code'], 'no_base_package_match')

    def test_empty_catalogue_refuses(self):
        mod = self.toc('m.utoc', [1], identity=900)
        self.assertEqual(self.plan([], [mod])['code'], 'empty_base_catalogue')

    def test_unsupported_container_is_not_silently_skipped(self):
        a = self.toc('a.utoc', [1])
        bad = self.folder / 'bad.utoc'
        bad.write_bytes(b'not an admitted container')
        mod = self.toc('m.utoc', [1], identity=900)
        self.assertEqual(self.plan([a, bad], [mod])['code'], 'toc_parse_refused')

    def test_order_is_deterministic(self):
        a = self.toc('a.utoc', [1])
        b = self.toc('b.utoc', [2], identity=701)
        mod = self.toc('m.utoc', [1], identity=900)
        self.assertEqual(self.plan([a, b], [mod]), self.plan([b, a], [mod]))


if __name__ == '__main__':
    unittest.main()
