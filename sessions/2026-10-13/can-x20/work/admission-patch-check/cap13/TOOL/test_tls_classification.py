"""Read-only PE TLS feature classification against known image bytes."""

import hashlib
import os
import struct
import tempfile
import unittest
from pathlib import Path

from pe_analyzer import analyze_file


ROOT = Path(__file__).resolve().parent.parent
TRUEFOV = Path(os.environ.get("CAP13_TRUEFOV_DLL", str(ROOT / "SAMPLE" / "TrueFOV-upstream-winmm.dll")))


class TlsClassificationTests(unittest.TestCase):
    def test_no_tls_directory_reports_none(self):
        _, report = analyze_file(str(ROOT / "FIXTURES" / "cap13-ue4ss-native-mod-FIXTURE.dll"))
        self.assertEqual(report["tls"]["status"], "none")

    def test_linker_artifact_directory_reports_empty(self):
        path = ROOT / "SAMPLE" / "cap13-mod-splitscreen.dll"
        self.assertEqual(
            hashlib.sha256(path.read_bytes()).hexdigest(),
            "73ed26e406b425c06a5f32706802297601343f53b4e369fcf470f8fc3067c6c7",
        )
        _, report = analyze_file(str(path))
        self.assertEqual(report["tls"]["status"], "empty")
        self.assertEqual(report["tls"]["initialized_bytes"], 0)
        self.assertEqual(report["tls"]["callback_count"], 0)

    def test_truefov_reports_real_tls_data_and_empty_callback_table(self):
        path = TRUEFOV
        if not path.is_file():
            self.skipTest("set CAP13_TRUEFOV_DLL to the unchanged upstream winmm.dll")
        self.assertEqual(
            hashlib.sha256(path.read_bytes()).hexdigest(),
            "5f23eee1c7479e75699d26d339ea69269eb0da0c33fc9be4cd60957ee63302a3",
        )
        _, report = analyze_file(str(path))
        self.assertEqual(report["tls"]["status"], "real")
        self.assertEqual(report["tls"]["initialized_bytes"], 8)
        self.assertEqual(report["tls"]["zero_fill_bytes"], 0)
        self.assertTrue(report["tls"]["callback_table_present"])
        self.assertEqual(report["tls"]["callback_count"], 0)

    def test_truncated_tls_directory_reports_malformed(self):
        source = (ROOT / "SAMPLE" / "cap13-mod-splitscreen.dll").read_bytes()
        data = bytearray(source)
        pe_off = struct.unpack_from("<I", data, 0x3C)[0]
        opt_off = pe_off + 4 + 20
        tls_dir_size_off = opt_off + 112 + 9 * 8 + 4
        struct.pack_into("<I", data, tls_dir_size_off, 4)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "bad-tls-FIXTURE.dll"
            path.write_bytes(data)
            _, report = analyze_file(str(path))
        self.assertEqual(report["tls"]["status"], "malformed")


if __name__ == "__main__":
    unittest.main()
