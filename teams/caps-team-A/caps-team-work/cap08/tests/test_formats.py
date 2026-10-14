#!/usr/bin/env python3
"""Unit tests for ue_formats.py (CityHash64, BLAKE3/IoHash, TOC, container header, Zen, ScriptObjects)."""
import os
import struct
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import fixture_builder as fb  # noqa: E402
import ue_formats as uf  # noqa: E402

OVERLAY_BUILDER = "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/pak-overlay"


class Blake3Tests(unittest.TestCase):
    def test_empty_vector(self):
        # Official BLAKE3 test vector for the empty input.
        self.assertEqual(uf.blake3(b"").hex(),
                         "af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262")

    def test_cross_check_independent_implementation(self):
        """Second source: the retained overlay_builder.py BLAKE3 (different code)."""
        if not os.path.isdir(OVERLAY_BUILDER):
            self.skipTest("overlay_builder not present")
        sys.path.insert(0, OVERLAY_BUILDER)
        import overlay_builder as ob
        checked = 0
        for n in (0, 1, 63, 64, 65, 1023, 1024, 1025, 2047, 2048, 3073, 8192, 70001):
            data = bytes((i * 7 + n) & 0xFF for i in range(n))
            self.assertEqual(ob.blake3_20(data), uf.io_hash(data), n)
            checked += 1
        print("  blake3 cross-check sizes:", checked)
        self.assertGreater(checked, 0)


class CityHashTests(unittest.TestCase):
    def test_empty_is_k2(self):
        self.assertEqual(uf.cityhash64(b""), 0x9AE16A3B2F90404F)

    def test_all_length_classes_run(self):
        for n in (1, 3, 4, 7, 8, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127, 128, 129, 300):
            v = uf.cityhash64(bytes(range(256))[:n] if n <= 256 else bytes(n))
            self.assertTrue(0 <= v < 1 << 64)


class TocTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.d = self.tmp.name

    def tearDown(self):
        self.tmp.cleanup()

    def _toc(self, **kw):
        utoc, ucas = fb.write_container(os.path.join(self.d, "c"), 0x1234,
                                        [(fb.chunk_id(5, 0, 1), b"A" * 70000, "Game/Content/x.uasset"),
                                         (fb.chunk_id(6, 0, 2), b"B" * 10, None)], **kw)
        return utoc, ucas

    def test_roundtrip_and_hash(self):
        utoc, ucas = self._toc()
        t = uf.parse_toc(uf.read_bytes(utoc))
        r = uf.ChunkReader(t, ucas)
        self.assertEqual(r.read_verified(0), b"A" * 70000)
        self.assertEqual(r.read_verified(1), b"B" * 10)
        self.assertEqual(t.files[0], "Game/Content/x.uasset")
        self.assertEqual(len(t.physical_ranges(0)), 2)

    def test_refuse_wrong_version(self):
        utoc, _ = self._toc(version=7)
        with self.assertRaises(uf.FormatError):
            uf.parse_toc(uf.read_bytes(utoc))

    def test_refuse_compression_and_encryption(self):
        utoc, _ = self._toc(compress_names=1)
        with self.assertRaises(uf.FormatError):
            uf.parse_toc(uf.read_bytes(utoc))
        utoc, _ = self._toc(flags=8 | 2)
        with self.assertRaises(uf.FormatError):
            uf.parse_toc(uf.read_bytes(utoc))

    def test_refuse_truncated(self):
        utoc, _ = self._toc()
        data = uf.read_bytes(utoc)
        with self.assertRaises(uf.FormatError):
            uf.parse_toc(data[:-5])

    def test_hash_mismatch_detected(self):
        utoc, ucas = self._toc()
        b = bytearray(uf.read_bytes(ucas))
        b[5] ^= 1
        open(ucas, "wb").write(bytes(b))
        t = uf.parse_toc(uf.read_bytes(utoc))
        with self.assertRaises(uf.FormatError):
            uf.ChunkReader(t, ucas).read_verified(0)


