"""Offline registration adapter controls with the actual durable store."""
import pathlib
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class RegistrationBridgeTests(unittest.TestCase):
    def test_registration_privacy_integrity_and_replay(self):
        with tempfile.TemporaryDirectory(prefix="mod-register-test-") as temporary:
            output = pathlib.Path(temporary)

            def run(*args):
                subprocess.run([str(arg) for arg in args], check=True)

            run("cc", "-std=c11", "-c", ROOT / "archive/private/posix_compat.c",
                "-o", output / "posix.o")
            run("c++", "-std=c++20", "-Wall", "-Wextra", "-Werror",
                "-I", ROOT, "-I", ROOT.parent / "third_party",
                ROOT / "mod_service/register_bridge.cpp",
                ROOT / "download_store/store.cpp",
                ROOT / "tests/mod_service_register_tests.cpp", output / "posix.o",
                "-o", output / "test")
            run(output / "test")


if __name__ == "__main__":
    unittest.main()
