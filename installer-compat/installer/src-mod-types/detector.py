#!/usr/bin/env python3
"""Mod archive type detector + requirements mapper (Mac-only, host-side tool).

Classifies a mod archive by inspecting the files it CONTAINS (never by the
archive's own filename):

  1. ue4ss-lua    Mods/<name>/Scripts/*.lua layout, or UE4SS API markers
                  (RegisterKeyBind / ExecuteInGameThread / FindFirstOf /
                  UEHelpers) in any .lua file
  2. ue4ss-cpp    Windows .dll payloads (flagged: NOT runnable on PS5)
  3. pak-iostore  .pak / .utoc / .ucas PC-cooked containers (needs conversion)
  4. bp-logicmod  .pak under a LogicMods directory / LogicMods pak name
  5. native-code  .so / .prx / .sprx native binaries
  6. config-only  nothing but .ini/.cfg/.json/.txt files
  7. mixed        more than one of the above detected (all listed)
  8. unknown      nothing matched (including empty archives)

Each type maps to runnable_on_ps5 (yes / no / needs-conversion / unknown),
required_runtime ("UE4SS" / "pak-loader" / "none" / "unknown") and
install_notes describing what an installer must do differently.

The module also parses bundled metadata (readme / description / mod-info
style files) for requirements:
  - required UE4SS version (semver-comparable)
  - required game version
  - required other mods by name or Nexus ID (resolvable against existing
    install records via ModRequirements.resolve_against)
  - required plugins (e.g. "requires EXNOTIK")

Archive formats readable with the Python stdlib: zip (including zip-in-zip,
depth-limited), tar, tar.gz/tgz, tar.bz2, tar.xz. 7z / RAR are sniffed and
reported as unsupported. Entirely local: no network, no console contact,
no writes; archive format is chosen by magic bytes, so misleading file
extensions are handled.
"""
from __future__ import annotations

import argparse
import io
import json
import re
import sys
import tarfile
import zipfile
from dataclasses import asdict, dataclass, field
from enum import Enum
from pathlib import PurePosixPath
from typing import Any, Callable, Dict, Iterable, List, Optional, Tuple

__all__ = [
    "ModType", "TYPE_INFO", "Classification", "ModRequirements",
    "VersionReq", "DependencyReq", "ArcEntry", "ArchiveError",
    "UnsupportedArchiveError", "classify_archive", "inspect_archive",
    "parse_requirements", "parse_semver", "compare_versions", "main",
]

# ---------------------------------------------------------------------------
# limits (runaway / zip-bomb protection)
MAX_ENTRIES = 5000
MAX_TOTAL_BYTES = 256 * 1024 * 1024
MAX_FILE_READ = 2 * 1024 * 1024        # per-file content read cap
MAX_META_READ = 256 * 1024             # metadata text cap
MAX_NESTED_BYTES = 96 * 1024 * 1024    # nested archive load cap
MAX_NESTED_DEPTH = 4

# ---------------------------------------------------------------------------
# detection constants
LUA_MARKERS = ("RegisterKeyBind", "ExecuteInGameThread", "FindFirstOf", "UEHelpers")
LUA_STRUCTURE_RE = re.compile(r"(?:^|/)mods/[^/]+/scripts/[^/]+\.lua$", re.IGNORECASE)
DLL_EXTS = {".dll"}
PAK_EXTS = {".pak", ".utoc", ".ucas"}
NATIVE_EXTS = {".so", ".prx", ".sprx"}
CONFIG_EXTS = {".ini", ".cfg", ".json", ".txt"}
NESTED_ARCHIVE_EXTS = {".7z", ".rar", ".tar", ".gz", ".tgz", ".bz2", ".xz"}

# metadata (readme / description / mod-info) file matching
META_BASENAME_RE = re.compile(
    r"^(?:readme|description|mod[-_ ]?info|modinfo|install(?:ation)?|"
    r"requirements?|dependencies|about|details|metadata)(?:\.[^.]*)?$",
    re.IGNORECASE,
)
META_EXTS = {".txt", ".md", ".json", ".ini", ""}

