#!/usr/bin/env python3
"""Zip safety edge cases for the modbridge installer (Mac-only, host-tested).

No console contact; everything happens in temp-dir sandboxes.  Covers the
edge cases beyond the traversal/symlink/encrypted tests in
test_installer.py:

* backslash paths   — Windows-created zips ("Scripts\\main.lua") install
                      correctly after in-code normalization, and
                      backslash-based traversal is still rejected
* duplicate entries — the same path twice in one zip (exact repeat,
                      "./x" vs "x", "a\\b" vs "a/b") is DETECTED and
                      rejected instead of silently last-one-wins; a
                      case-variant duplicate is rejected as a case conflict
* nested zip bombs  — zip-in-zip-in-zip: the installer stores nested
                      archives as opaque bytes (never expands them); a
                      member DECLARING an over-limit size (patched central
                      directory, as real bombs report huge sizes) trips
                      the per-member and total-size bomb guards; the
                      detector's nested-zip inspection stays bounded
                      (depth cap + byte caps)
* mixed real+fake   — a real Lua mod alongside a fake .dll (a text file
                      with a .dll name) installs byte-identically and is
                      flagged by the detector as a dll-without-MZ
* UE4SS-as-mod      — a UE4SS runtime zip (UE4SS.dll/dwmapi.dll at the
                      root, no Mods/ tree) is refused with a clear
                      "provided by the PS5 runtime layer; do not install
                      as a mod" message

Run:  python3 -m unittest -v test_zip_safety
"""

from __future__ import annotations

import hashlib
import io
import json
import os
import shutil
import sys
import tempfile
import unittest
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import installer as I  # noqa: E402
import detector as D  # noqa: E402
from installer import (  # noqa: E402
    CaseConflictError, Installer, UnsafeArchiveError)

TITLE = "PPSA33286"
MAIN_LUA = b"-- main.lua\nRegisterKeyBind(Key.F, function() end)\n"
UTIL_LUA = b"-- util\n"
SETTINGS_INI = b"[Settings]\nfoo=1\n"
DLL_PAYLOAD = b"MZ\x90\x00" + b"\x00" * 60      # real PE header magic
FAKE_DLL = b"this is actually a plain text file, not a PE image\r\n"

LUA_MARKERS = """-- Item Spawner
local Pawn = FindFirstOf("Pawn")
ExecuteInGameThread(function() end)
RegisterKeyBind(Key.F1, function() end)
"""


def make_zip(path, entries):
    """Write a zip; duplicate names are allowed (that is the point)."""
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as zf:
        for name, data in entries:
            zf.writestr(zipfile.ZipInfo(name), data)
    return path


def zip_bytes(files):
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w", zipfile.ZIP_DEFLATED) as zf:
        for name, data in files.items():
            zf.writestr(name,
                        data.encode("utf-8") if isinstance(data, str)
                        else data)
    return buf.getvalue()


def sha(data):
    return hashlib.sha256(data).hexdigest()


def snapshot(root):
    out = {}
    for dirpath, dirnames, filenames in os.walk(root):
        for d in dirnames:
            out[os.path.relpath(os.path.join(dirpath, d), root)] = None
        for f in filenames:
            p = os.path.join(dirpath, f)
            with open(p, "rb") as fh:
                out[os.path.relpath(p, root)] = sha(fh.read())
    return out


def patch_declared_sizes(path, sizes):
    """Patch the central-directory uncompressed sizes of the named members.

    Simulates zip-bomb style entries whose DECLARED sizes are huge without
    materializing gigabytes on disk (the installer's guards read the
    declared sizes before extracting anything).  The central directory is
    located through the end-of-central-directory record so compressed
    member data can never be mistaken for CD entries.
    """
    with open(path, "rb") as f:
        data = f.read()
    eocd = data.rfind(b"PK\x05\x06")
    assert eocd >= 0, "no EOCD record found"
    cd_offset = int.from_bytes(data[eocd + 16:eocd + 20], "little")
    i = cd_offset
    patched = []
    while data[i:i + 4] == b"PK\x01\x02":
        nlen = int.from_bytes(data[i + 28:i + 30], "little")
        elen = int.from_bytes(data[i + 30:i + 32], "little")
        clen = int.from_bytes(data[i + 32:i + 34], "little")
        name = data[i + 46:i + 46 + nlen].decode("utf-8", "replace")
        if name in sizes:
            new = sizes[name].to_bytes(4, "little")
            data = data[:i + 24] + new + data[i + 28:]
            patched.append(name)
        i += 46 + nlen + elen
    missing = set(sizes) - set(patched)
    assert not missing, "members not found for size patch: %s" % missing
    with open(path, "wb") as f:
        f.write(data)


