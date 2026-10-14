#!/usr/bin/env python3
"""modbridge installer core — Nexus-mod zip -> UE4SS Lua mod install.

Mac-only host tool (stdlib only, no pip dependencies). It never contacts a
console: every path it touches is a parameter, defaulting to the documented
console layout so the same core can later run console-side:

    <roots>/<TITLE_ID>/ue4ss/Mods/<ModName>/Scripts/main.lua   (mod files)
    <roots>/<TITLE_ID>/ue4ss/Mods/<ModName>/enabled.txt        (enable flag)
    <roots>/<TITLE_ID>/ue4ss/Mods/mods.txt                     (UE4SS mods list)
    <roots>/<TITLE_ID>/loadlist.txt                            (modforge TSV)
    <plugins>/ploader.ini                                      ([TITLE] prx=2)
    <roots>/<TITLE_ID>/manifests/<ModName>.json                (per-mod manifest)

The UE4SS runtime (see its UE4SSProgram.cpp start_mods + Platform/
NativeModSource.cpp) starts a mod when EITHER holds:

    - ue4ss/Mods/mods.txt carries an enabled row "Name : 1"
      (all spaces/tabs are stripped at parse time; ';' lines are comments;
      load order = row order, mods.txt rows start BEFORE marker mods), or
    - ue4ss/Mods/<Name>/enabled.txt exists (empty marker file; also enables
      a mods.txt-disabled mod).

The installer always writes BOTH (row + marker): the row pins the mod into
the load order, the marker is the belt-and-braces enable flag.  When the
archive itself ships an exact-case enabled.txt at the mod root, that file is
installed byte-for-byte from the archive and no second marker is written.

Pipeline (journal-first, transactional):

    validate metadata/title -> UE4SS-runtime refusal check (an archive that
    IS the UE4SS runtime itself — dwmapi.dll / UE4SS.dll at the archive
    root or ue4ss/UE4SS.dll, with no mods beyond UE4SS's own defaults —
    installs NOTHING: UE4SS is provided by the PS5 runtime layer; a mod
    that merely BUNDLES those runtime files installs only its mod files
    and lists the runtime files as skipped in the manifest) ->
    safe-inspect zip (traversal/symlink/size/
    encryption/duplicate-entry guards) -> case-conflict check (within the
    mod's file list and across all installed mods' file lists) ->
    compute layout + planned config content -> write
    journal BEFORE any target file operation -> execute journaled actions
    (mkdir / write-with-backup) -> verify every installed file's sha256
    equals the archive original -> write per-mod manifest -> mark journal
    committed -> clean up journal + backups.

    Any failure rolls back to the pre-install state.  On startup, a leftover
    journal is completed (phase == committed -> drop backups + journal) or
    rolled back (every earlier phase).  Reuses the proven concepts of the
    worker-7 modinst.c slice: journal phases, atomic tmp+fsync+rename writes,
    backups for rollback, ownership guard, crash-point injection for tests.

Exit codes: 0 ok, 1 internal/io, 2 bad request (metadata / unsafe or
unsupported archive / unrecognized shared config), 3 verification failure,
4 rollback incomplete.
"""

from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import os
import re
import shutil
import stat as statmod
import sys
import time
import uuid
import zipfile
from typing import Any, Dict, List, Optional, Tuple

SCHEMA_MANIFEST = "modbridge-manifest/1"
SCHEMA_JOURNAL = "modbridge-journal/1"

# Safety limits (zip-bomb / resource guards).
MAX_ENTRIES = 4096
MAX_TOTAL_UNCOMPRESSED = 1 << 30  # 1 GiB across all members
MAX_MEMBER_SIZE = 1 << 28  # 256 MiB per member
MAX_PATH_LEN = 1024  # normalized archive member path
MAX_COMPONENT_LEN = 255  # single path component
MAX_DEST_LEN = 4096  # absolute install path

DEFAULT_PLOADER_PRX = "modbridge_supplier_composition.prx"
DEFAULT_PLOADER_MODE = "2"

TITLE_ID_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,31}$")
LOADLIST_HEADER = "modforge-loadlist\t1"

# UE4SS mod-list / enable-flag names (exact case the runtime looks for).
MODS_TXT_NAME = "mods.txt"           # <ue4ss>/Mods/mods.txt
ENABLED_MARKER_NAME = "enabled.txt"  # <ue4ss>/Mods/<ModName>/enabled.txt
ENABLED_VALUES = ("0", "1")

READ_CHUNK = 1 << 20

EX_OK = 0
EX_IO = 1
EX_BAD = 2
EX_VERIFY = 3
EX_ROLLBACK = 4


# --------------------------------------------------------------------------
# errors
# --------------------------------------------------------------------------

class InstallError(Exception):
    """Base class; exit code 2 unless overridden by exit_code."""


class UnsupportedArchiveFormat(InstallError):
    def __init__(self, fmt: str, detail: str):
        super().__init__(
            "unsupported archive format: %s — %s" % (fmt, detail))
        self.fmt = fmt


class UnsafeArchiveError(InstallError):
    def __init__(self, reasons: List[str]):
        super().__init__(
            "unsafe archive rejected (%d violation(s)): %s"
            % (len(reasons), "; ".join(reasons)))
        self.reasons = reasons


class ArchiveCorruptError(InstallError):
    pass


class MetadataError(InstallError):
    pass


class ConfigFormatError(InstallError):
    pass


class OwnershipError(InstallError):
    pass


class VerificationError(InstallError):
    def __init__(self, mismatches: List[str]):
        super().__init__(
            "install verification failed (%d mismatch(es)): %s"
            % (len(mismatches), "; ".join(mismatches)))
        self.mismatches = mismatches


class RollbackError(InstallError):
    exit_code = EX_ROLLBACK


class CaseConflictError(InstallError):
    """Case-sensitivity conflict between file paths (exit code 2).

    The Mac host disk is case-INsensitive by default while the PS5 target
    is case-sensitive: paths that differ only in case collide into one
    file/directory on the host (one silently shadows the other) but are
    distinct on the console, where the runtime then looks in the wrong
    one.  Detected in code BEFORE any file is written; the installer
    never renames mod files to "fix" a case problem.
    """


# --------------------------------------------------------------------------
# small helpers
# --------------------------------------------------------------------------