# ---------------------------------------------------------------------------
# requirements-parsing constants
UE4SS_VER_RE = re.compile(r"(?i)\bUE4SS\b[^0-9\n]{0,16}?(\d+(?:\.\d+){1,3})")
GAME_VER_RE = re.compile(
    r"(?i)\b(?:game|build|patch|update|title)\s+"
    r"(?:version\s+|update\s+|patch\s+|build\s+|v)?(\d+(?:\.\d+){1,3})"
)
GAME_VER_EQ_RE = re.compile(
    r"(?i)\bversion(?:\s+of\s+(?:the\s+)?(?:game|title))?\s*[:=]\s*v?(\d+(?:\.\d+){1,3})"
)
# cue words that turn a spotted version number into a *requirement*
CUE_RE = re.compile(
    r"(?i)(?:requir|need|at least|minimum|\bmin\b|>=|or later|or newer|or above|"
    r"compatible with|works with|updated?\s+(?:for|to)|made for)"
)
QUOTED_DEP_RE = re.compile(
    r"\b(?:[Rr]equires?|[Nn]eeds?|[Dd]epends\s+on)\s+(?:[Tt]he\s+)?"
    r"[\"'\u201c\u2018]([^\"'\u201d\u2019\n]{2,60})[\"'\u201d\u2018]"
)
SUFFIX_DEP_RE = re.compile(
    r"\b(?:[Rr]equires?|[Nn]eeds?)\s+(?:[Tt]he\s+)?"
    r"([A-Z][\w'\u2019.\-]*(?:\s+[A-Z][\w'\u2019.\-]*)*)\s+"
    r"([Mm]od|[Pp]lugin|[Ff]ramework|[Ll]oader)\b"
)
KNOWN_PLUGIN_RE = re.compile(
    r"\b(?:[Rr]equires?|[Nn]eeds?)\s+(?:[Tt]he\s+)?"
    r"(EXNOTIK|BPModLoader|USSL|CXX\s?[Mm]od)\b"
)
NEXUS_ID_RES = [
    # URL form: nexusmods.com[/<segment>]*/mods/<id> — handles game pages
    # ("/<game>/mods/<id>"), the newer "/games/<title>/mods/<id>" layout,
    # locale prefixes ("/en/...") and hyphenated slugs.  The literal "/mods/"
    # stop keeps path segments and game slugs (e.g. a "2077" slug) from ever
    # being mistaken for the mod id.
    re.compile(r"(?i)\bnexus(?:mods)?(?:\.com)?(?:/[A-Za-z0-9_\-]+)*/mods/(\d{3,9})\b"),
    # prose form: "nexus 12345", "nexus: 12345", "nexus #12345",
    # "nexusmods.com: 12345"
    re.compile(r"(?i)\bnexus(?:mods)?(?:\.com)?\s*[:#]?\s*(\d{3,9})\b"),
    # "nexus mod id 12345" / "nexus mods page #12345"
    re.compile(r"(?i)\bnexus(?:\s+mods)?\s+(?:id|page|mod)\s*(?:id)?\s*[:#]?\s*(\d{3,9})\b"),
]
KNOWN_PLUGIN_KEYS = {"exnotik", "bpmodloader", "ussl", "cxxmod"}


# ---------------------------------------------------------------------------
# semver helpers
def parse_semver(version: str) -> Tuple[int, int, int]:
    """Parse 'v3.0.1' / '3.0' / '2' into a comparable (major, minor, patch) tuple."""
    m = re.match(r"^\s*[vV]?(\d+)(?:\.(\d+))?(?:\.(\d+))?", str(version))
    if not m:
        raise ValueError(f"not a semantic version: {version!r}")
    return tuple(int(g) if g else 0 for g in m.groups())  # type: ignore[return-value]


def compare_versions(a: str, b: str) -> int:
    """Return -1 / 0 / 1 comparing two loose semver strings."""
    va, vb = parse_semver(a), parse_semver(b)
    return (va > vb) - (va < vb)


# ---------------------------------------------------------------------------
# type model
class ModType(str, Enum):
    UE4SS_LUA = "ue4ss-lua"
    UE4SS_CPP = "ue4ss-cpp"
    PAK_IOSTORE = "pak-iostore"
    BP_LOGICMOD = "bp-logicmod"
    NATIVE = "native-code"
    CONFIG_ONLY = "config-only"
    MIXED = "mixed"
    UNKNOWN = "unknown"


CANONICAL_ORDER = [
    ModType.UE4SS_LUA, ModType.UE4SS_CPP, ModType.PAK_IOSTORE,
    ModType.BP_LOGICMOD, ModType.NATIVE, ModType.CONFIG_ONLY,
]