class Env(object):
    def __init__(self):
        self.base = tempfile.mkdtemp(
            prefix="zipsafety-test-",
            dir=os.environ.get("MODBRIDGE_TEST_TMPDIR") or None)
        self.roots = os.path.join(self.base, "roots")
        self.plugins = os.path.join(self.base, "plugins")
        self.state = os.path.join(self.base, "state")
        for d in (self.roots, self.plugins, self.state):
            os.makedirs(d, exist_ok=True)
        self.archive = os.path.join(self.base, "mod.zip")

    def meta(self, name="Zip Mod", version="1.0.0"):
        return {"name": name, "version": version}

    def mod_dir(self, mod="Zip_Mod"):
        return os.path.join(self.roots, TITLE, "ue4ss", "Mods", mod)

    def installer(self):
        return Installer(self.roots, self.plugins, self.state)

    def cleanup(self):
        shutil.rmtree(self.base, ignore_errors=True)


class ZipSafetyTestBase(unittest.TestCase):
    def setUp(self):
        self.env = Env()

    def tearDown(self):
        self.env.cleanup()

    def install(self, entries, meta=None):
        make_zip(self.env.archive, entries)
        return self.env.installer().install(
            self.env.archive, meta or self.env.meta(), TITLE)


# --------------------------------------------------------------------------
# 2a. backslash paths (Windows-created zips)
# --------------------------------------------------------------------------

class TestBackslashPaths(ZipSafetyTestBase):
    def test_windows_backslash_zip_installs(self):
        manifest = self.install([
            ("MyMod\\Scripts\\main.lua", MAIN_LUA),
            ("MyMod\\Scripts\\helpers\\util.lua", UTIL_LUA),
            ("MyMod\\config\\settings.ini", SETTINGS_INI)])
        md = self.env.mod_dir()
        with open(os.path.join(md, "Scripts", "main.lua"), "rb") as f:
            self.assertEqual(f.read(), MAIN_LUA)
        with open(os.path.join(md, "Scripts", "helpers", "util.lua"),
                  "rb") as f:
            self.assertEqual(f.read(), UTIL_LUA)
        with open(os.path.join(md, "config", "settings.ini"), "rb") as f:
            self.assertEqual(f.read(), SETTINGS_INI)
        # manifest paths are normalized to '/'
        self.assertEqual(
            sorted(f["archive_path"] for f in manifest["files"]),
            ["MyMod/Scripts/helpers/util.lua",
             "MyMod/Scripts/main.lua",
             "MyMod/config/settings.ini"])
        self.assertTrue(self.env.installer().verify_installed(
            TITLE, "Zip Mod")["ok"])

    def test_mixed_separators(self):
        self.install([("MyMod/Scripts\\util.lua", UTIL_LUA),
                      ("MyMod\\Scripts/main.lua", MAIN_LUA)])
        md = self.env.mod_dir()
        self.assertTrue(os.path.exists(
            os.path.join(md, "Scripts", "main.lua")))
        self.assertTrue(os.path.exists(
            os.path.join(md, "Scripts", "util.lua")))

    def test_backslash_traversal_rejected(self):
        before = snapshot(self.env.roots)
        with self.assertRaises(UnsafeArchiveError) as cm:
            self.install([
                ("MyMod/Scripts/main.lua", MAIN_LUA),
                ("MyMod\\Scripts\\..\\..\\evil.txt", b"evil")])
        self.assertIn("'..'", str(cm.exception))
        self.assertEqual(snapshot(self.env.roots), before)

    def test_backslash_drive_letter_rejected(self):
        with self.assertRaises(UnsafeArchiveError):
            self.install([
                ("MyMod/Scripts/main.lua", MAIN_LUA),
                ("C:\\Windows\\evil.txt", b"evil")])


# --------------------------------------------------------------------------
# 2b. duplicate entries (last-one-wins must be detected, not guessed)
# --------------------------------------------------------------------------

