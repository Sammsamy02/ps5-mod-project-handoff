#!/usr/bin/env python3
"""Host-only (Mac) end-to-end tests for adapter.py — the PRODUCTION
integration adapter that closes tester findings #22/#23.

Run:  python3 -m unittest -v test_adapter.py

These tests exercise ONLY the InstallPipeline API (never the tester's
custom bridge in tests/src-tester/integration_tests.py) and prove the
whole lifecycle works through one entry point:

  * clean install -> verify -> uninstall -> verify clean (config outputs
    produced and restored: loadlist, mods.txt, enabled flag, ploader.ini)
  * conflicting candidates blocked at the conflicts stage, first mod kept
  * tampered install -> verification catches it -> uninstall still works
  * UE4SS-runtime-as-mod refused at the detect stage
  * compat catalog grading wired into the install path

No FTP, no payloads, no launches, no network, no console contact:
everything happens in a temp dir.
"""

import json
import os
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import installer  # noqa: E402  (asserting installer-side behaviour)
from adapter import (  # noqa: E402
    CheckResult,
    InstallPipeline,
    InstallResult,
    UninstallResult,
    installer_manifest_to_uninstall,
)

TITLE = "PPSA01325"
HERE = Path(__file__).resolve().parent
# compat grader fixture (invented statuses, perfect for grade assertions)
CATALOG = HERE.parent / "compat" / "sample_catalog_TEST_FIXTURE.json"

LUA_OK = (
    "local it = FindFirstOf('Item')\n"
    "RegisterKeyBind(Key.F, function() print(it) end)\n"
)
# same shape but no uncataloged constants (Key.F is not in the fixture
# catalog and would make the grade 'unknown'): used for grade assertions
LUA_COMPAT_OK = (
    "local it = FindFirstOf('Item')\n"
    "RegisterKeyBind('F', function() print(tostring(it)) end)\n"
)
LUA_UNSUPPORTED = (
    "RegisterConsoleCommandHandler('spawn', function() end)\n"
    "RegisterKeyBind(Key.G, function() end)\n"
)

BASE_LOADLIST = (
    "modforge-loadlist\t1\n"
    "title\t" + TITLE + "\n"
    "mod\tOtherMod\t9.9\n"
    "file\tlua\tue4ss/Mods/OtherMod/Scripts/main.lua\t1\n"
)
BASE_MODS_TXT = "PreExistingMod : 1\n"
BASE_PLOADER = "[OtherTitle]\nfoo=1\n"


def make_zip(path, entries):
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
        for rel, data in entries.items():
            if isinstance(data, str):
                data = data.encode("utf-8")
            z.writestr(rel, data)
    return str(path)


