"""Host-only (Mac) pre-install conflict detection for mod packages.

Given candidate mod specs plus the current install state, this module
flags problems BEFORE anything is installed:

* ``PATH_CONFLICT``       (blocker)  two mods writing the same target path
* ``KEYBIND_CONFLICT``    (warning)  two mods binding the same key
* ``HOOK_CONFLICT``       (warning)  two mods hooking the same function
* ``VERSION_MISMATCH``    (blocker)  mod requires a different game version
* ``MISSING_DEPENDENCY``  (blocker)  a required mod is not installed

Keybinds are extracted from Lua sources via ``RegisterKeyBind(...)`` and
hooks via ``RegisterHook("...")``.  Lua comments (line ``--`` and block
``--[[ ]]``) are stripped before matching, so commented-out calls are
ignored.  Version constraints may be exact (``"1.004.000"``) or ranged
(``">=1.003"``, ``"<2"``) and are compared numerically per component.

This module only reads local files; it never performs FTP, payload,
launch or console operations.
"""

from __future__ import annotations

import json
import os
import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence, Set, Tuple, Union

__all__ = [
    "PATH_CONFLICT", "KEYBIND_CONFLICT", "HOOK_CONFLICT",
    "VERSION_MISMATCH", "MISSING_DEPENDENCY",
    "SEVERITY_ERROR", "SEVERITY_WARNING",
    "Conflict", "ConflictReport", "ModSpec", "InstalledRegistry",
    "check_install", "check_batch",
    "parse_keybinds", "parse_hooks", "normalize_key",
    "strip_lua_comments", "version_allows",
]

PATH_CONFLICT = "PATH_CONFLICT"
KEYBIND_CONFLICT = "KEYBIND_CONFLICT"
HOOK_CONFLICT = "HOOK_CONFLICT"
VERSION_MISMATCH = "VERSION_MISMATCH"
MISSING_DEPENDENCY = "MISSING_DEPENDENCY"

SEVERITY_ERROR = "error"
SEVERITY_WARNING = "warning"


# --------------------------------------------------------------------------
# report model
# --------------------------------------------------------------------------

@dataclass
class Conflict:
    kind: str
    severity: str
    message: str
    mods: Tuple[str, ...] = ()
    detail: Dict = field(default_factory=dict)


@dataclass
class ConflictReport:
    conflicts: List[Conflict] = field(default_factory=list)

    def add(self, conflict: Conflict) -> None:
        self.conflicts.append(conflict)

    @property
    def blockers(self) -> List[Conflict]:
        return [c for c in self.conflicts if c.severity == SEVERITY_ERROR]

    @property
    def warnings(self) -> List[Conflict]:
        return [c for c in self.conflicts if c.severity == SEVERITY_WARNING]

    @property
    def ok(self) -> bool:
        """True when nothing blocking was found (warnings allowed)."""
        return not self.blockers

    def __iter__(self):
        return iter(self.conflicts)

    def __len__(self) -> int:
        return len(self.conflicts)

    def __str__(self) -> str:
        if not self.conflicts:
            return "no conflicts"
        return "\n".join(
            "[%s] %s: %s" % (c.severity, c.kind, c.message) for c in self.conflicts
        )


# --------------------------------------------------------------------------
# candidate mod spec
# --------------------------------------------------------------------------

@dataclass
class ModSpec:
    """A mod about to be installed (candidate), not yet on disk."""

    mod_id: str
    files: Tuple[str, ...] = ()              # target paths, relative to game root
    requires: Tuple[str, ...] = ()
    game_versions: Tuple[str, ...] = ()      # e.g. ("1.004.000",) or (">=1.003", "<2")
    lua_sources: Dict[str, str] = field(default_factory=dict)  # relpath -> lua text
    root: Optional[Path] = None              # optional staging dir scanned for *.lua

    def __post_init__(self):
        if not str(self.mod_id or "").strip():
            raise ValueError("ModSpec requires a non-empty mod_id")
        self.files = tuple(self.files)
        self.requires = tuple(self.requires)
        self.game_versions = tuple(self.game_versions)
        if self.root is not None:
            self.root = Path(self.root)


# --------------------------------------------------------------------------
# lua parsing
# --------------------------------------------------------------------------

