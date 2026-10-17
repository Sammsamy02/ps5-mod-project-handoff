#!/usr/bin/env python3
"""Case-sensitivity tests (Mac-only, host-tested; no console contact).

The Mac host disk is case-INSENSITIVE by default; the PS5 target is
case-sensitive.  Paths differing only in case collide into one file on the
host but are distinct on the console, where the runtime then looks in the
wrong one — and os.path.exists() on the host CANNOT see the difference.
These tests cover:

* detect_caseConflicts()  — pure in-code detection (no filesystem probing)
  of whole-path case variants ("Scripts/main.lua" vs "scripts/main.lua")
  and directory case variants ("Scripts/main.lua" vs "scripts/util.lua")
* installer wiring        — within a single mod's file list AND across all
  installed mods' file lists (manifests), rejecting the install with
  CaseConflictError before anything is written
* runtime folder case     — a mod whose folder case differs from what the
  runtime expects (e.g. runtime 'Mods/' vs archive 'mods/') is REPORTED as
  a runtime/config issue; mod files are NEVER renamed
* CaseSensitiveVolumeTests — creates a real case-sensitive APFS disk image
  with hdiutil, proves the volume really is case-sensitive in code, reruns
  the FULL installer test suite with every sandbox on that volume, and
  detaches it afterwards (skipped automatically when hdiutil is missing)

Run:  python3 -m unittest -v test_case_sensitivity
"""

from __future__ import annotations

import json
import os
import re
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
    CaseConflictError, Installer, UnsafeArchiveError, detect_caseConflicts)

TITLE = "PPSA33286"
MAIN_LUA = b"-- main.lua\nRegisterMod('X', 'Mod')\n"
UTIL_LUA = b"-- util\n"
SETTINGS_INI = b"[Settings]\nfoo=1\n"


def make_zip(path, entries):
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as zf:
        for name, data in entries:
            zf.writestr(zipfile.ZipInfo(name), data)
    return path


def snapshot(root):
    out = {}
    for dirpath, dirnames, filenames in os.walk(root):
        for d in dirnames:
            out[os.path.relpath(os.path.join(dirpath, d), root)] = None
        for f in filenames:
            p = os.path.join(dirpath, f)
            with open(p, "rb") as fh:
                import hashlib
                out[os.path.relpath(p, root)] = \
                    hashlib.sha256(fh.read()).hexdigest()
    return out


class CaseEnv(object):
    """Per-test sandbox mirroring test_installer.Env."""

    def __init__(self):
        self.base = tempfile.mkdtemp(
            prefix="case-test-",
            dir=os.environ.get("MODBRIDGE_TEST_TMPDIR") or None)
        self.roots = os.path.join(self.base, "roots")
        self.plugins = os.path.join(self.base, "plugins")
        self.state = os.path.join(self.base, "state")
        for d in (self.roots, self.plugins, self.state):
            os.makedirs(d, exist_ok=True)
        self.archive = os.path.join(self.base, "mod.zip")

    def meta(self, name="Case Mod", version="1.0.0"):
        return {"name": name, "version": version}

    def installer(self):
        return Installer(self.roots, self.plugins, self.state)

    def cleanup(self):
        shutil.rmtree(self.base, ignore_errors=True)


# --------------------------------------------------------------------------
# 1a. the pure detection function
# --------------------------------------------------------------------------