class AdapterTestCase(unittest.TestCase):
    """Fresh sandbox per test: game root, plugins dir, state dir, pipeline."""

    def setUp(self):
        # MODBRIDGE_TEST_TMPDIR (Mac-only host testing hook): rerun this
        # suite on a case-sensitive APFS disk image (test_case_sensitivity).
        tmp = tempfile.TemporaryDirectory(
            prefix="adapter-test-",
            dir=os.environ.get("MODBRIDGE_TEST_TMPDIR") or None)
        self.addCleanup(tmp.cleanup)
        self.base = Path(tmp.name)
        self.game = self.base / "game"
        self.game.mkdir(parents=True)
        self.plugins = self.base / "plugins"
        self.state = self.base / "state"
        self.pipe = InstallPipeline(
            plugins_dir=str(self.plugins), state_dir=str(self.state),
            game_version="1.004.000")
        self.zipdir = self.base / "zips"
        self.zipdir.mkdir()

    # ---- helpers -------------------------------------------------------

    def zip_mod(self, name, lua=LUA_OK, extra=None):
        entries = {"%s/Scripts/main.lua" % name: lua}
        entries.update(extra or {})
        return make_zip(self.zipdir / (name + ".zip"), entries)

    def install(self, name, lua=LUA_OK, extra=None, md_extra=None,
                catalog=None):
        archive = self.zip_mod(name, lua, extra)
        metadata = {"name": name, "version": "1.0.0"}
        metadata.update(md_extra or {})
        return self.pipe.install_mod(archive, metadata, TITLE, str(self.game),
                                     catalog_path=catalog)

    def manifest_path(self, mod_dir):
        return self.game / TITLE / "manifests" / (mod_dir + ".json")

    def title_root(self):
        return self.game / TITLE

    def read(self, path):
        with open(path, "r", encoding="utf-8", newline="") as f:
            return f.read()

    def seed_configs(self):
        """Pre-existing shared config that must survive install+uninstall."""
        mods_dir = self.title_root() / "ue4ss" / "Mods"
        mods_dir.mkdir(parents=True)
        (mods_dir / "mods.txt").write_text(BASE_MODS_TXT, encoding="utf-8")
        (self.title_root() / "loadlist.txt").write_text(
            BASE_LOADLIST, encoding="utf-8")
        self.plugins.mkdir(parents=True, exist_ok=True)
        (self.plugins / "ploader.ini").write_text(
            BASE_PLOADER, encoding="utf-8")

    # ================================================================
    # 1. clean lifecycle: install -> verify -> uninstall -> verify clean
    # ================================================================
    def test_clean_lifecycle_and_config_outputs(self):
        self.seed_configs()
        res = self.install("LifeMod",
                           extra={"LifeMod/config/overrides.ini": "[x]\ny=1\n"})
        self.assertIsInstance(res, InstallResult)
        self.assertTrue(res.ok, "install refused: %s" % res.refusal or res.error)
        self.assertEqual(res.stage, "done")
        self.assertEqual(res.mod_id, "LifeMod")
        self.assertTrue(res.detection["label"] == "ue4ss-lua")
        self.assertTrue(res.verify["ok"])
        self.assertEqual(res.manifest_path, str(self.manifest_path("LifeMod")))
        self.assertTrue(os.path.isfile(res.manifest_path))

        # --- all config outputs produced ---
        tr = self.title_root()
        ll = self.read(tr / "loadlist.txt")
        self.assertIn("mod\tLifeMod\t1.0.0", ll)
        self.assertIn("ue4ss/Mods/LifeMod/Scripts/main.lua", ll)
        mt = self.read(tr / "ue4ss" / "Mods" / "mods.txt")
        self.assertIn("LifeMod : 1", mt)
        marker = tr / "ue4ss" / "Mods" / "LifeMod" / "enabled.txt"
        self.assertTrue(marker.is_file(), "enable marker missing")
        pl = self.read(self.plugins / "ploader.ini")
        self.assertIn("[%s]" % TITLE, pl)
        self.assertIn("modbridge_supplier_composition.prx=2", pl)

        # unified registry state written (feeds uninstall + conflicts)
        installed = self.pipe.installed_mods(str(self.game), TITLE)
        self.assertIn("LifeMod", installed)
        self.assertEqual(installed["LifeMod"]["requires"], [])
        self.assertTrue(any(f["path"].endswith("LifeMod/Scripts/main.lua")
                            for f in installed["LifeMod"]["files"]))

        # --- uninstall through the SAME entry point ---
        ures = self.pipe.uninstall_mod(str(self.manifest_path("LifeMod")),
                                       str(self.game))
        self.assertIsInstance(ures, UninstallResult)
        self.assertTrue(ures.ok, "uninstall failed: %s" % ures.error)
        self.assertEqual(ures.stage, "done")
        self.assertTrue(ures.core.removed or ures.core.removed_modified)
        self.assertTrue(ures.ploader["line_removed"])

        # --- verify clean ---
        self.assertFalse((tr / "ue4ss" / "Mods" / "LifeMod").exists(),
                         "mod dir left after uninstall")
        self.assertFalse(self.manifest_path("LifeMod").exists(),
                         "installer manifest left after uninstall")
        ll = self.read(tr / "loadlist.txt")
        self.assertNotIn("mod\tLifeMod", ll)
        self.assertNotIn("ue4ss/Mods/LifeMod/", ll)
        self.assertIn("modforge-loadlist\t1", ll, "loadlist header damaged")
        self.assertIn("mod\tOtherMod\t9.9", ll, "foreign mod row lost")
        mt = self.read(tr / "ue4ss" / "Mods" / "mods.txt")
        self.assertNotIn("LifeMod", mt)
        self.assertIn("PreExistingMod : 1", mt, "foreign mods.txt row lost")
        pl = self.read(self.plugins / "ploader.ini")
        self.assertNotIn("[%s]" % TITLE, pl,
                         "installer-created ploader section not cleaned")
        self.assertNotIn("modbridge_supplier_composition.prx=2", pl)
        self.assertIn("[OtherTitle]", pl, "foreign ploader section lost")
        self.assertIn("foo=1", pl)
        self.assertEqual(self.pipe.installed_mods(str(self.game), TITLE), {})
        # installer-side verification must now fail (mod gone)
        with self.assertRaises(installer.InstallError):
            self.pipe.verify_mod("LifeMod", TITLE, str(self.game))

    # ================================================================
    # 2. conflicting candidates: first installs, second blocked
    # ================================================================
    def test_second_mod_blocked_missing_dependency(self):
        first = self.install("ModA")
        self.assertTrue(first.ok, first.refusal or first.error)
        blocked = self.install("ModB", md_extra={"requires": ["Ghost"]})
        self.assertFalse(blocked.ok)
        self.assertEqual(blocked.stage, "conflicts")
        self.assertIn("MISSING_DEPENDENCY", blocked.refusal)
        self.assertIn("Ghost", blocked.refusal)
        # first mod untouched, nothing of the second on disk or in state
        self.assertTrue(self.manifest_path("ModA").exists())
        self.assertFalse(self.manifest_path("ModB").exists())
        self.assertFalse((self.title_root() / "ue4ss" / "Mods" /
                          "ModB").exists())
        installed = self.pipe.installed_mods(str(self.game), TITLE)
        self.assertEqual(list(installed), ["ModA"])

    def test_second_mod_blocked_version_mismatch(self):
        self.assertTrue(self.install("ModA").ok)
        blocked = self.install("ModB", md_extra={"game_versions": ["9.9.9"]})
        self.assertFalse(blocked.ok)
        self.assertEqual(blocked.stage, "conflicts")
        self.assertIn("VERSION_MISMATCH", blocked.refusal)
        self.assertFalse(self.manifest_path("ModB").exists())

    def test_dependency_on_installed_mod_is_satisfied(self):
        self.assertTrue(self.install("ModA").ok)
        ok = self.install("ModB", md_extra={"requires": ["ModA"]})
        self.assertTrue(ok.ok, ok.refusal or ok.error)
        self.assertEqual(
            self.pipe.installed_mods(str(self.game), TITLE)["ModB"]
            ["requires"], ["ModA"])

    def test_keybind_conflict_warns_but_installs(self):
        # pairwise PATH conflicts between distinct UE4SS Lua mods are
        # structurally impossible (each mod owns ue4ss/Mods/<mod>/), so the
        # wired-in pairwise check that fires end-to-end is the keybind
        # overlap (warning) — both mods install, with the warning recorded.
        self.assertTrue(self.install("KeyModA").ok)
        res = self.install("KeyModB")  # same RegisterKeyBind(Key.F, ...)
        self.assertTrue(res.ok, res.refusal or res.error)
        self.assertTrue(any("KEYBIND_CONFLICT" in w
                            for w in res.conflict_warnings),
                        "keybind warning not reported: %s" % res.warnings)

    # ================================================================
    # 3. tampered install: verify catches it, uninstall still works
    # ================================================================
    def test_tampered_install_verify_then_uninstall(self):
        res = self.install("TamperMod")
        self.assertTrue(res.ok)
        main_lua = (self.title_root() / "ue4ss" / "Mods" / "TamperMod" /
                    "Scripts" / "main.lua")
        data = bytearray(main_lua.read_bytes())
        data[0] ^= 0xFF
        main_lua.write_bytes(bytes(data))

        report = self.pipe.verify_mod("TamperMod", TITLE, str(self.game))
        self.assertFalse(report["ok"])
        self.assertTrue(any(p.startswith("tampered:") for p in
                            report["problems"]), report["problems"])

        # uninstall refuses without confirmation ...
        ures = self.pipe.uninstall_mod(str(self.manifest_path("TamperMod")),
                                       str(self.game))
        self.assertFalse(ures.ok)
        self.assertEqual(ures.stage, "confirm")
        self.assertEqual(ures.error_kind, "ConfirmationRequiredError")
        self.assertTrue(any(p.endswith("Scripts/main.lua")
                            for p in ures.modified_files))
        self.assertTrue(main_lua.exists(), "tampered file removed anyway")
        # ... and works with it
        ures2 = self.pipe.uninstall_mod(str(self.manifest_path("TamperMod")),
                                        str(self.game), confirm_modified=True)
        self.assertTrue(ures2.ok, ures2.error)
        self.assertFalse(main_lua.exists())
        self.assertFalse((self.title_root() / "ue4ss" / "Mods" /
                          "TamperMod").exists())

    # ================================================================
    # 4. UE4SS-runtime-as-mod refused at the detect stage
    # ================================================================
    def test_ue4ss_runtime_refused_at_detect(self):
        archive = make_zip(self.zipdir / "runtime.zip", {
            "UE4SS.dll": b"MZ fake runtime dll",
            "dwmapi.dll": b"MZ fake dwmapi",
            "UE4SS-settings.ini": "[Engine]\n",
            "Mods/ConsoleEnablerMod/Scripts/main.lua": LUA_OK,
        })
        res = self.pipe.install_mod(
            archive, {"name": "RuntimeZip", "version": "1.0"}, TITLE,
            str(self.game))
        self.assertFalse(res.ok)
        self.assertEqual(res.stage, "detect")
        self.assertIn("UE4SS runtime", res.refusal)
        self.assertEqual(res.detection["label"], "ue4ss-runtime")
        # nothing was created anywhere
        self.assertFalse(self.title_root().exists(),
                         "detect refusal must not touch the game root")

    def test_pak_mod_refused_at_detect(self):
        archive = make_zip(self.zipdir / "pak.zip", {
            "Content/Paks/MyMod_P.pak": b"not-really-a-pak",
        })
        res = self.pipe.install_mod(
            archive, {"name": "PakMod", "version": "1.0"}, TITLE,
            str(self.game))
        self.assertFalse(res.ok)
        self.assertEqual(res.stage, "detect")
        self.assertIn("pak-iostore", res.refusal)
        self.assertFalse(self.title_root().exists())

    # ================================================================
    # 5. compat grading wired into the install path
    # ================================================================
    def test_compat_compatible_grade_installs(self):
        if not CATALOG.is_file():
            self.skipTest("compat fixture catalog not found")
        res = self.install("CompatOK", lua=LUA_COMPAT_OK, catalog=str(CATALOG))
        self.assertTrue(res.ok, res.refusal or res.error)
        self.assertEqual(res.compat["grade"], "compatible")

    def test_compat_unsupported_grade_blocks(self):
        if not CATALOG.is_file():
            self.skipTest("compat fixture catalog not found")
        res = self.install("CompatBad", lua=LUA_UNSUPPORTED,
                           catalog=str(CATALOG))
        self.assertFalse(res.ok)
        self.assertEqual(res.stage, "compat")
        self.assertIn("unsupported", res.refusal)
        self.assertIn("RegisterConsoleCommandHandler",
                      res.compat["missing_apis"])
        self.assertFalse(self.title_root().exists(),
                         "compat refusal must not touch the game root")

    # ================================================================
    # 6. check_mod: the dry-run decision, zero writes
    # ================================================================
    def test_check_mod_dry_run(self):
        self.assertTrue(self.install("ModA").ok)
        archive = self.zip_mod("CheckMod",
                               extra={"CheckMod/config/x.ini": "[a]\nb=2\n"})
        cres = self.pipe.check_mod(archive, {"name": "CheckMod",
                                             "version": "1.0"}, TITLE,
                                   str(self.game))
        self.assertIsInstance(cres, CheckResult)
        self.assertTrue(cres.ok, cres.refusal)
        self.assertEqual(cres.mod_id, "CheckMod")
        self.assertEqual(cres.planned_files, [
            "ue4ss/Mods/CheckMod/Scripts/main.lua",
            "ue4ss/Mods/CheckMod/config/x.ini"])
        self.assertEqual(cres.planned_marker,
                         "ue4ss/Mods/CheckMod/enabled.txt")
        # zero writes: no manifest, no mod dir, no registry change
        self.assertFalse(self.manifest_path("CheckMod").exists())
        self.assertFalse((self.title_root() / "ue4ss" / "Mods" /
                          "CheckMod").exists())
        self.assertEqual(list(self.pipe.installed_mods(str(self.game),
                                                       TITLE)), ["ModA"])

        blocked = self.pipe.check_mod(self.zip_mod("BadMod"),
                                      {"name": "BadMod", "version": "1",
                                       "requires": ["Ghost"]},
                                      TITLE, str(self.game))
        self.assertFalse(blocked.ok)
        self.assertEqual(blocked.stage, "conflicts")
        self.assertIn("MISSING_DEPENDENCY", blocked.refusal)

    # ================================================================
    # 7. dependents block uninstall (uninstall flow step b)
    # ================================================================
    def test_dependents_block_uninstall(self):
        self.assertTrue(self.install("LibMod").ok)
        self.assertTrue(self.install("UserMod",
                                     md_extra={"requires": ["LibMod"]}).ok)
        blocked = self.pipe.uninstall_mod(str(self.manifest_path("LibMod")),
                                          str(self.game))
        self.assertFalse(blocked.ok)
        self.assertEqual(blocked.stage, "dependents")
        self.assertEqual(blocked.error_kind, "ModStillRequiredError")
        self.assertIn("UserMod", blocked.error)
        self.assertTrue(self.manifest_path("LibMod").exists(),
                        "dependent-blocked uninstall must not remove files")
        # uninstall in dependency order succeeds
        self.assertTrue(self.pipe.uninstall_mod(
            str(self.manifest_path("UserMod")), str(self.game)).ok)
        last = self.pipe.uninstall_mod(str(self.manifest_path("LibMod")),
                                       str(self.game))
        self.assertTrue(last.ok, last.error)

    # ================================================================
    # 8. contract reconciliation details (#22)
    # ================================================================
    def test_manifest_game_root_mismatch_rejected(self):
        self.assertTrue(self.install("WhereMod").ok)
        res = self.pipe.uninstall_mod(str(self.manifest_path("WhereMod")),
                                      str(self.base))  # wrong roots base
        self.assertFalse(res.ok)
        self.assertEqual(res.stage, "manifest")
        self.assertIn("belongs to title root", res.error)

    def test_archive_shipped_enabled_marker(self):
        res = self.install("MarkMod",
                           extra={"MarkMod/enabled.txt": ""})
        self.assertTrue(res.ok, res.refusal or res.error)
        with open(self.manifest_path("MarkMod"), "r",
                  encoding="utf-8") as f:
            imf = json.load(f)
        ce = imf["config_entries"]
        self.assertEqual(ce["enabled_flag_source"], "archive-shipped")
        # conversion: no extra enabled_markers entry (file already in files)
        um = installer_manifest_to_uninstall(imf)
        self.assertEqual(um["enabled_markers"], [])
        self.assertTrue(any(f["path"].endswith("MarkMod/enabled.txt")
                            for f in um["files"]))
        ures = self.pipe.uninstall_mod(str(self.manifest_path("MarkMod")),
                                       str(self.game))
        self.assertTrue(ures.ok, ures.error)
        self.assertFalse((self.title_root() / "ue4ss" / "Mods" /
                          "MarkMod").exists())

    def test_overrides_and_stale_files_removed_on_uninstall(self):
        md = {"name": "OverrideMod", "version": "1.0.0",
              "ue4ss_settings": {"General": {"ConsoleEnabled": "1"}}}
        archive = make_zip(self.zipdir / "ov1.zip", {
            "OverrideMod/Scripts/main.lua": LUA_OK,
            "OverrideMod/old.txt": "stale payload",
        })
        res = self.pipe.install_mod(archive, md, TITLE, str(self.game))
        self.assertTrue(res.ok)
        # upgrade without old.txt -> recorded as stale, left in place
        archive2 = make_zip(self.zipdir / "ov2.zip", {
            "OverrideMod/Scripts/main.lua": LUA_OK + "-- v2\n",
        })
        md2 = dict(md, version="2.0.0")
        res2 = self.pipe.install_mod(archive2, md2, TITLE, str(self.game))
        self.assertTrue(res2.ok)
        self.assertEqual(res2.manifest["stale_files_left_in_place"],
                         ["old.txt"])
        ov = (self.title_root() / "ue4ss" / "overrides" /
              "OverrideMod.ini")
        self.assertTrue(ov.is_file(), "ue4ss overrides ini not written")
        # unified state updated to the new version's manifest
        installed = self.pipe.installed_mods(str(self.game), TITLE)
        self.assertEqual(installed["OverrideMod"]["manifest"],
                         str(self.manifest_path("OverrideMod")))

        ures = self.pipe.uninstall_mod(
            str(self.manifest_path("OverrideMod")), str(self.game))
        self.assertTrue(ures.ok, ures.error)
        self.assertFalse((self.title_root() / "ue4ss" / "Mods" /
                          "OverrideMod").exists())
        self.assertFalse(ov.exists(), "overrides ini left behind")
        self.assertFalse((self.title_root() / "ue4ss" / "Mods" /
                          "OverrideMod" / "old.txt").exists(),
                         "stale file left behind")

    def test_register_existing_adopts_raw_installer_install(self):
        # an install made directly through installer.py (no unified state)
        self.assertTrue(self.install("OrphanMod").ok)
        state_file = (self.state / "install_state" /
                      (TITLE + ".json"))
        state_file.unlink()
        res = self.pipe.uninstall_mod(str(self.manifest_path("OrphanMod")),
                                      str(self.game))
        self.assertFalse(res.ok)
        self.assertEqual(res.stage, "plan")
        self.assertIn("register_existing", res.error)
        path = self.pipe.register_existing(str(self.manifest_path("OrphanMod")),
                                           str(self.game))
        self.assertEqual(path, str(state_file))
        ures = self.pipe.uninstall_mod(str(self.manifest_path("OrphanMod")),
                                       str(self.game))
        self.assertTrue(ures.ok, ures.error)

    def test_uninstall_rollback_restores_everything(self):
        self.seed_configs()
        self.assertTrue(self.install("RollMod").ok)
        ures = self.pipe.uninstall_mod(str(self.manifest_path("RollMod")),
                                       str(self.game))
        self.assertTrue(ures.ok)
        token = ures.core.rollback_token
        rr = self.pipe.rollback_uninstall(token, TITLE, str(self.game))
        self.assertTrue(rr["ok"], rr.get("error"))
        self.assertTrue(rr["ploader_restored"],
                        "ploader.ini snapshot not restored")
        # files, manifest, config rows and registry entry are all back
        self.assertTrue((self.title_root() / "ue4ss" / "Mods" / "RollMod" /
                         "Scripts" / "main.lua").is_file())
        self.assertTrue(self.manifest_path("RollMod").exists())
        self.assertIn("mod\tRollMod\t1.0.0",
                      self.read(self.title_root() / "loadlist.txt"))
        pl = self.read(self.plugins / "ploader.ini")
        self.assertIn("[%s]" % TITLE, pl)
        self.assertIn("modbridge_supplier_composition.prx=2", pl)
        self.assertIn("RollMod",
                      self.pipe.installed_mods(str(self.game), TITLE))

    def test_shared_ploader_line_kept_while_other_mod_needs_it(self):
        # both mods use the DEFAULT prx line; uninstalling one must keep
        # the shared ploader line alive for the other
        self.assertTrue(self.install("SharedA").ok)
        self.assertTrue(self.install("SharedB").ok)
        ures = self.pipe.uninstall_mod(str(self.manifest_path("SharedA")),
                                       str(self.game))
        self.assertTrue(ures.ok, ures.error)
        self.assertTrue(ures.ploader["line_kept_shared"])
        self.assertFalse(ures.ploader["line_removed"])
        pl = self.read(self.plugins / "ploader.ini")
        self.assertIn("modbridge_supplier_composition.prx=2", pl)
        # the last one out removes it
        ures2 = self.pipe.uninstall_mod(str(self.manifest_path("SharedB")),
                                        str(self.game))
        self.assertTrue(ures2.ok)
        self.assertTrue(ures2.ploader["line_removed"])
        self.assertNotIn("modbridge_supplier_composition.prx=2",
                         self.read(self.plugins / "ploader.ini"))

    def test_unified_state_feeds_conflict_checker(self):
        # #23 from the conflict side: an install done through the pipeline
        # is immediately visible to the conflict check for the next one
        self.assertTrue(self.install("SeenMod").ok)
        res = self.install("NeedsSeen",
                           md_extra={"requires": ["SeenMod"]})
        self.assertTrue(res.ok, res.refusal)  # dependency seen -> no block
        gone = self.pipe.uninstall_mod(str(self.manifest_path("SeenMod")),
                                       str(self.game))
        # NeedsSeen now depends on a removed mod -> uninstall blocked
        self.assertFalse(gone.ok)
        self.assertEqual(gone.stage, "dependents")


if __name__ == "__main__":
    unittest.main(verbosity=2)
