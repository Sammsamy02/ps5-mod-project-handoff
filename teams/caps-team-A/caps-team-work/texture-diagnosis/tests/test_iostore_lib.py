"""Reader correctness: real data cross-checked against the pinned retoc binary
(an independent implementation), plus fail-closed refusal cases."""
import os
import struct
import subprocess
import unittest

from common import (MOD2K_UTOC, MOD2K_UCAS, MOD2K_PAK, GLOBAL_UTOC, GLOBAL_UCAS, BASE_UTOCS, RETOC,
                    fresh_tmp, record, require_inputs)
import iostore_lib as io
from zen_import_rewrite import split_number


class TestReaderVsRetoc(unittest.TestCase):
    def setUp(self):
        require_inputs(self, [MOD2K_UTOC, MOD2K_UCAS, MOD2K_PAK, GLOBAL_UTOC, GLOBAL_UCAS, RETOC])

    def test_exports_and_classes_match_retoc_legacy(self):
        tmp = fresh_tmp("retoc-legacy")
        inp = os.path.join(tmp, "in")
        out = os.path.join(tmp, "out")
        os.makedirs(inp)
        for src in (MOD2K_UTOC, MOD2K_UCAS, MOD2K_PAK, GLOBAL_UTOC, GLOBAL_UCAS):
            os.symlink(src, os.path.join(inp, os.path.basename(src)))
        p = subprocess.run([RETOC, "to-legacy", "--no-shaders", inp, out], capture_output=True, text=True)
        self.assertEqual(p.returncode, 0, p.stderr[-2000:])
        script = io.load_script_objects(GLOBAL_UTOC, GLOBAL_UCAS)
        toc = io.parse_toc(MOD2K_UTOC)
        n = 0
        with open(MOD2K_UCAS, "rb") as fh:
            for i in range(toc.entry_count):
                if toc.chunk_type(i) != 1:
                    continue
                z = io.parse_zen(io.read_chunk(toc, MOD2K_UCAS, i, fh))
                rel = toc.paths[i]
                stem = os.path.join(out, rel.rsplit(".", 1)[0])
                uexp = os.path.getsize(stem + ".uexp")
                # legacy .uexp = all export payloads + 4-byte package tag
                self.assertEqual(uexp, sum(e["size"] for e in z.exports) + 4, rel)
                legacy = open(stem + ".uasset", "rb").read()
                for e in z.exports:
                    cls = io.resolve_index(z, e["class"], script).rsplit(".", 1)[-1]
                    self.assertIn(cls.encode(), legacy, (rel, cls))
                    self.assertIn(split_number(e["name"])[0].encode(), legacy)  # FName base (number stored apart)
                n += 1
        self.assertEqual(n, 127)
        record("reader_vs_retoc_packages", n)

    def test_script_objects_cover_every_class_reference(self):
        script = io.load_script_objects(GLOBAL_UTOC, GLOBAL_UCAS)
        toc = io.parse_toc(MOD2K_UTOC)
        n = 0
        with open(MOD2K_UCAS, "rb") as fh:
            for i in range(toc.entry_count):
                if toc.chunk_type(i) == 1:
                    z = io.parse_zen(io.read_chunk(toc, MOD2K_UCAS, i, fh))
                    for e in z.exports:
                        self.assertTrue(io.resolve_index(z, e["class"], script).startswith("/Script/"))
                        n += 1
        self.assertGreater(n, 127)
        record("class_refs_resolved", n)

    def test_base_tocs_parse(self):
        require_inputs(self, BASE_UTOCS)
        n = 0
        for p in BASE_UTOCS:
            t = io.parse_toc(p)
            self.assertEqual(t.version, 8)
            self.assertGreater(len(t.paths), 0)
            n += len(t.paths)
        self.assertGreater(n, 50000)
        record("base_toc_paths", n)


class TestRefusals(unittest.TestCase):
    def setUp(self):
        require_inputs(self, [MOD2K_UTOC, MOD2K_UCAS])
        toc = io.parse_toc(MOD2K_UTOC)
        i = next(i for i in range(toc.entry_count) if toc.chunk_type(i) == 1)
        self.pkg = io.read_chunk(toc, MOD2K_UCAS, i)
        self.utoc = open(MOD2K_UTOC, "rb").read()

    def _toc_variant(self, mutate):
        tmp = fresh_tmp("toc-variant")
        b = bytearray(self.utoc)
        mutate(b)
        p = os.path.join(tmp, "x.utoc")
        open(p, "wb").write(bytes(b))
        return p

    def test_refusals(self):
        cases = 0
        with self.assertRaises(io.FormatError):  # truncated summary
            io.parse_zen(self.pkg[:40])
        cases += 1
        b = bytearray(self.pkg); struct.pack_into("<I", b, 0, 1)  # versioned package
        with self.assertRaises(io.FormatError):
            io.parse_zen(bytes(b))
        cases += 1
        b = bytearray(self.pkg); b += b"XXXX"  # trailing bytes after the package tag
        with self.assertRaises(io.FormatError):
            io.parse_zen(bytes(b))
        cases += 1
        with self.assertRaises(io.FormatError):  # TOC version != 8
            io.parse_toc(self._toc_variant(lambda b: b.__setitem__(16, 9)))
        cases += 1
        with self.assertRaises(io.FormatError):  # encrypted flag
            io.parse_toc(self._toc_variant(lambda b: b.__setitem__(80, b[80] | 2)))
        cases += 1
        with self.assertRaises(io.FormatError):  # bad magic
            io.parse_toc(self._toc_variant(lambda b: b.__setitem__(0, 0)))
        cases += 1
        record("reader_refusal_cases", cases)


if __name__ == "__main__":
    unittest.main()