_RUNNABLE_ORDER = {"no": 0, "needs-conversion": 1, "unknown": 2, "yes": 3}

TYPE_INFO: Dict[ModType, Dict[str, str]] = {
    ModType.UE4SS_LUA: {
        "runnable_on_ps5": "unknown",
        "required_runtime": "UE4SS",
        "install_notes": (
            "Install into the UE4SS Mods tree (Mods/<name>/Scripts/main.lua) and "
            "enable in mods.txt. Lua source is platform-agnostic, but it needs a "
            "UE4SS-compatible Lua runtime on the target; no standard PS5 UE4SS "
            "build exists, so runnability on console is unknown until a runtime "
            "is provided."
        ),
    },
    ModType.UE4SS_CPP: {
        "runnable_on_ps5": "no",
        "required_runtime": "UE4SS",
        "install_notes": (
            "UE4SS C++ mod: on PC it installs as Mods/<name>/dll/main.dll. "
            "FLAG: Windows PE DLLs cannot load on PS5; a console-native rebuild "
            "would be required. Never install the DLL on console."
        ),
    },
    ModType.PAK_IOSTORE: {
        "runnable_on_ps5": "needs-conversion",
        "required_runtime": "pak-loader",
        "install_notes": (
            "PC-cooked .pak/.utoc/.ucas assets: on PC drop into the game's Paks "
            "folder (often under ~mods). FLAG: PS5 uses console-cooked IoStore; "
            "the assets must be converted/recooked for the target before use."
        ),
    },
    ModType.BP_LOGICMOD: {
        "runnable_on_ps5": "needs-conversion",
        "required_runtime": "UE4SS",
        "install_notes": (
            "UE4SS Blueprint (LogicMods) pak: on PC install under "
            "Content/Paks/LogicMods with UE4SS BP mod loading enabled. The pak "
            "still needs a pak-loader mount, and it must be converted/recooked "
            "for PS5 IoStore."
        ),
    },
    ModType.NATIVE: {
        "runnable_on_ps5": "unknown",
        "required_runtime": "none",
        "install_notes": (
            "Native binaries (.so/.prx/.sprx): verify the target platform from "
            "the ELF/SELF header evidence. Console binaries must be properly "
            "signed before any launch; never install PC binaries on console."
        ),
    },
    ModType.CONFIG_ONLY: {
        "runnable_on_ps5": "unknown",
        "required_runtime": "none",
        "install_notes": (
            "Text config only (.ini/.cfg/.json/.txt): merge into the game's "
            "config location and keep backups. Whether the console build reads "
            "the same config paths must be verified per game."
        ),
    },
    ModType.UNKNOWN: {
        "runnable_on_ps5": "unknown",
        "required_runtime": "unknown",
        "install_notes": "No known mod pattern matched; inspect manually.",
    },
    ModType.MIXED: {
        "runnable_on_ps5": "unknown",
        "required_runtime": "unknown",
        "install_notes": "Placeholder; mixed results aggregate per-type info.",
    },
}


# ---------------------------------------------------------------------------
# requirement dataclasses
@dataclass
class VersionReq:
    version: str
    op: str = ">="
    raw: str = ""
    source: str = ""


@dataclass
class DependencyReq:
    name: Optional[str] = None
    nexus_id: Optional[str] = None
    kind: str = "mod"          # "mod" | "plugin"
    raw: str = ""
    source: str = ""


