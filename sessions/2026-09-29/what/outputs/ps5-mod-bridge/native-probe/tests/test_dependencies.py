"""Offline dependency validation with temporary local files."""
import hashlib
import importlib.util
import pathlib
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]

class DependencyTests(unittest.TestCase):
    def test_checker_exists(self):
        self.assertTrue((ROOT / "verify_dependencies.py").is_file(), "dependency checks are not implemented")

    def test_files_require_hashes_and_stay_inside_project(self):
        script = ROOT / "verify_dependencies.py"
        if not script.is_file(): self.skipTest("implementation absent")
        spec = importlib.util.spec_from_file_location("verify_dependencies", script)
        verifier = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(verifier)
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            path = root / "tool"
            path.write_bytes(b"fixture")
            good = {"path": "tool", "sha256": hashlib.sha256(b"fixture").hexdigest()}
            self.assertEqual(verifier.verify_files(root, [good]), 1)
            for bad in [dict(good, sha256="0"*64), dict(good, path="missing"), dict(good, path="../outside"), dict(good, path="/outside")]:
                with self.assertRaises(ValueError): verifier.verify_files(root, [bad])
            (root / "escape").symlink_to(root.parent)
            with self.assertRaises(ValueError): verifier.verify_files(root, [dict(good, path="escape/outside")])

if __name__ == "__main__": unittest.main()