class TestDetectCaseConflicts(unittest.TestCase):
    """In-code detection: no filesystem calls, works on any disk."""

    def test_whole_path_variant_detected(self):
        self.assertEqual(
            detect_caseConflicts(["Scripts/main.lua", "scripts/main.lua"]),
            [("Scripts/main.lua", "scripts/main.lua")])

    def test_directory_variant_detected(self):
        # same directory, different case, DIFFERENT file names: still a
        # conflict because the 'Scripts' component collides
        self.assertEqual(
            detect_caseConflicts(["Scripts/main.lua", "scripts/util.lua"]),
            [("Scripts/main.lua", "scripts/util.lua")])

    def test_config_dir_variant_detected(self):
        self.assertEqual(
            detect_caseConflicts(["Config/settings.ini",
                                  "config/settings.ini"]),
            [("Config/settings.ini", "config/settings.ini")])

    def test_no_conflict_when_case_matches(self):
        self.assertEqual(
            detect_caseConflicts(["Scripts/main.lua", "Scripts/util.lua"]),
            [])

    def test_identical_duplicates_are_not_case_conflicts(self):
        self.assertEqual(detect_caseConflicts(["x/y", "x/y"]), [])

    def test_different_trees_do_not_conflict(self):
        self.assertEqual(
            detect_caseConflicts(["a/Config/s.ini", "b/config/s.ini"]), [])

    def test_backslash_inputs_normalized(self):
        self.assertEqual(
            detect_caseConflicts(["Scripts\\main.lua", "scripts/main.lua"]),
            [("Scripts/main.lua", "scripts/main.lua")])

    def test_multiple_conflicts_all_returned(self):
        pairs = detect_caseConflicts([
            "Scripts/main.lua", "scripts/util.lua",
            "Config/a.ini", "config/b.ini", "README.md"])
        self.assertEqual(
            pairs, [("Config/a.ini", "config/b.ini"),
                    ("Scripts/main.lua", "scripts/util.lua")])

    def test_deep_component_variant_detected(self):
        self.assertEqual(
            detect_caseConflicts(
                ["Mods/MyMod/Scripts/main.lua",
                 "Mods/MyMod/scripts/util.lua"]),
            [("Mods/MyMod/Scripts/main.lua",
              "Mods/MyMod/scripts/util.lua")])

    def test_empty_list(self):
        self.assertEqual(detect_caseConflicts([]), [])


# --------------------------------------------------------------------------
# 1b/1c. installer wiring: within one mod and across installed mods
# --------------------------------------------------------------------------

class TestInstallerCaseWiring(unittest.TestCase):
    def setUp(self):
        self.env = CaseEnv()

    def tearDown(self):
        self.env.cleanup()

    def install(self, entries, name="Case Mod"):
        make_zip(self.env.archive, entries)
        return self.env.installer().install(
            self.env.archive, self.env.meta(name=name), TITLE)

    def test_within_mod_case_conflict_rejected(self):
        # one mod: Scripts/main.lua + scripts/util.lua (same dir, diff case)
        before = snapshot(self.env.roots)
        with self.assertRaises(CaseConflictError) as cm:
            self.install([
                ("MyMod/Scripts/main.lua", MAIN_LUA),
                ("MyMod/scripts/util.lua", UTIL_LUA)])
        msg = str(cm.exception)
        self.assertIn("Scripts/main.lua", msg)
        self.assertIn("scripts/util.lua", msg)
        self.assertIn("case", msg)
        # runtime expectation reported; never renamed
        self.assertIn("never renames", msg)
        # transactional: nothing written, no journal left behind
        self.assertEqual(snapshot(self.env.roots), before)
        self.assertEqual(os.listdir(os.path.join(self.env.state, "journal")),
                         [])

    def test_within_mod_whole_path_variant_rejected(self):
        with self.assertRaises(CaseConflictError) as cm:
            self.install([
                ("MyMod/Scripts/main.lua", MAIN_LUA),
                ("MyMod/Scripts/Main.lua", b"-- other main\n")])
        self.assertIn("Scripts/main.lua", str(cm.exception))
        self.assertIn("Scripts/Main.lua", str(cm.exception))

    def test_runtime_entrypoint_note_in_message(self):
        with self.assertRaises(CaseConflictError) as cm:
            self.install([
                ("MyMod/Scripts/main.lua", MAIN_LUA),
                ("MyMod/scripts/main.lua", MAIN_LUA)])
        msg = str(cm.exception)
        # runtime/config framing, exact-case expectation, never rename
        self.assertIn("runtime/config issue", msg)
        self.assertIn("exact case", msg)
        self.assertIn("never renames", msg)

    def test_across_mods_case_conflict_rejected(self):
        # mod A: Config/settings.ini (installs fine)
        self.install([
            ("ModA/Scripts/main.lua", MAIN_LUA),
            ("ModA/Config/settings.ini", SETTINGS_INI)], name="Mod A")
        self.assertTrue(os.path.exists(os.path.join(
            self.env.roots, TITLE, "ue4ss", "Mods", "Mod_A", "Config",
            "settings.ini")))
        # mod B: config/settings.ini — case-conflicts with A's Config/
        zip_b = os.path.join(self.env.base, "modB.zip")
        make_zip(zip_b, [
            ("ModB/Scripts/main.lua", MAIN_LUA),
            ("ModB/config/settings.ini", SETTINGS_INI)])
        before = snapshot(self.env.roots)
        with self.assertRaises(CaseConflictError) as cm:
            self.env.installer().install(
                zip_b, self.env.meta(name="Mod B"), TITLE)
        msg = str(cm.exception)
        self.assertIn("Config/settings.ini", msg)
        self.assertIn("config/settings.ini", msg)
        self.assertIn("Mod_A", msg)  # the conflicting installed mod
        self.assertIn("Mod_B", msg)
        # A's install untouched
        self.assertEqual(snapshot(self.env.roots), before)

    def test_same_spelling_across_mods_is_not_a_conflict(self):
        # every UE4SS mod ships Scripts/main.lua — must NOT be flagged
        self.install([("ModA/Scripts/main.lua", MAIN_LUA)], name="Mod A")
        manifest = self.install(
            [("ModB/Scripts/main.lua", MAIN_LUA)], name="Mod B")
        self.assertEqual(len(manifest["files"]), 1)

    def test_find_case_conflicts_report_structure(self):
        self.install([
            ("ModA/Scripts/main.lua", MAIN_LUA),
            ("ModA/Config/settings.ini", SETTINGS_INI)], name="Mod A")
        findings = self.env.installer().find_case_conflicts(
            TITLE, ["Scripts/main.lua", "config/settings.ini"], "Mod_B")
        self.assertEqual(findings["within_mod"], [])
        self.assertEqual(len(findings["across_mods"]), 1)
        self.assertEqual(findings["across_mods"][0]["mod"], "Mod_A")
        self.assertEqual(findings["across_mods"][0]["pair"],
                         ["Config/settings.ini", "config/settings.ini"])

    def test_own_reinstall_case_variant_allowed(self):
        # reinstalling the SAME mod over its own files is never a conflict
        self.install([
            ("ModA/Scripts/main.lua", MAIN_LUA),
            ("ModA/Config/settings.ini", SETTINGS_INI)], name="Mod A")
        manifest = self.install(
            [("ModA/Scripts/main.lua", MAIN_LUA),
             ("ModA/Config/settings.ini", SETTINGS_INI)], name="Mod A")
        self.assertEqual(manifest["mod_dir"], "Mod_A")