@dataclass
class ModRequirements:
    ue4ss_version: Optional[VersionReq] = None
    game_version: Optional[VersionReq] = None
    other_mods: List[DependencyReq] = field(default_factory=list)
    plugins: List[DependencyReq] = field(default_factory=list)
    ue4ss_versions_mentioned: List[str] = field(default_factory=list)
    game_versions_mentioned: List[str] = field(default_factory=list)
    sources_scanned: List[str] = field(default_factory=list)

    def ue4ss_satisfied(self, installed: Optional[str]) -> Optional[bool]:
        if self.ue4ss_version is None:
            return None
        if not installed:
            return False
        return compare_versions(installed, self.ue4ss_version.version) >= 0

    def game_version_satisfied(self, installed: Optional[str]) -> Optional[bool]:
        if self.game_version is None:
            return None
        if not installed:
            return False
        return compare_versions(installed, self.game_version.version) >= 0

    def resolve_against(self, records: Iterable[Dict[str, Any]]) -> Dict[str, List[Dict[str, Any]]]:
        """Map detected dependencies onto existing install records.

        Records are dicts with at least "name" and optionally "nexus_id" and
        "installed" (default True). Returns resolved / missing / uninstalled.
        """
        pool = [dict(r) for r in (records or [])]
        resolved: List[Dict[str, Any]] = []
        missing: List[Dict[str, Any]] = []
        uninstalled: List[Dict[str, Any]] = []
        for dep in list(self.other_mods) + list(self.plugins):
            rec = self._match(dep, pool)
            if rec is None:
                missing.append(asdict(dep))
            elif not rec.get("installed", True):
                uninstalled.append({"dependency": asdict(dep), "record": rec})
            else:
                resolved.append({"dependency": asdict(dep), "record": rec})
        return {"resolved": resolved, "missing": missing, "uninstalled": uninstalled}

    @staticmethod
    def _match(dep: DependencyReq, pool: List[Dict[str, Any]]) -> Optional[Dict[str, Any]]:
        for rec in pool:
            if dep.nexus_id and str(rec.get("nexus_id", "")) == dep.nexus_id:
                return rec
            if dep.name and _norm_name(dep.name) and _norm_name(rec.get("name")) == _norm_name(dep.name):
                return rec
        return None

    def to_dict(self) -> Dict[str, Any]:
        return {
            "ue4ss_version": asdict(self.ue4ss_version) if self.ue4ss_version else None,
            "game_version": asdict(self.game_version) if self.game_version else None,
            "other_mods": [asdict(d) for d in self.other_mods],
            "plugins": [asdict(d) for d in self.plugins],
            "ue4ss_versions_mentioned": list(self.ue4ss_versions_mentioned),
            "game_versions_mentioned": list(self.game_versions_mentioned),
            "metadata_files_scanned": list(self.sources_scanned),
        }


@dataclass
class Classification:
    archive: str
    types: List[ModType]
    label: ModType
    mixed: bool
    runnable_on_ps5: str
    required_runtime: str
    per_type: Dict[str, Dict[str, str]]
    install_notes: List[str]
    evidence: Dict[str, List[str]]
    flags: Dict[str, Any]
    requirements: ModRequirements
    notes: List[str]
    entries_scanned: int

    @property
    def classified(self) -> bool:
        return self.label not in (ModType.UNKNOWN,)

    def to_dict(self) -> Dict[str, Any]:
        return {
            "archive": self.archive,
            "label": self.label.value,
            "mod_types": [t.value for t in self.types],
            "mixed": self.mixed,
            "classified": self.classified,
            "runnable_on_ps5": self.runnable_on_ps5,
            "required_runtime": self.required_runtime,
            "per_type": self.per_type,
            "install_notes": self.install_notes,
            "evidence": self.evidence,
            "flags": self.flags,
            "requirements": self.requirements.to_dict(),
            "notes": self.notes,
            "entries_scanned": self.entries_scanned,
        }


# ---------------------------------------------------------------------------
# archive inspection (format chosen by magic bytes, never by extension)
class ArchiveError(Exception):
    pass


class UnsupportedArchiveError(ArchiveError):
    pass


@dataclass
class ArcEntry:
    path: str                              # display path; nested archives prefix inner names
    size: int
    container: str
    read: Callable[[int], bytes]           # reads up to n bytes; valid only during classify

    @property
    def name(self) -> str:
        return self.path.rsplit("/", 1)[-1]

    @property
    def ext(self) -> str:
        return PurePosixPath(self.path).suffix.lower()

    @property
    def components(self) -> List[str]:
        return [c.lower() for c in self.path.split("/") if c]


@dataclass
class _ScanState:
    count: int = 0
    total: int = 0
    holders: List[Any] = field(default_factory=list)   # keeps archive handles open while scanning
    nested: List[str] = field(default_factory=list)


def _sniff_bytes(head: bytes) -> Optional[str]:
    if head[:4] in (b"PK\x03\x04", b"PK\x05\x06", b"PK\x07\x08"):
        return "zip"
    if head[:6] == b"7z\xbc\xaf\x27\x1c":
        return "7z"
    if head[:7] == b"Rar!\x1a\x07":
        return "rar"
    if head[:2] == b"\x1f\x8b":
        return "gzip"
    if head[:3] == b"BZh":
        return "bzip2"
    if head[:6] == b"\xfd7zXZ\x00":
        return "xz"
    if len(head) >= 262 and head[257:262] == b"ustar":
        return "tar"
    return None


