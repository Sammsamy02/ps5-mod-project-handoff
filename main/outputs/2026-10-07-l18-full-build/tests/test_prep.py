"""Focused local prep checks; no compose, package, console or network run."""
import copy
import importlib.util
import pathlib
import sys
import tempfile
import unittest

OWN = pathlib.Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True
sys.path.insert(0, str(OWN))
import prep_guard


def load_private_packager():
    spec = importlib.util.spec_from_file_location("l18_private_packager", OWN / "package_prx_l18.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class PreparationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.packager = load_private_packager()

    def test_pending_manifest_refuses_execution(self):
        from unittest.mock import patch
        with patch("prep_guard.json.loads", return_value={"readiness": "PENDING"}):
            with self.assertRaisesRegex(ValueError, "HOLD"):
                prep_guard.require_ready()

    def test_output_escape_and_symlink_refused(self):
        with self.assertRaisesRegex(ValueError, "outside"):
            prep_guard.require_owned(OWN.parent / "outside")
        with tempfile.TemporaryDirectory(dir=OWN / "tmp") as dirname:
            link = pathlib.Path(dirname) / "escape"
            link.symlink_to(OWN.parent, target_is_directory=True)
            with self.assertRaisesRegex(ValueError, "outside"):
                prep_guard.require_owned(link / "receipt.json")

    def test_unlink_exact_mapping_and_nid(self):
        p = self.packager
        self.assertEqual(p.BOOTSTRAP_ONLY_IMPORTS["sceKernelUnlink"], "sceKernelOpen")
        self.assertNotIn("sceKernelUnlink", p.EXTENDED_IMPORTS)
        self.assertEqual(p.sce_nid("sceKernelUnlink"), "AUXVxWeJU-A")
        self.assertEqual(p.sce_nid("malloc"), "gQX+4GDQjpM")
        self.assertEqual(p.sce_nid("module_start"), "BaOKcng8g88")

    def test_unlink_evidence_accepts_pinned_sdk_only(self):
        p = self.packager
        result = p.unlink_import_evidence(p.Build(OWN / "tests", False))
        self.assertEqual(result["nid"], "AUXVxWeJU-A")
        self.assertEqual(result["sdk_defined_matches"], ["sceKernelUnlink T 1218c 0"])
        self.assertFalse(result["console_resolution_tested"])

    def test_wrong_identity_nid_and_extra_import_refused(self):
        p = self.packager
        original = p.PIN["unlink_import_evidence"]
        original_sha = p.PIN["unlink_import_evidence_sha256"]
        import json
        receipt = json.loads(original.read_text())
        with tempfile.TemporaryDirectory(dir=OWN / "tmp") as dirname:
            path = pathlib.Path(dirname) / "wrong.json"
            for mutation in ("identity", "nid", "unexpected_import"):
                bad = copy.deepcopy(receipt)
                if mutation == "identity":
                    bad["identity"]["Soname"] = "unrelated.prx"
                elif mutation == "nid":
                    bad["nid"] = "AAAAAAAAAAA"
                else:
                    bad["new_imports"].append("not_admitted")
                path.write_text(json.dumps(bad))
                p.PIN["unlink_import_evidence"] = path
                p.PIN["unlink_import_evidence_sha256"] = p.sha256_file(path)
                with self.assertRaisesRegex(RuntimeError, "Unlink"):
                    p.unlink_import_evidence(p.Build(OWN / "tests", False))
        p.PIN["unlink_import_evidence"] = original
        p.PIN["unlink_import_evidence_sha256"] = original_sha

    def test_environment_and_offline_restore_are_local(self):
        env = prep_guard.local_environment()
        for key in ("TMPDIR", "TEMP", "TMP", "DOTNET_CLI_HOME", "NUGET_PACKAGES", "XDG_CACHE_HOME"):
            prep_guard.require_owned(env[key])
        self.assertEqual(env["PYTHONDONTWRITEBYTECODE"], "1")
        config = (OWN / "inputs" / "NuGet.Config").read_text()
        self.assertIn("<clear />", config)
        self.assertNotIn("https://", config)


if __name__ == "__main__":
    unittest.main()
