"""Desktop-only behavioral tests; never a console execution claim."""
import json
import pathlib
import subprocess
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]

class ReportTests(unittest.TestCase):
    def test_native_report_preserves_failure_values_and_unmeasured_capabilities(self):
        sources = [ROOT / name for name in ("report.c", "probe.c", "report.h", "payload_args.h", "native.c")]
        self.assertTrue(all(p.is_file() for p in sources), "native diagnostic behavior is not implemented")
        binary = ROOT / "build/test-report"
        binary.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(["clang", "-std=c11", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined", "-g", "-I", str(ROOT), str(ROOT / "tests/report_tests.c"), str(ROOT / "report.c"), str(ROOT / "probe.c"), str(ROOT / "native.c"), "-o", str(binary)], check=True)
        record = json.loads(subprocess.check_output([str(binary)]))
        self.assertEqual(record["record_kind"], "native_probe_report")
        self.assertEqual(record["evidence_origin"], "desktop_fixture")
        self.assertEqual(record["observations"]["getpid"]["value"], -9223372036854775808)
        self.assertEqual(record["observations"]["getuid"]["status"], "unavailable")
        self.assertEqual(record["observations"]["getuid"]["resolve_code"], 77)
        self.assertIsNone(record["observations"]["getuid"]["value"])
        self.assertEqual(record["observations"]["getpagesize"]["value"], -1)
        self.assertEqual(record["observations"]["geteuid"]["status"], "observed")
        self.assertTrue(all(c["status"] == "unknown" for c in record["capabilities"]))
        self.assertIsNone(record["environment"]["firmware_verified"])
        self.assertIsNone(record["environment"]["loader"])

if __name__ == "__main__":
    unittest.main()
