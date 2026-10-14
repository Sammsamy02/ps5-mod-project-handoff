#!/usr/bin/env python3
"""Host-only (Mac) EDGE-CASE tests for adapter.py — written by an
independent verifier, complementing test_adapter.py.

Run:  python3 -m unittest -v test_adapter_edge.py

Each test maps to a requested edge case:

  a. test_install_empty_archive_fails_gracefully
  b. test_install_missing_game_root_is_created  (+ file-blocking game root)
  c. test_uninstall_corrupted_manifest_fails_clearly
     (+ wrong-schema and missing manifest variants)
  d. test_check_mod_non_zip_reports_unsupported_format
     (plain text masquerading as .zip, and a real 7z magic file)
  e. test_two_mods_share_ploader_line_without_duplicate
  f. test_installed_mods_list_after_install_and_uninstall
  g. test_full_round_trip_install_verify_check_uninstall_clean

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

LUA_OK = (
    "local it = FindFirstOf('Item')\n"
    "RegisterKeyBind(Key.F, function() print(it) end)\n"
)

BASE_PLOADER = "[OtherTitle]\nfoo=1\n"
PLOADER_LINE = "modbridge_supplier_composition.prx=2"


def make_zip(path, entries):
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
        for rel, data in entries.items():
            if isinstance(data, str):
                data = data.encode("utf-8")
            z.writestr(rel, data)
    return str(path)


class AdapterEdgeTestCase(unittest.TestCase):
    """Fresh sandbox per test: game root, plugins dir, state dir, pipeline."""

    def setUp(self):
        tmp = tempfile.TemporaryDirectory(
            prefix="adapter-edge-",
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

    # ================================================================
    # a. empty archive: fail gracefully with a clear error
    # ================================================================
    def test_install_empty_archive_fails_gracefully(self):
        empty = make_zip(self.zipdir / "empty.zip", {})
        md = {"name": "EmptyMod", "version": "1.0"}

        res = self.pipe.install_mod(empty, md, TITLE, str(self.game))
        self.assertIsInstance(res, InstallResult)
        self.assertFalse(res.ok)
        self.assertEqual(res.stage, "detect")
        self.assertEqual(res.error_kind, "Refused")
        # clear reason: not silently swallowed, not a traceback
        self.assertIn("not an installable UE4SS Lua mod", res.refusal)
        self.assertIn("unknown", res.refusal)
        self.assertEqual(res.detection["label"], "unknown")
        self.assertTrue(res.detection["flags"]["empty_archive"])
        # graceful: nothing created anywhere, nothing registered
        self.assertFalse(self.title_root().exists())
        self.assertFalse(self.plugins.exists())
        self.assertEqual(self.pipe.installed_mods(str(self.game), TITLE), {})

        # check_mod (dry run) refuses the same way
        cres = self.pipe.check_mod(empty, md, TITLE, str(self.game))
        self.assertIsInstance(cres, CheckResult)
        self.assertFalse(cres.ok)
        self.assertEqual(cres.stage, "detect")
        self.assertIn("not an installable UE4SS Lua mod", cres.refusal)
        self.assertEqual(self.pipe.installed_mods(str(self.game), TITLE), {})

    # ================================================================
    # b. missing game root: created; path blocked by a file: clear error
    # ================================================================
    def test_install_missing_game_root_is_created(self):
        missing_root = self.base / "created" / "by" / "installer"
        self.assertFalse(missing_root.exists())
        archive = self.zip_mod("FreshRootMod")
        md = {"name": "FreshRootMod", "version": "1.0.0"}

        res = self.pipe.install_mod(archive, md, TITLE, str(missing_root))
        self.assertTrue(res.ok, "install refused: %s" % (res.refusal or res.error))
        self.assertEqual(res.stage, "done")
        # the whole title tree was created under the previously-missing root
        self.assertTrue((missing_root / TITLE / "ue4ss" / "Mods" /
                         "FreshRootMod" / "Scripts" / "main.lua").is_file())
        self.assertTrue((missing_root / TITLE / "manifests" /
                         "FreshRootMod.json").is_file())
        state_file = self.state / "install_state" / (TITLE + ".json")
        self.assertTrue(state_file.is_file(), "unified state not written")
        self.assertIn("FreshRootMod",
                      self.pipe.installed_mods(str(missing_root), TITLE))
        # and it uninstalls cleanly from the created tree too
        ures = self.pipe.uninstall_mod(
            str(missing_root / TITLE / "manifests" / "FreshRootMod.json"),
            str(missing_root))
        self.assertTrue(ures.ok, ures.error)
        self.assertEqual(self.pipe.installed_mods(str(missing_root), TITLE), {})

    def test_install_game_root_occupied_by_file_fails_clearly(self):
        blocker = self.base / "blocker"
        blocker.write_text("i am a file, not a directory", encoding="utf-8")
        archive = self.zip_mod("BlockedMod")
        md = {"name": "BlockedMod", "version": "1.0.0"}

        res = self.pipe.install_mod(archive, md, TITLE, str(blocker))
        self.assertFalse(res.ok)
        self.assertEqual(res.stage, "install")
        self.assertEqual(res.error_kind, "OSError")
        self.assertIn("io:", res.error)  # reported, not raised as a crash
        # the blocker file was not clobbered
        self.assertEqual(blocker.read_text(encoding="utf-8"),
                         "i am a file, not a directory")

    # ================================================================
    # c. corrupted manifest: clear error, no crash, files untouched
    # ================================================================
    def test_uninstall_corrupted_manifest_fails_clearly(self):
        self.assertTrue(self.install("CorruptMod").ok)
        mp = self.manifest_path("CorruptMod")
        main_lua = (self.title_root() / "ue4ss" / "Mods" / "CorruptMod" /
                    "Scripts" / "main.lua")
        self.assertTrue(main_lua.is_file())

        mp.write_text("{ this is not valid json !!!", encoding="utf-8")
        ures = self.pipe.uninstall_mod(str(mp), str(self.game))
        self.assertIsInstance(ures, UninstallResult)
        self.assertFalse(ures.ok)
        self.assertEqual(ures.stage, "manifest")
        self.assertEqual(ures.error_kind, "JSONDecodeError")
        self.assertIn("cannot read manifest", ures.error)
        # graceful: nothing was removed, registration intact
        self.assertTrue(main_lua.is_file(), "files removed under corrupt manifest")
        self.assertTrue(mp.exists())
        self.assertIn("CorruptMod",
                      self.pipe.installed_mods(str(self.game), TITLE))

    def test_uninstall_wrong_schema_manifest_fails_clearly(self):
        self.assertTrue(self.install("SchemaMod").ok)
        mp = self.manifest_path("SchemaMod")
        mp.write_text(json.dumps({"schema": "something-else/9"}),
                      encoding="utf-8")
        ures = self.pipe.uninstall_mod(str(mp), str(self.game))
        self.assertFalse(ures.ok)
        self.assertEqual(ures.stage, "manifest")
        self.assertEqual(ures.error_kind, "ManifestError")
        self.assertIn("not a modbridge-manifest/1 installer manifest",
                      ures.error)
        self.assertTrue((self.title_root() / "ue4ss" / "Mods" / "SchemaMod"
                         ).exists(), "files removed under wrong-schema manifest")

    def test_uninstall_missing_manifest_fails_clearly(self):
        ures = self.pipe.uninstall_mod(
            str(self.manifest_path("GhostMod")), str(self.game))
        self.assertFalse(ures.ok)
        self.assertEqual(ures.stage, "manifest")
        self.assertEqual(ures.error_kind, "FileNotFoundError")
        self.assertIn("cannot read manifest", ures.error)

    # ================================================================
    # d. non-zip archive: unsupported format reported, no crash
    # ================================================================
    def test_check_mod_non_zip_reports_unsupported_format(self):
        # plain text masquerading as a .zip (no zip/tar magic bytes)
        notazip = self.zipdir / "notazip.zip"
        notazip.write_text("this is definitely not a zip file",
                           encoding="utf-8")
        md = {"name": "FakeZip", "version": "1.0"}
        cres = self.pipe.check_mod(str(notazip), md, TITLE, str(self.game))
        self.assertIsInstance(cres, CheckResult)
        self.assertFalse(cres.ok)
        self.assertEqual(cres.stage, "detect")
        self.assertIn("unrecognized archive format", cres.refusal)
        self.assertIn("no zip/tar magic bytes", cres.refusal)
        # install path refuses identically (reported, not raised)
        res = self.pipe.install_mod(str(notazip), md, TITLE, str(self.game))
        self.assertFalse(res.ok)
        self.assertEqual(res.stage, "detect")
        self.assertEqual(res.error_kind, "Refused")
        self.assertIn("unrecognized archive format", res.refusal)
        self.assertFalse(self.title_root().exists())

    def test_check_mod_7z_reports_unsupported_format(self):
        # real 7z magic bytes -> explicit unsupported-format + guidance
        seven = self.zipdir / "seven.7z"
        seven.write_bytes(b"7z\xbc\xaf\x27\x1c" + b"\x00" * 64)
        cres = self.pipe.check_mod(str(seven), {"name": "SevenZip",
                                                "version": "1.0"},
                                   TITLE, str(self.game))
        self.assertFalse(cres.ok)
        self.assertEqual(cres.stage, "detect")
        self.assertIn("7z archive", cres.refusal)
        self.assertIn("supports zip/tar", cres.refusal)
        self.assertEqual(self.pipe.installed_mods(str(self.game), TITLE), {})

    # ================================================================
    # e. two mods, same ploader.ini line: shared, never duplicated
    # ================================================================
    def test_two_mods_share_ploader_line_without_duplicate(self):
        self.plugins.mkdir(parents=True)
        (self.plugins / "ploader.ini").write_text(BASE_PLOADER,
                                                  encoding="utf-8")
        self.assertTrue(self.install("ShareA").ok)
        self.assertTrue(self.install("ShareB").ok)

        text = self.read(self.plugins / "ploader.ini")
        self.assertEqual(text.count(PLOADER_LINE), 1,
                         "ploader line duplicated: %r" % text)
        self.assertEqual(text.count("[%s]" % TITLE), 1,
                         "title section duplicated: %r" % text)
        self.assertIn("[OtherTitle]", text, "foreign ploader section lost")

        # first uninstall keeps the shared line alive for the other mod
        ures = self.pipe.uninstall_mod(str(self.manifest_path("ShareA")),
                                       str(self.game))
        self.assertTrue(ures.ok, ures.error)
        self.assertTrue(ures.ploader["line_kept_shared"])
        self.assertFalse(ures.ploader["line_removed"])
        text = self.read(self.plugins / "ploader.ini")
        self.assertEqual(text.count(PLOADER_LINE), 1,
                         "shared ploader line removed too early")
        self.assertIn("[%s]" % TITLE, text)

        # second (last) uninstall removes line AND installer-created section
        ures2 = self.pipe.uninstall_mod(str(self.manifest_path("ShareB")),
                                        str(self.game))
        self.assertTrue(ures2.ok, ures2.error)
        self.assertTrue(ures2.ploader["line_removed"])
        self.assertTrue(ures2.ploader["section_removed"])
        text = self.read(self.plugins / "ploader.ini")
        self.assertNotIn(PLOADER_LINE, text)
        self.assertNotIn("[%s]" % TITLE, text)
        self.assertIn("[OtherTitle]", text, "foreign ploader section lost")

    # ================================================================
    # f. installed_mods(): correct list after install + after uninstall
    # ================================================================
    def test_installed_mods_list_after_install_and_uninstall(self):
        self.assertEqual(self.pipe.installed_mods(str(self.game), TITLE), {},
                         "fresh registry must be empty")

        self.assertTrue(self.install("ListA").ok)
        self.assertTrue(self.install("ListB",
                                     md_extra={"requires": ["ListA"]}).ok)
        installed = self.pipe.installed_mods(str(self.game), TITLE)
        self.assertEqual(sorted(installed), ["ListA", "ListB"])
        # entry structure: manifest path, requires, hashed files, ploader info
        entry = installed["ListA"]
        self.assertEqual(entry["manifest"],
                         str(self.manifest_path("ListA")))
        self.assertEqual(entry["requires"], [])
        self.assertTrue(all("path" in f and "sha256" in f
                            for f in entry["files"]))
        self.assertTrue(any(f["path"].endswith("ListA/Scripts/main.lua")
                            for f in entry["files"]))
        self.assertEqual(entry["ploader"]["name"],
                         "modbridge_supplier_composition.prx")
        self.assertEqual(entry["ploader"]["mode"], "2")
        self.assertEqual(installed["ListB"]["requires"], ["ListA"])

        # uninstalling ListA is BLOCKED (ListB still requires it) — the
        # registry must still show both mods afterwards
        blocked = self.pipe.uninstall_mod(str(self.manifest_path("ListA")),
                                          str(self.game))
        self.assertFalse(blocked.ok)
        self.assertEqual(blocked.stage, "dependents")
        self.assertEqual(sorted(self.pipe.installed_mods(str(self.game),
                                                         TITLE)),
                         ["ListA", "ListB"])
        # uninstall in dependency order, registry shrinks step by step
        self.assertTrue(self.pipe.uninstall_mod(
            str(self.manifest_path("ListB")), str(self.game)).ok)
        self.assertEqual(list(self.pipe.installed_mods(str(self.game),
                                                       TITLE)), ["ListA"])
        self.assertTrue(self.pipe.uninstall_mod(
            str(self.manifest_path("ListA")), str(self.game)).ok)
        self.assertEqual(self.pipe.installed_mods(str(self.game), TITLE), {},
                         "registry not empty after uninstalling everything")

    # ================================================================
    # g. full round trip: install -> verify -> check (dry run shows
    #    installed) -> uninstall -> verify clean -> installed_mods() empty
    # ================================================================
    def test_full_round_trip_install_verify_check_uninstall_clean(self):
        self.plugins.mkdir(parents=True)
        (self.plugins / "ploader.ini").write_text(BASE_PLOADER,
                                                  encoding="utf-8")
        archive = self.zip_mod("RoundTrip",
                               extra={"RoundTrip/config/x.ini": "[a]\nb=2\n"})
        md = {"name": "RoundTrip", "version": "1.0.0"}

        # 1. install
        res = self.pipe.install_mod(archive, md, TITLE, str(self.game))
        self.assertTrue(res.ok, "install refused: %s" % (res.refusal or res.error))
        self.assertEqual(res.stage, "done")
        self.assertTrue(res.verify["ok"])

        # 2. verify (re-check through the standalone entry point)
        report = self.pipe.verify_mod("RoundTrip", TITLE, str(self.game))
        self.assertTrue(report["ok"], report["problems"])

        # 3. check dry-run of the SAME archive: reinstall-over-own-files is
        #    fine, and the registry shows the mod installed
        cres = self.pipe.check_mod(archive, md, TITLE, str(self.game))
        self.assertTrue(cres.ok, cres.refusal)
        self.assertEqual(cres.mod_id, "RoundTrip")
        self.assertIn("RoundTrip",
                      self.pipe.installed_mods(str(self.game), TITLE))

        # 4. uninstall (dry run first, then the real one)
        dres = self.pipe.uninstall_mod(str(self.manifest_path("RoundTrip")),
                                       str(self.game), dry_run=True)
        self.assertTrue(dres.ok, dres.error)
        self.assertTrue(dres.core.dry_run)
        # dry run changed nothing
        self.assertTrue((self.title_root() / "ue4ss" / "Mods" / "RoundTrip"
                         / "Scripts" / "main.lua").is_file())
        ures = self.pipe.uninstall_mod(str(self.manifest_path("RoundTrip")),
                                       str(self.game))
        self.assertTrue(ures.ok, ures.error)
        self.assertEqual(ures.stage, "done")

        # 5. verify clean: mod dir, manifest, config rows all gone
        self.assertFalse((self.title_root() / "ue4ss" / "Mods" /
                          "RoundTrip").exists(), "mod dir left after uninstall")
        self.assertFalse(self.manifest_path("RoundTrip").exists(),
                         "manifest left after uninstall")
        self.assertNotIn("RoundTrip",
                         self.read(self.title_root() / "loadlist.txt"))
        self.assertNotIn("RoundTrip",
                         self.read(self.title_root() / "ue4ss" / "Mods" /
                                   "mods.txt"))
        pl = self.read(self.plugins / "ploader.ini")
        self.assertNotIn("[%s]" % TITLE, pl)
        self.assertNotIn(PLOADER_LINE, pl)
        self.assertIn("[OtherTitle]", pl)
        # installer-side verification now fails (nothing left to verify)
        with self.assertRaises(installer.InstallError):
            self.pipe.verify_mod("RoundTrip", TITLE, str(self.game))
        # and a fresh dry-run uninstall finds no manifest either
        ures2 = self.pipe.uninstall_mod(str(self.manifest_path("RoundTrip")),
                                        str(self.game))
        self.assertFalse(ures2.ok)
        self.assertEqual(ures2.stage, "manifest")

        # 6. installed_mods() empty
        self.assertEqual(self.pipe.installed_mods(str(self.game), TITLE), {})

    # ================================================================
    # h. documented JSON response schemas (docs/HOWTO-MOD-MANAGER.md
    #    sections 2/3/5): every field the doc documents must be present,
    #    with the documented shape, on the adapter's real outputs.
    # ================================================================
    def test_response_schemas_match_howto_doc(self):
        md = {"name": "SchemaMod", "version": "1.2.0",
              "ploader_prx": {"name": "modbridge_supplier_composition.prx",
                              "mode": "2"}}
        res = self.install("SchemaMod", md_extra=md)

        # -- doc section 2: manifest artifact (schema modbridge-manifest/1)
        m = res.manifest
        self.assertEqual(m["schema"], "modbridge-manifest/1")
        for key in ("mod_name", "mod_dir", "title_id", "version"):
            self.assertIsInstance(m[key], str, "manifest.%s" % key)
        self.assertEqual(m["title_id"], TITLE)
        # files[] entries carry exactly the documented fields
        fe = m["files"][0]
        self.assertEqual(
            sorted(fe),
            ["archive_path", "install_path", "install_rel",
             "installed_sha256", "original_sha256", "role", "size"])
        self.assertTrue(fe["install_rel"].startswith("ue4ss/Mods/SchemaMod/"))
        self.assertTrue(os.path.isabs(fe["install_path"]))
        # documented config_entries keys
        ce = m["config_entries"]
        for key in ("mods_txt_row", "enabled_marker_rel",
                    "ploader_prx_name", "ploader_prx_mode"):
            self.assertIn(key, ce, "manifest.config_entries.%s" % key)
        # "Also recorded" list from the doc
        self.assertIn("sha256", m["archive"])
        for key in ("installed_at", "transaction_id",
                    "skipped_outside_mod_root", "stale_files_left_in_place",
                    "file_list_mismatch"):
            self.assertIn(key, m, "manifest.%s (doc 'also recorded')" % key)

        # -- doc section 2: verify response (exact documented key set)
        self.assertEqual(sorted(res.verify),
                         ["files_checked", "mod", "ok", "problems",
                          "title_id"])

        # -- doc section 3: uninstaller record produced by the adapter
        um = installer_manifest_to_uninstall(m)
        self.assertEqual(um["mod_id"], "SchemaMod")
        self.assertEqual(um["requires"], [])
        self.assertEqual(sorted(um),
                         ["config_entries", "enabled_markers", "files",
                          "mod_id", "mod_version", "name", "requires",
                          "schema"])
        for f in um["files"]:
            self.assertEqual(sorted(f), ["path", "sha256"])
            self.assertFalse(os.path.isabs(f["path"]))
            self.assertFalse(f["path"].startswith(".."))
        # ploader.ini is intentionally NOT in the record: it lives outside
        # the uninstaller's game_root and the adapter cleans it up itself
        self.assertNotIn("ploader.ini", um["config_entries"])
        self.assertIn("loadlist.txt", um["config_entries"])
        self.assertEqual(um["config_entries"]["loadlist.txt"][0],
                         "mod\tSchemaMod\t1.2.0")

        # -- doc section 3: install_state shape {"game_version", "mods": {...}}
        state = json.loads((self.state / "install_state" /
                            (TITLE + ".json")).read_text(encoding="utf-8"))
        self.assertIn("game_version", state)
        self.assertEqual(sorted(state["mods"]["SchemaMod"]),
                         ["files", "manifest", "ploader", "requires"])

        # -- doc section 3: UninstallResult response lists + rollback token
        ures = self.pipe.uninstall_mod(str(self.manifest_path("SchemaMod")),
                                       str(self.game))
        self.assertTrue(ures.ok, ures.error)
        for key in ("mod_id", "removed", "removed_modified", "missing",
                    "shared_kept", "markers_removed", "config_lines_removed",
                    "pruned_dirs", "rollback_token", "dry_run"):
            self.assertTrue(hasattr(ures.core, key),
                            "uninstall result.%s" % key)
        self.assertFalse(ures.core.dry_run)

        # -- doc section 5: grade vocabulary on the compat summary
        if res.compat is not None:
            self.assertIn(res.compat["grade"],
                          {"compatible", "partly-compatible", "unsupported",
                           "unknown"})

    def test_compat_grade_vocabulary_matches_doc(self):
        fixture = Path(__file__).resolve().parent.parent / "compat" / \
            "sample_catalog_TEST_FIXTURE.json"
        if not fixture.is_file():
            self.skipTest("compat fixture catalog not found")
        lua_ok = ("local it = FindFirstOf('Item')\n"
                  "RegisterKeyBind('F', function() print(tostring(it)) end)\n")
        lua_bad = ("RegisterConsoleCommandHandler('spawn', function() end)\n"
                   "RegisterKeyBind(Key.G, function() end)\n")
        good = self.install("GradeOK", lua=lua_ok, catalog=str(fixture))
        self.assertTrue(good.ok, good.refusal or good.error)
        self.assertEqual(good.compat["grade"], "compatible")
        bad = self.install("GradeBad", lua=lua_bad, catalog=str(fixture))
        self.assertFalse(bad.ok)
        self.assertEqual(bad.stage, "compat")
        self.assertEqual(bad.compat["grade"], "unsupported")
        self.assertIn("RegisterConsoleCommandHandler",
                      bad.compat["missing_apis"])
        for summary in (good.compat, bad.compat):
            for key in ("grade", "grade_reason", "catalog_version"):
                self.assertIn(key, summary, "compat.%s (doc section 5)" % key)


if __name__ == "__main__":
    unittest.main(verbosity=2)
