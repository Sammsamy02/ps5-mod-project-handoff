#!/usr/bin/env python3
"""Phase-2 config-output integration tests — Mac-only, host-tested.

Proves ONE install produces ALL FOUR runtime config outputs:

    1. loadlist.txt row                  (modforge TSV block)
    2. ue4ss/Mods/mods.txt entry         (UE4SS mods list, "<Name> : 1")
    3. ue4ss/Mods/<ModName>/enabled.txt  (empty enable marker)
    4. ploader.ini line                  ([TITLE] <prx>=2)

and ONE uninstall restores ALL FOUR to the pre-install state
byte-for-byte (the marker back to absent).  Also covers: journal rollback
of all four on a mid-install crash, archive-shipped enabled.txt, a
case-variant Enabled.txt rejection, and the mods.txt line editor.

The runtime side of output 2/3 is UE4SS (see its UE4SSProgram.cpp
start_mods + Platform/NativeModSource.cpp): a mod starts when mods.txt
carries an enabled "Name : 1" row OR ue4ss/Mods/<Name>/enabled.txt
exists; the native parser requires every row to be '<name>:<0|1>' after
whitespace stripping, so every row written here must round-trip through
installer.parse_mods_txt_row.

Host-only: no FTP, no payloads, no launches, no console contact.
Everything happens in temp dirs.

Run:  python3 -m unittest -v test_config_outputs
"""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import installer  # noqa: E402
from installer import (  # noqa: E402
    CaseConflictError, Installer, atomic_write_bytes, ini_has_line,
    mods_txt_has_mod, parse_mods_txt_row, upsert_mods_txt)

import uninstall  # noqa: E402
from uninstall import Uninstaller  # noqa: E402

INSTALLER_PY = os.path.join(HERE, "installer.py")

TITLE = "PPSA33286"
MOD_NAME = "Townfall Item Spawner"
MOD_DIR = "Townfall_Item_Spawner"
PRX = "modbridge_supplier_composition.prx"

MAIN_LUA = b"-- main.lua\nRegisterMod('ItemSpawner', 'Mod')\n"
UTIL_LUA = b"-- util\nfunction helper() end\n"

# Pre-install shared state: another mod's rows/lines in every config file.
LOADLIST_BEFORE = (
    "modforge-loadlist\t1\n"
    "title\t%s\n"
    "engine\tunreal\n"
    "mod\tOtherMod\t0.9\n"
    "file\tlua\tue4ss/Mods/OtherMod/Scripts/main.lua\t7\n" % TITLE)
MODS_TXT_BEFORE = (
    "; UE4SS mods list (rows: 'Name : 1' enabled / 'Name : 0' disabled)\n"
    "OtherMod : 1\n")
PLOADER_BEFORE = "; managed by hand\n[OTHERGAME]\nfoo.prx=2\n"


def make_zip(path, entries):
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as zf:
        for name, data in entries:
            zf.writestr(name, data)
    return path


def read_bytes(path):
    with open(path, "rb") as f:
        return f.read()


class ConfigOutputsEnv(object):
    """Sandbox whose whole game tree lives under ONE game_root (the title
    root), with ploader.ini inside it — exactly the view the uninstaller
    takes.  Installer state (journals/backups/manifests bookkeeping for
    uninstall) lives OUTSIDE the game tree."""

    def __init__(self):
        self.base = tempfile.mkdtemp(
            prefix="modbridge-config-test-",
            dir=os.environ.get("MODBRIDGE_TEST_TMPDIR") or None)
        self.roots = os.path.join(self.base, "roots")
        self.state = os.path.join(self.base, "state")
        self.game_root = os.path.join(self.roots, TITLE)
        # plugins dir == title root: ploader.ini lands inside game_root so
        # the uninstaller (game_root-relative) covers all four outputs
        self.plugins = self.game_root
        os.makedirs(self.state, exist_ok=True)
        self.archive = os.path.join(self.base, "mod.zip")
        self.metadata_path = os.path.join(self.base, "meta.json")

    # paths of the four config outputs -------------------------------
    @property
    def loadlist(self):
        return os.path.join(self.game_root, "loadlist.txt")

    @property
    def mods_txt(self):
        return os.path.join(self.game_root, "ue4ss", "Mods", "mods.txt")

    @property
    def marker(self):
        return os.path.join(self.game_root, "ue4ss", "Mods", MOD_DIR,
                            "enabled.txt")

    @property
    def ploader(self):
        return os.path.join(self.game_root, "ploader.ini")

    @property
    def mods_dir(self):
        return os.path.join(self.game_root, "ue4ss", "Mods")

    @property
    def mod_dir(self):
        return os.path.join(self.mods_dir, MOD_DIR)

    def seed_shared_configs(self):
        os.makedirs(os.path.join(self.game_root, "ue4ss", "Mods"),
                    exist_ok=True)
        atomic_write_bytes(self.loadlist, LOADLIST_BEFORE.encode("utf-8"))
        atomic_write_bytes(self.mods_txt, MODS_TXT_BEFORE.encode("utf-8"))
        atomic_write_bytes(self.ploader, PLOADER_BEFORE.encode("utf-8"))

    def write_meta(self, **over):
        md = {
            "name": MOD_NAME,
            "version": "1.2.0",
            "source_url": "https://www.nexusmods.com/townfall/mods/42",
            "downloaded_at": "2026-10-07T01:02:03Z",
            "file_list": ["MyMod/Scripts/main.lua",
                          "MyMod/Scripts/helpers/util.lua"],
        }
        md.update(over)
        with open(self.metadata_path, "w") as f:
            json.dump(md, f)
        return md

    def installer(self, **kw):
        return Installer(self.roots, self.plugins, self.state, **kw)

    def cleanup(self):
        shutil.rmtree(self.base, ignore_errors=True)