class TestDuplicateEntries(ZipSafetyTestBase):
    def test_exact_duplicate_rejected(self):
        before = snapshot(self.env.roots)
        with self.assertRaises(UnsafeArchiveError) as cm:
            self.install([
                ("MyMod/Scripts/main.lua", MAIN_LUA),
                ("MyMod/Scripts/main.lua", b"-- second copy\n")])
        msg = str(cm.exception)
        self.assertIn("duplicate", msg)
        self.assertIn("MyMod/Scripts/main.lua", msg)
        self.assertEqual(snapshot(self.env.roots), before)

    def test_dot_slash_duplicate_rejected(self):
        # "./MyMod/x" normalizes to the same path as "MyMod/x"
        with self.assertRaises(UnsafeArchiveError) as cm:
            self.install([
                ("MyMod/Scripts/main.lua", MAIN_LUA),
                ("./MyMod/Scripts/main.lua", b"-- second copy\n")])
        self.assertIn("duplicate", str(cm.exception))

    def test_backslash_duplicate_rejected(self):
        # "MyMod\\Scripts\\main.lua" is the same member as the '/' spelling
        with self.assertRaises(UnsafeArchiveError) as cm:
            self.install([
                ("MyMod/Scripts/main.lua", MAIN_LUA),
                ("MyMod\\Scripts\\main.lua", b"-- second copy\n")])
        self.assertIn("duplicate", str(cm.exception))

    def test_duplicate_directory_entry_rejected(self):
        make_zip(self.env.archive, [
            ("MyMod/Scripts/", b""),
            ("MyMod/Scripts/", b""),
            ("MyMod/Scripts/main.lua", MAIN_LUA)])
        with self.assertRaises(UnsafeArchiveError) as cm:
            self.env.installer().install(
                self.env.archive, self.env.meta(), TITLE)
        self.assertIn("duplicate", str(cm.exception))

    def test_case_variant_duplicate_is_case_conflict(self):
        # different norms (case preserved) -> not a "duplicate", but the
        # in-code case-conflict preflight rejects it with the runtime note
        before = snapshot(self.env.roots)
        with self.assertRaises(CaseConflictError) as cm:
            self.install([
                ("MyMod/Scripts/main.lua", MAIN_LUA),
                ("MyMod/scripts/main.lua", b"-- other-case copy\n")])
        msg = str(cm.exception)
        self.assertIn("Scripts/main.lua", msg)
        self.assertIn("scripts/main.lua", msg)
        self.assertIn("exact case", msg)
        self.assertEqual(snapshot(self.env.roots), before)


# --------------------------------------------------------------------------
# 2c. nested zip bombs
# --------------------------------------------------------------------------