def utc_now_iso() -> str:
    return datetime.datetime.now(
        datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while True:
            chunk = f.read(READ_CHUNK)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()


def fsync_dir(path: str) -> None:
    """Best-effort directory fsync (durability of renames/unlinks)."""
    try:
        fd = os.open(path, os.O_RDONLY)
    except OSError:
        return
    try:
        os.fsync(fd)
    except OSError:
        pass
    finally:
        os.close(fd)


def atomic_write_bytes(path: str, data: bytes, mode: int = 0o644) -> None:
    """Durable write: tmp file + fsync + rename + fsync dir (as modinst.c)."""
    tmp = path + ".tmp-modbridge"
    fd = os.open(tmp, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, mode)
    try:
        with os.fdopen(fd, "wb") as f:
            f.write(data)
            f.flush()
            os.fsync(f.fileno())
    except BaseException:
        try:
            os.unlink(tmp)
        except OSError:
            pass
        raise
    os.replace(tmp, path)
    fsync_dir(os.path.dirname(path) or ".")


def sanitize_component(name: str, max_len: int = 64, field: str = "name") -> str:
    """Make an arbitrary mod name safe as one filesystem component."""
    if not isinstance(name, str) or not name.strip():
        raise MetadataError("metadata %s must be a non-empty string" % field)
    cleaned = re.sub(r"[^A-Za-z0-9._-]+", "_", name.strip())
    cleaned = cleaned.strip("._")
    if not cleaned:
        cleaned = "mod"
    if cleaned in (".", ".."):
        cleaned = "mod"
    if len(cleaned) > max_len:
        digest = sha256_bytes(name.encode("utf-8"))[:8]
        cleaned = cleaned[: max_len - 9] + "-" + digest
    return cleaned


# --------------------------------------------------------------------------
# case-sensitivity detection (Mac host is case-insensitive, PS5 is not)
# --------------------------------------------------------------------------

# folders the UE4SS runtime expects with EXACT case (never renamed by us)
RUNTIME_EXACT_CASE_DIRS = ("Scripts", "Mods", "Config")


def detect_caseConflicts(paths: List[str]) -> List[Tuple[str, str]]:
    """Return path pairs that differ only in case — pure code, no filesystem.

    The host Mac disk is case-insensitive by default, so
    os.path.exists("Scripts/main.lua") is True even when the real name is
    "scripts/main.lua"; filesystem comparison CANNOT detect the problem.
    This function compares the given strings directly.

    Two paths conflict when they agree case-insensitively on any shared
    path prefix (including the full path):

      - whole-path variants:  "Scripts/main.lua" vs "scripts/main.lua"
      - directory variants:   "Scripts/main.lua" vs "scripts/util.lua"
        (the "Scripts" component itself collides case-insensitively; the
        two files would land in ONE directory on the host but in TWO
        different directories on the case-sensitive PS5 target)

    Identical duplicates (the same string twice) are NOT case conflicts.
    Backslashes are normalized to '/' first (Windows-created lists).
    Returns a sorted list of (a, b) pairs with a < b.
    """
    # lowercased prefix -> {exact spelling -> set of paths carrying it}
    owners: Dict[str, Dict[str, set]] = {}
    for p in paths:
        norm = str(p).replace("\\", "/").strip().lstrip("/")
        if not norm:
            continue
        parts = norm.split("/")
        for i in range(1, len(parts) + 1):
            prefix = "/".join(parts[:i])
            owners.setdefault(prefix.lower(), {}) \
                  .setdefault(prefix, set()).add(norm)
    conflicts: set = set()
    for by_spelling in owners.values():
        spellings = sorted(by_spelling)
        for i in range(len(spellings)):
            for j in range(i + 1, len(spellings)):
                for a in by_spelling[spellings[i]]:
                    for b in by_spelling[spellings[j]]:
                        conflicts.add((min(a, b), max(a, b)))
    return sorted(conflicts)


def _runtime_case_notes(pairs: List[Tuple[str, str]]) -> List[str]:
    """Explain case conflicts that break runtime folder expectations.

    If a conflicting component is a case variant of a folder the runtime
    looks for with exact case (Scripts/, Mods/, Config/), this is reported
    as a runtime/config issue in the mod package.  The installer NEVER
    renames mod files to paper over it.
    """
    notes: List[str] = []
    seen: set = set()

    def add(note: str) -> None:
        if note not in seen:
            seen.add(note)
            notes.append(note)

    for a, b in pairs:
        ca, cb = a.replace("\\", "/").split("/"), b.replace("\\", "/").split("/")
        for i in range(min(len(ca), len(cb))):
            if ca[i] == cb[i]:
                continue
            low = ca[i].lower()
            if low != cb[i].lower():
                continue  # not a case variant of the same name
            if low in (d.lower() for d in RUNTIME_EXACT_CASE_DIRS):
                canonical = next(d for d in RUNTIME_EXACT_CASE_DIRS
                                 if d.lower() == low)
                add(
                    "runtime/config issue: the runtime expects %r with "
                    "exact case; archive carries %r vs %r — fix the mod "
                    "package or the runtime config; the installer never "
                    "renames mod files" % (canonical + "/", ca[i], cb[i]))
        if "scripts/main.lua" in (a.lower(), b.lower()):
            add(
                "runtime/config issue: the UE4SS Lua entrypoint is "
                "'Scripts/main.lua' with exact case; a case-variant copy "
                "will not load on the case-sensitive PS5 target — mod "
                "files are never renamed automatically")
        for p, q in ((a, b), (b, a)):
            if p.lower() == ENABLED_MARKER_NAME and p != ENABLED_MARKER_NAME:
                add(
                    "runtime/config issue: UE4SS enables a mod via the "
                    "exact-case 'enabled.txt' marker in the mod directory; "
                    "archive carries %r alongside %r — the case-variant "
                    "will not enable on the case-sensitive PS5 target"
                    % (p, q))
    return notes


def _rel_paths_from_manifest(manifest: Dict[str, Any],
                             mod_dir_name: str) -> List[str]:
    """Extract mod-relative file paths ('Scripts/main.lua', ...) from an
    installed mod's manifest (install_rel = ue4ss/Mods/<mod>/<rel>)."""
    prefix = "ue4ss/Mods/%s/" % mod_dir_name
    out: List[str] = []
    for f in manifest.get("files", []):
        rel = f.get("install_rel", "")
        if isinstance(rel, str) and rel.startswith(prefix):
            out.append(rel[len(prefix):])
    return out


def _format_case_conflicts(mod_dir_name: str,
                           findings: Dict[str, Any]) -> str:
    """Human-readable CaseConflictError message (never suggests renaming)."""
    lines: List[str] = []
    within = [tuple(f["pair"]) for f in findings["within_mod"]]
    across = findings["across_mods"]
    lines.append(
        "case conflict rejected (nothing installed): the host Mac disk is "
        "case-insensitive but the PS5 target is case-sensitive, so these "
        "paths would collide into one file on the host yet behave "
        "differently on the console")
    for a, b in within:
        lines.append("within mod %r: %r vs %r" % (mod_dir_name, a, b))
    for f in across:
        lines.append("across mods %r and %r: %r vs %r"
                     % (mod_dir_name, f["mod"], f["pair"][0], f["pair"][1]))
    lines.extend(_runtime_case_notes(within + [
        tuple(f["pair"]) for f in across]))
    lines.append(
        "fix the mod package (or runtime config) at the source; the "
        "installer never renames mod files")
    return "; ".join(lines)


# --------------------------------------------------------------------------
# archive detection + safe zip inspection
# --------------------------------------------------------------------------

ZIP_MAGICS = (b"PK\x03\x04", b"PK\x05\x06", b"PK\x07\x08")
SEVENZ_MAGIC = b"7z\xbc\xaf\x27\x1c"
RAR_MAGICS = (b"Rar!\x1a\x07\x00", b"Rar!\x1a\x07\x01\x00")


def detect_archive_format(path: str) -> Optional[str]:
    try:
        with open(path, "rb") as f:
            head = f.read(16)
    except OSError as e:
        raise InstallError("cannot read archive %s: %s" % (path, e))
    if any(head.startswith(m) for m in ZIP_MAGICS):
        return "zip"
    if head.startswith(SEVENZ_MAGIC):
        return "7z"
    if any(head.startswith(m) for m in RAR_MAGICS):
        return "rar"
    return None


def _unsupported_detail(fmt: str) -> str:
    if fmt == "7z":
        return ("this stdlib-only core cannot extract 7-Zip archives; convert "
                "host-side to .zip first (e.g. `7zz x mod.7z -o out && cd out "
                "&& zip -r ../mod.zip .` or the py7zr package) and submit the "
                ".zip")
    if fmt == "rar":
        return ("this stdlib-only core cannot extract RAR archives; extract "
                "host-side with `unar`/`unrar`/`7zz` and repackage as .zip, "
                "then submit the .zip")
    return "no zip/7z/rar magic found; provide a valid archive"


def normalize_arcname(name: str) -> str:
    """Validate + normalize one archive member name.

    Rejects: absolute paths, drive letters, '..' components, empty pieces of
    interest, NUL/control characters, over-long names/components.  Backslashes
    are treated as separators (Windows-created zips) and normalized to '/'.
    Returns the canonical '/'-joined relative path.
    """
    if not name:
        raise ValueError("empty member name")
    if "\x00" in name:
        raise ValueError("NUL byte in member name")
    if any(ord(c) < 0x20 for c in name):
        raise ValueError("control character in member name")
    name = name.replace("\\", "/")
    if name.startswith("/"):
        raise ValueError("absolute member path")
    parts = name.split("/")
    out: List[str] = []
    for p in parts:
        if p in ("", "."):
            continue
        if p == "..":
            raise ValueError("'..' path component (path traversal)")
        if ":" in p:
            raise ValueError("':' in path component %r (drive letter / "
                             "alternate stream)" % p)
        if len(p) > MAX_COMPONENT_LEN:
            raise ValueError("path component longer than %d: %r"
                             % (MAX_COMPONENT_LEN, p[:64]))
        out.append(p)
    norm = "/".join(out)
    if not norm:
        raise ValueError("member name reduces to nothing")
    if len(norm) > MAX_PATH_LEN:
        raise ValueError("member path longer than %d" % MAX_PATH_LEN)
    return norm


class ArchiveEntry(object):
    __slots__ = ("orig_name", "norm", "is_dir", "size", "sha256", "unix_mode",
                 "encrypted")

    def __init__(self, orig_name: str, norm: str, is_dir: bool, size: int,
                 sha256_hex: Optional[str], unix_mode: int,
                 encrypted: bool) -> None:
        self.orig_name = orig_name
        self.norm = norm
        self.is_dir = is_dir
        self.size = size
        self.sha256 = sha256_hex
        self.unix_mode = unix_mode
        self.encrypted = encrypted


class SafeArchive(object):
    """Validated zip: safe names, regular files only, hashes precomputed."""

    def __init__(self, path: str) -> None:
        fmt = detect_archive_format(path)
        if fmt == "7z":
            raise UnsupportedArchiveFormat("7z", _unsupported_detail("7z"))
        if fmt == "rar":
            raise UnsupportedArchiveFormat("RAR", _unsupported_detail("rar"))
        if fmt is None:
            raise UnsupportedArchiveFormat(
                "unknown", "file is not a zip, 7z or rar archive")
        self.path = os.path.abspath(path)
        self.archive_sha256 = sha256_file(path)
        self.entries: List[ArchiveEntry] = []
        self._by_norm: Dict[str, ArchiveEntry] = {}
        try:
            self.zf = zipfile.ZipFile(path, "r")
        except (zipfile.BadZipFile, OSError) as e:
            raise ArchiveCorruptError("cannot open zip %s: %s" % (path, e))
        try:
            self._inspect()
        except zipfile.BadZipFile as e:
            raise ArchiveCorruptError("corrupt zip %s: %s" % (path, e))
        except BaseException:
            self.zf.close()
            raise

    def _inspect(self) -> None:
        infos = self.zf.infolist()
        if len(infos) > MAX_ENTRIES:
            raise UnsafeArchiveError(
                ["archive has %d entries (limit %d)" % (len(infos),
                                                        MAX_ENTRIES)])
        reasons: List[str] = []
        total = 0
        seen_norms: Dict[str, str] = {}  # normalized name -> first orig name
        for zi in infos:
            try:
                norm = normalize_arcname(zi.filename)
            except ValueError as e:
                reasons.append("%r: %s" % (zi.filename, e))
                continue
            unix_mode = (zi.external_attr >> 16) & 0xFFFF
            filetype = statmod.S_IFMT(unix_mode)
            encrypted = bool(zi.flag_bits & 0x1)
            if encrypted:
                reasons.append("%r: encrypted entries are not supported"
                               % zi.filename)
                continue
            if filetype not in (0, statmod.S_IFREG, statmod.S_IFDIR):
                kind = {
                    statmod.S_IFLNK: "symlink",
                    statmod.S_IFCHR: "character device",
                    statmod.S_IFBLK: "block device",
                    statmod.S_IFIFO: "fifo",
                    statmod.S_IFSOCK: "socket",
                }.get(filetype, "non-regular file (mode %o)" % unix_mode)
                reasons.append("%r: %s entry rejected" % (zi.filename, kind))
                continue
            # duplicate detection: two entries whose names normalize to the
            # same path (exact repeats, "./x" vs "x", "a\\b" vs "a/b") would
            # silently overwrite each other on extract (last one wins) —
            # reject instead of guessing which copy was meant.
            prev = seen_norms.get(norm)
            if prev is not None:
                reasons.append(
                    "duplicate archive entry after normalization: %r and %r "
                    "both map to %r (on extract the last one would silently "
                    "win)" % (prev, zi.filename, norm))
                continue
            seen_norms[norm] = zi.filename
            is_dir = zi.is_dir() or filetype == statmod.S_IFDIR
            size = zi.file_size
            if not is_dir:
                if size > MAX_MEMBER_SIZE:
                    reasons.append("%r: member size %d exceeds limit %d"
                                   % (zi.filename, size, MAX_MEMBER_SIZE))
                    continue
                total += size
                if total > MAX_TOTAL_UNCOMPRESSED:
                    reasons.append(
                        "total uncompressed size exceeds %d (zip bomb guard)"
                        % MAX_TOTAL_UNCOMPRESSED)
                    continue
            self.entries.append(ArchiveEntry(
                zi.filename, norm, is_dir, size, None, unix_mode, encrypted))
        if reasons:
            raise UnsafeArchiveError(reasons)
        # hash every file member by streaming it (zipfile checks CRC at EOF)
        for e in self.entries:
            if e.is_dir:
                continue
            h = hashlib.sha256()
            try:
                with self.zf.open(e.orig_name) as member:
                    while True:
                        chunk = member.read(READ_CHUNK)
                        if not chunk:
                            break
                        h.update(chunk)
            except (zipfile.BadZipFile, OSError, RuntimeError) as ex:
                raise ArchiveCorruptError(
                    "cannot read member %r: %s" % (e.orig_name, ex))
            e.sha256 = h.hexdigest()
            self._by_norm[e.norm] = e

    def close(self) -> None:
        self.zf.close()

    def __enter__(self) -> "SafeArchive":
        return self

    def __exit__(self, *exc: Any) -> None:
        self.close()

    def read(self, norm: str) -> bytes:
        e = self._by_norm[norm]
        return self.zf.read(e.orig_name)

    def find_mod_root(self,
                      exclude_mod_names: Optional[set] = None) -> Optional[str]:
        """Locate the UE4SS Lua mod root (directory containing
        Scripts/main.lua).  Prefers the shortest (most top-level) match.

        exclude_mod_names: lower-case mod directory names whose roots must
        NOT be chosen (used to skip UE4SS's own default mods —
        ConsoleEnablerMod, LineTraceMod, ... — when the archive bundles the
        UE4SS runtime alongside an actual mod)."""
        cands = [e.norm for e in self.entries
                 if not e.is_dir and
                 (e.norm == "Scripts/main.lua" or
                  e.norm.endswith("/Scripts/main.lua"))]
        if exclude_mod_names:
            def _mod_name_of(cand: str) -> str:
                prefix = cand[: -len("Scripts/main.lua")]
                if not prefix:
                    return ""          # mod root IS the archive root
                return prefix.rstrip("/").rsplit("/", 1)[-1].lower()
            cands = [c for c in cands
                     if _mod_name_of(c) not in exclude_mod_names]
        if not cands:
            return None
        cands.sort(key=lambda n: (len(n), n))
        best = cands[0]
        return "" if best == "Scripts/main.lua" else \
            best[: -len("Scripts/main.lua")]

    def lowercase_mainlua_candidates(self) -> List[str]:
        return [e.norm for e in self.entries
                if not e.is_dir and e.norm.lower().endswith(
                    "scripts/main.lua")]


# UE4SS runtime detection (Mac host-side; mirrors detector.py rules).
# Real UE4SS 3.x release zips ship flat (UE4SS.dll + dwmapi.dll +
# UE4SS-settings.ini + Mods/<defaults>/ at the archive root); installed-
# style layouts nest the runtime tree under ue4ss/ (dwmapi.dll at the
# root + ue4ss/UE4SS.dll + ue4ss/Mods/<defaults>/).  Any signal means the
# UE4SS runtime is inside the archive; the PS5 runtime layer provides
# UE4SS itself, so no part of such a distribution is installed as a mod.
UE4SS_RUNTIME_ROOT_DLLS = ("ue4ss.dll", "dwmapi.dll")
UE4SS_NESTED_RUNTIME_DLL = "ue4ss/ue4ss.dll"
# mods shipped inside the UE4SS runtime distribution itself (Mods/ tree of
# UE4SS_v3.0.1 plus the 2.x-era BPML_CXXModifier); 'shared' carries the
# bundled UEHelpers/jsbProfiler libraries, 'keybinds' ships with 3.x.
UE4SS_DEFAULT_MOD_NAMES = frozenset({
    "actordumpermod", "bpml_genericfunctions", "bpmodloadermod",
    "bpml_cxxmodifier", "cheatmanagerenablermod", "consolecommandsmod",
    "consoleenablermod", "jsbluaprofilermod", "keybinds", "linetracemod",
    "splitscreenmod", "shared",
})
_MODS_SUBDIR_RE = re.compile(r"(?:^|/)mods/([^/]+)/", re.IGNORECASE)
_MODS_LIST_FILE_RE = re.compile(r"(?:^|/)mods/mods\.txt$", re.IGNORECASE)


def ue4ss_runtime_signal_files(entries: List[ArchiveEntry]) -> List[str]:
    """Normalized archive paths that signal the UE4SS runtime itself:
    dwmapi.dll / UE4SS.dll at the archive root, or ue4ss/UE4SS.dll."""
    out = set()
    for e in entries:
        if e.is_dir:
            continue
        low = e.norm.lower()
        if "/" not in low and low in UE4SS_RUNTIME_ROOT_DLLS:
            out.add(e.norm)
        elif low == UE4SS_NESTED_RUNTIME_DLL:
            out.add(e.norm)
    return sorted(out)


def _mod_dir_names(entries: List[ArchiveEntry]) -> set:
    """Lower-cased mod directory names found anywhere in the archive.

    A directory under any .../Mods/ tree ("Mods/MyMod/..." or
    "ue4ss/Mods/MyMod/..." -> "mymod"; Mods/mods.txt is a file, never a
    mod name) or a top-level loose mod ("MyMod/Scripts/..." -> "mymod").
    """
    names = set()
    for e in entries:
        low = e.norm.lower()
        m = _MODS_SUBDIR_RE.search(low)
        if m:
            names.add(m.group(1))
            continue
        parts = low.split("/")
        if len(parts) >= 2 and parts[1] == "scripts" and \
                parts[0] not in ("mods", "ue4ss"):
            names.add(parts[0])
    return names


def ue4ss_runtime_distribution_norms(entries: List[ArchiveEntry],
                                     keep_mods: set) -> List[str]:
    """Normalized paths belonging to the UE4SS runtime distribution itself.

    Everything the UE4SS release ships EXCEPT the payload of mods named in
    keep_mods (lower-case): the signal DLLs, UE4SS-settings.ini, mods.txt /
    .../Mods/mods.txt, the ue4ss/ runtime tree (overrides/, ...) and every
    .../Mods/<default>/ tree.  Used when a MOD bundles the UE4SS runtime
    for convenience: these files are skipped (never installed) and listed
    in the manifest.
    """
    keep = set(keep_mods or ())
    out = set()
    for e in entries:
        if e.is_dir:
            continue
        low = e.norm.lower()
        if "/" not in low and low in UE4SS_RUNTIME_ROOT_DLLS + (
                "ue4ss-settings.ini", "mods.txt"):
            out.add(e.norm)
            continue
        if low == UE4SS_NESTED_RUNTIME_DLL or \
                low == "ue4ss/ue4ss-settings.ini":
            out.add(e.norm)
            continue
        if _MODS_LIST_FILE_RE.search(low):
            out.add(e.norm)
            continue
        m = _MODS_SUBDIR_RE.search(low)
        if m:
            if m.group(1) not in keep:
                out.add(e.norm)      # UE4SS default mod (or foreign default)
            continue
        if low.startswith("ue4ss/"):
            rest = low[len("ue4ss/"):]
            if rest.startswith("mods/"):
                name = rest[len("mods/"):].split("/", 1)[0]
                if name in keep:
                    continue          # the actual mod living under ue4ss/Mods/
            out.add(e.norm)          # ue4ss/ runtime tree (overrides/, ...)
    return sorted(out)


# --------------------------------------------------------------------------
# metadata
# --------------------------------------------------------------------------

def load_metadata(path: str) -> Dict[str, Any]:
    try:
        with open(path, "r", encoding="utf-8") as f:
            doc = json.load(f)
    except (OSError, ValueError) as e:
        raise MetadataError("cannot read metadata %s: %s" % (path, e))
    if not isinstance(doc, dict):
        raise MetadataError("metadata must be a JSON object")
    return doc


def validate_metadata(md: Dict[str, Any]) -> Dict[str, Any]:
    out: Dict[str, Any] = {}
    name = md.get("name")
    if not isinstance(name, str) or not name.strip():
        raise MetadataError("metadata 'name' must be a non-empty string")
    version = md.get("version")
    if not isinstance(version, (str, int, float)) or \
            not str(version).strip():
        raise MetadataError("metadata 'version' must be non-empty")
    out["name"] = name.strip()
    out["version"] = str(version).strip()
    url = md.get("source_url")
    if url is not None and not isinstance(url, str):
        raise MetadataError("metadata 'source_url' must be a string")
    out["source_url"] = url
    dl = md.get("downloaded_at")
    if dl is not None and not isinstance(dl, str):
        raise MetadataError("metadata 'downloaded_at' must be a string")
    out["downloaded_at"] = dl
    fl = md.get("file_list")
    if fl is not None:
        if not isinstance(fl, list) or \
                not all(isinstance(x, str) for x in fl):
            raise MetadataError("metadata 'file_list' must be a list of "
                                "strings")
    out["file_list"] = fl
    prx = md.get("ploader_prx")
    if prx is not None:
        if not isinstance(prx, dict):
            raise MetadataError("metadata 'ploader_prx' must be an object "
                                "{name, mode}")
        pname = prx.get("name", DEFAULT_PLOADER_PRX)
        pmode = str(prx.get("mode", DEFAULT_PLOADER_MODE))
        if not isinstance(pname, str) or not re.fullmatch(
                r"[A-Za-z0-9._+-]{1,96}\.prx", pname):
            raise MetadataError("metadata 'ploader_prx.name' must be a "
                                "simple '<name>.prx' string")
        if pmode not in ("0", "1", "2"):
            raise MetadataError("metadata 'ploader_prx.mode' must be one of "
                                "0/1/2 (0=off, 2=inject)")
        prx = {"name": pname, "mode": pmode}
    out["ploader_prx"] = prx
    settings = md.get("ue4ss_settings")
    if settings is not None:
        if not isinstance(settings, dict) or \
                not all(isinstance(v, dict) for v in settings.values()):
            raise MetadataError("metadata 'ue4ss_settings' must be an object "
                                "of section -> {key: value}")
        for sec, kv in settings.items():
            if not isinstance(sec, str) or not sec.strip() or \
                    sec.startswith("["):
                raise MetadataError("ue4ss_settings section %r invalid" % sec)
            for k, v in kv.items():
                if not isinstance(k, str) or not k.strip() or "\n" in k or \
                        "=" in k:
                    raise MetadataError("ue4ss_settings key %r invalid" % k)
                if not isinstance(v, str) or "\n" in v or "\r" in v:
                    raise MetadataError("ue4ss_settings value for %r must be "
                                        "a single-line string" % k)
    out["ue4ss_settings"] = settings
    return out


# --------------------------------------------------------------------------
# layout
# --------------------------------------------------------------------------

class Layout(object):
    def __init__(self, roots_base: str, plugins_dir: str, state_dir: str,
                 title_id: str) -> None:
        self.roots_base = os.path.abspath(roots_base)
        self.plugins_dir = os.path.abspath(plugins_dir)
        self.state_dir = os.path.abspath(state_dir)
        self.title_id = title_id
        self.title_root = os.path.join(self.roots_base, title_id)
        self.ue4ss_dir = os.path.join(self.title_root, "ue4ss")
        self.mods_dir = os.path.join(self.ue4ss_dir, "Mods")
        self.mods_txt_path = os.path.join(self.mods_dir, MODS_TXT_NAME)
        self.loadlist_path = os.path.join(self.title_root, "loadlist.txt")
        self.manifests_dir = os.path.join(self.title_root, "manifests")
        self.overrides_dir = os.path.join(self.ue4ss_dir, "overrides")
        self.ploader_path = os.path.join(self.plugins_dir, "ploader.ini")
        self.journal_dir = os.path.join(self.state_dir, "journal")
        self.backups_root = os.path.join(self.state_dir, "backups")

    def mod_dir(self, mod_dir_name: str) -> str:
        return os.path.join(self.mods_dir, mod_dir_name)

    def enabled_marker(self, mod_dir_name: str) -> str:
        return os.path.join(self.mod_dir(mod_dir_name), ENABLED_MARKER_NAME)

    def manifest_path(self, mod_dir_name: str) -> str:
        return os.path.join(self.manifests_dir, mod_dir_name + ".json")

    def overrides_path(self, mod_dir_name: str) -> str:
        return os.path.join(self.overrides_dir, mod_dir_name + ".ini")


# --------------------------------------------------------------------------
# config editors (pure functions; verified by readback after write)
# --------------------------------------------------------------------------

def parse_loadlist(text: str) -> List[List[str]]:
    """Parse modforge-loadlist TSV into records; raises ConfigFormatError."""
    lines = text.splitlines()
    if not lines or lines[0] != LOADLIST_HEADER:
        raise ConfigFormatError(
            "loadlist.txt: missing %r header" % LOADLIST_HEADER)
    recs: List[List[str]] = []
    for i, line in enumerate(lines[1:], start=2):
        if not line:
            continue
        fields = line.split("\t")
        recs.append(fields)
    return recs


def _newline_of(text: str) -> str:
    """Preserve the file's dominant line ending (byte-level safety)."""
    return "\r\n" if "\r\n" in text else "\n"


def upsert_loadlist(text: str, title_id: str, mod_id: str, version: str,
                    file_rows: List[str], engine: str = "unreal") -> str:
    """Insert or replace one mod's block (mod row + its file rows)."""
    nl = _newline_of(text)
    if text.strip() == "":
        lines = [LOADLIST_HEADER, "title\t%s" % title_id,
                 "engine\t%s" % engine]
    else:
        lines = text.splitlines()
        if lines[0] != LOADLIST_HEADER:
            raise ConfigFormatError(
                "loadlist.txt: unrecognized content (expected %r header); "
                "refusing to modify" % LOADLIST_HEADER)
        titles = [l for l in lines[1:] if l.startswith("title\t")]
        if not titles:
            lines = lines[:1] + ["title\t%s" % title_id] + lines[1:]
        else:
            for t in titles:
                if t != "title\t%s" % title_id:
                    raise ConfigFormatError(
                        "loadlist.txt carries title %r but this root is %r; "
                        "refusing to modify" % (t.split("\t", 1)[1], title_id))
    block = ["mod\t%s\t%s" % (mod_id, version)] + file_rows
    out: List[str] = [LOADLIST_HEADER]  # header always first
    replaced = False
    i = 1  # header handled above
    while i < len(lines):
        line = lines[i]
        fields = line.split("\t")
        if fields[0] == "mod" and len(fields) >= 2 and fields[1] == mod_id:
            i += 1
            while i < len(lines) and lines[i].startswith("file\t"):
                i += 1
            if not replaced:
                out.extend(block)
                replaced = True
            continue
        out.append(line)
        i += 1
    if not replaced:
        out.extend(block)
    return nl.join(out) + nl


def loadlist_has_mod(text: str, mod_id: str) -> bool:
    try:
        recs = parse_loadlist(text)
    except ConfigFormatError:
        return False
    for fields in recs:
        if fields[0] == "mod" and len(fields) >= 2 and fields[1] == mod_id:
            return True
    return False


def upsert_ini_line(text: str, section: str, key: str, value: str) -> str:
    """Line-preserving INI editor: set key=value inside [section]."""
    nl = _newline_of(text)
    lines = text.splitlines() if text.strip() else []
    newline = "%s=%s" % (key, value)
    sec_idx = None
    for i, l in enumerate(lines):
        s = l.strip()
        if s.startswith("[") and s.endswith("]") and \
                s[1:-1].strip() == section:
            sec_idx = i
            break
    if sec_idx is None:
        if lines and lines[-1].strip() != "":
            lines.append("")
        lines.append("[%s]" % section)
        lines.append(newline)
    else:
        j = sec_idx + 1
        found = False
        while j < len(lines):
            s = lines[j].strip()
            if s.startswith("[") and s.endswith("]"):
                break
            if not s.startswith("#") and not s.startswith(";") and \
                    "=" in s and s.split("=", 1)[0].strip() == key:
                lines[j] = newline
                found = True
                break
            j += 1
        if not found:
            lines.insert(j, newline)
    return nl.join(lines) + nl


def ini_has_line(text: str, section: str, key: str, value: str) -> bool:
    in_sec = False
    want = "%s=%s" % (key, value)
    for l in text.splitlines():
        s = l.strip()
        if s.startswith("[") and s.endswith("]"):
            in_sec = s[1:-1].strip() == section
            continue
        if in_sec and not s.startswith(("#", ";")) and "=" in s:
            if s.replace(" ", "") == want:
                return True
    return False


def ini_has_section(text: str, section: str) -> bool:
    for l in text.splitlines():
        s = l.strip()
        if s.startswith("[") and s.endswith("]") and \
                s[1:-1].strip() == section:
            return True
    return False


def parse_mods_txt_row(line: str) -> Optional[Tuple[str, str]]:
    """Parse one ue4ss/Mods/mods.txt row -> (mod_name, "0"|"1"); None when
    the line is not an enable/disable row (comment, blank, malformed).

    Mirrors the runtime parsers (UE4SSProgram.cpp start_mods + Platform/
    NativeModSource.cpp): all spaces/tabs are stripped, ';' lines are
    comments, the row is '<name>:<0|1>' after stripping.  The native parser
    REQUIRES that exact shape (it throws on anything else), so every row the
    installer writes must round-trip through this function.
    """
    s = line.strip()
    if not s or ";" in s:
        return None
    compact = "".join(s.split())  # drop all whitespace, like the runtime
    name, sep, flag = compact.partition(":")
    if not sep or not name or flag not in ENABLED_VALUES:
        return None
    return name, flag


def upsert_mods_txt(text: str, mod_name: str, enabled: bool = True) -> str:
    """Insert or replace one mod's row in ue4ss/Mods/mods.txt.

    Row format: '<mod_name> : 1' (the canonical UE4SS spelling; the runtime
    strips the spaces).  Comments (';'), blank lines, foreign rows and the
    file's dominant line ending survive byte-for-byte; an existing row for
    the same mod (any flag) is replaced IN PLACE so its load-order position
    is kept, and duplicate rows for the same mod collapse to one.  A mod
    with no row yet is appended at the end.
    """
    nl = _newline_of(text)
    row = "%s : %s" % (mod_name, "1" if enabled else "0")
    out: List[str] = []
    replaced = False
    for line in text.splitlines():
        parsed = parse_mods_txt_row(line)
        if parsed is not None and parsed[0] == mod_name:
            if not replaced:
                out.append(row)
                replaced = True
            # drop extra duplicate rows for the same mod
        else:
            out.append(line)
    if not replaced:
        out.append(row)
    return nl.join(out) + nl


def mods_txt_has_mod(text: str, mod_name: str) -> bool:
    """True when mods.txt carries an ENABLED row ('<name> : 1') for the mod
    (same semantics the runtime applies when deciding to start it)."""
    for line in text.splitlines():
        parsed = parse_mods_txt_row(line)
        if parsed is not None and parsed[0] == mod_name and parsed[1] == "1":
            return True
    return False


def render_overrides_ini(mod_name: str, version: str,
                         settings: Dict[str, Dict[str, str]]) -> str:
    """Runtime-owned UE4SS settings overrides (never inside the mod dir)."""
    out = [
        "; modbridge runtime UE4SS-settings overrides for %r %s" % (
            mod_name, version),
        "; generated by the modbridge installer; runtime config only — "
        "mod files are never modified",
    ]
    for sec in sorted(settings):
        out.append("[%s]" % sec)
        for k in sorted(settings[sec]):
            out.append("%s = %s" % (k, settings[sec][k]))
    return "\n".join(out) + "\n"


# --------------------------------------------------------------------------
# journal
# --------------------------------------------------------------------------

class Journal(object):
    def __init__(self, path: str, doc: Dict[str, Any]) -> None:
        self.path = path
        self.doc = doc

    def save(self) -> None:
        atomic_write_bytes(self.path,
                           json.dumps(self.doc, indent=1, sort_keys=True)
                           .encode("utf-8"))

    def set_phase(self, phase: str) -> None:
        self.doc["phase"] = phase
        self.save()

    def mark(self, idx: int, **flags: Any) -> None:
        self.doc["actions"][idx].update(flags)
        self.save()

    @classmethod
    def load(cls, path: str) -> "Journal":
        with open(path, "r", encoding="utf-8") as f:
            doc = json.load(f)
        if not isinstance(doc, dict) or \
                doc.get("schema") != SCHEMA_JOURNAL:
            raise ValueError("not a %s journal" % SCHEMA_JOURNAL)
        return cls(path, doc)


def new_txid() -> str:
    return "%x-%x-%s" % (int(time.time() * 1000), os.getpid(),
                         uuid.uuid4().hex[:8])


# --------------------------------------------------------------------------
# installer
# --------------------------------------------------------------------------

class Installer(object):
    def __init__(self, roots_base: str, plugins_dir: str, state_dir: str,
                 crash_after: Optional[int] = None,
                 crash_at: Optional[str] = None) -> None:
        self.roots_base = os.path.abspath(roots_base)
        self.plugins_dir = os.path.abspath(plugins_dir)
        self.state_dir = os.path.abspath(state_dir)
        # infrastructure dirs (not install-target mutations; created before
        # any journal exists)
        for d in (self.roots_base, self.plugins_dir, self.state_dir,
                  os.path.join(self.state_dir, "journal"),
                  os.path.join(self.state_dir, "backups")):
            os.makedirs(d, exist_ok=True)
        self.crash_after = crash_after
        self.crash_at = crash_at
        self._actions_done = 0

    # ---- crash injection (test/verification hook, mirrors modinst.c) ----
    def _maybe_crash(self, point: str) -> None:
        if self.crash_at == point:
            sys.stderr.write("SIMULATED CRASH at %s\n" % point)
            sys.stderr.flush()
            os._exit(86)

    # ---- primitive ops used by actions (override points for tests) ----
    def _write_file_bytes(self, path: str, data: bytes) -> None:
        atomic_write_bytes(path, data)

    # ---- recovery ---------------------------------------------------------
    def recover(self) -> List[Dict[str, Any]]:
        """Complete or roll back every leftover journal (startup rule)."""
        reports: List[Dict[str, Any]] = []
        try:
            names = sorted(os.listdir(self.state_dir + "/journal"))
        except OSError:
            names = []
        for name in names:
            if not name.endswith(".json"):
                continue
            path = os.path.join(self.state_dir, "journal", name)
            try:
                j = Journal.load(path)
            except (OSError, ValueError) as e:
                # atomic writes mean partial journals cannot happen; a broken
                # one is externally corrupted — quarantine, never guess.
                quarantine = path + ".corrupt-%s" % uuid.uuid4().hex[:6]
                os.rename(path, quarantine)
                reports.append({"txid": name, "action": "quarantined",
                                "error": str(e)})
                continue
            txid = j.doc.get("txid", name)
            if j.doc.get("phase") == "committed":
                self._cleanup_tx(j.doc.get("backups_dir", ""),
                                 j.path)
                reports.append({"txid": txid, "action": "completed",
                                "phase": "committed"})
            else:
                errors = self._rollback(j.doc)
                self._cleanup_tx(j.doc.get("backups_dir", ""),
                                 j.path)
                reports.append({
                    "txid": txid, "action": "rolled-back",
                    "phase": j.doc.get("phase"),
                    "rollback_incomplete": errors})
            fsync_dir(os.path.join(self.state_dir, "journal"))
        return reports

    def _cleanup_tx(self, backups_dir: str, journal_path: str) -> None:
        if backups_dir and os.path.isdir(backups_dir):
            shutil.rmtree(backups_dir, ignore_errors=True)
        try:
            os.unlink(journal_path)
        except OSError:
            pass

    def _rollback(self, doc: Dict[str, Any]) -> List[str]:
        """Undo every started/done action in reverse order. Returns a list of
        things that could not be undone (empty = clean rollback)."""
        errors: List[str] = []
        for act in reversed(doc.get("actions", [])):
            path = act.get("path", "")
            try:
                if act["type"] == "mkdir":
                    if not act.get("existed") and \
                            (act.get("started") or act.get("done")) and \
                            os.path.isdir(path):
                        try:
                            os.rmdir(path)
                        except OSError:
                            if os.listdir(path):
                                errors.append(
                                    "created dir not empty, left in place: "
                                    "%s" % path)
                            else:
                                raise
                elif act["type"] == "write":
                    if act.get("existed"):
                        if act.get("backed_up"):
                            with open(act["backup"], "rb") as bf:
                                data = bf.read()
                            atomic_write_bytes(path, data)
                        elif act.get("started") or act.get("done"):
                            # existed but no backup recorded: either the file
                            # was never touched, or the journal predates the
                            # backup step; only restore when it differs from
                            # the recorded original hash.
                            try:
                                cur = sha256_file(path)
                            except OSError:
                                cur = None
                            if cur is not None and \
                                    cur != act.get("orig_sha256"):
                                errors.append(
                                    "cannot restore original bytes of %s "
                                    "(no backup)" % path)
                    else:
                        if act.get("started") or act.get("done"):
                            if os.path.lexists(path):
                                os.unlink(path)
            except OSError as e:
                errors.append("rollback failed for %s: %s" % (path, e))
        return errors

    # ---- case-conflict preflight -------------------------------------------
    def find_case_conflicts(self, title_id: str, rel_paths: List[str],
                            mod_dir_name: str) -> Dict[str, Any]:
        """Case-conflict findings for a candidate mod's file list.

        Checks IN CODE (no filesystem probing — the host disk is
        case-insensitive and cannot answer this):

          within_mod  — pairs inside the candidate's own file list
          across_mods — pairs between the candidate and every OTHER
                        installed mod's manifest file list (same
                        runtime-relative path, different casing)

        Reinstalling a mod over its own files is not a conflict (its own
        manifest is skipped), and two mods using the IDENTICAL spelling
        (e.g. both ship Scripts/main.lua) is normal UE4SS layout, not a
        conflict.
        """
        within = detect_caseConflicts(rel_paths)
        across: List[Dict[str, Any]] = []
        layout = Layout(self.roots_base, self.plugins_dir, self.state_dir,
                        title_id)
        if os.path.isdir(layout.manifests_dir):
            cand = set(rel_paths)
            for name in sorted(os.listdir(layout.manifests_dir)):
                if not name.endswith(".json"):
                    continue
                other = name[: -len(".json")]
                if other == mod_dir_name:
                    continue  # own previous version: overwrite is expected
                try:
                    with open(os.path.join(layout.manifests_dir, name),
                              "r", encoding="utf-8") as f:
                        manifest = json.load(f)
                except (OSError, ValueError):
                    continue  # unreadable foreign manifest: not our evidence
                other_rels = _rel_paths_from_manifest(manifest, other)
                other_set = set(other_rels)
                for a, b in detect_caseConflicts(list(cand) + other_rels):
                    if (a in cand and b in other_set) or \
                            (b in cand and a in other_set):
                        across.append({"mod": other, "pair": [a, b]})
        return {"within_mod": [{"pair": list(p)} for p in within],
                "across_mods": across}

    # ---- install ----------------------------------------------------------
    def install(self, archive_path: str, metadata: Dict[str, Any],
                title_id: str,
                strict_file_list: bool = False) -> Dict[str, Any]:
        self.recover()  # startup rule: resolve leftovers before a new tx

        if not isinstance(title_id, str) or not TITLE_ID_RE.fullmatch(title_id):
            raise MetadataError(
                "invalid title id %r (want e.g. PPSA33286)" % title_id)
        md = validate_metadata(metadata)
        mod_dir_name = sanitize_component(md["name"], 64, "name")
        layout = Layout(self.roots_base, self.plugins_dir, self.state_dir,
                        title_id)
        mod_dir = layout.mod_dir(mod_dir_name)
        manifest_path = layout.manifest_path(mod_dir_name)

        # ---- plan: everything is computed BEFORE the journal is written ----
        with SafeArchive(archive_path) as arc:
            # UE4SS-as-mod check FIRST: a real UE4SS 3.x release zip carries
            # its own default mods (Mods/ConsoleEnablerMod/Scripts/main.lua,
            # ...), so a mod root WOULD be found — the runtime check must
            # not depend on find_mod_root() failing.
            runtime_sigs = ue4ss_runtime_signal_files(arc.entries)
            non_default_mods: set = set()
            runtime_skip_norms: List[str] = []
            if runtime_sigs:
                non_default_mods = (_mod_dir_names(arc.entries)
                                    - UE4SS_DEFAULT_MOD_NAMES)
                if not non_default_mods:
                    raise UnsafeArchiveError([
                        "this zip is the UE4SS runtime itself (detected %s); "
                        "any bundled Mods/ tree carries only UE4SS's own "
                        "default mods" % ", ".join(runtime_sigs),
                        "UE4SS runtime + default mods are provided by the "
                        "PS5 runtime layer; do not install any part of this "
                        "archive as a mod"])
                # a MOD that bundles the UE4SS runtime: install only the mod
                # files; the runtime's own files are skipped and listed in
                # the manifest (never installed, never chosen as mod root)
                runtime_skip_norms = ue4ss_runtime_distribution_norms(
                    arc.entries, non_default_mods)
            mod_root = arc.find_mod_root(
                exclude_mod_names=(UE4SS_DEFAULT_MOD_NAMES - non_default_mods)
                if runtime_sigs else None)
            if mod_root is None:
                lower = arc.lowercase_mainlua_candidates()
                hint = (" (found %r — UE4SS requires exact 'Scripts/main.lua' "
                        "case)" % lower[:3]) if lower else ""
                raise UnsafeArchiveError(
                    ["no Scripts/main.lua in archive; not a UE4SS Lua mod%s"
                     % hint])
            planned: List[Dict[str, Any]] = []
            skipped: List[str] = []
            for e in arc.entries:
                if e.is_dir:
                    if mod_root == "" or e.norm.startswith(mod_root):
                        pass  # dir entries become mkdir actions below
                    continue
                if mod_root != "" and not e.norm.startswith(mod_root):
                    skipped.append(e.norm)
                    continue
                rel_in_mod = e.norm[len(mod_root):]
                if not rel_in_mod:
                    continue
                dest = os.path.join(mod_dir, *rel_in_mod.split("/"))
                if len(dest) > MAX_DEST_LEN:
                    raise UnsafeArchiveError(
                        ["install path too long for %r" % e.norm])
                if os.path.commonpath([mod_dir, dest]) != mod_dir:
                    raise UnsafeArchiveError(
                        ["install path escapes mod dir for %r" % e.norm])
                planned.append({
                    "arcname": e.norm, "rel_in_mod": rel_in_mod,
                    "dest": dest, "size": e.size, "sha256": e.sha256,
                    "bytes": None})
            for p in planned:
                p["bytes"] = arc.read(p["arcname"])
            archive_sha = arc.archive_sha256

        # report (never "fix") a mod-root folder whose case differs from the
        # canonical runtime tree (e.g. archive 'mods/' vs runtime 'Mods/'):
        # the mod root is stripped on install, so nothing is renamed — the
        # manifest records the variant for the runtime/config audit trail.
        mod_root_case_variants: List[str] = []
        if mod_root:
            for comp in mod_root.rstrip("/").split("/"):
                if comp.lower() in ("mods", "ue4ss") and \
                        comp not in ("Mods", "ue4ss"):
                    mod_root_case_variants.append(comp)

        # ---- case-conflict preflight (host is case-insensitive, PS5 is not)
        planned_rels = [p["rel_in_mod"] for p in planned]
        # The installer-written enable marker joins the case check: an
        # archive 'Enabled.txt' would collide with it into ONE file on the
        # host yet be a distinct (non-enabling) name on the PS5 target.
        # When the archive itself ships an exact-case marker, its file IS
        # the marker and no second one is written.
        archive_ships_marker = ENABLED_MARKER_NAME in planned_rels
        case_paths = list(planned_rels)
        if not archive_ships_marker:
            case_paths.append(ENABLED_MARKER_NAME)
        case_findings = self.find_case_conflicts(title_id, case_paths,
                                                 mod_dir_name)
        if case_findings["within_mod"] or case_findings["across_mods"]:
            raise CaseConflictError(_format_case_conflicts(
                mod_dir_name, case_findings))

        # ownership guard: never replace a mod directory we did not install
        if os.path.isdir(mod_dir) and not os.path.exists(manifest_path):
            raise OwnershipError(
                "refusing to replace unmanaged directory %s (no manifest; "
                "install would overwrite unknown content)" % mod_dir)

        # reinstall: files from a previous version not in the new set are
        # left on disk (never deleted blind) but recorded as stale
        stale_files: List[str] = []
        if os.path.isdir(mod_dir):
            new_rels = {p["rel_in_mod"] for p in planned}
            for dirpath, _dirnames, filenames in os.walk(mod_dir):
                for fn in filenames:
                    rel = os.path.relpath(os.path.join(dirpath, fn),
                                          mod_dir).replace(os.sep, "/")
                    if rel == ENABLED_MARKER_NAME:
                        # installer-owned runtime flag, never stale mod
                        # content: it is rewritten on every install
                        continue
                    if rel not in new_rels:
                        stale_files.append(rel)
            stale_files.sort()

        # planned target dirs (deepest-last so rollback can rmdir in reverse)
        dirs: List[str] = []
        seen_dirs = set()

        def need_dir(d: str) -> None:
            # walk up from d; stop at the first existing or already-planned
            # ancestor (an already-planned dir always has its ancestors
            # planned too), then emit the missing part shallow -> deep.
            chain: List[str] = []
            cur = d
            while not os.path.exists(cur):
                real = os.path.realpath(cur)
                if real in seen_dirs:
                    break
                chain.append(cur)
                seen_dirs.add(real)
                parent = os.path.dirname(cur)
                if parent == cur:
                    break
                cur = parent
            chain.reverse()  # shallow -> deep
            dirs.extend(chain)

        need_dir(mod_dir)
        for p in planned:
            need_dir(os.path.dirname(p["dest"]))
        for e in arc.entries:  # explicit (possibly empty) dirs under mod root
            if e.is_dir and (mod_root == "" or e.norm.startswith(mod_root)):
                need_dir(os.path.join(mod_dir,
                                      *e.norm[len(mod_root):].split("/")))
        need_dir(layout.manifests_dir)
        overrides_bytes: Optional[bytes] = None
        overrides_path: Optional[str] = None
        if md["ue4ss_settings"]:
            overrides_path = layout.overrides_path(mod_dir_name)
            overrides_bytes = render_overrides_ini(
                md["name"], md["version"], md["ue4ss_settings"]).encode(
                "utf-8")
            need_dir(layout.overrides_dir)

        # planned config content (read current + compute new now, write later)
        def _read_or_empty(path: str) -> str:
            if os.path.exists(path):
                with open(path, "r", encoding="utf-8", newline="") as f:
                    return f.read()
            return ""

        def role_for(rel_in_mod: str) -> str:
            low = rel_in_mod.lower()
            if low.startswith("scripts/") and low.endswith(".lua"):
                return "lua"
            if low.startswith("config/") or low.endswith(".ini") or \
                    low.endswith(".cfg"):
                return "config"
            return "asset"

        file_rows = []
        for p in planned:
            file_rows.append("file\t%s\t%s\t%d" % (
                role_for(p["rel_in_mod"]),
                "ue4ss/Mods/%s/%s" % (mod_dir_name, p["rel_in_mod"]),
                p["size"]))
        loadlist_cur = _read_or_empty(layout.loadlist_path)
        loadlist_new = upsert_loadlist(
            loadlist_cur, title_id, mod_dir_name, md["version"], file_rows)
        prx = md["ploader_prx"] or {"name": DEFAULT_PLOADER_PRX,
                                    "mode": DEFAULT_PLOADER_MODE}
        ploader_cur = _read_or_empty(layout.ploader_path)
        ploader_new = upsert_ini_line(ploader_cur, title_id, prx["name"],
                                      prx["mode"])
        # the installer creates the [TITLE] section itself only when the
        # pre-install ploader.ini lacked it (uninstall removes exactly what
        # the installer added, so this is recorded in the manifest).
        # ploader_added_lines mirrors upsert_ini_line's construction: a
        # blank separator is added when the file ends mid-content, then the
        # section header (when new), then the key line.
        ploader_added_lines: List[str] = []
        if ploader_new != ploader_cur:
            if not ini_has_section(ploader_cur, title_id):
                cur_lines = ploader_cur.splitlines() \
                    if ploader_cur.strip() else []
                if cur_lines and cur_lines[-1].strip() != "":
                    ploader_added_lines.append("")
                ploader_added_lines.append("[%s]" % title_id)
            ploader_added_lines.append("%s=%s" % (prx["name"], prx["mode"]))
        ploader_section_created = (ploader_new != ploader_cur and
                                   not ini_has_section(ploader_cur, title_id))
        # UE4SS mods list + enable marker (see module docstring): the row
        # pins the mod into the mods.txt load order, the empty marker file
        # is the runtime's belt-and-braces enable flag
        mods_txt_row = "%s : 1" % mod_dir_name
        mods_txt_cur = _read_or_empty(layout.mods_txt_path)
        mods_txt_new = upsert_mods_txt(mods_txt_cur, mod_dir_name,
                                       enabled=True)
        enabled_marker_path = layout.enabled_marker(mod_dir_name)
        write_enabled_marker = not archive_ships_marker

        # metadata file list cross-check
        mismatch: Dict[str, List[str]] = {"missing_from_archive": [],
                                          "extra_in_archive": []}
        if md["file_list"] is not None:
            arch_names = {p["arcname"] for p in planned} | set(skipped)
            for want in md["file_list"]:
                w = want.replace("\\", "/").lstrip("./")
                if w not in arch_names:
                    mismatch["missing_from_archive"].append(want)
            for p in planned:
                if p["arcname"] not in {x.replace("\\", "/").lstrip("./")
                                        for x in md["file_list"]}:
                    mismatch["extra_in_archive"].append(p["arcname"])
        if strict_file_list and (mismatch["missing_from_archive"] or
                                 mismatch["extra_in_archive"]):
            raise MetadataError(
                "strict file-list check failed: %s" % json.dumps(mismatch))

        # ---- actions (the whole transaction, planned up front) ----
        actions: List[Dict[str, Any]] = []
        contents: List[bytes] = []

        def add_mkdir(path: str) -> None:
            actions.append({"type": "mkdir", "path": path,
                            "existed": os.path.isdir(path),
                            "started": False, "done": False})
            contents.append(b"")

        def add_write(path: str, source: str, new_sha: str,
                      data: bytes) -> None:
            existed = os.path.exists(path)
            actions.append({
                "type": "write", "path": path, "source": source,
                "existed": existed,
                "orig_sha256": sha256_file(path) if existed else None,
                "new_sha256": new_sha,
                "backup": None, "backed_up": False,
                "started": False, "done": False})
            contents.append(data)

        for d in dirs:
            add_mkdir(d)
        for p in sorted(planned, key=lambda x: x["dest"]):
            add_write(p["dest"], "archive:%s" % p["arcname"], p["sha256"],
                      p["bytes"])
        if write_enabled_marker:
            add_write(enabled_marker_path, "config:enabled-marker",
                      sha256_bytes(b""), b"")
        if overrides_bytes is not None and overrides_path is not None:
            add_write(overrides_path, "ue4ss-overrides",
                      sha256_bytes(overrides_bytes), overrides_bytes)
        if loadlist_new != loadlist_cur:
            add_write(layout.loadlist_path, "config:loadlist",
                      sha256_bytes(loadlist_new.encode("utf-8")),
                      loadlist_new.encode("utf-8"))
        if ploader_new != ploader_cur:
            add_write(layout.ploader_path, "config:ploader",
                      sha256_bytes(ploader_new.encode("utf-8")),
                      ploader_new.encode("utf-8"))
        if mods_txt_new != mods_txt_cur:
            add_write(layout.mods_txt_path, "config:mods-txt",
                      sha256_bytes(mods_txt_new.encode("utf-8")),
                      mods_txt_new.encode("utf-8"))

        txid = new_txid()
        backups_dir = os.path.join(layout.backups_root, txid)
        journal_doc = {
            "schema": SCHEMA_JOURNAL, "txid": txid,
            "created_at": utc_now_iso(), "phase": "begin",
            "title_id": title_id, "mod_name": md["name"],
            "mod_dir": mod_dir_name, "version": md["version"],
            "archive_path": os.path.abspath(archive_path),
            "archive_sha256": archive_sha,
            "backups_dir": backups_dir,
            "actions": actions}
        journal_path = os.path.join(layout.journal_dir, txid + ".json")
        journal = Journal(journal_path, journal_doc)

        # >>> THE JOURNAL IS ON DISK BEFORE ANY TARGET FILE OPERATION <<<
        journal.save()
        fsync_dir(layout.journal_dir)
        self._maybe_crash("journal")
        self._actions_done = 0

        def fail(err: Exception) -> None:
            errors = self._rollback(journal.doc)
            self._cleanup_tx(backups_dir, journal_path)
            fsync_dir(layout.journal_dir)
            if errors:
                raise RollbackError(
                    "install failed (%s) AND rollback left residue: %s"
                    % (err, "; ".join(errors))) from err
            raise err

        try:
            os.makedirs(backups_dir, exist_ok=True)  # tx infra, not target
            for idx, act in enumerate(actions):
                journal.mark(idx, started=True)
                data = contents[idx]
                if act["type"] == "mkdir":
                    os.makedirs(act["path"], exist_ok=True)
                else:
                    if act["existed"] and os.path.exists(act["path"]):
                        with open(act["path"], "rb") as src:
                            orig = src.read()
                        bpath = os.path.join(
                            backups_dir, "%04d.bak" % idx)
                        atomic_write_bytes(bpath, orig)
                        journal.mark(idx, backup=bpath, backed_up=True)
                    self._write_file_bytes(act["path"], data)
                journal.mark(idx, done=True)
                self._actions_done += 1
                if self.crash_after is not None and \
                        self._actions_done >= self.crash_after:
                    sys.stderr.write(
                        "SIMULATED CRASH after %d actions\n"
                        % self._actions_done)
                    sys.stderr.flush()
                    os._exit(86)

            self._maybe_crash("pre-verify")

            # ---- verify: installed bytes must equal the archive originals --
            mismatches: List[str] = []
            for p in planned:
                got = sha256_file(p["dest"])
                if got != p["sha256"]:
                    mismatches.append(
                        "%s: installed sha256 %s != archive %s"
                        % (p["dest"], got, p["sha256"]))
            if overrides_path is not None and \
                    sha256_file(overrides_path) != \
                    sha256_bytes(overrides_bytes or b""):
                mismatches.append("%s: overrides sha256 mismatch"
                                  % overrides_path)
            if os.path.exists(layout.loadlist_path):
                ll_now = _read_or_empty(layout.loadlist_path)
                if not loadlist_has_mod(ll_now, mod_dir_name):
                    mismatches.append("loadlist.txt: mod row for %s missing "
                                      "after install" % mod_dir_name)
                if loadlist_new != loadlist_cur and \
                        ll_now != loadlist_new:
                    mismatches.append("loadlist.txt: content != planned")
            elif loadlist_new != loadlist_cur:
                mismatches.append("loadlist.txt missing after install")
            if os.path.exists(layout.ploader_path):
                pl_now = _read_or_empty(layout.ploader_path)
                if not ini_has_line(pl_now, title_id, prx["name"],
                                    prx["mode"]):
                    mismatches.append(
                        "ploader.ini: [%s] %s=%s missing after install"
                        % (title_id, prx["name"], prx["mode"]))
            elif ploader_new != ploader_cur:
                mismatches.append("ploader.ini missing after install")
            if write_enabled_marker and \
                    not os.path.exists(enabled_marker_path):
                mismatches.append("%s: enable marker missing after install"
                                  % enabled_marker_path)
            if os.path.exists(layout.mods_txt_path):
                mt_now = _read_or_empty(layout.mods_txt_path)
                if not mods_txt_has_mod(mt_now, mod_dir_name):
                    mismatches.append(
                        "ue4ss/Mods/mods.txt: enabled row for %s missing "
                        "after install" % mod_dir_name)
                if mods_txt_new != mods_txt_cur and mt_now != mods_txt_new:
                    mismatches.append(
                        "ue4ss/Mods/mods.txt: content != planned")
            elif mods_txt_new != mods_txt_cur:
                mismatches.append("ue4ss/Mods/mods.txt missing after "
                                  "install")
            if mismatches:
                # raise (do NOT call fail() here): the handler below performs
                # exactly one rollback + cleanup, then re-raises this
                raise VerificationError(mismatches)

            # ---- manifest (also journaled, so a rollback removes it) ------
            manifest = {
                "schema": SCHEMA_MANIFEST,
                "mod_name": md["name"],
                "mod_dir": mod_dir_name,
                "title_id": title_id,
                "title_root": layout.title_root,
                "version": md["version"],
                "source_url": md["source_url"],
                "downloaded_at": md["downloaded_at"],
                "installed_at": utc_now_iso(),
                "archive": {
                    "path": os.path.abspath(archive_path),
                    "sha256": archive_sha,
                    "format": "zip"},
                "mod_root_in_archive": mod_root,
                "mod_root_case_variants": mod_root_case_variants,
                "files": [],
                "config_entries": {
                    "loadlist_path": layout.loadlist_path,
                    "loadlist_rows": (
                        ["mod\t%s\t%s" % (mod_dir_name, md["version"])] +
                        file_rows),
                    "ploader_path": layout.ploader_path,
                    "ploader_section": title_id,
                    "ploader_section_created": ploader_section_created,
                    "ploader_added_lines": ploader_added_lines,
                    "ploader_prx_name": prx["name"],
                    "ploader_prx_mode": prx["mode"],
                    "ue4ss_overrides_path": overrides_path,
                    "mods_txt_path": layout.mods_txt_path,
                    "mods_txt_row": mods_txt_row,
                    "enabled_marker_path": enabled_marker_path,
                    "enabled_marker_rel": "ue4ss/Mods/%s/%s" % (
                        mod_dir_name, ENABLED_MARKER_NAME),
                    "enabled_flag_source": (
                        "archive-shipped" if archive_ships_marker
                        else "installer-created"),
                },
                "ue4ss_settings": md["ue4ss_settings"],
                "skipped_outside_mod_root": sorted(skipped),
                # UE4SS runtime files bundled alongside the actual mod:
                # skipped (skipped: runtime files), never installed — UE4SS
                # itself is provided by the PS5 runtime layer
                "skipped_ue4ss_runtime_files": sorted(runtime_skip_norms),
                "stale_files_left_in_place": stale_files,
                "metadata_file_list": md["file_list"],
                "file_list_mismatch": (
                    mismatch if md["file_list"] is not None else None),
                "transaction_id": txid,
            }
            for p in planned:
                installed_sha = sha256_file(p["dest"])
                manifest["files"].append({
                    "archive_path": p["arcname"],
                    "install_path": p["dest"],
                    "install_rel": "ue4ss/Mods/%s/%s" % (mod_dir_name,
                                                         p["rel_in_mod"]),
                    "role": role_for(p["rel_in_mod"]),
                    "size": p["size"],
                    "original_sha256": p["sha256"],
                    "installed_sha256": installed_sha,
                })
            manifest_bytes = json.dumps(manifest, indent=1,
                                        sort_keys=True).encode("utf-8")
            add_write(manifest_path, "manifest",
                      sha256_bytes(manifest_bytes), manifest_bytes)
            midx = len(actions) - 1
            journal.save()  # journal now includes the manifest action
            journal.mark(midx, started=True)
            if actions[midx]["existed"]:
                with open(manifest_path, "rb") as src:
                    orig = src.read()
                bpath = os.path.join(backups_dir, "%04d.bak" % midx)
                atomic_write_bytes(bpath, orig)
                journal.mark(midx, backup=bpath, backed_up=True)
            self._write_file_bytes(manifest_path, manifest_bytes)
            journal.mark(midx, done=True)
        except BaseException as e:
            fail(e if isinstance(e, Exception) else InstallError(repr(e)))

        journal.set_phase("committed")
        self._maybe_crash("commit")
        self._cleanup_tx(backups_dir, journal_path)
        fsync_dir(layout.journal_dir)
        return manifest

    # ---- post-install verification ---------------------------------------
    def verify_installed(self, title_id: str, mod_name: str) -> Dict[str, Any]:
        layout = Layout(self.roots_base, self.plugins_dir, self.state_dir,
                        title_id)
        mod_dir_name = sanitize_component(mod_name, 64, "name")
        manifest_path = layout.manifest_path(mod_dir_name)
        if not os.path.exists(manifest_path):
            raise InstallError("no manifest for %r under %s"
                               % (mod_name, title_id))
        with open(manifest_path, "r", encoding="utf-8") as f:
            manifest = json.load(f)
        problems: List[str] = []
        for entry in manifest.get("files", []):
            path = entry["install_path"]
            if not os.path.exists(path):
                problems.append("missing: %s" % path)
                continue
            got = sha256_file(path)
            if got != entry["original_sha256"]:
                problems.append(
                    "tampered: %s sha256 %s != original %s"
                    % (path, got[:16] + "...", entry["original_sha256"][:16]
                       + "..."))
        cfg = manifest.get("config_entries", {})
        if os.path.exists(layout.loadlist_path):
            with open(layout.loadlist_path, "r", encoding="utf-8",
                      newline="") as f:
                if not loadlist_has_mod(f.read(), mod_dir_name):
                    problems.append("loadlist.txt: mod row for %s missing"
                                    % mod_dir_name)
        else:
            problems.append("loadlist.txt missing")
        if os.path.exists(layout.ploader_path):
            with open(layout.ploader_path, "r", encoding="utf-8",
                      newline="") as f:
                if not ini_has_line(f.read(), title_id,
                                    cfg.get("ploader_prx_name",
                                            DEFAULT_PLOADER_PRX),
                                    cfg.get("ploader_prx_mode",
                                            DEFAULT_PLOADER_MODE)):
                    problems.append("ploader.ini: entry missing")
        else:
            problems.append("ploader.ini missing")
        if os.path.exists(layout.mods_txt_path):
            with open(layout.mods_txt_path, "r", encoding="utf-8",
                      newline="") as f:
                if not mods_txt_has_mod(f.read(), mod_dir_name):
                    problems.append("ue4ss/Mods/mods.txt: enabled row for %s "
                                    "missing" % mod_dir_name)
        else:
            problems.append("ue4ss/Mods/mods.txt missing")
        marker = layout.enabled_marker(mod_dir_name)
        if not os.path.exists(marker):
            problems.append("%s: enable marker missing" % marker)
        return {"ok": not problems,
                "mod": mod_dir_name,
                "title_id": title_id,
                "files_checked": len(manifest.get("files", [])),
                "problems": problems}


# --------------------------------------------------------------------------
# CLI
# --------------------------------------------------------------------------

def _exit_code(err: Exception) -> int:
    if isinstance(err, (VerificationError,)):
        return EX_VERIFY
    if isinstance(err, RollbackError):
        return EX_ROLLBACK
    if isinstance(err, InstallError):
        return EX_BAD
    return EX_IO


def main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(
        description="modbridge installer core (Mac-only host tool)")
    sub = ap.add_subparsers(dest="cmd", required=True)

    p_inst = sub.add_parser("install", help="install a mod zip")
    p_inst.add_argument("--archive", required=True)
    p_inst.add_argument("--metadata", required=True)
    p_inst.add_argument("--title", required=True)
    p_inst.add_argument("--roots", required=True,
                        help="mod roots base (console: /data/modbridge/roots)")
    p_inst.add_argument("--plugins", required=True,
                        help="plugins dir (console: /data/plugins)")
    p_inst.add_argument("--state", required=True,
                        help="installer state dir (journals + backups)")
    p_inst.add_argument("--strict-file-list", action="store_true")
    p_inst.add_argument("--crash-after", type=int, default=None,
                        help="TEST: hard-exit 86 after N actions")
    p_inst.add_argument("--crash-at", default=None,
                        choices=["journal", "pre-verify", "commit"],
                        help="TEST: hard-exit 86 at a named point")

    p_rec = sub.add_parser("recover", help="complete/roll back journals")
    p_rec.add_argument("--roots", required=True)
    p_rec.add_argument("--plugins", required=True)
    p_rec.add_argument("--state", required=True)

    p_ver = sub.add_parser("verify", help="verify an installed mod")
    p_ver.add_argument("--title", required=True)
    p_ver.add_argument("--mod", required=True)
    p_ver.add_argument("--roots", required=True)
    p_ver.add_argument("--plugins", required=True)
    p_ver.add_argument("--state", required=True)

    args = ap.parse_args(argv)
    try:
        if args.cmd == "install":
            inst = Installer(args.roots, args.plugins, args.state,
                             crash_after=args.crash_after,
                             crash_at=args.crash_at)
            md = load_metadata(args.metadata)
            manifest = inst.install(args.archive, md, args.title,
                                    strict_file_list=args.strict_file_list)
            print(json.dumps({
                "ok": True, "mod": manifest["mod_dir"],
                "title_id": manifest["title_id"],
                "files": len(manifest["files"]),
                "manifest": manifest_path_of(manifest),
            }, indent=1))
            return EX_OK
        if args.cmd == "recover":
            inst = Installer(args.roots, args.plugins, args.state)
            reports = inst.recover()
            print(json.dumps({"ok": True, "recovered": reports}, indent=1))
            return EX_OK
        if args.cmd == "verify":
            inst = Installer(args.roots, args.plugins, args.state)
            report = inst.verify_installed(args.title, args.mod)
            print(json.dumps(report, indent=1))
            return EX_OK if report["ok"] else EX_VERIFY
    except InstallError as e:
        print("error: %s" % e, file=sys.stderr)
        return _exit_code(e)
    except OSError as e:
        print("error: io: %s" % e, file=sys.stderr)
        return EX_IO
    return EX_BAD


def manifest_path_of(manifest: Dict[str, Any]) -> str:
    return os.path.join(manifest["title_root"], "manifests",
                        manifest["mod_dir"] + ".json")


if __name__ == "__main__":
    sys.exit(main())