# --------------------------------------------------------------------------
# 1e. runtime folder case mismatch: reported, NEVER renamed
# --------------------------------------------------------------------------

class TestRuntimeFolderCaseReporting(unittest.TestCase):
    def setUp(self):
        self.env = CaseEnv()

    def tearDown(self):
        self.env.cleanup()

    def test_mods_root_case_variant_reported_not_renamed(self):
        # runtime tree is ue4ss/Mods/ — this archive says mods/ (lowercase).
        # The mod root is stripped on install so nothing is renamed; the
        # manifest REPORTS the case variant as a runtime/config audit fact.
        make_zip(self.env.archive,
                 [("mods/MyMod/Scripts/main.lua", MAIN_LUA)])
        manifest = self.env.installer().install(
            self.env.archive, self.env.meta(), TITLE)
        self.assertEqual(manifest["mod_root_in_archive"], "mods/MyMod/")
        self.assertEqual(manifest["mod_root_case_variants"], ["mods"])
        # installed under the canonical runtime Mods/ tree, archive spelling
        # preserved inside the mod (never renamed)
        installed = os.path.join(self.env.roots, TITLE, "ue4ss", "Mods",
                                 "Case_Mod", "Scripts", "main.lua")
        self.assertTrue(os.path.exists(installed))

    def test_exact_case_mod_root_has_no_variants(self):
        make_zip(self.env.archive,
                 [("Mods/MyMod/Scripts/main.lua", MAIN_LUA)])
        manifest = self.env.installer().install(
            self.env.archive, self.env.meta(), TITLE)
        self.assertEqual(manifest["mod_root_case_variants"], [])

    def test_lowercase_scripts_entrypoint_still_reported_as_case_issue(self):
        # scripts/main.lua (all lowercase) is rejected by the exact-case
        # Scripts/main.lua gate with the case hint — reported, not renamed
        make_zip(self.env.archive,
                 [("MyMod/scripts/main.lua", MAIN_LUA)])
        before = snapshot(self.env.roots)
        with self.assertRaises(UnsafeArchiveError) as cm:
            self.env.installer().install(
                self.env.archive, self.env.meta(), TITLE)
        self.assertIn("case", str(cm.exception))
        self.assertIn("scripts/main.lua", str(cm.exception))
        self.assertEqual(snapshot(self.env.roots), before)


