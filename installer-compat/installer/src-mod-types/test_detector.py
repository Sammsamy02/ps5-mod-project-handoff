#!/usr/bin/env python3
"""Host-side tests for detector.py (Mac-only; no network, no console contact).

Builds small fixture archives in a temp dir INSIDE this project directory and
verifies every mod type is classified correctly, plus edge cases: empty
archive, nested archives, zip with misleading extension, garbage/7z inputs,
and an archive containing both .lua and .pak (mixed).
"""
from __future__ import annotations

import io
import json
import subprocess
import sys
import tarfile
import tempfile
import unittest
import zipfile
from pathlib import Path

BASE_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(BASE_DIR))

import detector as D  # noqa: E402

LUA_PAYLOAD = """-- Item Spawner (UE4SS Lua)
local Spawner = {}

function Spawner.Spawn()
    local Pawn = FindFirstOf("Pawn")
    if not Pawn:IsValid() then return end
    ExecuteInGameThread(function()
        local Player = UEHelpers.GetPlayer(0)
        Player:SpawnItem()
    end)
end

RegisterKeyBind(Key.F1, Spawner.Spawn)
return Spawner
"""

DLL_PAYLOAD = b"MZ\x90\x00" + b"\x00" * 60

ELF_X86_64 = (
    b"\x7fELF"            # magic
    b"\x02\x01\x01\x00"   # 64-bit, little-endian, SysV
    + b"\x00" * 8         # e_ident padding
    + b"\x02\x00"         # e_type = ET_EXEC
    + b"\x3e\x00"         # e_machine = 62 (x86-64)
    + b"\x00" * 4
)

DESCRIPTION = (
    "Item Spawner Menu for the game.\n"
    "Requires UE4SS 3.0.1 or later.\n"
    "Requires game version 1.08.\n"
    "This mod requires EXNOTIK.\n"
    "Requires the 'Item Spawner Core' mod: https://www.nexusmods.com/silenthill2/mods/12345\n"
    "Tested on UE4SS 3.0.0 builds. Credits to everyone.\n"
)


def zip_bytes(files: dict) -> bytes:
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w") as zf:
        for name, data in files.items():
            zf.writestr(name, data.encode("utf-8") if isinstance(data, str) else data)
    return buf.getvalue()