def strip_lua_comments(text: str) -> str:
    """Blank out -- line comments and --[[ ]] block comments (quote-aware)."""
    out = []
    i, n = 0, len(text)
    in_str = None
    while i < n:
        c = text[i]
        if in_str:
            out.append(c)
            if c == "\\" and i + 1 < n:
                out.append(text[i + 1])
                i += 2
                continue
            if c == in_str:
                in_str = None
            i += 1
            continue
        if c in ("'", '"'):
            in_str = c
            out.append(c)
            i += 1
            continue
        if c == "-" and i + 1 < n and text[i + 1] == "-":
            if i + 3 < n and text[i + 2] == "[" and text[i + 3] == "[":
                end = text.find("]]", i + 4)
                i = n if end == -1 else end + 2
                out.append(" ")
                continue
            end = text.find("\n", i)
            i = n if end == -1 else end  # keep the newline itself
            out.append(" ")
            continue
        out.append(c)
        i += 1
    return "".join(out)


_KEYBIND_RE = re.compile(r"\bRegisterKeyBind\s*\(\s*([^\n,()]+?)\s*,")
_HOOK_RE = re.compile(r"\bRegisterHook\s*\(\s*[\"']([^\"'\n]+)[\"']")


def normalize_key(raw: str) -> str:
    """Key.F / \"F\" / 75  ->  F / F / 75 (uppercased token)."""
    s = raw.strip()
    if len(s) >= 2 and s[0] in ("'", '"') and s[-1] == s[0]:
        s = s[1:-1]
    if s.upper().startswith("KEY."):
        s = s[4:]
    return s.strip().upper()


def parse_keybinds(text: str) -> List[str]:
    """Extract normalized key names from RegisterKeyBind(...) calls."""
    clean = strip_lua_comments(text)
    return [normalize_key(m.group(1)) for m in _KEYBIND_RE.finditer(clean)]


def parse_hooks(text: str) -> List[str]:
    """Extract hook targets from RegisterHook(\"...\") calls."""
    clean = strip_lua_comments(text)
    return [m.group(1).strip() for m in _HOOK_RE.finditer(clean)]


# --------------------------------------------------------------------------
# versions
# --------------------------------------------------------------------------

def _version_tuple(text: str) -> Tuple[int, ...]:
    parts = re.findall(r"\d+", str(text))
    return tuple(int(p) for p in parts) if parts else (0,)


def version_allows(spec: str, actual: str) -> bool:
    """True when the constraint `spec` accepts the version `actual`."""
    m = re.match(r"^\s*(>=|<=|==|>|<)?\s*(.+?)\s*$", str(spec))
    if not m:
        return False
    op = m.group(1) or "=="
    bound = _version_tuple(m.group(2))
    actual_t = _version_tuple(actual)
    width = max(len(bound), len(actual_t))
    bound += (0,) * (width - len(bound))
    actual_t += (0,) * (width - len(actual_t))
    # does the *actual* version satisfy the constraint bound?
    return {
        "==": actual_t == bound,
        ">=": actual_t >= bound,
        "<=": actual_t <= bound,
        ">": actual_t > bound,
        "<": actual_t < bound,
    }[op]


# --------------------------------------------------------------------------
# installed registry (from the installer's install state)
# --------------------------------------------------------------------------

def normalize_relpath(p: str) -> str:
    import posixpath
    norm = posixpath.normpath(str(p).replace("\\", "/").strip())
    return "" if norm == "." else norm