class TestNestedZipBombs(ZipSafetyTestBase):
    def test_nested_bomb_stored_not_expanded(self):
        # zip -> zip -> zip, each level expanding: 32 MiB of zeros packs to
        # ~32 KB.  The installer must store the nested archives as opaque
        # bytes (never recursively expand) and verify them byte-identical.
        inner = zip_bytes({"blob.bin": b"\x00" * (32 << 20)})
        mid = zip_bytes({"inner.zip": inner})
        bomb_size = len(mid)
        manifest = self.install([
            ("MyMod/Scripts/main.lua", MAIN_LUA),
            ("MyMod/assets/mid.zip", mid)])
        # mid.zip landed byte-identical, still ~tens of KB, not megabytes
        with open(os.path.join(self.env.mod_dir(), "assets", "mid.zip"),
                  "rb") as f:
            on_disk = f.read()
        self.assertEqual(on_disk, mid)
        self.assertEqual(len(on_disk), bomb_size)
        self.assertLess(bomb_size, 1 << 20)  # nothing exploded
        by_arc = {f["archive_path"]: f for f in manifest["files"]}
        self.assertEqual(by_arc["MyMod/assets/mid.zip"]["original_sha256"],
                         sha(mid))
        self.assertEqual(by_arc["MyMod/assets/mid.zip"]["installed_sha256"],
                         sha(mid))
        self.assertTrue(self.env.installer().verify_installed(
            TITLE, "Zip Mod")["ok"])

    def test_member_declared_size_rejected(self):
        # one member whose DECLARED uncompressed size exceeds the 256 MiB
        # per-member limit (patched central directory, like a real bomb)
        make_zip(self.env.archive, [
            ("MyMod/Scripts/main.lua", MAIN_LUA),
            ("MyMod/assets/huge.bin", b"\x00" * 4096)])
        patch_declared_sizes(self.env.archive,
                             {"MyMod/assets/huge.bin": 512 << 20})
        before = snapshot(self.env.roots)
        with self.assertRaises(UnsafeArchiveError) as cm:
            self.env.installer().install(
                self.env.archive, self.env.meta(), TITLE)
        self.assertIn("exceeds limit", str(cm.exception))
        self.assertEqual(snapshot(self.env.roots), before)

    def test_total_declared_size_rejected(self):
        # several members, each under the per-member limit, whose declared
        # total exceeds the 1 GiB zip-bomb guard
        entries = [("MyMod/Scripts/main.lua", MAIN_LUA)]
        sizes = {}
        for i in range(5):  # 5 x 220 MiB = 1100 MiB > 1 GiB
            name = "MyMod/assets/pad%d.bin" % i
            entries.append((name, b"\x00" * 1024))
            sizes[name] = 220 << 20
        make_zip(self.env.archive, entries)
        patch_declared_sizes(self.env.archive, sizes)
        before = snapshot(self.env.roots)
        with self.assertRaises(UnsafeArchiveError) as cm:
            self.env.installer().install(
                self.env.archive, self.env.meta(), TITLE)
        self.assertIn("zip bomb guard", str(cm.exception))
        self.assertEqual(snapshot(self.env.roots), before)

    def test_detector_nested_bomb_bounded(self):
        # the DETECTOR does expand nested zips (bounded): 3 levels, 32 MiB
        # inner payload — inspection completes with caps, no exception
        inner = zip_bytes({"blob.bin": b"\x00" * (32 << 20)})
        mid = zip_bytes({"inner.zip": inner})
        path = os.path.join(self.env.base, "nested.zip")
        make_zip(path, [
            ("mid.zip", mid),
            ("Mods/Bomb/Scripts/main.lua", LUA_MARKERS.encode("utf-8"))])
        c = D.classify_archive(path)
        self.assertEqual(c.label, D.ModType.UE4SS_LUA)
        self.assertEqual(c.flags["nested_archives"],
                         ["mid.zip", "mid.zip/inner.zip"])
        notes = "\n".join(c.notes)
        self.assertIn("inspected nested zip: mid.zip", notes)
        self.assertIn("inspected nested zip: mid.zip/inner.zip", notes)

    def test_detector_nested_depth_cap(self):
        # chain.zip -> w0 -> w4 -> w3 -> w2 -> w1 -> blob (6 zip levels):
        # only 4 nesting levels are inspected; the deepest zips stay opaque
        # and are never opened
        w = zip_bytes({"blob.bin": b"data"})
        for n in range(1, 5):  # w4 wraps w3 wraps w2 wraps w1 wraps blob
            w = zip_bytes({"w%d.zip" % n: w})
        path = os.path.join(self.env.base, "chain.zip")
        make_zip(path, [("w0.zip", w)])
        c = D.classify_archive(path)
        nested = c.flags["nested_archives"]
        self.assertEqual(len(nested), 4)  # depth cap (MAX_NESTED_DEPTH=4)
        self.assertTrue(any(n.endswith("w2.zip") for n in nested))
        self.assertFalse(any("w1.zip" in n for n in nested),
                         "5th nesting level must stay unopened: %s" % nested)


# --------------------------------------------------------------------------
# 2d. mixed real + fake entries
# --------------------------------------------------------------------------

class TestMixedRealFake(ZipSafetyTestBase):
    def test_real_lua_plus_fake_dll_installs_byte_identical(self):
        fake_dll = FAKE_DLL * 3
        manifest = self.install([
            ("MyMod/Scripts/main.lua", MAIN_LUA),
            ("MyMod/bin/native.dll", fake_dll)])
        with open(os.path.join(self.env.mod_dir(), "bin", "native.dll"),
                  "rb") as f:
            self.assertEqual(f.read(), fake_dll)  # content is NOT "fixed"
        by_arc = {f["archive_path"]: f for f in manifest["files"]}
        self.assertEqual(by_arc["MyMod/bin/native.dll"]["role"], "asset")
        self.assertEqual(by_arc["MyMod/bin/native.dll"]["installed_sha256"],
                         sha(fake_dll))
        self.assertTrue(self.env.installer().verify_installed(
            TITLE, "Zip Mod")["ok"])

    def test_detector_flags_fake_dll_with_warning(self):
        path = os.path.join(self.env.base, "mixed.zip")
        make_zip(path, [
            ("Mods/Thing/Scripts/main.lua",
             LUA_MARKERS.encode("utf-8")),
            ("Mods/Thing/bin/native.dll", FAKE_DLL)])
        c = D.classify_archive(path)
        self.assertTrue(c.mixed)
        self.assertEqual({t for t in c.types},
                         {D.ModType.UE4SS_LUA, D.ModType.UE4SS_CPP})
        # the dll is fake (no MZ header) — still flagged, with a warning
        self.assertFalse(c.flags["windows_dll_present"])
        self.assertTrue(any("without MZ header" in ln
                            for ln in c.evidence.get("ue4ss-cpp", [])))

    def test_real_dll_content_preserved_too(self):
        manifest = self.install([
            ("MyMod/Scripts/main.lua", MAIN_LUA),
            ("MyMod/bin/real.dll", DLL_PAYLOAD)])
        with open(os.path.join(self.env.mod_dir(), "bin", "real.dll"),
                  "rb") as f:
            self.assertEqual(f.read(), DLL_PAYLOAD)