def installer_manifest_to_uninstall(imanifest, game_root):
    """Adapter: the installer's per-mod manifest -> the uninstall module's
    manifest shape (game_root-relative paths + exact owned lines/markers).

    This encodes the config-entry ownership contract: uninstall removes
    EXACTLY what the installer recorded — the loadlist block, the single
    mods.txt row, the ploader lines the installer ADDED (including the
    blank separator + section header it created), and the enable marker
    (when installer-created; an archive-shipped marker is a tracked file).
    """
    ce = imanifest["config_entries"]
    files = [{"path": os.path.relpath(f["install_path"], game_root),
              "sha256": f["original_sha256"]}
             for f in imanifest["files"]]
    out = {
        "mod_id": imanifest["mod_dir"],
        "files": files,
        "config_entries": {
            "loadlist.txt": list(ce["loadlist_rows"]),
            "ue4ss/Mods/mods.txt": [ce["mods_txt_row"]],
            "ploader.ini": list(ce.get("ploader_added_lines") or []),
        },
    }
    if ce.get("enabled_flag_source") == "installer-created":
        out["enabled_markers"] = [ce["enabled_marker_rel"]]
    return out


class ConfigOutputsTestBase(unittest.TestCase):
    def setUp(self):
        self.env = ConfigOutputsEnv()
        make_zip(self.env.archive, [
            ("MyMod/Scripts/main.lua", MAIN_LUA),
            ("MyMod/Scripts/helpers/util.lua", UTIL_LUA)])
        self.env.write_meta()
        self.env.seed_shared_configs()
        # pre-install state of ALL FOUR outputs (captured BEFORE install)
        self.pre = {
            "loadlist": read_bytes(self.env.loadlist),
            "mods_txt": read_bytes(self.env.mods_txt),
            "ploader": read_bytes(self.env.ploader),
            "marker": None,  # absent pre-install
            "mods_listing": sorted(os.listdir(self.env.mods_dir)),
        }

    def tearDown(self):
        self.env.cleanup()

    def install_ok(self):
        with open(self.env.metadata_path) as f:
            md = json.load(f)
        return self.env.installer().install(self.env.archive, md, TITLE)


# --------------------------------------------------------------------------
# THE four outputs
# --------------------------------------------------------------------------