# --------------------------------------------------------------------------
# 1d. real case-sensitive APFS volume: prove it + rerun the FULL suite on it
# --------------------------------------------------------------------------

def _hdiutil(*args, check=True):
    return subprocess.run(["hdiutil", *args], capture_output=True,
                          text=True, check=check)


def _casetest_mounts():
    """(device, mountpoint) for every mounted CaseTest[/ N] volume.

    macOS appends ' N' when the bare name is already taken, so a leaked
    volume from an interrupted run hides at /Volumes/CaseTest 2 etc.; any
    of them must be found for reliable cleanup.
    """
    out = []
    proc = subprocess.run(["mount"], capture_output=True, text=True)
    for line in proc.stdout.splitlines():
        head = line.split(" ", 1)
        m = None
        if " on " in line:
            m = line.split(" on ", 1)[1].split(" (", 1)[0]
        if m and re.fullmatch(r"/Volumes/CaseTest( \d+)?", m):
            out.append((head[0], m))
    return out


def _detach_mount(dev, mount):
    """Detach one mounted volume, trying slice then whole disk, then force."""
    ok = _hdiutil("detach", dev, check=False).returncode == 0
    if not ok and "s" in dev.rsplit("/", 1)[-1]:
        parent = dev.rsplit("s", 1)[0]
        ok = _hdiutil("detach", parent, check=False).returncode == 0
    if not ok and os.path.ismount(mount):
        _hdiutil("detach", "-force", dev, check=False)


