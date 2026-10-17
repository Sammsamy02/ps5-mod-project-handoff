"""Driver rejection checks under optimized Python; no simulated port claims."""
from contextlib import redirect_stdout
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch
import io
import json
import os
import sys
import tempfile
import unittest

HERE = Path(__file__).resolve().parents[1]


class OptimizedDriverTests(unittest.TestCase):
    def run_control(self, mode):
        mbed = os.environ["PAK_MBEDTLS_SOURCE"]
        zlib = os.environ["PAK_ZLIB_SOURCE"]
        namespace = {"__file__": str(HERE / "verify.py"), "__name__": "driver_control"}
        # Test precisely the Python mode that removes ordinary asserts.
        exec(compile((HERE / "verify.py").read_text(), str(HERE / "verify.py"),
                     "exec", optimize=2), namespace)
        names = ("nested.txt", "test.png", "test.txt", "zeros.bin")
        metadata = dict(stage="complete", directory_parse_complete=True,
                        primary_sha1_checked=True, primary_sha1_matches=True,
                        file_count=4, entries=[dict(name=name, location=i, method="None")
                                               for i, name in enumerate(names)])

        def fake_process(argv, **unused):
            stdout, stderr, status = b"", b"", 0
            if "-o" in argv:
                Path(argv[argv.index("-o") + 1]).write_bytes(b"controlled test artifact")
            elif Path(argv[0]).name == "metadata_probe":
                stdout = json.dumps(metadata).encode()
            elif Path(argv[0]).name == "payload_probe":
                if mode == "rejected":
                    status, stderr = 2, b"controlled decoder rejection"
                elif mode == "mismatch":
                    stdout = b"controlled wrong bytes"
                elif Path(argv[1]).name == "primary-hash-mismatch.pak":
                    stdout = b"controlled unexpected publication"
                else:
                    stdout = (HERE / "tests/fixtures" / names[int(argv[2])]).read_bytes()
            return SimpleNamespace(returncode=status, stdout=stdout, stderr=stderr)

        with tempfile.TemporaryDirectory(prefix="pak-driver-control-") as temp:
            argv = ["verify.py", "--mbedtls-source", mbed, "--zlib-source", zlib,
                    "--output-dir", temp]
            with patch.object(sys, "argv", argv), patch("subprocess.run", fake_process), redirect_stdout(io.StringIO()):
                with self.assertRaises(RuntimeError):
                    namespace["main"]()

    def test_rejected_decoder_cannot_pass(self):
        self.run_control("rejected")

    def test_wrong_payload_bytes_cannot_pass(self):
        self.run_control("mismatch")

    def test_negative_probe_runs_and_rejects(self):
        self.run_control("negative")


if __name__ == "__main__":
    unittest.main()
