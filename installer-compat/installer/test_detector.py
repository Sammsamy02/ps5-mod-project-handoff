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


class UE4SSRuntimeTests(FixtureBase):
    """Users sometimes download UE4SS itself as a 'mod' archive.

    UE4SS runtime presence is signalled by dwmapi.dll / UE4SS.dll at the
    archive root OR ue4ss/UE4SS.dll.  With no mods beyond UE4SS's own
    defaults (ConsoleEnablerMod, LineTraceMod, ...) the ENTIRE archive —
    bundled default Mods/ included — is ue4ss-runtime:
    runnable_on_ps5=not-applicable and nothing in it may be installed as a
    mod (the PS5 runtime layer provides UE4SS itself).  Real UE4SS 3.x
    release zips ship flat (UE4SS.dll + dwmapi.dll + UE4SS-settings.ini +
    Mods/<defaults>/) and installed-style layouts nest the same tree under
    ue4ss/.

    Exception: a MOD that happens to bundle the UE4SS runtime files has
    mods OTHER than the defaults; the mod stays the primary type and the
    runtime files are noted as "skipped: runtime files".

    Fixtures mirror the real release layouts verified against
    UE4SS_v3.0.1.zip.  Mac-only / host-tested: builds zips in a temp dir
    inside the project tree, no network, no console contact.
    """

    # -- real UE4SS 3.x release layouts (Mac-only, host-tested) -----------

    def test_real_nested_release_layout_is_runtime(self):
        # installed-style real layout: dwmapi.dll at root, runtime tree
        # (with UE4SS's own default mods) under ue4ss/
        p = self.make_zip("ue4ss_real_nested.zip", {
            "dwmapi.dll": DLL_PAYLOAD,
            "ue4ss/UE4SS.dll": DLL_PAYLOAD,
            "ue4ss/UE4SS-settings.ini": "[Engine]\nGraphicsAPI = dx11\n",
            "ue4ss/Mods/ConsoleEnablerMod/Scripts/main.lua":
                "-- console enabler\n",
            "ue4ss/Mods/ConsoleEnablerMod/enabled.txt": "",
            "ue4ss/Mods/LineTraceMod/Scripts/main.lua": "-- line trace\n",
        })
        c = D.classify_archive(str(p))
        self.assertEqual(c.label, D.ModType.UE4SS_RUNTIME)
        self.assertFalse(c.mixed)
        self.assertEqual(c.runnable_on_ps5, "not-applicable")
        self.assertEqual(c.required_runtime, "none")
        self.assertTrue(c.flags["ue4ss_runtime"])
        self.assertEqual(c.flags["ue4ss_runtime_files"],
                         ["dwmapi.dll", "ue4ss/UE4SS.dll"])
        self.assertTrue(any("do not install any part" in n for n in c.notes))
        notes = " ".join(c.install_notes)
        self.assertIn("PS5 runtime layer", notes)
        self.assertIn("do not install", notes)
        # the bundled default mods are part of the runtime distribution,
        # never separate mod evidence
        self.assertNotIn("ue4ss-lua", c.evidence)
        json.dumps(c.to_dict())

    def test_real_flat_release_layout_is_runtime(self):
        # exactly how UE4SS_v3.0.1.zip ships: everything at the archive
        # root, UE4SS's default Mods/ tree included
        p = self.make_zip("ue4ss_real_flat.zip", {
            "UE4SS.dll": DLL_PAYLOAD,
            "dwmapi.dll": DLL_PAYLOAD,
            "UE4SS-settings.ini": "[Engine]\nGraphicsAPI = dx11\n",
            "Mods/mods.txt": "; SandboxInternal : 1\n",
            "Mods/ActorDumperMod/Scripts/main.lua": "-- dump\n",
            "Mods/BPML_GenericFunctions/Scripts/main.lua": "-- gf\n",
            "Mods/BPModLoaderMod/Scripts/main.lua": "-- bpl\n",
            "Mods/CheatManagerEnablerMod/Scripts/main.lua": "-- cme\n",
            "Mods/ConsoleCommandsMod/Scripts/main.lua": "-- cc\n",
            "Mods/ConsoleEnablerMod/Scripts/main.lua": "-- ce\n",
            "Mods/ConsoleEnablerMod/enabled.txt": "",
            "Mods/jsbLuaProfilerMod/Scripts/main.lua": "-- prof\n",
            "Mods/Keybinds/Scripts/main.lua": "-- kb\n",
            "Mods/LineTraceMod/Scripts/main.lua": "-- lt\n",
            "Mods/SplitScreenMod/Scripts/main.lua": "-- ss\n",
            "Mods/shared/UEHelpers/UEHelpers.lua": "-- helpers\n",
            "README.md": "# UE4SS\n",
            "Changelog.md": "# log\n",
        })
        c = D.classify_archive(str(p))
        self.assertEqual(c.label, D.ModType.UE4SS_RUNTIME)
        self.assertTrue(c.flags["ue4ss_runtime"])
        self.assertEqual(c.runnable_on_ps5, "not-applicable")
        # the ENTIRE archive (Mods/mods.txt + default mods + readme) is
        # the runtime's responsibility
        self.assertTrue(any("default mods" in ln
                            for ln in c.evidence.get("ue4ss-runtime", [])))
        self.assertTrue(any("do not install any part" in n for n in c.notes))

    def test_nested_ue4ss_dll_alone_signals_runtime(self):
        # ue4ss/UE4SS.dll without any root dwmapi.dll still signals the
        # runtime (e.g. someone zipped the installed ue4ss/ tree)
        p = self.make_zip("ue4ss_tree_only.zip", {
            "ue4ss/UE4SS.dll": DLL_PAYLOAD,
            "ue4ss/UE4SS-settings.ini": "[Engine]\n",
            "ue4ss/Mods/ConsoleEnablerMod/Scripts/main.lua": "-- ce\n",
        })
        c = D.classify_archive(str(p))
        self.assertEqual(c.label, D.ModType.UE4SS_RUNTIME)

    # The EXACT file set of the real UE4SS_v3.0.1.zip (every file entry,
    # dir entries omitted — see docs/UE4SS-RELEASE-LISTING.txt, sha256
    # 4b47d4bc...).  The verified-against-release regression tests below
    # mirror it 1:1 (payloads are stubs; paths are the real ones).
    REAL_RELEASE_FILES = {
        "UE4SS.dll": DLL_PAYLOAD,
        "dwmapi.dll": DLL_PAYLOAD,
        "UE4SS-settings.ini":
            "[Engine]\nGraphicsAPI = dx11\n[General]\n"
            "bUseUObjectArrayCache = true\n[Overrides]\n"
            "MultiThreadedDwmApiHook = false\n",
        "README.md": "# UE4SS\n",
        "Changelog.md": "# changelog\n",
        "Mods/mods.txt": "; SandboxInternal : 1\n",
        "Mods/ActorDumperMod/Scripts/main.lua": "-- dump actors\n",
        "Mods/BPML_GenericFunctions/Scripts/main.lua": "-- generic fns\n",
        "Mods/BPModLoaderMod/Scripts/main.lua": "-- load BP mods\n",
        "Mods/BPModLoaderMod/load_order.txt": "; order\n",
        "Mods/CheatManagerEnablerMod/Scripts/main.lua": "-- enable cm\n",
        "Mods/ConsoleCommandsMod/Scripts/main.lua": "-- console cmds\n",
        "Mods/ConsoleCommandsMod/Scripts/dump_object.lua": "-- dump obj\n",
        "Mods/ConsoleCommandsMod/Scripts/set.lua": "-- set var\n",
        "Mods/ConsoleCommandsMod/Scripts/summon_unloaded_assets.lua":
            "-- summon\n",
        "Mods/ConsoleEnablerMod/Scripts/main.lua": "-- enable console\n",
        "Mods/jsbLuaProfilerMod/Scripts/main.lua": "-- profiler\n",
        "Mods/Keybinds/Scripts/main.lua": "-- keybinds\n",
        "Mods/LineTraceMod/Scripts/main.lua": "-- line trace\n",
        "Mods/SplitScreenMod/Scripts/main.lua": "-- splitscreen\n",
        "Mods/shared/UEHelpers/UEHelpers.lua": "-- UEHelpers\n",
        "Mods/shared/jsbProfiler/jsbProfi.lua": "-- jsb profiler\n",
    }

    def test_flat_ue4ss_release_full_archive_refused(self):
        # the REAL UE4SS_v3.0.1.zip layout, file-for-file (23 file entries;
        # see docs/UE4SS-RELEASE-LISTING.txt): UE4SS.dll + dwmapi.dll +
        # UE4SS-settings.ini at the archive root and UE4SS's own default
        # Mods/ tree (11 default mods + mods.txt + load_order.txt) ->
        # classified ue4ss-runtime, nothing installs
        p = self.make_zip("UE4SS_v3.0.1_flat.zip", self.REAL_RELEASE_FILES)
        c = D.classify_archive(str(p))
        self.assertEqual(c.label, D.ModType.UE4SS_RUNTIME)
        self.assertEqual(c.label.value, "ue4ss-runtime")
        self.assertFalse(c.mixed)
        self.assertEqual(c.types, [D.ModType.UE4SS_RUNTIME])
        self.assertTrue(c.flags["ue4ss_runtime"])
        self.assertEqual(c.flags["ue4ss_runtime_files"],
                         ["UE4SS.dll", "dwmapi.dll"])
        self.assertEqual(c.runnable_on_ps5, "not-applicable")
        self.assertEqual(c.required_runtime, "none")
        self.assertTrue(any("do not install any part" in n for n in c.notes))
        notes = " ".join(c.install_notes)
        self.assertIn("PS5 runtime layer", notes)
        self.assertIn("do not install", notes)
        # the bundled default Mods/ tree is runtime distribution, never
        # separate mod evidence of any kind
        self.assertNotIn("ue4ss-lua", c.evidence)
        self.assertEqual(list(c.evidence), ["ue4ss-runtime"])
        self.assertTrue(any("default mods" in ln
                            for ln in c.evidence["ue4ss-runtime"]))
        json.dumps(c.to_dict())

    def test_nested_ue4ss_release_full_archive_refused(self):
        # the installed-style variant of the same release: the complete
        # 3.0.1 tree nested under ue4ss/ (dwmapi.dll stays at the game-exe
        # root next to the ue4ss/ folder, exactly like an installed copy
        # re-zipped) -> same result: ue4ss-runtime, nothing installs
        nested = {"dwmapi.dll": DLL_PAYLOAD}
        nested.update({"ue4ss/" + k: v
                       for k, v in self.REAL_RELEASE_FILES.items()
                       if k not in ("dwmapi.dll", "README.md",
                                    "Changelog.md")})
        p = self.make_zip("UE4SS_v3.0.1_nested.zip", nested)
        c = D.classify_archive(str(p))
        self.assertEqual(c.label, D.ModType.UE4SS_RUNTIME)
        self.assertFalse(c.mixed)
        self.assertEqual(c.runnable_on_ps5, "not-applicable")
        self.assertEqual(c.required_runtime, "none")
        self.assertTrue(c.flags["ue4ss_runtime"])
        self.assertEqual(c.flags["ue4ss_runtime_files"],
                         ["dwmapi.dll", "ue4ss/UE4SS.dll"])
        self.assertTrue(any("do not install any part" in n for n in c.notes))
        self.assertNotIn("ue4ss-lua", c.evidence)
        json.dumps(c.to_dict())

    def test_ue4ss_subfolder_real_user_mod_skips_runtime_files(self):
        # edge case: the ue4ss/ subfolder layout carrying ONE real user
        # mod (no UE4SS defaults, no root DLLs) -> NOT the runtime: the
        # user mod keeps its mod classification and only the runtime files
        # are reported as skipped
        p = self.make_zip("ue4ss_user_mod_only.zip", {
            "ue4ss/UE4SS.dll": DLL_PAYLOAD,
            "ue4ss/UE4SS-settings.ini": "[Engine]\nGraphicsAPI = dx11\n",
            "ue4ss/Mods/MyCustomMod/Scripts/main.lua":
                "RegisterKeyBind(Key.F1, function() end)\n",
        })
        c = D.classify_archive(str(p))
        self.assertEqual(c.label, D.ModType.UE4SS_LUA)
        self.assertFalse(c.flags["ue4ss_runtime"])
        self.assertTrue(c.flags["bundled_ue4ss_runtime"])
        self.assertEqual(c.flags["ue4ss_runtime_files_skipped"],
                         ["ue4ss/UE4SS-settings.ini", "ue4ss/UE4SS.dll"])
        self.assertTrue(any("MyCustomMod" in ln
                            for ln in c.evidence.get("ue4ss-lua", [])))
        self.assertTrue(any("skipped: runtime files" in n for n in c.notes))
        json.dumps(c.to_dict())

    # -- a mod that HAPPENS to bundle the UE4SS runtime -------------------

    def test_mod_bundling_ue4ss_is_lua_mod_with_runtime_skipped(self):
        # task fixture: MyMod + dwmapi.dll + ue4ss/UE4SS.dll — a mod
        # distribution bundling the runtime for convenience
        p = self.make_zip("mod_bundles_ue4ss.zip", {
            "MyMod/Scripts/main.lua": LUA_PAYLOAD,
            "dwmapi.dll": DLL_PAYLOAD,
            "ue4ss/UE4SS.dll": DLL_PAYLOAD,
        })
        c = D.classify_archive(str(p))
        self.assertEqual(c.label, D.ModType.UE4SS_LUA)
        self.assertFalse(c.mixed)
        self.assertFalse(c.flags["ue4ss_runtime"])
        self.assertTrue(c.flags["bundled_ue4ss_runtime"])
        self.assertEqual(c.flags["ue4ss_runtime_files_skipped"],
                         ["dwmapi.dll", "ue4ss/UE4SS.dll"])
        # the runtime DLLs are NOT mod evidence (no ue4ss-cpp component)
        self.assertNotIn("ue4ss-cpp", c.evidence)
        self.assertEqual(c.types, [D.ModType.UE4SS_LUA])
        self.assertTrue(any("skipped: runtime files" in n for n in c.notes))
        self.assertTrue(any("skipped: runtime files" in n
                            for n in c.install_notes))
        json.dumps(c.to_dict())

    def test_bundling_mod_under_ue4ss_mods_tree(self):
        # the mod itself lives under ue4ss/Mods/ alongside UE4SS's own
        # default mods: still the MOD, defaults skipped as runtime files
        p = self.make_zip("mod_in_ue4ss_tree.zip", {
            "dwmapi.dll": DLL_PAYLOAD,
            "ue4ss/UE4SS.dll": DLL_PAYLOAD,
            "ue4ss/Mods/ConsoleEnablerMod/Scripts/main.lua": "-- ce\n",
            "ue4ss/Mods/LineTraceMod/Scripts/main.lua": "-- lt\n",
            "ue4ss/Mods/MyMod/Scripts/main.lua": LUA_PAYLOAD,
        })
        c = D.classify_archive(str(p))
        self.assertEqual(c.label, D.ModType.UE4SS_LUA)
        self.assertTrue(c.flags["bundled_ue4ss_runtime"])
        skipped = c.flags["ue4ss_runtime_files_skipped"]
        self.assertIn("dwmapi.dll", skipped)
        self.assertIn("ue4ss/UE4SS.dll", skipped)
        self.assertIn("ue4ss/Mods/ConsoleEnablerMod/Scripts/main.lua", skipped)
        self.assertIn("ue4ss/Mods/LineTraceMod/Scripts/main.lua", skipped)
        self.assertNotIn("ue4ss/Mods/MyMod/Scripts/main.lua", skipped)
        # the actual mod still shows as lua evidence
        self.assertTrue(any("MyMod" in ln
                            for ln in c.evidence.get("ue4ss-lua", [])))

    def test_ue4ss_runtime_zip_classified_not_applicable(self):
        p = self.make_zip("ue4ss_runtime.zip", {
            "UE4SS.dll": DLL_PAYLOAD,
            "dwmapi.dll": DLL_PAYLOAD,
            "UE4SS-settings.ini": "[Engine]\nGraphicsAPI = dx11\n",
            "mods.txt": "; SandboxInternal : 1\n",
        })
        c = D.classify_archive(str(p))
        self.assertEqual(c.label, D.ModType.UE4SS_RUNTIME)
        self.assertEqual(c.label.value, "ue4ss-runtime")
        self.assertFalse(c.mixed)
        self.assertEqual(c.runnable_on_ps5, "not-applicable")
        self.assertEqual(c.required_runtime, "none")
        self.assertTrue(c.flags["ue4ss_runtime"])
        notes = " ".join(c.install_notes)
        self.assertIn("PS5 runtime layer", notes)
        self.assertIn("do not install", notes)
        lines = c.evidence.get("ue4ss-runtime", [])
        self.assertTrue(any("UE4SS.dll" in ln and "no Mods/" in ln
                            for ln in lines))
        self.assertTrue(any("do not install" in n for n in c.notes))
        # full report stays JSON-serializable for the CLI
        json.dumps(c.to_dict())

    def test_dwmapi_only_at_root_is_enough(self):
        p = self.make_zip("dwmapi_only.zip", {"dwmapi.dll": DLL_PAYLOAD})
        c = D.classify_archive(str(p))
        self.assertEqual(c.label, D.ModType.UE4SS_RUNTIME)

    def test_case_variant_dll_names_detected(self):
        p = self.make_zip("casey_runtime.zip", {
            "ue4ss.DLL": DLL_PAYLOAD,
            "DwmApi.dll": DLL_PAYLOAD,
        })
        c = D.classify_archive(str(p))
        self.assertEqual(c.label, D.ModType.UE4SS_RUNTIME)

    def test_full_ue4ss_distribution_with_mods_is_not_runtime(self):
        # a mod zip that carries a NON-default mod next to the runtime
        # DLLs: a mod distribution bundling UE4SS, not UE4SS itself — the
        # mod is the primary type (lua), the runtime DLLs are skipped
        p = self.make_zip("ue4ss_full.zip", {
            "UE4SS.dll": DLL_PAYLOAD,
            "dwmapi.dll": DLL_PAYLOAD,
            "UE4SS-settings.ini": "[Engine]\nGraphicsAPI = dx11\n",
            "Mods/ExampleMod/Scripts/main.lua": LUA_PAYLOAD,
        })
        c = D.classify_archive(str(p))
        self.assertNotEqual(c.label, D.ModType.UE4SS_RUNTIME)
        self.assertFalse(c.flags["ue4ss_runtime"])
        self.assertTrue(c.flags["bundled_ue4ss_runtime"])
        self.assertEqual(c.label, D.ModType.UE4SS_LUA)
        self.assertEqual(c.types, [D.ModType.UE4SS_LUA])
        # the bundled runtime files are skipped, never mod evidence
        self.assertEqual(c.flags["ue4ss_runtime_files_skipped"],
                         ["UE4SS-settings.ini", "UE4SS.dll",
                          "dwmapi.dll"])
        self.assertTrue(any("skipped: runtime files" in n for n in c.notes))

    def test_mods_dir_case_insensitive_counts(self):
        # 'MODS/' still means the archive carries a mod tree
        p = self.make_zip("upper_mods.zip", {
            "UE4SS.dll": DLL_PAYLOAD,
            "MODS/Example/Scripts/main.lua": LUA_PAYLOAD,
        })
        c = D.classify_archive(str(p))
        self.assertNotEqual(c.label, D.ModType.UE4SS_RUNTIME)

    def test_dll_not_at_root_is_not_runtime(self):
        p = self.make_zip("nested_dll.zip", {"Binaries/UE4SS.dll": DLL_PAYLOAD})
        c = D.classify_archive(str(p))
        self.assertNotEqual(c.label, D.ModType.UE4SS_RUNTIME)
        self.assertEqual(c.label, D.ModType.UE4SS_CPP)

    def test_runtime_inside_nested_zip_not_runtime(self):
        # a UE4SS.dll nested inside another zip is not at the OUTER root;
        # paths carry the "inner.zip/" container prefix
        inner = zip_bytes({"UE4SS.dll": DLL_PAYLOAD})
        p = self.make_zip("wrapped_runtime.zip", {"bundle.zip": inner})
        c = D.classify_archive(str(p))
        self.assertNotEqual(c.label, D.ModType.UE4SS_RUNTIME)


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

    def test_nexus_url_with_games_path_prefix(self):
        # nexusmods.com/games/<title>/mods/<id> puts an extra URL segment
        # before /mods — the id must still parse (adversarial suite pin,
        # det.requirements_adversarial_text).
        cases = [
            ("nexusmods.com/games/cyberpunk2077/mods/12345", "12345"),
            ("https://www.nexusmods.com/silenthill2/mods/777888", "777888"),
            # hyphenated game slug and locale prefix edge cases
            ("nexusmods.com/games/skyrim-special-edition/mods/52285", "52285"),
            ("https://www.nexusmods.com/en/games/cyberpunk2077/mods/999888",
             "999888"),
        ]
        for url, want in cases:
            with self.subTest(url=url):
                buf = io.BytesIO()
                with zipfile.ZipFile(buf, "w") as zf:
                    zf.writestr("Mods/R/Scripts/main.lua", LUA_PAYLOAD)
                    zf.writestr("Readme.txt", f"Requires the core mod: {url}\n")
                path = self.dir / "nexus_url_mod.zip"
                path.write_bytes(buf.getvalue())
                c = D.classify_archive(str(path))
                self.assertIn(want,
                              [d.nexus_id for d in c.requirements.other_mods])

    def test_nexus_url_without_mod_id_yields_no_phantom_dep(self):
        # A bare game-page URL (no /mods/<id>) must NOT leak digits from the
        # game slug ("cyberpunk2077") into the dependency's nexus_id.
        buf = io.BytesIO()
        with zipfile.ZipFile(buf, "w") as zf:
            zf.writestr("Mods/R/Scripts/main.lua", LUA_PAYLOAD)
            zf.writestr("Readme.txt",
                        "Requires the Core Mod: nexusmods.com/games/cyberpunk2077\n")
        path = self.dir / "nexus_noid_mod.zip"
        path.write_bytes(buf.getvalue())
        c = D.classify_archive(str(path))
        self.assertEqual([d.name for d in c.requirements.other_mods], ["Core"])
        self.assertEqual([d.nexus_id for d in c.requirements.other_mods],
                         [None])

    def test_nexus_url_short_mod_ids(self):
        # URL form accepts 1-9 digit mod ids: real ids as short as "7" or
        # "77" (e.g. nexusmods.com/townfall/mods/77) must yield the
        # dependency.  Formerly the URL form required 3-9 digits and these
        # parsed to no dependency at all (Claude review: nexus regex bug).
        cases = [
            ("https://www.nexusmods.com/townfall/mods/7", "7"),
            ("https://www.nexusmods.com/townfall/mods/77", "77"),
            ("nexusmods.com/townfall/mods/7", "7"),
        ]
        for url, want in cases:
            with self.subTest(url=url):
                buf = io.BytesIO()
                with zipfile.ZipFile(buf, "w") as zf:
                    zf.writestr("Mods/R/Scripts/main.lua", LUA_PAYLOAD)
                    zf.writestr("Readme.txt", f"Requires the core mod: {url}\n")
                path = self.dir / "nexus_short_id_mod.zip"
                path.write_bytes(buf.getvalue())
                c = D.classify_archive(str(path))
                self.assertIn(want,
                              [d.nexus_id for d in c.requirements.other_mods])

    def test_nexus_prose_short_id_not_matched(self):
        # Prose forms stay strict at 3-9 digits: a 1-digit number in prose
        # ("nexus 7") is ambiguous and must NOT yield a dependency.
        buf = io.BytesIO()
        with zipfile.ZipFile(buf, "w") as zf:
            zf.writestr("Mods/R/Scripts/main.lua", LUA_PAYLOAD)
            zf.writestr("Readme.txt",
                        "Requires the core mod. nexus 7\nSee also nexus: 42\n")
        path = self.dir / "nexus_prose_short.zip"
        path.write_bytes(buf.getvalue())
        c = D.classify_archive(str(path))
        self.assertEqual(
            [d.nexus_id for d in c.requirements.other_mods if d.nexus_id],
            [], "short prose numbers must not yield dependencies")

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