class TestOnCaseSensitiveVolume(unittest.TestCase):
    """Mac-only: hdiutil case-sensitive APFS image, full suite on it.

    Creates a real case-sensitive disk image, proves IN CODE that it is
    case-sensitive (os.path.exists under the wrong case returns False —
    impossible on the default Mac volume), reruns the FULL installer test
    suite with every sandbox on the volume, then detaches it.  Skipped
    with a clear reason when hdiutil is unavailable or attach fails;
    never contacts any console.
    """

    VOLNAME = "CaseTest"
    DMG = None
    MOUNT = None
    WORKDIR = None

    @classmethod
    def setUpClass(cls):
        if shutil.which("hdiutil") is None or shutil.which("mount") is None:
            raise unittest.SkipTest(
                "hdiutil/mount not available (Mac-only test): cannot "
                "create a case-sensitive APFS disk image")
        cls._tmpdir = tempfile.mkdtemp(prefix="casetest-dmg-")
        cls.DMG = os.path.join(cls._tmpdir, "casetest.dmg")
        cls.MOUNT = None
        cls.WORKDIR = None
        try:
            # detach stale CaseTest[/ N] volumes from interrupted runs
            for dev, mnt in _casetest_mounts():
                _detach_mount(dev, mnt)
            _hdiutil("create", "-size", "100m",
                     "-fs", "Case-sensitive APFS",
                     "-volname", cls.VOLNAME, cls.DMG)
            attach = _hdiutil("attach", cls.DMG)
            # the mount point is the LAST 3-column line of the attach table
            # (volume row); macOS may have suffixed the name if a stale
            # volume survived, so never assume /Volumes/CaseTest verbatim
            for line in attach.stdout.splitlines():
                cols = [c for c in line.split("\t") if c.strip()]
                if len(cols) == 3 and cols[2].startswith("/Volumes/"):
                    cls.MOUNT = cols[2]
            if not cls.MOUNT or not os.path.isdir(cls.MOUNT):
                raise RuntimeError(
                    "could not locate mount point; hdiutil said: %r / %r"
                    % (attach.stdout.strip(), attach.stderr.strip()))
            cls.WORKDIR = os.path.join(cls.MOUNT, "modbridge-suite")
            os.makedirs(cls.WORKDIR, exist_ok=True)
        except Exception as exc:
            cls._teardown_volume()
            raise unittest.SkipTest(
                "hdiutil case-sensitive volume unavailable: %s" % exc)

    @classmethod
    def _teardown_volume(cls):
        if cls.MOUNT and os.path.ismount(cls.MOUNT):
            for dev, mnt in _casetest_mounts():
                if mnt == cls.MOUNT:
                    _detach_mount(dev, mnt)
        if cls._tmpdir:
            shutil.rmtree(cls._tmpdir, ignore_errors=True)

    @classmethod
    def tearDownClass(cls):
        cls._teardown_volume()

    def test_00_volume_is_really_case_sensitive(self):
        # THE in-code proof the Mac default disk cannot give us:
        # a.txt exists, A.TXT (other case) does NOT.
        a = os.path.join(self.WORKDIR, "a.txt")
        with open(a, "w") as f:
            f.write("x")
        self.assertTrue(os.path.exists(a))
        self.assertFalse(os.path.exists(
            os.path.join(self.WORKDIR, "A.TXT")),
            "volume is case-INSENSITIVE — wrong image format mounted")
        # two same-name-different-case dirs coexist as distinct entries
        os.makedirs(os.path.join(self.WORKDIR, "Scripts"), exist_ok=True)
        os.makedirs(os.path.join(self.WORKDIR, "scripts"), exist_ok=True)
        self.assertTrue(os.path.isdir(os.path.join(self.WORKDIR, "Scripts")))
        self.assertTrue(os.path.isdir(os.path.join(self.WORKDIR, "scripts")))

    def test_01_case_conflict_still_detected_on_case_sensitive_disk(self):
        # detection is in CODE, so it fires even where the filesystem would
        # happily keep both spellings apart
        env_roots = os.path.join(self.WORKDIR, "roots")
        plugins = os.path.join(self.WORKDIR, "plugins")
        state = os.path.join(self.WORKDIR, "state")
        for d in (env_roots, plugins, state):
            os.makedirs(d, exist_ok=True)
        arc = os.path.join(self.WORKDIR, "mod.zip")
        make_zip(arc, [
            ("MyMod/Scripts/main.lua", MAIN_LUA),
            ("MyMod/scripts/util.lua", UTIL_LUA)])
        inst = Installer(env_roots, plugins, state)
        with self.assertRaises(CaseConflictError):
            inst.install(arc, {"name": "X", "version": "1"}, TITLE)
        for d in (env_roots, plugins, state):
            shutil.rmtree(d, ignore_errors=True)
        os.unlink(arc)

    def test_02_full_installer_suite_on_case_sensitive_volume(self):
        """Rerun the FULL installer test suite with every sandbox on the
        case-sensitive volume (MODBRIDGE_TEST_TMPDIR hook)."""
        env = dict(os.environ, MODBRIDGE_TEST_TMPDIR=self.WORKDIR)
        proc = subprocess.run(
            [sys.executable, "-m", "unittest", "-v",
             "test_installer", "test_zip_safety",
             "test_conflicts", "test_uninstall", "test_detector"],
            cwd=HERE, env=env, capture_output=True, text=True, timeout=600)
        self.assertEqual(
            proc.returncode, 0,
            "full suite failed on the case-sensitive volume:\n%s\n%s"
            % (proc.stdout[-4000:], proc.stderr[-4000:]))
        self.assertIn("OK", proc.stderr)
        # direct proof the redirect hook routes sandboxes onto the volume
        probe = subprocess.run(
            [sys.executable, "-c",
             "import os, tempfile\n"
             "d = tempfile.mkdtemp(prefix='probe-',\n"
             "                     dir=os.environ['MODBRIDGE_TEST_TMPDIR'])\n"
             "print(d)"],
            env=env, capture_output=True, text=True)
        self.assertTrue(probe.stdout.strip().startswith(self.WORKDIR),
                        "sandbox not created on the volume: %r"
                        % probe.stdout.strip())
        shutil.rmtree(probe.stdout.strip(), ignore_errors=True)


if __name__ == "__main__":
    unittest.main(verbosity=2)
