#!/usr/bin/env python3
"""Comprehensive tests for the modbridge installer core.

All host-side (Mac), stdlib-only, unittest.  Covers: clean install, tampered
file detection (install-time and post-install), rollback on failure, crash
recovery (subprocess hard-exit 86 + fresh-process recovery), config update
safety, and archive path-traversal/symlink/bomb protections.

Run:  python3 -m unittest -v test_installer
"""

from __future__ import annotations

import hashlib
import json
import os
import shutil
import stat as statmod
import subprocess
import sys
import tempfile
import unittest
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import installer  # noqa: E402
from installer import (  # noqa: E402
    ArchiveCorruptError, ConfigFormatError, Installer, InstallError,
    MetadataError, OwnershipError, SafeArchive, UnsupportedArchiveFormat,
    UnsafeArchiveError, VerificationError, atomic_write_bytes,
    detect_archive_format, loadlist_has_mod, normalize_arcname,
    upsert_ini_line, upsert_loadlist, ini_has_line)

TITLE = "PPSA33286"
MOD_NAME = "Townfall Item Spawner"
MOD_DIR = "Townfall_Item_Spawner"

INSTALLER_PY = os.path.join(HERE, "installer.py")

MAIN_LUA = b"-- main.lua\nRegisterMod('ItemSpawner', 'Mod')\n"
UTIL_LUA = b"-- util\nfunction helper() end\n"
SETTINGS_INI = b"[Settings]\nfoo=1\n"
README = b"readme top-level (outside mod root)\n"

ENTRIES = [
    ("MyMod/Scripts/main.lua", MAIN_LUA),
    ("MyMod/Scripts/helpers/util.lua", UTIL_LUA),
    ("MyMod/config/settings.ini", SETTINGS_INI),
    ("Readme.txt", README),  # stray, outside the MyMod/ mod root
]


def make_zip(path, entries):
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as zf:
        for item in entries:
            if len(item) == 3:
                name, data, attr = item
            else:
                name, data = item
                attr = None
            zi = zipfile.ZipInfo(name)
            if attr is not None:
                zi.external_attr = attr
            zf.writestr(zi, data)
    return path


def sha(data):
    return hashlib.sha256(data).hexdigest()


def snapshot(root):
    """path -> sha256 (files) / None (dirs), ignoring nothing."""
    out = {}
    for dirpath, dirnames, filenames in os.walk(root):
        for d in dirnames:
            p = os.path.join(dirpath, d)
            out[os.path.relpath(p, root)] = None
        for f in filenames:
            p = os.path.join(dirpath, f)
            with open(p, "rb") as fh:
                out[os.path.relpath(p, root)] = sha(fh.read())
    return out


