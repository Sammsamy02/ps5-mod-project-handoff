"""The default CLI text includes compatibility findings visible in JSON."""

import os
import subprocess
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
TRUEFOV = Path(os.environ.get("CAP13_TRUEFOV_DLL", str(ROOT / "SAMPLE" / "TrueFOV-upstream-winmm.dll")))


class TextReportTests(unittest.TestCase):
    def test_cli_prints_tls_and_codeview_identity(self):
        result = subprocess.run(
            [sys.executable, str(ROOT / "TOOL" / "pe_analyzer.py"), str(TRUEFOV)],
            check=True, capture_output=True, text=True,
        )
        self.assertIn("tls:                real", result.stdout)
        self.assertIn("initialized=8", result.stdout)
        self.assertIn("callbacks=0", result.stdout)
        self.assertIn("debug CodeView:     winmm.pdb", result.stdout)
        self.assertIn("af9cfae1-a970-4477-a17b-60c21eb6f6c1", result.stdout)
        self.assertIn("age=1", result.stdout)


if __name__ == "__main__":
    unittest.main()