class TestFourConfigOutputs(ConfigOutputsTestBase):
    def test_install_produces_all_four_outputs(self):
        manifest = self.install_ok()
        ce = manifest["config_entries"]

        # 1. loadlist.txt row
        ll = read_bytes(self.env.loadlist).decode("utf-8")
        self.assertIn("mod\t%s\t1.2.0" % MOD_DIR, ll)
        self.assertIn("file\tlua\tue4ss/Mods/%s/Scripts/main.lua\t%d"
                      % (MOD_DIR, len(MAIN_LUA)), ll)
        self.assertIn("mod\tOtherMod\t0.9", ll)  # shared rows survive

        # 2. ue4ss/Mods/mods.txt entry (UE4SS mods list)
        mt = read_bytes(self.env.mods_txt).decode("utf-8")
        self.assertTrue(mods_txt_has_mod(mt, MOD_DIR))
        self.assertIn("%s : 1" % MOD_DIR, mt)
        # every row round-trips the runtime's strict '<name>:<0|1>' parser
        for line in mt.splitlines():
            if line.strip() and ";" not in line:
                self.assertIsNotNone(
                    parse_mods_txt_row(line),
                    "mods.txt row must be runtime-parseable: %r" % line)
        # comment + foreign row survive byte-for-byte
        self.assertIn("; UE4SS mods list", mt)
        self.assertIn("OtherMod : 1", mt)

        # 3. ue4ss/Mods/<ModName>/enabled.txt (empty enable marker)
        self.assertTrue(os.path.exists(self.env.marker),
                        "enabled.txt marker missing")
        self.assertEqual(read_bytes(self.env.marker), b"")

        # 4. ploader.ini line
        pl = read_bytes(self.env.ploader).decode("utf-8")
        self.assertTrue(ini_has_line(pl, TITLE, PRX, "2"))
        self.assertIn("[OTHERGAME]", pl)

        # manifest records the two new entries + exactly-owned ploader lines
        self.assertEqual(ce["mods_txt_row"], "%s : 1" % MOD_DIR)
        self.assertEqual(ce["enabled_flag_source"], "installer-created")
        self.assertEqual(ce["enabled_marker_rel"],
                         "ue4ss/Mods/%s/enabled.txt" % MOD_DIR)
        self.assertTrue(ce["ploader_section_created"])
        # blank separator + section header + key line (in construction
        # order, mirroring upsert_ini_line)
        self.assertEqual(ce["ploader_added_lines"],
                         ["", "[%s]" % TITLE, "%s=2" % PRX])
        # verify command sees the full install (incl. both new outputs)
        report = self.env.installer().verify_installed(TITLE, MOD_NAME)
        self.assertTrue(report["ok"], report["problems"])
        # transaction fully closed out
        jdir = os.path.join(self.env.state, "journal")
        self.assertEqual(os.listdir(jdir), [])

    def test_uninstall_restores_all_four_byte_for_byte(self):
        manifest = self.install_ok()
        before = self.pre
        self.assertIsNone(before["marker"])

        un_manifest = installer_manifest_to_uninstall(
            manifest, self.env.game_root)
        # sanity: adapter carries all four outputs
        self.assertIn("ue4ss/Mods/mods.txt",
                      un_manifest["config_entries"])
        self.assertIn("ploader.ini", un_manifest["config_entries"])
        self.assertEqual(un_manifest["enabled_markers"],
                         ["ue4ss/Mods/%s/enabled.txt" % MOD_DIR])

        # installer-state bookkeeping lives outside the game tree
        record_path = os.path.join(self.env.state, "uninstall-manifest.json")
        with open(record_path, "w") as f:
            json.dump(un_manifest, f)
        state_path = os.path.join(self.env.state, "install_state.json")
        with open(state_path, "w") as f:
            json.dump({"game_version": "1.004.000",
                       "mods": {MOD_DIR: {
                           "manifest": record_path,
                           "requires": [],
                           "files": [dict(x) for x in
                                     un_manifest["files"]]}}}, f)
        un = Uninstaller(self.env.game_root, state_path=state_path,
                         transaction_root=os.path.join(self.env.state,
                                                       "uninstall-txns"))
        res = un.uninstall(un_manifest)
        self.assertEqual(res.markers_removed,
                         ["ue4ss/Mods/%s/enabled.txt" % MOD_DIR])
        self.assertEqual(res.config_lines_removed,
                         {"loadlist.txt": 3,   # mod row + 2 file rows
                          "ue4ss/Mods/mods.txt": 1,
                          "ploader.ini": 3})   # blank + section + key line

        # ALL FOUR back to the pre-install state, byte-for-byte:
        # 1. loadlist.txt
        self.assertEqual(read_bytes(self.env.loadlist), before["loadlist"])
        # 2. ue4ss/Mods/mods.txt
        self.assertEqual(read_bytes(self.env.mods_txt), before["mods_txt"])
        # 3. enabled.txt marker: absent again (whole mod dir gone)
        self.assertFalse(os.path.exists(self.env.marker))
        self.assertFalse(os.path.exists(self.env.mod_dir))
        # 4. ploader.ini
        self.assertEqual(read_bytes(self.env.ploader), before["ploader"])
        # Mods/ holds only what it held before (the shared mods.txt)
        self.assertEqual(sorted(os.listdir(self.env.mods_dir)),
                         before["mods_listing"])
        # no residue from the refused-then-removed install
        self.assertNotIn(MOD_DIR,
                         json.loads(read_bytes(state_path).decode("utf-8"))
                         ["mods"])

    def test_uninstall_rollback_restores_all_four(self):
        manifest = self.install_ok()
        installed = {  # the four outputs in their INSTALLED state
            "loadlist": read_bytes(self.env.loadlist),
            "mods_txt": read_bytes(self.env.mods_txt),
            "ploader": read_bytes(self.env.ploader),
        }
        un_manifest = installer_manifest_to_uninstall(
            manifest, self.env.game_root)
        record_path = os.path.join(self.env.state, "uninstall-manifest.json")
        with open(record_path, "w") as f:
            json.dump(un_manifest, f)
        state_path = os.path.join(self.env.state, "install_state.json")
        with open(state_path, "w") as f:
            json.dump({"game_version": "1.004.000",
                       "mods": {MOD_DIR: {
                           "manifest": record_path,
                           "requires": [],
                           "files": [dict(x) for x in
                                     un_manifest["files"]]}}}, f)
        un = Uninstaller(self.env.game_root, state_path=state_path,
                         transaction_root=os.path.join(self.env.state,
                                                       "uninstall-txns"))
        res = un.uninstall(un_manifest)

        summary = un.rollback(res.rollback_token)
        self.assertEqual(summary["markers_restored"],
                         ["ue4ss/Mods/%s/enabled.txt" % MOD_DIR])
        # all four outputs are back to their INSTALLED state
        self.assertEqual(read_bytes(self.env.loadlist), installed["loadlist"])
        self.assertEqual(read_bytes(self.env.mods_txt), installed["mods_txt"])
        self.assertEqual(read_bytes(self.env.ploader), installed["ploader"])
        self.assertTrue(os.path.exists(self.env.marker))
        # ...and the game passes the installer's own verify again
        report = self.env.installer().verify_installed(TITLE, MOD_NAME)
        self.assertTrue(report["ok"], report["problems"])