def sniff_format(path: str) -> Optional[str]:
    try:
        with open(path, "rb") as fh:
            head = fh.read(512)
    except OSError as exc:
        raise ArchiveError(f"cannot read {path}: {exc}") from exc
    return _sniff_bytes(head)


def _zip_reader(zf: zipfile.ZipFile, info: zipfile.ZipInfo) -> Callable[[int], bytes]:
    def read(limit: int = MAX_FILE_READ) -> bytes:
        with zf.open(info) as fh:
            return fh.read(limit)
    return read


def _tar_reader(tf: tarfile.TarFile, member: tarfile.TarInfo) -> Callable[[int], bytes]:
    def read(limit: int = MAX_FILE_READ) -> bytes:
        fh = tf.extractfile(member)
        if fh is None:
            return b""
        with fh:
            return fh.read(limit)
    return read


def _safe_read(entry: ArcEntry, limit: int, notes: Optional[List[str]] = None) -> bytes:
    try:
        return entry.read(limit)
    except Exception as exc:  # encrypted / corrupt entries must not abort the scan
        if notes is not None:
            notes.append(f"could not read {entry.path}: {exc}")
        return b""


def _scan_nested(out: List[ArcEntry], depth: int, state: _ScanState, notes: List[str]) -> None:
    if depth >= MAX_NESTED_DEPTH:
        return
    for e in list(out):
        if e.ext == ".zip":
            data = _safe_read(e, MAX_NESTED_BYTES, notes)
            sniffed = _sniff_bytes(data)
            if sniffed != "zip":
                notes.append(
                    f"nested {e.path}: named .zip but content sniffed as "
                    f"{sniffed or 'unknown'}; skipped")
                continue
            try:
                inner = zipfile.ZipFile(io.BytesIO(data))
            except zipfile.BadZipFile:
                notes.append(f"nested {e.path}: bad zip; skipped")
                continue
            state.holders.append(inner)
            state.nested.append(e.path)
            notes.append(f"inspected nested zip: {e.path}")
            out.extend(_entries_from_zip(inner, e.path + "/", depth + 1, state, notes))
        elif e.ext in NESTED_ARCHIVE_EXTS:
            notes.append(
                f"nested archive {e.path} ({e.ext}) is not auto-inspected; "
                f"classify it separately")


def _entries_from_zip(zf: zipfile.ZipFile, prefix: str, depth: int,
                      state: _ScanState, notes: List[str]) -> List[ArcEntry]:
    out: List[ArcEntry] = []
    for info in zf.infolist():
        if state.count >= MAX_ENTRIES:
            notes.append(f"{prefix or '<root>'}: entry limit reached ({MAX_ENTRIES}); scan truncated")
            break
        if info.is_dir():
            continue
        name = info.filename.replace("\\", "/")
        if not name or name.endswith("/"):
            continue
        state.count += 1
        state.total += info.file_size
        if state.total > MAX_TOTAL_BYTES:
            notes.append(f"{prefix or '<root>'}: declared size limit reached ({MAX_TOTAL_BYTES}); scan truncated")
            break
        out.append(ArcEntry(path=prefix + name, size=info.file_size,
                            container=prefix or "<root>", read=_zip_reader(zf, info)))
    _scan_nested(out, depth, state, notes)
    return out


def _entries_from_tar(tf: tarfile.TarFile, prefix: str, depth: int,
                      state: _ScanState, notes: List[str]) -> List[ArcEntry]:
    out: List[ArcEntry] = []
    for member in tf:
        if state.count >= MAX_ENTRIES:
            notes.append(f"{prefix or '<root>'}: entry limit reached ({MAX_ENTRIES}); scan truncated")
            break
        if not member.isfile():
            continue
        name = member.name.replace("\\", "/")
        if name.startswith("./"):
            name = name[2:]
        if not name:
            continue
        state.count += 1
        state.total += member.size
        if state.total > MAX_TOTAL_BYTES:
            notes.append(f"{prefix or '<root>'}: declared size limit reached ({MAX_TOTAL_BYTES}); scan truncated")
            break
        out.append(ArcEntry(path=prefix + name, size=member.size,
                            container=prefix or "<root>", read=_tar_reader(tf, member)))
    _scan_nested(out, depth, state, notes)
    return out