# --------------------------------------------------------------------------
# 3. UE4SS downloaded as a "mod" archive (installer side)
# --------------------------------------------------------------------------

class TestUE4SSRuntimeArchive(ZipSafetyTestBase):
    def make_runtime_zip(self, path):
        make_zip(path, [
            ("UE4SS.dll", DLL_PAYLOAD),
            ("dwmapi.dll", DLL_PAYLOAD),
            ("UE4SS-settings.ini",
             b"[Engine]\nGraphicsAPI = dx11\nConsoleEnabled = 1\n"),
            ("mods.txt", b"; sandbox : 1\n")])
        return path

    def test_installer_refuses_ue4ss_runtime_zip(self):
        self.make_runtime_zip(self.env.archive)
        before = snapshot(self.env.roots)
        with self.assertRaises(UnsafeArchiveError) as cm:
            self.env.installer().install(
                self.env.archive, self.env.meta(name="UE4SS"), TITLE)
        msg = str(cm.exception)
        self.assertIn("UE4SS runtime", msg)
        self.assertIn("PS5 runtime layer", msg)
        self.assertIn("do not", msg)
        self.assertEqual(snapshot(self.env.roots), before)
        self.assertEqual(os.listdir(os.path.join(self.env.state, "journal")),
                         [])

    def test_ue4ss_runtime_case_insensitive_names(self):
        make_zip(self.env.archive, [
            ("ue4ss.dll", DLL_PAYLOAD),
            ("DWMAPI.DLL", DLL_PAYLOAD)])
        with self.assertRaises(UnsafeArchiveError) as cm:
            self.env.installer().install(
                self.env.archive, self.env.meta(name="UE4SS"), TITLE)
        self.assertIn("UE4SS runtime", str(cm.exception))

    def test_runtime_dll_in_subdirectory_not_mistaken(self):
        # UE4SS.dll NOT at the archive root (and nothing else at root):
        # ordinary (bad) mod content, rejected for the normal reason (no
        # Scripts/main.lua), without claiming it is the runtime itself
        make_zip(self.env.archive, [
            ("Binaries/UE4SS.dll", DLL_PAYLOAD),
            ("Binaries/dwmapi.dll", DLL_PAYLOAD)])
        with self.assertRaises(UnsafeArchiveError) as cm:
            self.env.installer().install(
                self.env.archive, self.env.meta(), TITLE)
        msg = str(cm.exception)
        self.assertIn("no Scripts/main.lua", msg)
        self.assertNotIn("PS5 runtime layer", msg)

    def test_full_ue4ss_zip_with_mods_tree_installs_mod_only(self):
        # a complete UE4SS distribution WITH a Mods/ tree is not the pure
        # runtime case: the bundled mod installs; the runtime dlls outside
        # the mod root are recorded as skipped, never installed
        make_zip(self.env.archive, [
            ("UE4SS.dll", DLL_PAYLOAD),
            ("dwmapi.dll", DLL_PAYLOAD),
            ("UE4SS-settings.ini", b"[Engine]\nGraphicsAPI = dx11\n"),
            ("Mods/BundledMod/Scripts/main.lua", MAIN_LUA)])
        manifest = self.env.installer().install(
            self.env.archive, self.env.meta(name="Bundled Mod"), TITLE)
        md = os.path.join(self.env.roots, TITLE, "ue4ss", "Mods",
                          "Bundled_Mod")
        self.assertTrue(os.path.exists(
            os.path.join(md, "Scripts", "main.lua")))
        self.assertEqual(manifest["skipped_outside_mod_root"],
                         ["UE4SS-settings.ini", "UE4SS.dll", "dwmapi.dll"])
        self.assertFalse(os.path.exists(os.path.join(
            self.env.roots, TITLE, "ue4ss", "Mods", "Bundled_Mod",
            "UE4SS.dll")))


if __name__ == "__main__":
    unittest.main(verbosity=2)