# --------------------------------------------------------------------------
# journal wiring: a crash mid-install rolls back all four
# --------------------------------------------------------------------------

class TestCrashRestoresAllFour(ConfigOutputsTestBase):
    def cli(self, args):
        return subprocess.run(
            [sys.executable, INSTALLER_PY] + args,
            capture_output=True, text=True, timeout=120)

    def test_crash_pre_verify_rolls_back_all_four(self):
        before = {
            "loadlist": read_bytes(self.env.loadlist),
            "mods_txt": read_bytes(self.env.mods_txt),
            "ploader": read_bytes(self.env.ploader),
        }
        rc = self.cli([
            "install", "--archive", self.env.archive,
            "--metadata", self.env.metadata_path, "--title", TITLE,
            "--roots", self.env.roots, "--plugins", self.env.plugins,
            "--state", self.env.state, "--crash-at", "pre-verify"])
        self.assertEqual(rc.returncode, 86)
        # half-installed state IS on disk (files + marker + all configs)
        self.assertTrue(os.path.exists(self.env.marker))
        self.assertNotEqual(read_bytes(self.env.loadlist), before["loadlist"])
        # ...and a fresh process rolls every journal action back
        rec = self.cli(["recover", "--roots", self.env.roots,
                        "--plugins", self.env.plugins,
                        "--state", self.env.state])
        self.assertEqual(rec.returncode, 0)
        report = json.loads(rec.stdout)["recovered"][0]
        self.assertEqual(report["action"], "rolled-back")
        self.assertEqual(report["rollback_incomplete"], [])
        # ALL FOUR restored byte-for-byte / absent
        self.assertEqual(read_bytes(self.env.loadlist), before["loadlist"])
        self.assertEqual(read_bytes(self.env.mods_txt), before["mods_txt"])
        self.assertEqual(read_bytes(self.env.ploader), before["ploader"])
        self.assertFalse(os.path.exists(self.env.marker))
        self.assertFalse(os.path.exists(self.env.mod_dir))


# --------------------------------------------------------------------------
# archive-shipped / case-variant enable marker
# --------------------------------------------------------------------------

