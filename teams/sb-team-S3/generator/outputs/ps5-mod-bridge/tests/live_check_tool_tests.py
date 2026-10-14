"""tools/nexus_live_check.py with a fake client (the real script is run by hand against live Nexus)."""
import importlib.util
from pathlib import Path
import sys
import unittest

PROJECT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT / "companion"))
import modbridge_nexus as nexus  # noqa: E402

spec = importlib.util.spec_from_file_location("nexus_live_check", PROJECT / "tools" / "nexus_live_check.py")
live_check = importlib.util.module_from_spec(spec)
spec.loader.exec_module(live_check)


class FakeClient:
    def __init__(self, **overrides):
        self.calls, self.overrides = [], overrides

    def _call(self, name, default):
        self.calls.append(name)
        value = self.overrides.get(name, default)
        if isinstance(value, Exception):
            raise value
        return value

    def user(self):
        return self._call("user", type("Profile", (), {"is_premium": True})())

    def latest_mods(self, game):
        return self._call("latest_mods", [{"mod_id": 71, "name": "Townfall Item Spawner"}])

    def files_and_updates(self, game, mod):
        return self._call("files_and_updates", {"files": [{"file_id": 5, "name": "Main", "version": "1"}], "file_updates": []})

    def requirements(self, game, mod):
        return self._call("requirements", {"nexus_requirements": [{"id": "1"}], "dlc_requirements": []})

    def browse_mods(self, game, sort, page):
        return self._call("browse_mods", {"items": [{"mod_id": 71}], "total": 45, "page": 1, "pages": 3})

    def search_mods(self, game, text, page):
        self.calls.append(("search", text, page))
        return self._call("search_mods", {"items": [{"mod_id": 71}], "total": 3, "page": 1, "pages": 1})


class LiveCheckTool(unittest.TestCase):
    def run_checks(self, client, mod=None):
        lines = []
        return live_check.run(client, "silenthilltownfall", mod, out=lines.append), lines

    def test_all_checks_pass_and_search_uses_a_word_from_a_listed_mod(self):
        client = FakeClient()
        ok, lines = self.run_checks(client)
        self.assertTrue(ok, lines)
        self.assertIn(("search", "Townfall", 1), client.calls)
        self.assertEqual(lines[-1], "\n6 of 6 checks passed.")
        self.assertTrue(all(line.startswith("PASS") for line in lines[:-1]))

    def test_empty_search_fails_the_published_filter_check(self):
        ok, lines = self.run_checks(FakeClient(search_mods={"items": [], "total": 0, "page": 1, "pages": 0}))
        self.assertFalse(ok)
        self.assertTrue(any(line.startswith("FAIL search") and "published filter" in line for line in lines))

    def test_nexus_errors_are_reported_by_code_and_later_checks_still_run(self):
        ok, lines = self.run_checks(FakeClient(requirements=nexus.NexusError("payload")))
        self.assertFalse(ok)
        self.assertIn("FAIL requirements: Nexus error 'payload'", "\n".join(lines))
        self.assertTrue(any(line.startswith("PASS search") for line in lines))

    def test_unexpected_errors_print_only_the_type(self):
        ok, lines = self.run_checks(FakeClient(files_and_updates=ValueError("secret detail /private/path")))
        self.assertFalse(ok)
        self.assertNotIn("secret", "\n".join(lines))
        self.assertIn("FAIL files + update links: ValueError", "\n".join(lines))

    def test_no_listed_mods_stops_before_mod_checks(self):
        client = FakeClient(latest_mods=[])
        ok, lines = self.run_checks(client)
        self.assertFalse(ok)
        self.assertNotIn("files_and_updates", client.calls)


if __name__ == "__main__":
    unittest.main()