def seed_file(path, data):
    """Test helper: create parent dirs + write bytes atomically."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    atomic_write_bytes(path, data)


def mark_encrypted(path):
    """Craft a real encrypted-flag zip by patching local + central headers
    (zipfile's writestr resets flag_bits, so we patch after the fact)."""
    with open(path, "rb") as f:
        data = f.read()
    i = data.find(b"PK\x01\x02")  # central directory
    assert i >= 0, "no central directory found"
    flag = int.from_bytes(data[i + 8:i + 10], "little") | 0x1
    data = data[:i + 8] + flag.to_bytes(2, "little") + data[i + 10:]
    if data[:4] == b"PK\x03\x04":  # local header
        flag = int.from_bytes(data[6:8], "little") | 0x1
        data = data[:6] + flag.to_bytes(2, "little") + data[8:]
    with open(path, "wb") as f:
        f.write(data)


class Env(object):
    """Per-test sandbox: tmp roots/plugins/state + helpers."""

    def __init__(self):
        self.base = tempfile.mkdtemp(prefix="modbridge-test-")
        self.roots = os.path.join(self.base, "roots")
        self.plugins = os.path.join(self.base, "plugins")
        self.state = os.path.join(self.base, "state")
        for d in (self.roots, self.plugins, self.state):
            os.makedirs(d, exist_ok=True)
        self.archive = os.path.join(self.base, "mod.zip")
        self.metadata_path = os.path.join(self.base, "meta.json")

    def title_root(self, title=TITLE):
        return os.path.join(self.roots, title)

    def mod_dir(self, title=TITLE, mod=MOD_DIR):
        return os.path.join(self.roots, title, "ue4ss", "Mods", mod)

    def manifest_path(self, title=TITLE, mod=MOD_DIR):
        return os.path.join(self.roots, title, "manifests", mod + ".json")

    @property
    def loadlist(self):
        return os.path.join(self.roots, TITLE, "loadlist.txt")

    @property
    def ploader(self):
        return os.path.join(self.plugins, "ploader.ini")

    def journal_files(self):
        jdir = os.path.join(self.state, "journal")
        return sorted(f for f in os.listdir(jdir) if f.endswith(".json")) \
            if os.path.isdir(jdir) else []

    def backup_tx_dirs(self):
        bdir = os.path.join(self.state, "backups")
        return sorted(os.listdir(bdir)) if os.path.isdir(bdir) else []

    def write_meta(self, **over):
        md = {
            "name": MOD_NAME,
            "version": "1.2.0",
            "source_url": "https://www.nexusmods.com/townfall/mods/42",
            "downloaded_at": "2026-10-07T01:02:03Z",
            "file_list": [e[0] for e in ENTRIES],
        }
        md.update(over)
        with open(self.metadata_path, "w") as f:
            json.dump(md, f)
        return self.metadata_path

    def installer(self, **kw):
        return Installer(self.roots, self.plugins, self.state, **kw)

    def cleanup(self):
        shutil.rmtree(self.base, ignore_errors=True)


class InstallerTestBase(unittest.TestCase):
    def setUp(self):
        self.env = Env()
        make_zip(self.env.archive, ENTRIES)
        self.env.write_meta()

    def tearDown(self):
        self.env.cleanup()

    def install_ok(self, **kw):
        with open(self.env.metadata_path) as f:
            md = json.load(f)
        md.update(kw.pop("metadata_over", {}))
        inst = self.env.installer(**kw.pop("installer_kw", {}))
        return inst.install(self.env.archive, md, TITLE,
                            **kw)


# --------------------------------------------------------------------------
# pure helpers
# --------------------------------------------------------------------------

class TestNormalizeArcname(unittest.TestCase):
    def test_plain(self):
        self.assertEqual(normalize_arcname("a/b/c.txt"), "a/b/c.txt")

    def test_dot_and_empty_components_collapsed(self):
        self.assertEqual(normalize_arcname("./a//b/./c"), "a/b/c")

    def test_backslash_normalized(self):
        self.assertEqual(normalize_arcname("a\\b\\c.txt"), "a/b/c.txt")

    def test_dotdot_rejected(self):
        for name in ("../x", "a/../../x", "..", "a/../.."):
            with self.assertRaises(ValueError):
                normalize_arcname(name)

    def test_absolute_rejected(self):
        with self.assertRaises(ValueError):
            normalize_arcname("/etc/passwd")

    def test_drive_letter_rejected(self):
        with self.assertRaises(ValueError):
            normalize_arcname("C:\\evil.txt")
        with self.assertRaises(ValueError):
            normalize_arcname("C:/evil.txt")

    def test_nul_and_control_rejected(self):
        with self.assertRaises(ValueError):
            normalize_arcname("a\x00b")
        with self.assertRaises(ValueError):
            normalize_arcname("a\nb")

    def test_long_component_rejected(self):
        with self.assertRaises(ValueError):
            normalize_arcname("a" * 300 + ".txt")


class TestConfigEditors(unittest.TestCase):
    def test_loadlist_create(self):
        out = upsert_loadlist("", TITLE, "ModA", "1.0", ["file\tlua\ta\t1"])
        lines = out.splitlines()
        self.assertEqual(lines[0], "modforge-loadlist\t1")
        self.assertIn("title\t%s" % TITLE, lines)
        self.assertIn("mod\tModA\t1.0", lines)

    def test_loadlist_replace_block(self):
        base = ("modforge-loadlist\t1\ntitle\tT\nengine\tunreal\n"
                "mod\tModA\t1.0\nfile\tlua\ta.lua\t5\nmod\tModB\t2.0\n"
                "file\tlua\tb.lua\t6\n")
        out = upsert_loadlist(base, "T", "ModA", "9.9", ["file\tlua\tc\t7"])
        self.assertEqual(out.count("mod\tModA"), 1)
        self.assertIn("mod\tModA\t9.9", out)
        self.assertNotIn("a.lua", out)
        self.assertIn("mod\tModB\t2.0", out)
        self.assertIn("b.lua", out)

    def test_loadlist_wrong_title_refused(self):
        base = "modforge-loadlist\t1\ntitle\tOTHER\n"
        with self.assertRaises(ConfigFormatError):
            upsert_loadlist(base, TITLE, "ModA", "1.0", [])

    def test_ini_upsert_and_check(self):
        text = "; comment\n[OTHER]\nfoo.prx=2\n"
        out = upsert_ini_line(text, TITLE, "bar.prx", "2")
        self.assertIn("; comment", out)
        self.assertIn("[OTHER]", out)
        self.assertIn("foo.prx=2", out)
        self.assertTrue(ini_has_line(out, TITLE, "bar.prx", "2"))
        self.assertTrue(ini_has_line(out, "OTHER", "foo.prx", "2"))
        self.assertFalse(ini_has_line(out, TITLE, "foo.prx", "2"))
        # update in place
        out2 = upsert_ini_line(out, TITLE, "bar.prx", "0")
        self.assertEqual(out2.count("bar.prx="), 1)
        self.assertIn("bar.prx=0", out2)

    def test_crlf_endings_preserved(self):
        text = "; comment\r\n[OTHER]\r\nfoo.prx=2\r\n"
        out = upsert_ini_line(text, TITLE, "bar.prx", "2")
        self.assertIn("; comment\r\n", out)
        self.assertIn("foo.prx=2\r\n", out)
        self.assertTrue(out.endswith("bar.prx=2\r\n"))
        self.assertTrue(ini_has_line(out, TITLE, "bar.prx", "2"))
        ll = "modforge-loadlist\t1\r\ntitle\tT\r\nmod\tModA\t1.0\r\n"
        out_ll = upsert_loadlist(ll, "T", "ModB", "2.0", [])
        self.assertIn("mod\tModA\t1.0\r\n", out_ll)
        self.assertIn("mod\tModB\t2.0\r\n", out_ll)
        self.assertTrue(loadlist_has_mod(out_ll, "ModB"))


# --------------------------------------------------------------------------
# archive format detection / unsupported formats
# --------------------------------------------------------------------------

class TestArchiveFormats(InstallerTestBase):
    def test_detect_magic(self):
        self.assertEqual(detect_archive_format(self.env.archive), "zip")
        seven = os.path.join(self.env.base, "x.7z")
        with open(seven, "wb") as f:
            f.write(b"7z\xbc\xaf\x27\x1c" + b"\x00" * 32)
        self.assertEqual(detect_archive_format(seven), "7z")
        rar5 = os.path.join(self.env.base, "x.rar")
        with open(rar5, "wb") as f:
            f.write(b"Rar!\x1a\x07\x01\x00" + b"\x00" * 32)
        self.assertEqual(detect_archive_format(rar5), "rar")
        junk = os.path.join(self.env.base, "x.bin")
        with open(junk, "wb") as f:
            f.write(b"not an archive")
        self.assertEqual(detect_archive_format(junk), None)

    def test_7z_rejected_with_guidance(self):
        seven = os.path.join(self.env.base, "mod.7z")
        with open(seven, "wb") as f:
            f.write(b"7z\xbc\xaf\x27\x1c" + b"\x00" * 64)
        with open(self.env.metadata_path) as f:
            md = json.load(f)
        with self.assertRaises(UnsupportedArchiveFormat) as cm:
            self.env.installer().install(seven, md, TITLE)
        msg = str(cm.exception)
        self.assertIn("7z", msg)
        self.assertIn("zip", msg)  # what would be needed
        self.assertEqual(cm.exception.exit_code if hasattr(
            cm.exception, "exit_code") else 2, 2)

    def test_rar_rejected_with_guidance(self):
        rar = os.path.join(self.env.base, "mod.rar")
        with open(rar, "wb") as f:
            f.write(b"Rar!\x1a\x07\x00" + b"\x00" * 64)
        with open(self.env.metadata_path) as f:
            md = json.load(f)
        with self.assertRaises(UnsupportedArchiveFormat) as cm:
            self.env.installer().install(rar, md, TITLE)
        self.assertIn("RAR", str(cm.exception))

    def test_corrupt_zip_rejected(self):
        broken = os.path.join(self.env.base, "broken.zip")
        with open(self.env.archive, "rb") as f:
            data = f.read()
        with open(broken, "wb") as f:
            f.write(data[: len(data) // 2])  # truncate: no central dir
        with open(self.env.metadata_path) as f:
            md = json.load(f)
        with self.assertRaises((ArchiveCorruptError, UnsafeArchiveError)):
            self.env.installer().install(broken, md, TITLE)


# --------------------------------------------------------------------------
# path traversal / symlink / bomb protections
# --------------------------------------------------------------------------

class TestArchiveSafety(InstallerTestBase):
    def attempt(self, entries):
        make_zip(self.env.archive, entries)
        with open(self.env.metadata_path) as f:
            md = json.load(f)
        before = snapshot(self.env.roots)
        with self.assertRaises(UnsafeArchiveError):
            self.env.installer().install(self.env.archive, md, TITLE)
        self.assertEqual(snapshot(self.env.roots), before,
                         "target tree must be untouched after rejection")

    def test_dotdot_escape(self):
        self.attempt([
            ("MyMod/Scripts/main.lua", MAIN_LUA),
            ("MyMod/Scripts/../../evil.txt", b"evil"),
        ])

    def test_leading_dotdot(self):
        self.attempt([
            ("MyMod/Scripts/main.lua", MAIN_LUA),
            ("../evil.txt", b"evil"),
        ])

    def test_absolute_path(self):
        self.attempt([
            ("MyMod/Scripts/main.lua", MAIN_LUA),
            ("/tmp/modbridge-absolute-escape", b"evil"),
        ])

    def test_windows_drive_letter(self):
        self.attempt([
            ("MyMod/Scripts/main.lua", MAIN_LUA),
            ("C:\\Windows\\evil.txt", b"evil"),
        ])

    def test_symlink_entry_rejected(self):
        link_attr = (statmod.S_IFLNK | 0o777) << 16
        self.attempt([
            ("MyMod/Scripts/main.lua", MAIN_LUA),
            ("MyMod/Scripts/link", b"/etc/passwd", link_attr),
        ])

    def test_fifo_entry_rejected(self):
        fifo_attr = (statmod.S_IFIFO | 0o600) << 16
        self.attempt([
            ("MyMod/Scripts/main.lua", MAIN_LUA),
            ("MyMod/pipe", b"x", fifo_attr),
        ])

    def test_long_member_name_rejected(self):
        self.attempt([
            ("MyMod/Scripts/main.lua", MAIN_LUA),
            ("MyMod/" + "a" * 400 + ".txt", b"x"),
        ])

    def test_encrypted_entry_rejected(self):
        path = self.env.archive
        with zipfile.ZipFile(path, "w") as zf:
            zf.writestr("MyMod/Scripts/main.lua", MAIN_LUA)
        mark_encrypted(path)
        with open(self.env.metadata_path) as f:
            md = json.load(f)
        before = snapshot(self.env.roots)
        with self.assertRaises(UnsafeArchiveError) as cm:
            self.env.installer().install(path, md, TITLE)
        self.assertIn("encrypted", str(cm.exception))
        self.assertEqual(snapshot(self.env.roots), before)

    def test_missing_main_lua_rejected(self):
        make_zip(self.env.archive, [
            ("MyMod/scripts/main.lua", MAIN_LUA)])  # wrong case
        with open(self.env.metadata_path) as f:
            md = json.load(f)
        before = snapshot(self.env.roots)
        with self.assertRaises(UnsafeArchiveError) as cm:
            self.env.installer().install(self.env.archive, md, TITLE)
        self.assertIn("Scripts/main.lua", str(cm.exception))
        self.assertIn("case", str(cm.exception))
        make_zip(self.env.archive, [("Whatever/readme.txt", b"x")])
        with self.assertRaises(UnsafeArchiveError):
            self.env.installer().install(self.env.archive, md, TITLE)
        self.assertEqual(snapshot(self.env.roots), before)


# --------------------------------------------------------------------------
# clean install: layout, manifest, configs
# --------------------------------------------------------------------------

class TestCleanInstall(InstallerTestBase):
    def test_layout_and_manifest(self):
        manifest = self.install_ok()
        md = os.path.join(self.env.mod_dir())
        # mod files installed under ue4ss/Mods/<ModName>/, mod root stripped
        with open(os.path.join(md, "Scripts", "main.lua"), "rb") as f:
            self.assertEqual(f.read(), MAIN_LUA)
        with open(os.path.join(md, "Scripts", "helpers", "util.lua"),
                  "rb") as f:
            self.assertEqual(f.read(), UTIL_LUA)
        with open(os.path.join(md, "config", "settings.ini"), "rb") as f:
            self.assertEqual(f.read(), SETTINGS_INI)
        # stray outside the mod root NOT installed
        self.assertFalse(
            os.path.exists(os.path.join(md, "Readme.txt")))
        # manifest
        self.assertTrue(os.path.exists(self.env.manifest_path()))
        m = json.load(open(self.env.manifest_path()))
        self.assertEqual(m["schema"], "modbridge-manifest/1")
        self.assertEqual(m["source_url"],
                         "https://www.nexusmods.com/townfall/mods/42")
        self.assertEqual(m["version"], "1.2.0")
        self.assertEqual(m["downloaded_at"], "2026-10-07T01:02:03Z")
        self.assertTrue(m["installed_at"])
        self.assertEqual(m["mod_root_in_archive"], "MyMod/")
        self.assertEqual(m["archive"]["format"], "zip")
        self.assertEqual(m["archive"]["sha256"], sha(open(
            self.env.archive, "rb").read()))
        self.assertEqual(len(m["files"]), 3)
        by_arc = {f["archive_path"]: f for f in m["files"]}
        self.assertEqual(by_arc["MyMod/Scripts/main.lua"]["original_sha256"],
                         sha(MAIN_LUA))
        self.assertEqual(by_arc["MyMod/Scripts/main.lua"]["installed_sha256"],
                         sha(MAIN_LUA))
        self.assertEqual(
            by_arc["MyMod/config/settings.ini"]["role"], "config")
        self.assertEqual(by_arc["MyMod/Scripts/main.lua"]["role"], "lua")
        self.assertEqual(m["skipped_outside_mod_root"], ["Readme.txt"])
        self.assertTrue(m["transaction_id"])
        # transaction fully closed out
        self.assertEqual(self.env.journal_files(), [])
        self.assertEqual(self.env.backup_tx_dirs(), [])

    def test_configs_created_when_missing(self):
        self.install_ok()
        with open(self.env.loadlist) as f:
            ll = f.read()
        lines = ll.splitlines()
        self.assertEqual(lines[0], "modforge-loadlist\t1")
        self.assertIn("title\t%s" % TITLE, lines)
        self.assertIn("engine\tunreal", lines)
        self.assertIn("mod\t%s\t1.2.0" % MOD_DIR, lines)
        self.assertIn("file\tlua\tue4ss/Mods/%s/Scripts/main.lua\t%d"
                      % (MOD_DIR, len(MAIN_LUA)), lines)
        self.assertIn("file\tconfig\tue4ss/Mods/%s/config/settings.ini\t%d"
                      % (MOD_DIR, len(SETTINGS_INI)), lines)
        self.assertTrue(loadlist_has_mod(ll, MOD_DIR))
        with open(self.env.ploader) as f:
            pl = f.read()
        self.assertTrue(ini_has_line(
            pl, TITLE, "modbridge_supplier_composition.prx", "2"))
        # verify command passes
        report = self.env.installer().verify_installed(TITLE, MOD_NAME)
        self.assertTrue(report["ok"], report)
        self.assertEqual(report["files_checked"], 3)

    def test_existing_configs_preserved(self):
        seed_file(self.env.loadlist,
                  ("modforge-loadlist\t1\n"
                   "title\t%s\nengine\tunreal\n"
                   "mod\tOtherMod\t0.9\n"
                   "file\tlua\tue4ss/Mods/OtherMod/Scripts/main.lua"
                   "\t7\n" % TITLE).encode())
        seed_file(self.env.ploader,
                  b"; managed by hand\n[OTHERGAME]\nfoo.prx=2\n")
        self.install_ok()
        with open(self.env.loadlist) as f:
            ll = f.read()
        self.assertIn("mod\tOtherMod\t0.9", ll)
        self.assertIn("file\tlua\tue4ss/Mods/OtherMod/Scripts/main.lua\t7",
                      ll)
        self.assertIn("mod\t%s\t1.2.0" % MOD_DIR, ll)
        with open(self.env.ploader) as f:
            pl = f.read()
        self.assertIn("; managed by hand", pl)
        self.assertIn("[OTHERGAME]", pl)
        self.assertIn("foo.prx=2", pl)
        self.assertTrue(ini_has_line(
            pl, TITLE, "modbridge_supplier_composition.prx", "2"))

    def test_two_mods_coexist_and_reinstall_replaces_block(self):
        self.install_ok()
        # second mod, same title
        zip2 = os.path.join(self.env.base, "mod2.zip")
        make_zip(zip2, [("Other/Scripts/main.lua", b"-- other\n")])
        meta2 = self.env.write_meta(name="Other Mod", version="0.1",
                                    file_list=["Other/Scripts/main.lua"])
        with open(meta2) as f:
            md2 = json.load(f)
        self.env.installer().install(zip2, md2, TITLE)
        with open(self.env.loadlist) as f:
            ll = f.read()
        self.assertEqual(ll.count("mod\t%s\t" % MOD_DIR), 1)
        self.assertEqual(ll.count("mod\tOther_Mod\t"), 1)
        # reinstall (upgrade) of the first mod replaces its block
        make_zip(self.env.archive, [
            ("MyMod/Scripts/main.lua", b"-- v2 main\n"),
            ("MyMod/Scripts/helpers/util.lua", UTIL_LUA),
            ("MyMod/config/settings.ini", SETTINGS_INI),
            ("Readme.txt", README)])
        self.env.write_meta(version="2.0.0")
        self.install_ok()
        with open(self.env.loadlist) as f:
            ll2 = f.read()
        self.assertEqual(ll2.count("mod\t%s\t" % MOD_DIR), 1)
        self.assertIn("mod\t%s\t2.0.0" % MOD_DIR, ll2)
        self.assertIn("mod\tOther_Mod\t0.1", ll2)
        with open(os.path.join(self.env.mod_dir(), "Scripts", "main.lua"),
                  "rb") as f:
            self.assertEqual(f.read(), b"-- v2 main\n")
        m = json.load(open(self.env.manifest_path()))
        self.assertEqual(m["version"], "2.0.0")
        self.assertEqual(self.env.journal_files(), [])
        self.assertEqual(self.env.backup_tx_dirs(), [])

    def test_ue4ss_overrides_runtime_only(self):
        self.install_ok(metadata_over={
            "ue4ss_settings": {"ModLoader": {"bUseUobjectProxy": "true"}}})
        ov = os.path.join(self.env.roots, TITLE, "ue4ss", "overrides",
                          MOD_DIR + ".ini")
        self.assertTrue(os.path.exists(ov))
        text = open(ov).read()
        self.assertIn("[ModLoader]", text)
        self.assertIn("bUseUobjectProxy = true", text)
        # runtime-owned paths only: the shared settings file is never
        # created/modified by the installer, and nothing lands inside the
        # mod's own files
        self.assertFalse(os.path.exists(os.path.join(
            self.env.roots, TITLE, "ue4ss", "UE4SS-settings.ini")))
        mod_files = sorted(
            os.path.relpath(os.path.join(dp, f), self.env.mod_dir())
            for dp, _, fs in os.walk(self.env.mod_dir()) for f in fs)
        self.assertEqual(mod_files, [
            "Scripts/helpers/util.lua", "Scripts/main.lua",
            "config/settings.ini"])
        m = json.load(open(self.env.manifest_path()))
        self.assertEqual(m["config_entries"]["ue4ss_overrides_path"], ov)

    def test_manifest_records_config_entries(self):
        self.install_ok()
        m = json.load(open(self.env.manifest_path()))
        ce = m["config_entries"]
        self.assertEqual(ce["ploader_section"], TITLE)
        self.assertEqual(ce["ploader_prx_name"],
                         "modbridge_supplier_composition.prx")
        self.assertEqual(ce["ploader_prx_mode"], "2")
        self.assertEqual(ce["loadlist_rows"][0],
                         "mod\t%s\t1.2.0" % MOD_DIR)
        self.assertEqual(len(ce["loadlist_rows"]), 4)  # mod row + 3 files
        self.assertTrue(ce["loadlist_path"].endswith("loadlist.txt"))

    def test_custom_ploader_prx_from_metadata(self):
        self.install_ok(metadata_over={
            "ploader_prx": {"name": "my_custom.prx", "mode": "0"}})
        with open(self.env.ploader) as f:
            self.assertTrue(ini_has_line(f.read(), TITLE, "my_custom.prx",
                                         "0"))

    def test_file_list_mismatch_recorded_and_strict(self):
        self.env.write_meta(file_list=["MyMod/Scripts/main.lua",
                                       "DoesNotExist.txt"])
        manifest = self.install_ok()
        self.assertIn("DoesNotExist.txt",
                      manifest["file_list_mismatch"]["missing_from_archive"])
        self.assertIn("MyMod/config/settings.ini",
                      manifest["file_list_mismatch"]["extra_in_archive"])
        # strict mode refuses
        with open(self.env.metadata_path) as f:
            md = json.load(f)
        before = snapshot(self.env.roots)
        with self.assertRaises(MetadataError):
            self.env.installer().install(self.env.archive, md, TITLE,
                                         strict_file_list=True)
        self.assertEqual(snapshot(self.env.roots), before)

    def test_reinstall_with_fewer_files_records_stale(self):
        self.install_ok()
        # upgrade that drops util.lua from the archive
        make_zip(self.env.archive, [
            ("MyMod/Scripts/main.lua", b"-- v2 main\n"),
            ("MyMod/config/settings.ini", SETTINGS_INI),
            ("Readme.txt", README)])
        self.env.write_meta(version="2.0.0",
                            file_list=["MyMod/Scripts/main.lua",
                                       "MyMod/config/settings.ini",
                                       "Readme.txt"])
        manifest = self.install_ok()
        # stale file is left on disk but recorded, and not in the loadlist
        self.assertTrue(os.path.exists(os.path.join(
            self.env.mod_dir(), "Scripts", "helpers", "util.lua")))
        self.assertEqual(manifest["stale_files_left_in_place"],
                         ["Scripts/helpers/util.lua"])
        with open(self.env.loadlist) as f:
            ll = f.read()
        self.assertNotIn("util.lua", ll)
        # verify still passes (stale file is not tracked as installed)
        self.assertTrue(
            self.env.installer().verify_installed(TITLE, MOD_NAME)["ok"])

    def test_top_level_scripts_root(self):
        # archive whose root IS the mod (Scripts/ at top level)
        make_zip(self.env.archive, [("Scripts/main.lua", MAIN_LUA),
                                    ("config/settings.ini", SETTINGS_INI)])
        self.env.write_meta(file_list=["Scripts/main.lua",
                                       "config/settings.ini"])
        manifest = self.install_ok()
        self.assertEqual(manifest["mod_root_in_archive"], "")
        with open(os.path.join(self.env.mod_dir(), "Scripts", "main.lua"),
                  "rb") as f:
            self.assertEqual(f.read(), MAIN_LUA)


# --------------------------------------------------------------------------
# bad input
# --------------------------------------------------------------------------

class TestBadInput(InstallerTestBase):
    def test_bad_title_id(self):
        for bad in ("../evil", "a/b", "", "a b", "." * 40):
            with open(self.env.metadata_path) as f:
                md = json.load(f)
            with self.assertRaises(MetadataError):
                self.env.installer().install(self.env.archive, md, bad)

    def test_bad_metadata(self):
        for over in ({"name": ""}, {"name": None}, {"version": ""},
                     {"version": None}, {"source_url": 5},
                     {"file_list": "nope"}, {"ploader_prx": "../evil"},
                     {"ploader_prx": {"name": "nope", "mode": "9"}},
                     {"ue4ss_settings": {"[bad]": {"k": "v"}}}):
            with open(self.env.metadata_path) as f:
                md = json.load(f)
            md.update(over)
            with self.assertRaises(MetadataError):
                self.env.installer().install(self.env.archive, md, TITLE)

    def test_ownership_guard(self):
        foreign = os.path.join(self.env.mod_dir())
        seed_file(os.path.join(foreign, "Scripts", "main.lua"), b"foreign")
        before = snapshot(self.env.roots)
        with self.assertRaises(OwnershipError):
            self.install_ok()
        self.assertEqual(snapshot(self.env.roots), before)
        # foreign bytes untouched
        with open(os.path.join(foreign, "Scripts", "main.lua"), "rb") as f:
            self.assertEqual(f.read(), b"foreign")

    def test_unrecognized_loadlist_refused(self):
        seed_file(self.env.loadlist, b"some foreign format\n")
        before = snapshot(self.env.roots)
        with self.assertRaises(ConfigFormatError):
            self.install_ok()
        self.assertEqual(snapshot(self.env.roots), before)
        self.assertEqual(self.env.journal_files(), [])


# --------------------------------------------------------------------------
# tampering + rollback (in-process fault injection)
# --------------------------------------------------------------------------

class TamperingInstaller(Installer):
    """Corrupts util.lua as it is written (simulates bad transport/disk)."""

    def _write_file_bytes(self, path, data):
        if path.endswith("util.lua") and data:
            data = data[:-1] + bytes([data[-1] ^ 0xFF])
        Installer._write_file_bytes(self, path, data)


class RaisingInstaller(Installer):
    """Simulates a disk failure exactly at the loadlist write."""

    def _write_file_bytes(self, path, data):
        if os.path.basename(path) == "loadlist.txt":
            raise OSError("simulated disk failure")
        Installer._write_file_bytes(self, path, data)


class TestTamperAndRollback(InstallerTestBase):
    def test_tampered_write_detected_and_rolled_back(self):
        before = snapshot(self.env.roots)
        pl_before = snapshot(self.env.plugins)
        with open(self.env.metadata_path) as f:
            md = json.load(f)
        inst = TamperingInstaller(self.env.roots, self.env.plugins,
                                  self.env.state)
        with self.assertRaises(VerificationError) as cm:
            inst.install(self.env.archive, md, TITLE)
        self.assertIn("util.lua", str(cm.exception))
        self.assertIn("installed sha256", str(cm.exception))
        # full rollback: target trees byte-for-byte identical to before
        self.assertEqual(snapshot(self.env.roots), before)
        self.assertEqual(snapshot(self.env.plugins), pl_before)
        self.assertEqual(self.env.journal_files(), [])
        self.assertEqual(self.env.backup_tx_dirs(), [])

    def test_failure_during_config_write_rolls_back(self):
        # pre-existing shared configs with another mod's state
        ll_before = ("modforge-loadlist\t1\ntitle\t%s\nengine\tunreal\n"
                     "mod\tOtherMod\t0.9\n" % TITLE)
        pl_before = "; keep me\n[OTHERGAME]\nfoo.prx=2\n"
        seed_file(self.env.loadlist, ll_before.encode())
        seed_file(self.env.ploader, pl_before.encode())
        with open(self.env.metadata_path) as f:
            md = json.load(f)
        inst = RaisingInstaller(self.env.roots, self.env.plugins,
                                self.env.state)
        with self.assertRaises(OSError):
            inst.install(self.env.archive, md, TITLE)
        with open(self.env.loadlist, "rb") as f:
            self.assertEqual(f.read(), ll_before.encode())
        with open(self.env.ploader, "rb") as f:
            self.assertEqual(f.read(), pl_before.encode())
        self.assertFalse(os.path.exists(self.env.mod_dir()))
        self.assertEqual(self.env.journal_files(), [])
        self.assertEqual(self.env.backup_tx_dirs(), [])

    def test_verify_detects_post_install_tamper(self):
        self.install_ok()
        inst = self.env.installer()
        self.assertTrue(inst.verify_installed(TITLE, MOD_NAME)["ok"])
        victim = os.path.join(self.env.mod_dir(), "Scripts", "helpers",
                              "util.lua")
        with open(victim, "r+b") as f:
            f.seek(-1, 2)
            b = f.read(1)
            f.seek(-1, 2)
            f.write(bytes([b[0] ^ 0xFF]))
        report = inst.verify_installed(TITLE, MOD_NAME)
        self.assertFalse(report["ok"])
        self.assertTrue(any("util.lua" in p and "tampered" in p
                            for p in report["problems"]), report)

    def test_verify_detects_missing_file_and_config_row(self):
        self.install_ok()
        os.unlink(os.path.join(self.env.mod_dir(), "Scripts", "main.lua"))
        with open(self.env.loadlist, "w") as f:
            f.write("modforge-loadlist\t1\ntitle\t%s\n" % TITLE)
        report = self.env.installer().verify_installed(TITLE, MOD_NAME)
        self.assertFalse(report["ok"])
        joined = " ".join(report["problems"])
        self.assertIn("missing", joined)
        self.assertIn("loadlist", joined)


# --------------------------------------------------------------------------
# crash recovery (subprocess hard-exit 86, then a fresh process recovers)
# --------------------------------------------------------------------------

def run_cli(args):
    return subprocess.run(
        [sys.executable, INSTALLER_PY] + args,
        capture_output=True, text=True, timeout=120)


class TestCrashRecovery(InstallerTestBase):
    def cli_install(self, extra):
        return run_cli([
            "install", "--archive", self.env.archive,
            "--metadata", self.env.metadata_path, "--title", TITLE,
            "--roots", self.env.roots, "--plugins", self.env.plugins,
            "--state", self.env.state] + extra)

    def test_crash_after_actions_rolls_back_on_recovery(self):
        for n in (8, 12):  # mid-file-writes and mid-config-writes
            with self.subTest(crash_after=n):
                env = Env()
                try:
                    make_zip(env.archive, ENTRIES)
                    env.write_meta()
                    rc = run_cli([
                        "install", "--archive", env.archive,
                        "--metadata", env.metadata_path, "--title", TITLE,
                        "--roots", env.roots, "--plugins", env.plugins,
                        "--state", env.state,
                        "--crash-after", str(n)])
                    self.assertEqual(rc.returncode, 86)
                    # journal + backups are on disk
                    j = run_cli(["recover", "--roots", env.roots,
                                 "--plugins", env.plugins,
                                 "--state", env.state])
                    self.assertEqual(j.returncode, 0)
                    report = json.loads(j.stdout)["recovered"]
                    self.assertEqual(report[0]["action"], "rolled-back")
                    self.assertEqual(report[0]["rollback_incomplete"], [])
                    # the whole target tree is back to the pre-install state
                    self.assertFalse(os.path.exists(
                        os.path.join(env.roots, TITLE, "ue4ss")))
                    self.assertEqual(
                        sorted(os.listdir(env.roots)), [])
                    jj = run_cli(["recover", "--roots", env.roots,
                                  "--plugins", env.plugins,
                                  "--state", env.state])
                    self.assertEqual(json.loads(jj.stdout)["recovered"], [])
                finally:
                    env.cleanup()

    def test_crash_pre_verify_restores_configs_byte_for_byte(self):
        ll_before = ("modforge-loadlist\t1\ntitle\t%s\nengine\tunreal\n"
                     "mod\tOtherMod\t0.9\nfile\tlua\tx\t3\n" % TITLE)
        pl_before = "; keep me\n[OTHERGAME]\nfoo.prx=2\n"
        seed_file(self.env.loadlist, ll_before.encode())
        seed_file(self.env.ploader, pl_before.encode())
        roots_before = snapshot(self.env.roots)
        plugins_before = snapshot(self.env.plugins)
        rc = self.cli_install(["--crash-at", "pre-verify"])
        self.assertEqual(rc.returncode, 86)
        # half-installed state is visible...
        self.assertTrue(os.path.exists(self.env.mod_dir()))
        self.assertNotEqual(snapshot(self.env.roots), roots_before)
        self.assertEqual(len(self.env.journal_files()), 1)
        self.assertEqual(len(self.env.backup_tx_dirs()), 1)  # backups held
        # ...and a fresh process rolls it fully back
        j = run_cli(["recover", "--roots", self.env.roots, "--plugins",
                     self.env.plugins, "--state", self.env.state])
        self.assertEqual(j.returncode, 0)
        report = json.loads(j.stdout)["recovered"][0]
        self.assertEqual(report["action"], "rolled-back")
        self.assertEqual(report["rollback_incomplete"], [])
        self.assertEqual(snapshot(self.env.roots), roots_before)
        self.assertEqual(snapshot(self.env.plugins), plugins_before)
        with open(self.env.loadlist) as f:
            self.assertEqual(f.read(), ll_before)
        with open(self.env.ploader) as f:
            self.assertEqual(f.read(), pl_before)
        self.assertEqual(self.env.journal_files(), [])
        self.assertEqual(self.env.backup_tx_dirs(), [])

    def test_crash_at_commit_recovery_completes_forward(self):
        rc = self.cli_install(["--crash-at", "commit"])
        self.assertEqual(rc.returncode, 86)
        # everything is installed and the journal says committed
        self.assertTrue(os.path.exists(os.path.join(
            self.env.mod_dir(), "Scripts", "main.lua")))
        self.assertTrue(os.path.exists(self.env.manifest_path()))
        self.assertEqual(len(self.env.journal_files()), 1)
        with open(os.path.join(self.env.state, "journal",
                               self.env.journal_files()[0])) as f:
            self.assertEqual(json.load(f)["phase"], "committed")
        # fresh process completes: cleanup, no rollback of good install
        j = run_cli(["recover", "--roots", self.env.roots, "--plugins",
                     self.env.plugins, "--state", self.env.state])
        self.assertEqual(j.returncode, 0)
        report = json.loads(j.stdout)["recovered"][0]
        self.assertEqual(report["action"], "completed")
        self.assertEqual(self.env.journal_files(), [])
        self.assertEqual(self.env.backup_tx_dirs(), [])
        v = run_cli(["verify", "--title", TITLE, "--mod", MOD_NAME,
                     "--roots", self.env.roots, "--plugins",
                     self.env.plugins, "--state", self.env.state])
        self.assertEqual(v.returncode, 0, v.stdout + v.stderr)

    def test_recover_with_quarantine_of_garbage_journal(self):
        jdir = os.path.join(self.env.state, "journal")
        os.makedirs(jdir, exist_ok=True)
        with open(os.path.join(jdir, "garbage.json"), "w") as f:
            f.write("{not json")
        j = run_cli(["recover", "--roots", self.env.roots, "--plugins",
                     self.env.plugins, "--state", self.env.state])
        self.assertEqual(j.returncode, 0)
        report = json.loads(j.stdout)["recovered"][0]
        self.assertEqual(report["action"], "quarantined")
        self.assertEqual(self.env.journal_files(), [])


# --------------------------------------------------------------------------
# CLI end-to-end
# --------------------------------------------------------------------------

class TestCli(InstallerTestBase):
    def test_install_recover_verify(self):
        rc = run_cli([
            "install", "--archive", self.env.archive,
            "--metadata", self.env.metadata_path, "--title", TITLE,
            "--roots", self.env.roots, "--plugins", self.env.plugins,
            "--state", self.env.state])
        self.assertEqual(rc.returncode, 0, rc.stderr)
        out = json.loads(rc.stdout)
        self.assertTrue(out["ok"])
        self.assertEqual(out["mod"], MOD_DIR)
        self.assertEqual(out["files"], 3)
        v = run_cli(["verify", "--title", TITLE, "--mod", MOD_NAME,
                     "--roots", self.env.roots, "--plugins",
                     self.env.plugins, "--state", self.env.state])
        self.assertEqual(v.returncode, 0)
        self.assertTrue(json.loads(v.stdout)["ok"])
        # unsupported archive via CLI maps to exit 2
        seven = os.path.join(self.env.base, "x.7z")
        with open(seven, "wb") as f:
            f.write(b"7z\xbc\xaf\x27\x1c\x00\x00")
        rc2 = run_cli([
            "install", "--archive", seven,
            "--metadata", self.env.metadata_path, "--title", TITLE,
            "--roots", self.env.roots, "--plugins", self.env.plugins,
            "--state", self.env.state])
        self.assertEqual(rc2.returncode, 2)
        self.assertIn("7z", rc2.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