class TestArchiveShippedMarker(ConfigOutputsTestBase):
    def test_archive_enabled_txt_is_the_marker(self):
        make_zip(self.env.archive, [
            ("MyMod/Scripts/main.lua", MAIN_LUA),
            ("MyMod/enabled.txt", b"user-note inside\n")])
        self.env.write_meta(file_list=["MyMod/Scripts/main.lua",
                                       "MyMod/enabled.txt"])
        manifest = self.install_ok()
        # exactly ONE enabled.txt: the archive's own bytes (never rewritten)
        self.assertEqual(read_bytes(self.env.marker), b"user-note inside\n")
        self.assertEqual(manifest["config_entries"]["enabled_flag_source"],
                         "archive-shipped")
        # it is tracked as an installed archive file, not duplicated
        self.assertEqual(
            [f["archive_path"] for f in manifest["files"]],
            ["MyMod/Scripts/main.lua", "MyMod/enabled.txt"])
        # mods.txt row still written; verify passes
        mt = read_bytes(self.env.mods_txt).decode("utf-8")
        self.assertTrue(mods_txt_has_mod(mt, MOD_DIR))
        report = self.env.installer().verify_installed(TITLE, MOD_NAME)
        self.assertTrue(report["ok"], report["problems"])

    def test_case_variant_enabled_txt_rejected(self):
        # 'Enabled.txt' collides with the installer's exact-case
        # 'enabled.txt' marker on the case-insensitive host but would NOT
        # enable the mod on the case-sensitive PS5 target
        make_zip(self.env.archive, [
            ("MyMod/Scripts/main.lua", MAIN_LUA),
            ("MyMod/Enabled.txt", b"")])
        self.env.write_meta(file_list=["MyMod/Scripts/main.lua",
                                       "MyMod/Enabled.txt"])
        with open(self.env.metadata_path) as f:
            md = json.load(f)
        with self.assertRaises(CaseConflictError) as ctx:
            self.env.installer().install(self.env.archive, md, TITLE)
        self.assertIn("Enabled.txt", str(ctx.exception))
        self.assertIn("enabled.txt", str(ctx.exception))
        # nothing was installed; all shared configs untouched
        self.assertFalse(os.path.exists(self.env.mod_dir))
        self.assertFalse(os.path.exists(self.env.marker))
        self.assertEqual(read_bytes(self.env.loadlist),
                         LOADLIST_BEFORE.encode("utf-8"))
        self.assertEqual(read_bytes(self.env.mods_txt),
                         MODS_TXT_BEFORE.encode("utf-8"))
        self.assertEqual(read_bytes(self.env.ploader),
                         PLOADER_BEFORE.encode("utf-8"))


# --------------------------------------------------------------------------
# mods.txt line editor (pure functions)
# --------------------------------------------------------------------------

class TestModsTxtEditors(unittest.TestCase):
    def test_parse_row(self):
        self.assertEqual(parse_mods_txt_row("Spawner : 1"), ("Spawner", "1"))
        self.assertEqual(parse_mods_txt_row("Spawner:0"), ("Spawner", "0"))
        self.assertEqual(parse_mods_txt_row("\tSpawner \t:\t 1 \t"),
                         ("Spawner", "1"))
        self.assertIsNone(parse_mods_txt_row("; comment"))
        self.assertIsNone(parse_mods_txt_row(""))
        self.assertIsNone(parse_mods_txt_row("NoFlag : 2"))
        self.assertIsNone(parse_mods_txt_row("NoColon 1"))
        self.assertIsNone(parse_mods_txt_row(": 1"))  # empty name

    def test_upsert_creates_appends_and_preserves(self):
        out = upsert_mods_txt("", "ModA")
        self.assertEqual(out, "ModA : 1\n")
        base = "; comment\r\nOtherMod : 1\r\n"
        out = upsert_mods_txt(base, "ModA")
        self.assertEqual(out, "; comment\r\nOtherMod : 1\r\nModA : 1\r\n")
        # foreign rows + comments untouched, CRLF preserved

    def test_upsert_replaces_in_place_and_dedups(self):
        base = "; c\nModA : 0\nModB : 1\nModA : 0\n"
        out = upsert_mods_txt(base, "ModA", enabled=True)
        self.assertEqual(out, "; c\nModA : 1\nModB : 1\n")
        # position of the first row kept (load order preserved)

    def test_has_mod_requires_enabled_flag(self):
        text = "ModA : 0\nModB : 1\n"
        self.assertFalse(mods_txt_has_mod(text, "ModA"))
        self.assertTrue(mods_txt_has_mod(text, "ModB"))
        self.assertFalse(mods_txt_has_mod(text, "ModC"))

    def test_remove_lines_drops_recorded_blank(self):
        # the uninstall side must remove the blank separator the installer
        # recorded before a created INI section (multiset-exact)
        text = "; keep\n[OTHER]\nfoo=2\n\n[PPSA]\nprx=2\n"
        new, removed = uninstall._remove_lines(
            text, ["", "[PPSA]", "prx=2"])
        self.assertEqual(removed, 3)
        self.assertEqual(new, "; keep\n[OTHER]\nfoo=2\n")
        # unrecorded blanks survive
        new2, removed2 = uninstall._remove_lines("a\n\n\nb\n", ["x=1"])
        self.assertEqual((new2, removed2), ("a\n\n\nb\n", 0))


if __name__ == "__main__":
    unittest.main(verbosity=2)
