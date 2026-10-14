"""CityHash64 / BLAKE3 self-checks against values stored in the real files."""
import struct
import unittest

from common import (MOD2K_UTOC, MOD2K_UCAS, record, require_inputs)
import iostore_lib as io
from cityhash64 import cityhash64, fname_hash
import blake3_min


class TestHashes(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.toc = io.parse_toc(MOD2K_UTOC)

    def setUp(self):
        require_inputs(self, [MOD2K_UTOC, MOD2K_UCAS])

    def packages(self):
        with open(MOD2K_UCAS, "rb") as fh:
            for i in range(self.toc.entry_count):
                if self.toc.chunk_type(i) == 1:
                    yield i, io.read_chunk(self.toc, MOD2K_UCAS, i, fh)

    def test_cityhash_matches_every_stored_fname_hash(self):
        n = 0
        for i, b in self.packages():
            r = io.Reader(b, 60)  # 52-byte summary + 2 cell offsets (v5 layout)
            num = r.u32(); r.u32(); self.assertEqual(r.u64(), io.FNAME_HASH_ALGORITHM_ID)
            stored = [r.u64() for _ in range(num)]
            names = io.parse_zen(b).names
            for name, h in zip(names, stored):
                self.assertEqual(fname_hash(name), h, name)
                n += 1
        self.assertGreater(n, 10000)
        record("cityhash_fname_vectors", n)

    def test_package_id_and_public_export_hash_rules(self):
        store = [io.parse_container_header(io.read_chunk(self.toc, MOD2K_UCAS, i))
                 for i in range(self.toc.entry_count) if self.toc.chunk_type(i) == 6][0]["entries"]
        n_pid = n_exp = n_order = 0
        for i, b in self.packages():
            z = io.parse_zen(b)
            pid = self.toc.chunk_u64(i)
            self.assertEqual(cityhash64(z.name.lower().encode("utf-16-le")), pid)
            n_pid += 1
            ids = [cityhash64(x.lower().encode("utf-16-le")) for x in z.imported_package_names]
            self.assertEqual(ids, store[pid]["imports"])
            n_order += 1
            for e in z.exports:
                if e["public_export_hash"]:
                    self.assertEqual(cityhash64(e["name"].lower().encode("utf-16-le")), e["public_export_hash"])
                    n_exp += 1
        self.assertEqual(n_pid, 127)
        record("package_id_rule", n_pid)
        record("store_import_order_rule", n_order)
        record("public_export_hash_rule", n_exp)

    def test_blake3_known_vectors_and_toc_iohashes(self):
        self.assertEqual(blake3_min.blake3(b"").hex(),
                         "af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262")
        self.assertEqual(blake3_min.blake3(b"abc").hex(),
                         "6437b3ac38465133ffb63b75273a8db548c558465d79db03fd359c6cd5bd9d85")
        n = 0
        with open(MOD2K_UCAS, "rb") as fh:
            for i in range(self.toc.entry_count):
                if self.toc.offlens[i][1] <= 140000:
                    d = io.read_chunk(self.toc, MOD2K_UCAS, i, fh)
                    self.assertEqual(blake3_min.io_hash20(d), self.toc.metas[i][0], i)
                    n += 1
        self.assertGreater(n, 100)
        record("blake3_toc_iohash_vectors", n + 2)


if __name__ == "__main__":
    unittest.main()