def inspect_archive(path: str) -> Tuple[List[ArcEntry], List[str], _ScanState]:
    fmt = sniff_format(str(path))
    notes: List[str] = []
    state = _ScanState()
    if fmt is None:
        raise UnsupportedArchiveError(
            f"{path}: unrecognized archive format (no zip/tar magic bytes)")
    if fmt in ("7z", "rar"):
        raise UnsupportedArchiveError(
            f"{path}: {fmt} archive — detector supports zip/tar (.tar.gz/.bz2/.xz) "
            f"via the Python stdlib only")
    if fmt == "zip":
        try:
            zf = zipfile.ZipFile(str(path))
        except zipfile.BadZipFile as exc:
            raise ArchiveError(f"{path}: bad zip: {exc}") from exc
        state.holders.append(zf)
        entries = _entries_from_zip(zf, "", 0, state, notes)
    else:
        try:
            tf = tarfile.open(str(path), "r:*")
        except tarfile.TarError as exc:
            raise ArchiveError(f"{path}: cannot open tar ({fmt}): {exc}") from exc
        state.holders.append(tf)
        entries = _entries_from_tar(tf, "", 0, state, notes)
    return entries, notes, state


# ---------------------------------------------------------------------------
# classification
def _describe_native(head: bytes) -> Dict[str, str]:
    if head[:4] == b"\x7fELF":
        machine = int.from_bytes(head[18:20], "little") if len(head) >= 20 else -1
        names = {62: "x86-64", 183: "aarch64", 40: "arm", 3: "x86", 243: "riscv64"}
        return {"format": "ELF", "machine": names.get(machine, f"machine:{machine}")}
    if head[:4] in (bytes.fromhex("4f153d1d"), bytes.fromhex("5414f5ee")):
        return {"format": "SELF"}
    return {"format": "unknown"}


def _classify_entries(archive: str, entries: List[ArcEntry], notes: List[str]) -> Classification:
    evidence: Dict[str, List[str]] = {}
    flags: Dict[str, Any] = {
        "windows_dll_present": False,
        "pc_cooked_assets": False,
        "logicmods_pak": False,
        "native_formats": [],
        "empty_archive": False,
    }

    def ev(t: ModType, line: str) -> None:
        evidence.setdefault(t.value, []).append(line)

    files = entries
    scanned = len(files)

    if not files:
        flags["empty_archive"] = True
        notes = list(notes) + ["archive contains no file entries"]
        return _finalize(archive, [], evidence, flags, ModRequirements(), notes, 0)

    for e in files:
        try:
            if e.ext == ".lua":
                structural = LUA_STRUCTURE_RE.search(e.path)
                text = _safe_read(e, MAX_FILE_READ, notes).decode("utf-8", "replace")
                found = [mk for mk in LUA_MARKERS if mk in text]
                if structural:
                    ev(ModType.UE4SS_LUA, f"{e.path} (Mods/<name>/Scripts/*.lua layout)")
                if found:
                    ev(ModType.UE4SS_LUA, f"{e.path} (UE4SS API markers: {', '.join(found)})")
            elif e.ext in DLL_EXTS:
                is_pe = _safe_read(e, 2, notes) == b"MZ"
                flags["windows_dll_present"] |= is_pe
                detail = "Windows PE DLL (MZ header)" if is_pe else ".dll without MZ header — flagged anyway"
                ev(ModType.UE4SS_CPP, f"{e.path} ({detail})")
            elif e.ext in PAK_EXTS:
                stem = PurePosixPath(e.path).stem.lower()
                is_logic = ("logicmods" in e.components) or (e.ext == ".pak" and "logicmods" in stem)
                flags["pc_cooked_assets"] = True
                if is_logic:
                    flags["logicmods_pak"] = True
                    ev(ModType.BP_LOGICMOD, f"{e.path} (LogicMods pak)")
                else:
                    ev(ModType.PAK_IOSTORE, f"{e.path} ({e.ext} PC-cooked container)")
            elif e.ext in NATIVE_EXTS:
                desc = _describe_native(_safe_read(e, 20, notes))
                flags["native_formats"].append({"path": e.path, **desc})
                detail = desc.get("format", "unknown")
                if desc.get("machine"):
                    detail += f", {desc['machine']}"
                ev(ModType.NATIVE, f"{e.path} ({detail})")
        except Exception as exc:  # never let one bad entry abort classification
            notes.append(f"error inspecting {e.path}: {exc}")

    types = [t for t in CANONICAL_ORDER if t.value in evidence]

    if not types:
        non_config = [e for e in files if e.ext not in CONFIG_EXTS]
        ext_summary = ", ".join(sorted({e.ext or "<none>" for e in files}))
        if not non_config:
            ev(ModType.CONFIG_ONLY, f"{len(files)} config-only file(s) [{ext_summary}]")
            types = [ModType.CONFIG_ONLY]
        else:
            notes.append(f"no known mod pattern matched (extensions present: {ext_summary})")

    requirements = _extract_requirements(files, notes)
    return _finalize(archive, types, evidence, flags, requirements, notes, scanned)


