"""tools/survey_mods.py against the real core on owned fixture archives."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile

PROJECT = Path(__file__).resolve().parents[1]
BINARY = Path(os.environ.get("MODBRIDGE_BIN", PROJECT / "build" / "modbridge"))
TOOL = PROJECT / "tools" / "survey_mods.py"


@unittest.skipUnless(BINARY.is_file(), "desktop core is not built")
class SurveyTool(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory(prefix="modbridge-survey-test-")
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.mods = self.root / "downloads"
        self.mods.mkdir()

    def archive(self, name, members):
        with zipfile.ZipFile(self.mods / name, "w") as package:
            for path, text in members.items():
                package.writestr(path, text)

    def run_tool(self, *paths):
        # A test-owned runtime (only RegisterKeyBind implemented), so the ranking stays meaningful
        # whatever the bundled default manifest says.
        runtime = self.root / "runtime.json"
        runtime.write_text(json.dumps({"schema": "modbridge.lua-runtime/1", "runtime": "survey-test", "version": "1",
            "console_wired": False, "functions": {"RegisterKeyBind": {"status": "implemented"}}, "modules": {}}))
        out = self.root / "survey.json"
        result = subprocess.run([sys.executable, "-I", str(TOOL), *map(str, paths), "--binary", str(BINARY),
                                 "--runtime", str(runtime), "--json", str(out)], capture_output=True, text=True, timeout=120)
        self.assertEqual(result.returncode, 0, result.stderr)
        return result.stdout, json.loads(out.read_text())

    def test_folder_of_mods_is_ranked_by_mods_blocked(self):
        self.archive("a-spawner.zip", {"Mods/A/Scripts/main.lua":
            "RegisterKeyBind(Key.F1, function() ExecuteInGameThread(function() end) end)\n"
            "local c = StaticFindObject('/Script/Engine.Actor')\n"})
        self.archive("b-finder.zip", {"Mods/B/Scripts/main.lua": "local p = FindFirstOf('PlayerController')\nStaticFindObject('/Game/X')\n"})
        self.archive("c-keys.zip", {"Mods/C/Scripts/main.lua": "RegisterKeyBind(1, function() end)\n"})
        self.archive("d-textures.zip", {"Content/Paks/~mods/x.txt": "not lua"})
        (self.mods / "e-broken.zip").write_bytes(b"PK\x03\x04 not really a zip")
        report, data = self.run_tool(self.mods)
        rows = {row["mod"]: row for row in data["rows"]}
        self.assertEqual(sorted(rows), ["a-spawner.zip", "b-finder.zip", "c-keys.zip", "d-textures.zip", "e-broken.zip"])
        self.assertEqual(rows["a-spawner.zip"]["status"], "blocked")
        self.assertEqual(rows["a-spawner.zip"]["missing"], ["ExecuteInGameThread", "Key", "StaticFindObject"])
        self.assertEqual(rows["b-finder.zip"]["missing"], ["FindFirstOf", "StaticFindObject"])
        # RegisterKeyBind is implemented in the test runtime; console_wired stays separate.
        self.assertEqual((rows["c-keys.zip"]["status"], rows["c-keys.zip"]["console_wired"]), ("covered", False))
        self.assertEqual(rows["d-textures.zip"]["status"], "no_lua")
        self.assertEqual((rows["e-broken.zip"]["status"], rows["e-broken.zip"]["error"]), ("unreadable", "archive_error"))
        self.assertEqual(data["ranking"][0], {"name": "StaticFindObject", "mods_blocked": 2, "uses": 2})
        self.assertEqual([item["name"] for item in data["ranking"]],
                         ["StaticFindObject", "ExecuteInGameThread", "FindFirstOf", "Key"])
        self.assertIn("| `StaticFindObject` | 2 of 3 checked | 2 |", report)
        self.assertIn("not a runtime test", report)
        self.assertIn("not running on the console yet", report)
        self.assertNotIn(str(self.root), report)

    def test_unpacked_mod_folder_is_one_mod(self):
        folder = self.root / "TownfallLike"
        (folder / "Scripts").mkdir(parents=True)
        (folder / "Scripts" / "main.lua").write_text("ExecuteInGameThread(function() end)\n")
        (folder / "enabled.txt").write_text("")
        _, data = self.run_tool(folder)
        self.assertEqual([(row["mod"], row["status"], row["missing"]) for row in data["rows"]],
                         [("TownfallLike", "blocked", ["ExecuteInGameThread"])])


if __name__ == "__main__":
    unittest.main()
