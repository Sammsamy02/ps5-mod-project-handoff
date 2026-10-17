"""PE CodeView identity is image-specific and read without loading a PDB."""

import hashlib
import os
import struct
import tempfile
import unittest
from pathlib import Path

from pe_analyzer import analyze_file


ROOT = Path(__file__).resolve().parent.parent
TRUEFOV = Path(os.environ.get("CAP13_TRUEFOV_DLL", str(ROOT / "SAMPLE" / "TrueFOV-upstream-winmm.dll")))


class DebugIdentityTests(unittest.TestCase):
    def test_truefov_image_reports_its_own_codeview_pdb_identity(self):
        if not TRUEFOV.is_file():
            self.skipTest("set CAP13_TRUEFOV_DLL to the unchanged upstream winmm.dll")
        self.assertEqual(
            hashlib.sha256(TRUEFOV.read_bytes()).hexdigest(),
            "5f23eee1c7479e75699d26d339ea69269eb0da0c33fc9be4cd60957ee63302a3",
        )
        _, report = analyze_file(str(TRUEFOV))
        self.assertEqual(report["debug"]["status"], "rsds")
        self.assertEqual(
            report["debug"]["codeview"],
            [{"guid": "af9cfae1-a970-4477-a17b-60c21eb6f6c1", "age": 1,
              "pdb_name": "winmm.pdb"}],
        )

    def test_image_without_debug_directory_reports_none(self):
        _, report = analyze_file(str(ROOT / "FIXTURES" / "cap13-ue4ss-native-mod-FIXTURE.dll"))
        self.assertEqual(report["debug"], {"status": "none", "codeview": []})

    def test_truncated_rsds_record_is_malformed(self):
        if not TRUEFOV.is_file():
            self.skipTest("set CAP13_TRUEFOV_DLL to the unchanged upstream winmm.dll")
        data = bytearray(TRUEFOV.read_bytes())
        image, _ = analyze_file(str(TRUEFOV))
        directory = image._dir(6)
        debug_entry = image.rva_to_offset(directory["rva"])
        struct.pack_into("<I", data, debug_entry + 16, 8)  # CodeView SizeOfData
        with tempfile.TemporaryDirectory() as directory_name:
            path = Path(directory_name) / "short-rsds-FIXTURE.dll"
            path.write_bytes(data)
            _, report = analyze_file(str(path))
        self.assertEqual(report["debug"]["status"], "malformed")

    def test_other_codeview_signature_is_not_mistaken_for_corrupt_rsds(self):
        if not TRUEFOV.is_file():
            self.skipTest("set CAP13_TRUEFOV_DLL to the unchanged upstream winmm.dll")
        data = bytearray(TRUEFOV.read_bytes())
        image, _ = analyze_file(str(TRUEFOV))
        directory = image._dir(6)
        debug_entry = image.rva_to_offset(directory["rva"])
        record_off = struct.unpack_from("<I", data, debug_entry + 24)[0]
        struct.pack_into("<I", data, debug_entry + 16, 20)
        data[record_off:record_off + 4] = b"NB10"
        with tempfile.TemporaryDirectory() as directory_name:
            path = Path(directory_name) / "nb10-FIXTURE.dll"
            path.write_bytes(data)
            _, report = analyze_file(str(path))
        self.assertEqual(report["debug"], {"status": "present_no_rsds", "codeview": []})

    def test_rsds_path_with_empty_basename_is_malformed(self):
        data = bytearray(TRUEFOV.read_bytes())
        image, _ = analyze_file(str(TRUEFOV))
        debug_entry = image.rva_to_offset(image._dir(6)["rva"])
        record_off = struct.unpack_from("<I", data, debug_entry + 24)[0]
        data[record_off + 24:record_off + 32] = b"C:\\dir\\\0"
        with tempfile.TemporaryDirectory() as directory_name:
            path = Path(directory_name) / "empty-pdb-name-FIXTURE.dll"
            path.write_bytes(data)
            _, report = analyze_file(str(path))
        self.assertEqual(report["debug"]["status"], "malformed")


if __name__ == "__main__":
    unittest.main()
