"""The confirmed title -> Nexus game table: one JSON file, bundled unchanged into web/app.js."""
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

PROJECT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT / "tools"))
import sync_title_map as sync

TITLE_ID = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$")
DOMAIN = re.compile(r"^[a-z0-9][a-z0-9_-]{0,79}$")


class TitleMapTests(unittest.TestCase):
    def test_file_is_one_flat_object_of_valid_ids_and_domains(self):
        text = (PROJECT / "examples" / "title-map.json").read_text(encoding="utf-8")
        value = json.loads(text)
        self.assertIs(type(value), dict, "no extra top-level structure: the file is the table")
        self.assertTrue(value)
        for key, domain in value.items():
            self.assertRegex(key, TITLE_ID)
            self.assertIs(type(domain), str)
            self.assertRegex(domain, DOMAIN)
        # No duplicate keys hide behind the parser (json keeps the last one).
        pairs = json.loads(text, object_pairs_hook=lambda items: items)
        self.assertEqual(len({key for key, _ in pairs}), len(pairs))
        self.assertEqual(sync.load_map(), value)

    def test_confirmed_matches_only(self):
        # Only matches with a profile or other evidence belong here; a guess would show a wrong Nexus page.
        self.assertEqual(sync.load_map(), {"PPSA33286": "silenthilltownfall"})
        profile = json.loads((PROJECT / "examples" / "game-profiles" / "PPSA33286.json").read_text(encoding="utf-8"))
        self.assertEqual(profile["nexus_domain"], sync.load_map()["PPSA33286"])

    def test_app_js_bundles_exactly_the_file(self):
        source = (PROJECT / "web" / "app.js").read_text(encoding="utf-8")
        self.assertEqual(sync.bundled(source), sync.load_map())
        self.assertEqual(source.count(sync.BEGIN), 1)
        self.assertEqual(source.count(sync.END), 1)

    def test_check_command_passes_and_detects_drift(self):
        script = PROJECT / "tools" / "sync_title_map.py"
        done = subprocess.run([sys.executable, "-I", str(script)], capture_output=True, text=True)
        self.assertEqual(done.returncode, 0, done.stderr)
        with tempfile.TemporaryDirectory() as temporary:
            table = Path(temporary) / "title-map.json"
            table.write_text(json.dumps({"PPSA33286": "silenthilltownfall", "PPSA00001": "another"}), encoding="utf-8")
            drift = subprocess.run([sys.executable, "-I", str(script), "--map", str(table)], capture_output=True, text=True)
            self.assertEqual(drift.returncode, 1)
            copy = Path(temporary) / "app.js"
            copy.write_text((PROJECT / "web" / "app.js").read_text(encoding="utf-8"), encoding="utf-8")
            rewrite = subprocess.run([sys.executable, "-I", str(script), "--map", str(table), "--app", str(copy), "--write"],
                                     capture_output=True, text=True)
            self.assertEqual(rewrite.returncode, 0, rewrite.stderr)
            self.assertEqual(sync.bundled(copy.read_text(encoding="utf-8")), json.loads(table.read_text(encoding="utf-8")))
            again = subprocess.run([sys.executable, "-I", str(script), "--map", str(table), "--app", str(copy)],
                                   capture_output=True, text=True)
            self.assertEqual(again.returncode, 0, again.stderr)

    def test_invalid_tables_are_refused(self):
        with tempfile.TemporaryDirectory() as temporary:
            for body in ('[]', '{"bad id": "x"}', '{"PPSA1": "Bad Domain"}', '{"PPSA1": 3}', '{"-PPSA1": "x"}', '{"PPSA1": ""}'):
                path = Path(temporary) / "map.json"
                path.write_text(body, encoding="utf-8")
                with self.assertRaises(ValueError, msg=body):
                    sync.load_map(path)

    def test_block_with_a_second_constant_is_refused(self):
        source = "// BEGIN title-map\nconst titleMap = Object.freeze({});\nconst extra = 1;\n// END title-map\n"
        with self.assertRaises(ValueError):
            sync.bundled(source)
        with self.assertRaises(ValueError):
            sync.bundled("no block here")


if __name__ == "__main__":
    unittest.main()
