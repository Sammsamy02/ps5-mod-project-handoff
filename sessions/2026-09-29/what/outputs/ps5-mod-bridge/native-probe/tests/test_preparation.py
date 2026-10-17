"""Public-archive preparation rejects untrusted names and links."""
import importlib.util
import io
import pathlib
import tarfile
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]

class PreparationTests(unittest.TestCase):
    def test_preparation_exists(self):
        self.assertTrue((ROOT / "prepare_deps.py").is_file(), "local archive preparation is not implemented")

    def test_rejects_escaping_names_links_and_special_files(self):
        script = ROOT / "prepare_deps.py"
        if not script.is_file(): self.skipTest("implementation absent")
        spec = importlib.util.spec_from_file_location("prepare_deps", script)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        for name in ("/absolute", "../outside", "root/../../outside"):
            with self.assertRaises(ValueError): module.safe_name(name)
        for target in ("/absolute", "../../outside"):
            with self.assertRaises(ValueError): module.safe_link("root/link", target)
        self.assertEqual(module.safe_link("root/bin/clang", "clang-21"), "root/bin/clang-21")
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            archive = root / "fixture.tar"
            with tarfile.open(archive, "w") as tar:
                wanted = tarfile.TarInfo("LLVM-21.1.8-macOS-ARM64/bin/clang-21")
                wanted.size = 7; tar.addfile(wanted, io.BytesIO(b"fixture"))
                unwanted = tarfile.TarInfo("../outside")
                unwanted.size = 0; tar.addfile(unwanted, io.BytesIO())
            with self.assertRaises(ValueError): module.extract_tools(archive, root / "destination")
            self.assertFalse((root / "destination").exists())
            self.assertFalse((root / "outside").exists())
            with tarfile.open(archive, "w") as tar:
                special = tarfile.TarInfo("root/device"); special.type = tarfile.CHRTYPE
                tar.addfile(special)
            with self.assertRaises(ValueError): module.extract_tools(archive, root / "destination")

if __name__ == "__main__": unittest.main()