class ContainerHeaderTests(unittest.TestCase):
    def test_roundtrip(self):
        h = fb.container_header(77, [(1, [2, 3], 1), (2, [], 0), (3, [1], 2)])
        p = uf.parse_container_header(h)
        self.assertEqual(p.container_id, 77)
        self.assertEqual(p.store[1]["imported_packages"], [2, 3])
        self.assertEqual(len(p.store[3]["shader_map_hashes"]), 2)
        self.assertEqual(p.store[2], {"imported_packages": [], "shader_map_hashes": []})

    def test_refuse_signature_and_version(self):
        h = bytearray(fb.container_header(77, [(1, [], 0)]))
        bad = bytes(h[:4][::-1]) + bytes(h[4:])
        with self.assertRaises(uf.FormatError):
            uf.parse_container_header(bad)
        h2 = bytearray(h)
        struct.pack_into("<I", h2, 4, 4)
        with self.assertRaises(uf.FormatError):
            uf.parse_container_header(bytes(h2))

    def test_refuse_array_view_outside_buffer(self):
        h = bytearray(fb.container_header(77, [(1, [2], 0)]))
        # entry 0 starts after: 16 header + 4 + 8 ids + 4 size  -> offset 32; set rel huge
        struct.pack_into("<I", h, 32 + 4, 1 << 20)
        with self.assertRaises(uf.FormatError):
            uf.parse_container_header(bytes(h))


class ZenTests(unittest.TestCase):
    def _pkg(self):
        mat = fb.script_index("/Script/Engine.Material")
        return fb.Pkg("/Game/T/M_A", [{"name": "M_A", "cls": mat, "payload": b"xyz"}],
                      imports=[mat, fb.package_import(0, 0)],
                      imported_packages=[("/Game/T/Tex", [0x55])], bulk_sizes=[16, 8])

    def test_roundtrip(self):
        d = fb.build_zen(self._pkg())
        z = uf.parse_zen_package(d)
        self.assertEqual(z.name, "/Game/T/M_A")
        self.assertEqual(z.imported_package_names, ["/Game/T/Tex"])
        self.assertEqual(z.imported_public_export_hashes, [0x55])
        self.assertEqual(uf.export_data(d, z, z.exports[0]), b"xyz")
        self.assertEqual([b[2] for b in z.bulk_data], [16, 8])
        self.assertEqual(uf.package_import_ref(z.import_map[1]), (0, 0))

    def test_refuse_versioned(self):
        d = bytearray(fb.build_zen(self._pkg()))
        struct.pack_into("<I", d, 0, 1)
        with self.assertRaises(uf.FormatError):
            uf.parse_zen_package(bytes(d))

    def test_refuse_nonmonotonic(self):
        d = bytearray(fb.build_zen(self._pkg()))
        struct.pack_into("<i", d, 28, 10)  # import map offset before name map end
        with self.assertRaises(uf.FormatError):
            uf.parse_zen_package(bytes(d))

    def test_refuse_bad_public_hash_index(self):
        mat = fb.script_index("/Script/Engine.Material")
        p = fb.Pkg("/Game/T/M_B", [{"name": "M_B", "cls": mat, "payload": b"x"}],
                   imports=[fb.package_import(0, 5)], imported_packages=[("/Game/T/Tex", [0x55])])
        with self.assertRaises(uf.FormatError):
            uf.parse_zen_package(fb.build_zen(p))


class ScriptObjectsTests(unittest.TestCase):
    def test_paths(self):
        with tempfile.TemporaryDirectory() as d:
            utoc, ucas = fb.write_script_objects(os.path.join(d, "global"), ["/Script/Engine.Material"])
            t = uf.parse_toc(uf.read_bytes(utoc))
            so = uf.parse_script_objects(uf.ChunkReader(t, ucas).read_verified(0))
            self.assertEqual(so.path(fb.script_index("/Script/Engine.Material")), "/Script/Engine.Material")
            self.assertIsNone(so.path(fb.script_index("/Script/Engine.Nope")))


if __name__ == "__main__":
    unittest.main(verbosity=2)
