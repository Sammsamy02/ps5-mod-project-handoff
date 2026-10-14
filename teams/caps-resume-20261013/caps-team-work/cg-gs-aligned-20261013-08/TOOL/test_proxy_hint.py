"""Regression for a forwarding WinMM proxy with no WinMM import."""

import hashlib
import os
import struct
import tempfile
import unittest
from pathlib import Path

from pe_analyzer import analyze_file


ROOT = Path(__file__).resolve().parent.parent
TRUEFOV = Path(os.environ.get("CAP13_TRUEFOV_DLL", str(ROOT / "SAMPLE" / "TrueFOV-upstream-winmm.dll")))


class ProxyHintTests(unittest.TestCase):
    def test_forwarding_winmm_proxy_is_detected_without_winmm_import(self):
        path = TRUEFOV
        if not path.is_file():
            self.skipTest("set CAP13_TRUEFOV_DLL to the unchanged upstream winmm.dll")
        self.assertEqual(
            hashlib.sha256(path.read_bytes()).hexdigest(),
            "5f23eee1c7479e75699d26d339ea69269eb0da0c33fc9be4cd60957ee63302a3",
        )
        _, report = analyze_file(str(path))
        self.assertEqual(report["exports"]["dll_name"].lower(), "winmm.dll")
        self.assertEqual(len(report["exports"]["forwarders"]), 181)
        self.assertEqual(sum(item.startswith("Ordinal#") for item in report["exports"]["forwarders"]), 1)
        self.assertFalse(any(row["dll"].lower() == "winmm.dll" for row in report["imports"]))
        self.assertTrue(report["ue4ss"]["looks_like_proxy_dll"])

    def test_ordinary_exporting_mod_is_not_marked_as_proxy(self):
        path = ROOT / "FIXTURES" / "cap13-ue4ss-native-mod-FIXTURE.dll"
        _, report = analyze_file(str(path))
        self.assertEqual(report["exports"]["forwarders"], [])
        self.assertFalse(report["ue4ss"]["looks_like_proxy_dll"])

    def test_ordinal_only_forwarders_still_mark_proxy(self):
        data = bytearray(TRUEFOV.read_bytes())
        image, _ = analyze_file(str(TRUEFOV))
        export_off = image.rva_to_offset(image._dir(0)["rva"])
        struct.pack_into("<I", data, export_off + 24, 0)  # NumberOfNames
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "ordinal-only-FIXTURE.dll"
            path.write_bytes(data)
            _, report = analyze_file(str(path))
        self.assertEqual(report["exports"]["names"], [])
        self.assertEqual(len(report["exports"]["forwarders"]), 181)
        self.assertTrue(report["ue4ss"]["looks_like_proxy_dll"])


if __name__ == "__main__":
    unittest.main()