class FixtureBase(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # Keep all test artifacts inside the allowed project tree.
        cls._tmp = tempfile.TemporaryDirectory(dir=str(BASE_DIR))
        cls.dir = Path(cls._tmp.name)
        cls.addClassCleanup(cls._tmp.cleanup)

    def make_zip(self, name: str, files: dict) -> Path:
        path = self.dir / name
        path.write_bytes(zip_bytes(files))
        return path

    def make_targz(self, name: str, files: dict) -> Path:
        path = self.dir / name
        with tarfile.open(path, "w:gz") as tf:
            for fname, data in files.items():
                if isinstance(data, str):
                    data = data.encode("utf-8")
                ti = tarfile.TarInfo(fname)
                ti.size = len(data)
                tf.addfile(ti, io.BytesIO(data))
        return path


class SemverTests(unittest.TestCase):
    def test_parse(self):
        self.assertEqual(D.parse_semver("3.0.1"), (3, 0, 1))
        self.assertEqual(D.parse_semver("v3.0"), (3, 0, 0))
        self.assertEqual(D.parse_semver("2"), (2, 0, 0))

    def test_parse_invalid(self):
        with self.assertRaises(ValueError):
            D.parse_semver("abc")

    def test_compare(self):
        self.assertEqual(D.compare_versions("3.0.1", "3.0"), 1)
        self.assertEqual(D.compare_versions("2.5.2", "3.0.0"), -1)
        self.assertEqual(D.compare_versions("1.0", "1.0.0"), 0)
        self.assertEqual(D.compare_versions("v1.2", "1.1"), 1)


class DetectionTests(FixtureBase):
    def test_ue4ss_lua_structure_and_markers(self):
        p = self.make_zip("lua_mod.zip", {
            "Mods/ItemSpawner/Scripts/main.lua": LUA_PAYLOAD,
            "Mods/ItemSpawner/enabled.txt": "",
            "mods.txt": "ItemSpawner : 1",
        })
        c = D.classify_archive(str(p))
        self.assertEqual(c.label, D.ModType.UE4SS_LUA)
        self.assertEqual(c.required_runtime, "UE4SS")
        self.assertEqual(c.runnable_on_ps5, "unknown")
        self.assertFalse(c.mixed)
        lines = c.evidence.get("ue4ss-lua", [])
        self.assertTrue(any("Scripts/*.lua layout" in ln for ln in lines))
        self.assertTrue(any("RegisterKeyBind" in ln for ln in lines))

    def test_ue4ss_lua_markers_only(self):
        p = self.make_zip("lua_markers.zip", {"init.lua": LUA_PAYLOAD})
        c = D.classify_archive(str(p))
        self.assertEqual(c.label, D.ModType.UE4SS_LUA)

    def test_plain_lua_without_markers_is_unknown(self):
        p = self.make_zip("plain_lua.zip", {"Scripts/main.lua": "print('hello')\n"})
        c = D.classify_archive(str(p))
        self.assertEqual(c.label, D.ModType.UNKNOWN)
        self.assertEqual(c.required_runtime, "unknown")

    def test_ue4ss_cpp_dll_flagged(self):
        p = self.make_zip("cpp_mod.zip", {
            "Mods/NativeThing/dll/main.dll": DLL_PAYLOAD,
            "Mods/NativeThing/enabled.txt": "",
        })
        c = D.classify_archive(str(p))
        self.assertEqual(c.label, D.ModType.UE4SS_CPP)
        self.assertEqual(c.runnable_on_ps5, "no")           # Windows DLL -> flagged
        self.assertEqual(c.required_runtime, "UE4SS")
        self.assertTrue(c.flags["windows_dll_present"])
        self.assertTrue(any("Windows PE DLL" in ln for ln in c.evidence.get("ue4ss-cpp", [])))

    def test_dll_without_mz_header_still_flagged_with_warning(self):
        p = self.make_zip("dll_fake.zip", {"Mods/X/dll/main.dll": b"not-a-pe"})
        c = D.classify_archive(str(p))
        self.assertEqual(c.label, D.ModType.UE4SS_CPP)
        self.assertFalse(c.flags["windows_dll_present"])
        self.assertTrue(any("MZ" in ln for ln in c.evidence.get("ue4ss-cpp", [])))

    def test_pak_iostore_needs_conversion(self):
        p = self.make_zip("pak_mod.zip", {
            "Content/Paks/MyMod_P.pak": b"\x00" * 32,
            "Content/Paks/MyMod.utoc": b"\x01" * 16,
            "Content/Paks/MyMod.ucas": b"\x02" * 16,
        })
        c = D.classify_archive(str(p))
        self.assertEqual(c.label, D.ModType.PAK_IOSTORE)
        self.assertEqual(c.runnable_on_ps5, "needs-conversion")
        self.assertEqual(c.required_runtime, "pak-loader")
        self.assertTrue(c.flags["pc_cooked_assets"])

    def test_blueprint_logicmod(self):
        p = self.make_zip("logicmod.zip", {
            "Content/Paks/LogicMods/ItemMenu_P.pak": b"\x00" * 32,
        })
        c = D.classify_archive(str(p))
        self.assertEqual(c.label, D.ModType.BP_LOGICMOD)
        self.assertEqual(c.runnable_on_ps5, "needs-conversion")
        self.assertEqual(c.required_runtime, "UE4SS")
        self.assertTrue(c.flags["logicmods_pak"])

    def test_native_code(self):
        p = self.make_zip("native_mod.zip", {
            "plugin.prx": ELF_X86_64,
            "libgame.so": ELF_X86_64,
        })
        c = D.classify_archive(str(p))
        self.assertEqual(c.label, D.ModType.NATIVE)
        self.assertEqual(c.required_runtime, "none")
        fmts = c.flags["native_formats"]
        self.assertEqual(len(fmts), 2)
        self.assertTrue(all(f["format"] == "ELF" and f["machine"] == "x86-64" for f in fmts))

    def test_config_only(self):
        p = self.make_zip("config_mod.zip", {
            "config.ini": "[Section]\nKey=1\n",
            "settings.json": "{}",
            "readme.txt": "config only mod",
        })
        c = D.classify_archive(str(p))
        self.assertEqual(c.label, D.ModType.CONFIG_ONLY)
        self.assertEqual(c.required_runtime, "none")
        self.assertEqual(c.runnable_on_ps5, "unknown")

    def test_mixed_lua_and_pak(self):
        p = self.make_zip("mixed.zip", {
            "Mods/Spawner/Scripts/main.lua": LUA_PAYLOAD,
            "Content/Paks/skin_P.pak": b"\x00" * 16,
        })
        c = D.classify_archive(str(p))
        self.assertTrue(c.mixed)
        self.assertEqual(c.label, D.ModType.MIXED)
        self.assertEqual({t for t in c.types}, {D.ModType.UE4SS_LUA, D.ModType.PAK_IOSTORE})
        self.assertEqual(c.runnable_on_ps5, "needs-conversion")   # worst informative result
        self.assertEqual(c.required_runtime, "unknown")           # UE4SS + pak-loader both needed
        self.assertTrue(any("Multiple runtimes required" in n for n in c.install_notes))
        self.assertIn("pak-iostore", c.per_type)
        self.assertIn("ue4ss-lua", c.per_type)

    def test_mixed_logicmod_and_plain_pak(self):
        p = self.make_zip("mixed_pak.zip", {
            "Content/Paks/LogicMods/A.pak": b"\x00" * 8,
            "Content/Paks/B_P.pak": b"\x00" * 8,
        })
        c = D.classify_archive(str(p))
        self.assertEqual({t for t in c.types}, {D.ModType.PAK_IOSTORE, D.ModType.BP_LOGICMOD})

    def test_classification_ignores_archive_name(self):
        # Zip NAMED like a UE4SS mod but containing only config -> config-only.
        p = self.make_zip("totally_a_ue4ss_lua_mod.zip", {"config.ini": "x=1\n"})
        c = D.classify_archive(str(p))
        self.assertEqual(c.label, D.ModType.CONFIG_ONLY)

    def test_tar_gz_supported(self):
        p = self.make_targz("config_mod.tar.gz", {
            "config.ini": "[Section]\nKey=1\n",
            "readme.txt": "hi",
        })
        c = D.classify_archive(str(p))
        self.assertEqual(c.label, D.ModType.CONFIG_ONLY)


class EdgeCaseTests(FixtureBase):
    def test_empty_archive(self):
        p = self.make_zip("empty.zip", {})
        c = D.classify_archive(str(p))
        self.assertEqual(c.label, D.ModType.UNKNOWN)
        self.assertTrue(c.flags["empty_archive"])
        self.assertEqual(c.entries_scanned, 0)
        self.assertEqual(c.required_runtime, "unknown")
        # full report must still be JSON-serializable
        json.dumps(c.to_dict())

    def test_nested_zip(self):
        inner = zip_bytes({"Mods/Inner/Scripts/main.lua": LUA_PAYLOAD})
        p = self.make_zip("outer.zip", {"inner.zip": inner, "notes.txt": "x"})
        c = D.classify_archive(str(p))
        self.assertEqual(c.label, D.ModType.UE4SS_LUA)
        self.assertTrue(any(ln.startswith("inner.zip/Mods/") for ln in c.evidence.get("ue4ss-lua", [])))
        self.assertEqual(c.flags["nested_archives"], ["inner.zip"])
        self.assertTrue(any("inspected nested zip" in n for n in c.notes))

    def test_deeply_nested_zip(self):
        inner = zip_bytes({"Mods/Deep/Scripts/main.lua": LUA_PAYLOAD})
        mid = zip_bytes({"inner.zip": inner})
        p = self.make_zip("outer2.zip", {"mid.zip": mid})
        c = D.classify_archive(str(p))
        self.assertEqual(c.label, D.ModType.UE4SS_LUA)
        self.assertEqual(c.flags["nested_archives"], ["mid.zip", "mid.zip/inner.zip"])

    def test_nested_fake_zip_is_noted_and_skipped(self):
        p = self.make_zip("fake_nested.zip", {
            "fake.zip": b"this is not really a zip",
            "config.ini": "[x]\ny=1\n",
        })
        c = D.classify_archive(str(p))
        # The opaque fake.zip still counts as a non-config file, so the
        # archive cannot be config-only; nothing recognizable matched.
        self.assertEqual(c.label, D.ModType.UNKNOWN)
        self.assertTrue(any("fake.zip" in n for n in c.notes))

    def test_misleading_extension_zip_sniffed_by_magic(self):
        # A real zip named .pak must still be treated as a zip archive.
        p = self.make_zip("looks_like_a_pak.pak", {
            "Mods/Spawner/Scripts/main.lua": LUA_PAYLOAD,
        })
        c = D.classify_archive(str(p))
        self.assertEqual(c.label, D.ModType.UE4SS_LUA)

    def test_garbage_zip_raises(self):
        p = self.dir / "broken.zip"
        p.write_bytes(b"garbage that is not an archive at all")
        with self.assertRaises(D.UnsupportedArchiveError):
            D.classify_archive(str(p))

    def test_7z_magic_unsupported(self):
        p = self.dir / "seven.7z"
        p.write_bytes(b"7z\xbc\xaf\x27\x1c" + b"\x00" * 8)
        with self.assertRaises(D.UnsupportedArchiveError):
            D.classify_archive(str(p))


class RequirementsTests(FixtureBase):
    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        cls.req_zip = cls.make_zip_fixture()

    @classmethod
    def make_zip_fixture(cls) -> Path:
        buf = io.BytesIO()
        with zipfile.ZipFile(buf, "w") as zf:
            zf.writestr("Mods/Spawner/Scripts/main.lua", LUA_PAYLOAD)
            zf.writestr("description.txt", DESCRIPTION)
        path = cls.dir / "req_mod.zip"
        path.write_bytes(buf.getvalue())
        return path

    def test_ue4ss_version_requirement(self):
        c = D.classify_archive(str(self.req_zip))
        self.assertIsNotNone(c.requirements.ue4ss_version)
        self.assertEqual(c.requirements.ue4ss_version.version, "3.0.1")
        self.assertEqual(c.requirements.ue4ss_version.op, ">=")
        # non-cued mention is kept separately, never promoted to a requirement
        self.assertIn("3.0.0", c.requirements.ue4ss_versions_mentioned)

    def test_game_version_requirement(self):
        c = D.classify_archive(str(self.req_zip))
        self.assertEqual(c.requirements.game_version.version, "1.08")

    def test_semver_satisfaction(self):
        c = D.classify_archive(str(self.req_zip))
        self.assertTrue(c.requirements.ue4ss_satisfied("3.0.1"))
        self.assertTrue(c.requirements.ue4ss_satisfied("4"))
        self.assertFalse(c.requirements.ue4ss_satisfied("3.0.0"))
        self.assertFalse(c.requirements.ue4ss_satisfied("2.9.9"))
        self.assertFalse(c.requirements.ue4ss_satisfied(None))
        self.assertTrue(c.requirements.game_version_satisfied("1.08"))
        self.assertFalse(c.requirements.game_version_satisfied("1.07"))

    def test_plugins_extracted(self):
        c = D.classify_archive(str(self.req_zip))
        self.assertEqual([p.name for p in c.requirements.plugins], ["EXNOTIK"])
        self.assertEqual(c.requirements.plugins[0].kind, "plugin")

    def test_other_mods_extracted_with_nexus_id(self):
        c = D.classify_archive(str(self.req_zip))
        self.assertEqual(len(c.requirements.other_mods), 1)
        dep = c.requirements.other_mods[0]
        self.assertEqual(dep.name, "Item Spawner Core")
        self.assertEqual(dep.nexus_id, "12345")
        self.assertIn("description.txt", c.requirements.sources_scanned)

    def test_resolve_against_install_records(self):
        c = D.classify_archive(str(self.req_zip))
        records = [
            {"name": "Item Spawner Core", "nexus_id": "12345", "install_path": "/mods/core"},
            {"name": "EXNOTIK"},
        ]
        res = c.requirements.resolve_against(records)
        self.assertEqual(len(res["resolved"]), 2)
        self.assertEqual(res["missing"], [])
        self.assertEqual(res["uninstalled"], [])
        by_name = {r["dependency"]["name"]: r["record"] for r in res["resolved"]}
        self.assertEqual(by_name["Item Spawner Core"]["nexus_id"], "12345")

    def test_resolve_reports_missing_and_uninstalled(self):
        c = D.classify_archive(str(self.req_zip))
        res = c.requirements.resolve_against([])
        self.assertEqual(len(res["missing"]), 2)

        res2 = c.requirements.resolve_against([
            {"name": "EXNOTIK", "installed": False},
        ])
        self.assertEqual(len(res2["uninstalled"]), 1)
        self.assertEqual(len(res2["missing"]), 1)


class CliTests(FixtureBase):
    def test_cli_json_output(self):
        p = self.make_zip("cli_mod.zip", {"Mods/Cli/Scripts/main.lua": LUA_PAYLOAD})
        r = subprocess.run(
            [sys.executable, str(BASE_DIR / "detector.py"), str(p)],
            capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        data = json.loads(r.stdout)
        self.assertEqual(data[0]["label"], "ue4ss-lua")
        self.assertIn("requirements", data[0])

    def test_cli_with_records_resolution(self):
        p = self.make_zip("cli_req.zip", {
            "Mods/Spawner/Scripts/main.lua": LUA_PAYLOAD,
            "description.txt": DESCRIPTION,
        })
        rec = self.dir / "records.json"
        rec.write_text(json.dumps([
            {"name": "Item Spawner Core", "nexus_id": "12345"},
            {"name": "EXNOTIK"},
        ]))
        r = subprocess.run(
            [sys.executable, str(BASE_DIR / "detector.py"), str(p), "--records", str(rec)],
            capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        data = json.loads(r.stdout)
        item = data[0]
        self.assertEqual(item["label"], "ue4ss-lua")
        res = item["dependency_resolution"]
        self.assertEqual(len(res["resolved"]), 2)
        self.assertEqual(res["missing"], [])

    def test_cli_error_for_garbage(self):
        p = self.dir / "cli_bad.zip"
        p.write_bytes(b"not an archive")
        r = subprocess.run(
            [sys.executable, str(BASE_DIR / "detector.py"), str(p)],
            capture_output=True, text=True)
        self.assertEqual(r.returncode, 1)
        data = json.loads(r.stdout)
        self.assertIn("error", data[0])


if __name__ == "__main__":
    unittest.main(verbosity=2)