class InstalledRegistry:
    """Read-only view of the current install state (mods + their files)."""

    def __init__(self, state: Union[dict, str, os.PathLike], game_root=None):
        if isinstance(state, (str, os.PathLike)):
            state = json.loads(Path(state).read_text(encoding="utf-8"))
        self.state = state or {}
        self.game_root = Path(game_root).expanduser().resolve() if game_root else None
        self.mods: Dict[str, Dict] = {}
        for mid, info in (self.state.get("mods") or {}).items():
            files = []
            for f in (info or {}).get("files") or []:
                rel = f["path"] if isinstance(f, dict) else str(f)
                files.append(normalize_relpath(rel))
            self.mods[mid] = {
                "files": files,
                "requires": list((info or {}).get("requires") or []),
            }
        self._key_index: Optional[Dict[str, Set[str]]] = None
        self._hook_index: Optional[Dict[str, Set[str]]] = None

    @classmethod
    def from_state_file(cls, path, game_root=None) -> "InstalledRegistry":
        return cls(path, game_root=game_root)

    @property
    def game_version(self) -> Optional[str]:
        return self.state.get("game_version")

    def owners_of(self, relpath: str) -> List[str]:
        norm = normalize_relpath(relpath)
        return sorted(mid for mid, info in self.mods.items() if norm in info["files"])

    def _lua_path(self, rel: str) -> Optional[Path]:
        if self.game_root is None:
            return None
        p = Path(rel.replace("\\", "/"))
        if p.is_absolute() or ".." in p.parts:
            return None
        target = self.game_root / p
        try:
            target.resolve().relative_to(self.game_root)
        except (ValueError, OSError):
            return None
        return target

    def _iter_lua_texts(self):
        for mid, info in self.mods.items():
            for rel in info["files"]:
                if not rel.lower().endswith(".lua"):
                    continue
                target = self._lua_path(rel)
                if target is None or not target.is_file():
                    continue
                try:
                    yield mid, target.read_text(encoding="utf-8", errors="replace")
                except OSError:
                    continue

    def keybind_index(self) -> Dict[str, Set[str]]:
        if self._key_index is None:
            index: Dict[str, Set[str]] = {}
            for mid, text in self._iter_lua_texts():
                for key in parse_keybinds(text):
                    index.setdefault(key, set()).add(mid)
            self._key_index = index
        return self._key_index

    def hook_index(self) -> Dict[str, Set[str]]:
        if self._hook_index is None:
            index: Dict[str, Set[str]] = {}
            for mid, text in self._iter_lua_texts():
                for hook in parse_hooks(text):
                    index.setdefault(hook, set()).add(mid)
            self._hook_index = index
        return self._hook_index


# --------------------------------------------------------------------------
# candidate script scanning
# --------------------------------------------------------------------------

def _candidate_texts(spec: ModSpec) -> List[str]:
    texts = [t for t in spec.lua_sources.values() if t]
    if spec.root is not None:
        for p in sorted(Path(spec.root).rglob("*.lua")):
            try:
                texts.append(p.read_text(encoding="utf-8", errors="replace"))
            except OSError:
                continue
    return texts


def collect_keybinds(spec: ModSpec) -> Set[str]:
    keys: Set[str] = set()
    for text in _candidate_texts(spec):
        keys.update(parse_keybinds(text))
    return keys


def collect_hooks(spec: ModSpec) -> Set[str]:
    hooks: Set[str] = set()
    for text in _candidate_texts(spec):
        hooks.update(parse_hooks(text))
    return hooks


# --------------------------------------------------------------------------
# checks
# --------------------------------------------------------------------------