def _finalize(archive: str, types: List[ModType], evidence: Dict[str, List[str]],
              flags: Dict[str, Any], requirements: ModRequirements,
              notes: List[str], scanned: int) -> Classification:
    final = types if types else [ModType.UNKNOWN]
    mixed = len(types) > 1
    label = ModType.MIXED if mixed else final[0]

    runtimes: List[str] = []
    for t in final:
        rt = TYPE_INFO[t]["required_runtime"]
        if rt not in runtimes:
            runtimes.append(rt)
    run_values = [TYPE_INFO[t]["runnable_on_ps5"] for t in final]
    runnable = min(run_values, key=lambda v: _RUNNABLE_ORDER[v])
    required_runtime = runtimes[0] if len(runtimes) == 1 else "unknown"

    per_type = {t.value: dict(TYPE_INFO[t]) for t in final}

    install_notes: List[str] = []
    if mixed:
        install_notes.append(
            "Mixed mod — multiple mod types detected ("
            + ", ".join(t.value for t in final)
            + "); each component needs its own handling.")
        if len(runtimes) > 1:
            install_notes.append(
                "Multiple runtimes required: "
                + "; ".join(
                    f"{rt} for " + ", ".join(
                        t.value for t in final
                        if TYPE_INFO[t]["required_runtime"] == rt)
                    for rt in runtimes))
    for t in final:
        install_notes.append(f"[{t.value}] {TYPE_INFO[t]['install_notes']}")

    return Classification(
        archive=archive, types=final, label=label, mixed=mixed,
        runnable_on_ps5=runnable, required_runtime=required_runtime,
        per_type=per_type, install_notes=install_notes,
        evidence=evidence, flags=flags, requirements=requirements,
        notes=notes, entries_scanned=scanned,
    )


def classify_archive(path: "str") -> Classification:
    """Classify a mod archive and map its requirements (host-side, read-only)."""
    p = str(path)
    entries, notes, state = inspect_archive(p)
    cls = _classify_entries(p, entries, notes)
    cls.flags["nested_archives"] = list(state.nested)
    return cls


# ---------------------------------------------------------------------------
# requirements parsing
def _norm_name(s: Optional[str]) -> str:
    return re.sub(r"[^a-z0-9]", "", (s or "").lower())


def _extract_requirements(files: List[ArcEntry], notes: List[str]) -> ModRequirements:
    sources: List[Tuple[str, str]] = []
    for e in files:
        base = e.name.lower()
        suffix = PurePosixPath(base).suffix.lower()
        if suffix in META_EXTS and META_BASENAME_RE.match(base):
            text = _safe_read(e, MAX_META_READ, notes).decode("utf-8", "replace")
            sources.append((e.path, text))
    return parse_requirements(sources)


