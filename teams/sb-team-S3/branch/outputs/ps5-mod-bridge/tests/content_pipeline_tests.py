#!/usr/bin/env python3
"""Content index -> compatibility pipeline through the real compiled CLI.

Game data here is synthetic (tools/synthetic_utoc.py); the checks prove the
wiring: a mod's Lua references, a game .utoc directory index and a reflection
dump become a content index that turns an "unknown" verdict into a real one.
"""
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
PROFILE = PROJECT / "examples" / "game-profiles" / "PPSA33286.json"
SCRIPT = """
local axe = StaticFindObject("/Game/Townfall/Items/BP_Axe")
local widget = StaticFindObject("/Script/UMG.UserWidget")
local db = FindFirstOf("ItemDatabaseSubsystem")
"""


class ContentPipelineTests(unittest.TestCase):
    def setUp(self):
        self.assertTrue(BINARY.is_file(), "CLI implementation is missing")
        self.temp = tempfile.TemporaryDirectory(prefix="modbridge-pipeline-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def run_cli(self, *args, expected=0):
        result = subprocess.run([str(BINARY), *map(str, args)], text=True, capture_output=True, timeout=30)
        self.assertEqual(result.returncode, expected, result.stderr)
        return result

    def references(self):
        package = self.root / "mod.zip"
        with zipfile.ZipFile(package, "w") as archive:
            archive.writestr("ue4ss/Mods/Demo/Scripts/main.lua", SCRIPT)
        inventory = json.loads(self.run_cli("analyze", package).stdout)
        lua = [e for e in inventory["entries"] if e["kind"] == "lua"]
        self.assertEqual(len(lua), 1)
        return lua[0]["facts"]["lua_references"]

    def index(self, assets, classes, name):
        utoc = self.root / f"{name}.utoc"
        subprocess.run([sys.executable, str(PROJECT / "tools" / "synthetic_utoc.py"), str(utoc),
                        *[f"Townfall/Content/{a}.uasset" for a in assets]], check=True)
        dump = self.root / f"{name}.dump.json"
        dump.write_text(json.dumps({
            "schema": "modbridge.reflection-dump/1", "platform": "ps5",
            "image": {"sha256": "0" * 64, "address_kind": "vaddr"}, "engine_version": "5.6",
            "functions": [], "classes": classes}))
        out = self.root / f"{name}.index.json"
        self.run_cli("content-index", "--project", "Townfall", "--console-version", "02.003.000",
                     "--utoc", utoc, "--reflection-dump", dump, "--complete", "--output", out)
        return out

    def compat(self, refs, index=None):
        (self.root / "refs.json").write_text(json.dumps(refs))
        (self.root / "param.json").write_text('{"contentVersion":"02.003.000"}')
        (self.root / "files.json").write_text("[]")
        args = ["compat", "--profile", PROFILE, "--param", self.root / "param.json",
                "--files", self.root / "files.json", "--refs", self.root / "refs.json"]
        if index:
            args += ["--index", index]
        return json.loads(self.run_cli(*args).stdout)

    def test_index_turns_unknown_into_real_verdicts(self):
        refs = self.references()
        self.assertEqual(refs["assets"], ["/Game/Townfall/Items/BP_Axe"])
        self.assertEqual(refs["short_class_names"], ["ItemDatabaseSubsystem"])
        without = self.compat(refs)
        self.assertEqual(without["verdict"], "unknown")
        self.assertIn("no_index", [r["code"] for r in without["reasons"]])

        classes = ["/Script/UMG.UserWidget", "/Script/Townfall.ItemDatabaseSubsystem"]
        present = self.compat(refs, self.index(["Townfall/Items/BP_Axe"], classes, "present"))
        self.assertEqual(present["verdict"], "compatible")
        self.assertEqual(present["content"]["present"], present["content"]["checked"])
        self.assertEqual([r["code"] for r in present["reasons"]], ["unknown_build"])

        missing = self.compat(refs, self.index(["Townfall/Items/Other"], classes, "missing"))
        self.assertEqual(missing["verdict"], "partially_compatible")
        self.assertEqual(missing["content"]["missing"]["assets"], ["/Game/Townfall/Items/BP_Axe"])

    def test_index_for_unlisted_build_is_refused(self):
        self.setup_inputs()
        index = self.index(["Townfall/Items/BP_Axe"], [], "other")
        data = json.loads(index.read_text())
        data["console_content_version"] = "09.000.000"
        index.write_text(json.dumps(data))
        result = subprocess.run([str(BINARY), "compat", "--profile", str(PROFILE), "--index", str(index),
                                 "--param", str(self.root / "param.json"), "--files", str(self.root / "files.json"),
                                 "--refs", str(self.root / "refs.json")], text=True, capture_output=True, timeout=30)
        self.assertEqual(result.returncode, 2)
        self.assertIn("not listed", result.stderr)

    def setup_inputs(self):
        (self.root / "param.json").write_text('{"contentVersion":"02.003.000"}')
        (self.root / "files.json").write_text("[]")
        (self.root / "refs.json").write_text(json.dumps(self.references()))


if __name__ == "__main__":
    unittest.main()