def check_install(spec: ModSpec,
                  state: Union[dict, InstalledRegistry, str, os.PathLike],
                  game_version: Optional[str] = None,
                  game_root=None,
                  candidates: Sequence[ModSpec] = ()) -> ConflictReport:
    """Check one candidate against installed mods and optional batch peers.

    ``state`` may be the parsed install-state dict, a path to it, or an
    already-built ``InstalledRegistry``.  ``game_root`` is only needed
    when ``state`` is a dict/path and installed Lua sources should be
    scanned for keybind/hook conflicts.
    """
    if isinstance(state, InstalledRegistry):
        registry = state
    else:
        registry = InstalledRegistry(state, game_root=game_root)

    target_version = game_version or registry.game_version
    peers = [c for c in candidates if c.mod_id != spec.mod_id]
    report = ConflictReport()

    # 1. PATH_CONFLICT (blocker): same target path as another mod
    peer_files: Dict[str, List[ModSpec]] = {}
    for peer in peers:
        for f in peer.files:
            peer_files.setdefault(normalize_relpath(f), []).append(peer)
    for rel in spec.files:
        norm = normalize_relpath(rel)
        for owner in registry.owners_of(norm):
            if owner == spec.mod_id:
                continue  # reinstalling a mod over its own files is fine
            report.add(Conflict(
                PATH_CONFLICT, SEVERITY_ERROR,
                "%s wants to write '%s' which is already installed by '%s'"
                % (spec.mod_id, norm, owner),
                (spec.mod_id, owner),
                {"path": norm, "owner": owner},
            ))
        for peer in peer_files.get(norm, []):
            report.add(Conflict(
                PATH_CONFLICT, SEVERITY_ERROR,
                "%s and %s both write '%s'" % (spec.mod_id, peer.mod_id, norm),
                (spec.mod_id, peer.mod_id),
                {"path": norm, "owner": peer.mod_id, "candidate": True},
            ))

    # 2. KEYBIND_CONFLICT (warning): same key registered twice
    cand_keys = collect_keybinds(spec)
    key_index = registry.keybind_index()
    for key in sorted(cand_keys):
        for other in sorted(key_index.get(key, set()) - {spec.mod_id}):
            report.add(Conflict(
                KEYBIND_CONFLICT, SEVERITY_WARNING,
                "keybind '%s' already registered by '%s'" % (key, other),
                (spec.mod_id, other),
                {"key": key, "other": other},
            ))
        for peer in peers:
            if key in collect_keybinds(peer):
                report.add(Conflict(
                    KEYBIND_CONFLICT, SEVERITY_WARNING,
                    "keybind '%s' registered by both %s and %s"
                    % (key, spec.mod_id, peer.mod_id),
                    (spec.mod_id, peer.mod_id),
                    {"key": key},
                ))

    # 3. HOOK_CONFLICT (warning): same function hooked twice
    cand_hooks = collect_hooks(spec)
    hook_index = registry.hook_index()
    for hook in sorted(cand_hooks):
        for other in sorted(hook_index.get(hook, set()) - {spec.mod_id}):
            report.add(Conflict(
                HOOK_CONFLICT, SEVERITY_WARNING,
                "hook '%s' already registered by '%s'" % (hook, other),
                (spec.mod_id, other),
                {"hook": hook, "other": other},
            ))
        for peer in peers:
            if hook in collect_hooks(peer):
                report.add(Conflict(
                    HOOK_CONFLICT, SEVERITY_WARNING,
                    "hook '%s' registered by both %s and %s"
                    % (hook, spec.mod_id, peer.mod_id),
                    (spec.mod_id, peer.mod_id),
                    {"hook": hook},
                ))

    # 4. VERSION_MISMATCH (blocker): game version constraint not satisfied
    if spec.game_versions and target_version:
        if not any(version_allows(v, target_version) for v in spec.game_versions):
            report.add(Conflict(
                VERSION_MISMATCH, SEVERITY_ERROR,
                "%s requires game version %s but target is %s"
                % (spec.mod_id, " / ".join(spec.game_versions), target_version),
                (spec.mod_id,),
                {"required": list(spec.game_versions), "target": target_version},
            ))

    # 5. MISSING_DEPENDENCY (blocker): required mod absent
    installed_ids = set(registry.mods) - {spec.mod_id}
    peer_ids = {p.mod_id for p in peers}
    for dep in spec.requires:
        if dep not in installed_ids and dep not in peer_ids:
            report.add(Conflict(
                MISSING_DEPENDENCY, SEVERITY_ERROR,
                "%s requires '%s' which is not installed" % (spec.mod_id, dep),
                (spec.mod_id,),
                {"dependency": dep},
            ))

    return report


def _conflict_subject(conflict: Conflict) -> str:
    detail = conflict.detail or {}
    for key in ("path", "key", "hook", "dependency"):
        if key in detail:
            return "%s=%s" % (key, detail[key])
    return json.dumps(detail, sort_keys=True, default=str)


def check_batch(specs: Sequence[ModSpec],
                state: Union[dict, InstalledRegistry, str, os.PathLike],
                game_version: Optional[str] = None,
                game_root=None) -> ConflictReport:
    """Check a batch of candidates: each vs installed mods and vs peers.

    Dependencies satisfied by another member of the same batch do not
    trigger MISSING_DEPENDENCY.  Pairwise findings reported once per
    (kind, mod pair, subject).
    """
    specs = list(specs)
    merged = ConflictReport()
    seen = set()
    for spec in specs:
        report = check_install(spec, state, game_version=game_version,
                               game_root=game_root, candidates=specs)
        for conflict in report.conflicts:
            key = (conflict.kind, tuple(sorted(conflict.mods)),
                   _conflict_subject(conflict))
            if key in seen:
                continue
            seen.add(key)
            merged.add(conflict)
    return merged
