"""Binding/parser fixtures are local tooling checks, never engine evidence."""
import copy
import hashlib
import pathlib
import sys
import tempfile
import unittest

OWN = pathlib.Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True
sys.path.insert(0, str(OWN))
import bind_sources_l18 as binding
import selection_checks_l18 as checks


class BindingTests(unittest.TestCase):
    def fixture(self, folder):
        records = {}
        for role, basename in binding.REQUIRED_LAYOUT.items():
            path = folder / basename
            path.write_text("// LOCAL TOOLING FIXTURE " + basename + "\n")
            records[role] = {"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
        return {"schema": 1, "selected_sources": records}

    def test_complete_layout_and_aliases(self):
        with tempfile.TemporaryDirectory(dir=OWN / "tmp") as dirname:
            data = self.fixture(pathlib.Path(dirname))
            result = binding.validate_selection(data)
            self.assertEqual(set(result), set(binding.REQUIRED_LAYOUT))
            self.assertEqual(result["runtime_owner"]["path"], data["selected_sources"]["runtime_owner"]["path"])

    def test_missing_role_wrong_basename_and_split_topology_refused(self):
        with tempfile.TemporaryDirectory(dir=OWN / "tmp") as dirname:
            folder = pathlib.Path(dirname)
            data = self.fixture(folder)
            bad = copy.deepcopy(data)
            del bad["selected_sources"]["CodeImageCache.hpp"]
            with self.assertRaisesRegex(ValueError, "required"):
                binding.validate_selection(bad)
            bad = copy.deepcopy(data)
            bad["selected_sources"]["runtime_owner"] = data["selected_sources"]["initializer"]
            with self.assertRaisesRegex(ValueError, "basename"):
                binding.validate_selection(bad)
            other = folder / "other"
            other.mkdir()
            path = other / "CodeImageCache.hpp"
            path.write_text("// LOCAL TOOLING FIXTURE\n")
            bad = copy.deepcopy(data)
            bad["selected_sources"]["CodeImageCache.hpp"] = {"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
            with self.assertRaisesRegex(ValueError, "one source directory"):
                binding.validate_selection(bad)

    def test_hash_drift_and_unlisted_file_refused(self):
        with tempfile.TemporaryDirectory(dir=OWN / "tmp") as dirname:
            folder = pathlib.Path(dirname)
            data = self.fixture(folder)
            (folder / "unlisted.hpp").write_text("// LOCAL TOOLING FIXTURE\n")
            with self.assertRaisesRegex(ValueError, "inventory"):
                binding.validate_selection(data)
            (folder / "unlisted.hpp").unlink()
            (folder / "CodeImageCache.hpp").write_text("// CHANGED LOCAL TOOLING FIXTURE\n")
            with self.assertRaisesRegex(ValueError, "drift"):
                binding.validate_selection(data)

    def test_helper_dependencies_use_exact_selected_file(self):
        with tempfile.TemporaryDirectory(dir=OWN / "tmp") as dirname:
            folder = pathlib.Path(dirname)
            data = self.fixture(folder)
            dep = folder / "unit.d"
            selected = data["selected_sources"]
            dep.write_text("unit.o: " + selected["CodeImageCache.hpp"]["path"] + "\n")
            result = checks.helper_dependencies(dep, selected, {"CodeImageCache.hpp"})
            self.assertEqual(result["CodeImageCache.hpp"]["path"], selected["CodeImageCache.hpp"]["path"])
            old = folder / "old"
            old.mkdir()
            stale = old / "CodeImageCache.hpp"
            stale.write_bytes((folder / "CodeImageCache.hpp").read_bytes())
            dep.write_text("unit.o: " + str(stale) + "\n")
            with self.assertRaisesRegex(ValueError, "prior-source"):
                checks.helper_dependencies(dep, selected, {"CodeImageCache.hpp"})

    def test_exact_provider_names_are_internal_only(self):
        self.assertEqual(checks.CACHE_PROVIDERS, {"bridge_code_cache_context", "bridge_code_cache_signature"})
        self.assertEqual(checks.CONSUMERS["runtime_owner"], {"bridge_copyin_initializer_enter", "bridge_copyin_initializer_leave"})
        self.assertEqual(checks.CONSUMERS["unreal-unrealinitializer"], {"bridge_copyin_initializer_complete", *checks.CACHE_PROVIDERS})
        self.assertEqual(set(checks.REQUIRED_BRIDGE_TYPES), checks.ALL_NEW_PROVIDERS)
        self.assertTrue(all(kind == "T" for kind in checks.REQUIRED_BRIDGE_TYPES.values()))


if __name__ == "__main__":
    unittest.main()
