import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

HERE = Path(__file__).resolve().parent
SOURCE = HERE.parents[2] / 'outputs/2026-10-08-l25-prep/pak-overlay'
spec = importlib.util.spec_from_file_location('projection_fixture', SOURCE/'test_overlay_builder.py')
fixture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fixture)
api = __import__('overlay_builder')
try:
    import project_packages as projection
except ModuleNotFoundError:
    projection = None


class ProjectionTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(projection, 'package projection implementation is missing')
        (HERE/'tmp').mkdir(exist_ok=True)
        self.tmp = tempfile.TemporaryDirectory(dir=HERE/'tmp')
        self.addCleanup(self.tmp.cleanup)
        self.folder = Path(self.tmp.name)
        self.pid_a, self.pid_b = 0x1001, 0x1002
        cid = struct.pack('<Q', 500)
        self.header = fixture.make_container_header_bytes(500, {
            self.pid_a: ((self.pid_b, 0x9001), ()), self.pid_b: ((), ())})
        self.payloads = {
            fixture.chunk_id_for(self.pid_a): b'A'*70000,
            fixture.chunk_id_for(self.pid_a, chunk_type=2): b'BULK A'*200,
            fixture.chunk_id_for(self.pid_b): b'B'*180000,
        }
        chunks = [(cid+bytes.fromhex('00000006'), self.header)] + list(self.payloads.items())
        files = [('../../../Game/Content/a.uasset', fixture.chunk_id_for(self.pid_a)),
                 ('../../../Game/Content/a.ubulk', fixture.chunk_id_for(self.pid_a, chunk_type=2)),
                 ('../../../Game/Content/b.uasset', fixture.chunk_id_for(self.pid_b))]
        archive = fixture.SyntheticArchive(cid, chunks, files)
        self.utoc, self.ucas = self.folder/'original.utoc', self.folder/'original.ucas'
        self.utoc.write_bytes(archive.utoc); self.ucas.write_bytes(archive.ucas)
        self.mod = api.ModSpec('testmod', str(self.utoc), str(self.ucas))

    def build(self, keep):
        return projection.project(api, self.mod, keep)

    def test_selected_export_and_bulk_are_exact_omitted_package_absent(self):
        utoc, ucas, receipt = self.build({self.pid_a})
        toc = api.parse_toc(utoc)
        self.assertEqual(len(toc.ids), 3)
        for cid, payload in self.payloads.items():
            if int.from_bytes(cid[:8], 'little') == self.pid_a:
                self.assertEqual(api.extract_chunk_bytes(toc, toc.ids.index(cid), ucas), payload)
            else:
                self.assertNotIn(cid, toc.ids)
        self.assertEqual(receipt['selected_package_ids'], ['0000000000001001'])
        self.assertFalse(receipt['runtime_mount_ready'])

    def test_subset_header_preserves_import_order_and_exposes_external_imports(self):
        utoc, ucas, receipt = self.build({self.pid_a})
        toc = api.parse_toc(utoc)
        cid = toc.container_id+bytes.fromhex('00000006')
        header = api.parse_container_header(api.extract_chunk_bytes(toc, toc.ids.index(cid), ucas))
        self.assertEqual(set(header.packages), {self.pid_a})
        self.assertEqual(header.packages[self.pid_a].imported_packages, (self.pid_b, 0x9001))
        self.assertEqual(receipt['imports_outside_projection'], ['0000000000001002', '0000000000009001'])

    def test_projection_is_smaller_and_inputs_unchanged(self):
        before = (self.utoc.read_bytes(), self.ucas.read_bytes())
        utoc, ucas, receipt = self.build({self.pid_a})
        self.assertLess(len(ucas), 100000)
        self.assertGreater(len(before[1]), 250000)
        self.assertEqual(before, (self.utoc.read_bytes(), self.ucas.read_bytes()))
        toc = api.parse_toc(utoc)
        for cid in self.payloads:
            if cid in toc.ids:
                self.assertEqual(toc.meta(toc.ids.index(cid)), self.mod.toc.meta(self.mod.toc.ids.index(cid)))

    def test_empty_selection_refuses(self):
        with self.assertRaisesRegex(ValueError, 'empty_package_selection'):
            self.build(set())

    def test_unknown_selection_refuses(self):
        with self.assertRaisesRegex(ValueError, 'selected_package_not_in_source'):
            self.build({0xdead})

    def test_source_drift_refuses_before_generation(self):
        self.ucas.write_bytes(self.ucas.read_bytes()+b'x')
        with self.assertRaisesRegex(ValueError, 'source_identity_changed'):
            self.build({self.pid_a})

    def test_all_package_projection_preserves_every_payload(self):
        utoc, ucas, receipt = self.build({self.pid_a, self.pid_b})
        toc = api.parse_toc(utoc)
        for cid, payload in self.payloads.items():
            self.assertEqual(api.extract_chunk_bytes(toc, toc.ids.index(cid), ucas), payload)
        self.assertEqual(receipt['omitted_package_count'], 0)

    def test_projection_composes_with_existing_overlay_without_lost_bulk(self):
        utoc, ucas, _ = self.build({self.pid_a})
        projected_toc = self.folder/'subset.utoc'
        projected_ucas = self.folder/'subset.ucas'
        projected_toc.write_bytes(utoc); projected_ucas.write_bytes(ucas)
        projected = api.ModSpec('subset', str(projected_toc), str(projected_ucas))
        base, _, _ = fixture.synth_base(str(self.folder/'synthetic-base'))
        base_toc = api.parse_toc(base.utoc)
        header_id = base_toc.container_id + bytes.fromhex('00000006')
        base_header = api.extract_chunk_bytes(base_toc, base_toc.ids.index(header_id), base.ucas)
        combined, sidecar, _ = api.build_overlay(base.utoc, len(base.ucas), base_header, [projected])
        toc = api.parse_toc(combined)
        api.validate_lookup(toc)
        view = fixture._view(sidecar, len(base.ucas))
        for cid, payload in self.payloads.items():
            if int.from_bytes(cid[:8], 'little') == self.pid_a:
                self.assertEqual(api.extract_chunk_bytes(toc, toc.ids.index(cid), view), payload)
        merged = api.parse_container_header(api.extract_chunk_bytes(toc, toc.ids.index(header_id), view))
        self.assertEqual(merged.packages[self.pid_a].imported_packages, (self.pid_b, 0x9001))

    def test_nonpackage_chunk_refuses_instead_of_silently_omitting(self):
        header_id = self.mod.header_chunk_id
        chunks = [(header_id, self.header)] + list(self.payloads.items())
        chunks.append((fixture.chunk_id_for(0x9999, chunk_type=8), b'shader-library'))
        archive = fixture.SyntheticArchive(self.mod.toc.container_id, chunks,
            [('../../../Game/Content/a.uasset', fixture.chunk_id_for(self.pid_a))])
        self.utoc.write_bytes(archive.utoc); self.ucas.write_bytes(archive.ucas)
        self.mod = api.ModSpec('with-nonpackage', str(self.utoc), str(self.ucas))
        with self.assertRaisesRegex(ValueError, 'unsupported_nonpackage_chunk'):
            self.build({self.pid_a})

    def test_container_header_identity_mismatch_refuses(self):
        mismatched = fixture.make_container_header_bytes(999, {
            self.pid_a: ((self.pid_b, 0x9001), ()), self.pid_b: ((), ())})
        archive = fixture.SyntheticArchive(self.mod.toc.container_id,
            [(self.mod.header_chunk_id, mismatched)] + list(self.payloads.items()),
            [('../../../Game/Content/a.uasset', fixture.chunk_id_for(self.pid_a))])
        self.utoc.write_bytes(archive.utoc); self.ucas.write_bytes(archive.ucas)
        self.mod = api.ModSpec('mismatched', str(self.utoc), str(self.ucas))
        with self.assertRaisesRegex(ValueError, 'source_container_header_identity_mismatch'):
            self.build({self.pid_a})

    def test_invalid_source_lookup_refuses_instead_of_repairing(self):
        original = self.utoc.read_bytes()
        head = bytearray(original[:144])
        struct.pack_into('<I', head, 84, 1)
        at = 144 + self.mod.toc.chunk_count * 22
        self.utoc.write_bytes(bytes(head) + original[144:at] + bytes(4) + original[at:])
        self.mod = api.ModSpec('bad-lookup', str(self.utoc), str(self.ucas))
        with self.assertRaisesRegex(ValueError, 'chunk_lookup_does_not_resolve_all_rows'):
            self.build({self.pid_a})


if __name__ == '__main__':
    unittest.main()