def parse_requirements(sources: Iterable[Tuple[str, str]]) -> ModRequirements:
    """Parse (source_name, text) metadata for version/mod/plugin requirements."""
    req = ModRequirements()
    for source, text in sources:
        if not text:
            continue
        if source not in req.sources_scanned:
            req.sources_scanned.append(source)

        # -- UE4SS version -------------------------------------------------
        for m in UE4SS_VER_RE.finditer(text):
            ver = m.group(1)
            ctx = text[max(0, m.start() - 80): m.end() + 40]
            if CUE_RE.search(ctx):
                if req.ue4ss_version is None:
                    req.ue4ss_version = VersionReq(ver, ">=", m.group(0).strip(), source)
            elif ver not in req.ue4ss_versions_mentioned:
                req.ue4ss_versions_mentioned.append(ver)

        # -- game version ---------------------------------------------------
        for rx in (GAME_VER_RE, GAME_VER_EQ_RE):
            for m in rx.finditer(text):
                ver = m.group(1)
                ctx = text[max(0, m.start() - 80): m.end() + 40]
                if CUE_RE.search(ctx):
                    if req.game_version is None:
                        req.game_version = VersionReq(ver, ">=", m.group(0).strip(), source)
                elif ver not in req.game_versions_mentioned:
                    req.game_versions_mentioned.append(ver)

        # -- named / Nexus-ID / plugin dependencies --------------------------
        for sent in re.split(r"(?<=[.!?])\s+|\n+", text):
            _parse_sentence(sent, source, req)

    _dedupe_deps(req)
    return req


def _parse_sentence(sent: str, source: str, req: ModRequirements) -> None:
    ids: List[str] = []
    for rx in NEXUS_ID_RES:
        ids.extend(rx.findall(sent))

    for m in QUOTED_DEP_RE.finditer(sent):
        name = m.group(1).strip()
        kind = "plugin" if _norm_name(name) in KNOWN_PLUGIN_KEYS else "mod"
        (req.other_mods if kind == "mod" else req.plugins).append(
            DependencyReq(name=name, kind=kind, raw=m.group(0), source=source))

    for m in SUFFIX_DEP_RE.finditer(sent):
        name, noun = m.group(1).strip(), m.group(2).lower()
        kind = "plugin" if noun in ("plugin", "framework", "loader") else "mod"
        (req.other_mods if kind == "mod" else req.plugins).append(
            DependencyReq(name=name, kind=kind, raw=m.group(0), source=source))

    for m in KNOWN_PLUGIN_RE.finditer(sent):
        req.plugins.append(
            DependencyReq(name=m.group(1), kind="plugin", raw=m.group(0), source=source))

    for nid in dict.fromkeys(ids):
        attached = False
        for dep in req.other_mods:
            if (dep.source == source and dep.name and dep.nexus_id is None
                    and dep.raw and dep.raw in sent):
                dep.nexus_id = nid
                attached = True
                break
        if not attached:
            req.other_mods.append(
                DependencyReq(nexus_id=nid, kind="mod",
                              raw=sent.strip()[:120], source=source))


def _dedupe_deps(req: ModRequirements) -> None:
    def key(d: DependencyReq) -> Tuple[str, str]:
        return (_norm_name(d.name) or f"#id:{d.nexus_id}", d.kind)

    seen: Dict[Tuple[str, str], DependencyReq] = {}
    for attr in ("other_mods", "plugins"):
        kept: List[DependencyReq] = []
        for dep in getattr(req, attr):
            k = key(dep)
            if k in seen:
                existing = seen[k]
                if existing.nexus_id is None and dep.nexus_id:
                    existing.nexus_id = dep.nexus_id
                if not existing.raw and dep.raw:
                    existing.raw = dep.raw
            else:
                seen[k] = dep
                kept.append(dep)
        setattr(req, attr, kept)


# ---------------------------------------------------------------------------
# CLI
def main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(
        description="Classify a mod archive and map its requirements (Mac-only, host-side).")
    ap.add_argument("archives", nargs="+", help="archive files to inspect")
    ap.add_argument("--records", metavar="JSON",
                    help="JSON file with install records (name/nexus_id/installed) "
                         "to resolve dependencies against")
    ap.add_argument("--compact", action="store_true", help="single-line JSON output")
    args = ap.parse_args(argv)

    records: List[Dict[str, Any]] = []
    if args.records:
        try:
            with open(args.records, encoding="utf-8") as fh:
                records = json.load(fh)
        except (OSError, ValueError) as exc:
            print(json.dumps({"error": f"cannot load records {args.records}: {exc}"}))
            return 2

    results: List[Dict[str, Any]] = []
    status = 0
    for archive in args.archives:
        try:
            cls = classify_archive(archive)
        except ArchiveError as exc:
            results.append({"archive": archive, "error": str(exc)})
            status = 1
            continue
        item = cls.to_dict()
        if records:
            item["dependency_resolution"] = cls.requirements.resolve_against(records)
        results.append(item)

    print(json.dumps(results, indent=None if args.compact else 2))
    return status


if __name__ == "__main__":
    sys.exit(main())
