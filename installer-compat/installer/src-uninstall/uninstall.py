"""Host-side (Mac-only) uninstall, config cleanup and rollback for installed mods.

Scope
-----
This module operates purely on local host files.  It never contacts a
console: no FTP, no payloads, no launches, no network.  Everything it
touches lives under a local ``game_root`` directory that mirrors the
target's mod layout, plus the installer's own records.

Inputs
------
1. A mod manifest (the installer's install record), JSON::

       {
         "mod_id": "spawner",
         "name": "Townfall Item Spawner",
         "mod_version": "1.2.0",
         "requires": [],
         "files": [
           {"path": "Mods/spawner/scripts/main.lua", "sha256": "<hex>"}
         ],
         "config_entries": {
           "Mods/mods.txt": ["SpawnerMod : 1"],
           "ploader.ini":   ["payload=/data/payloads/spawner.prx"]
         }
       }

2. The current install state (``install_state.json``), JSON::

       {
         "game_version": "1.004.000",
         "mods": {
           "spawner": {
             "manifest": "/abs/.install-records/spawner.json",
             "requires": [],
             "files": [{"path": "Mods/...", "sha256": "..."}]
           }
         }
       }

Behaviour
---------
* Removes exactly the manifest file list - nothing else.  Paths that
  would escape the game root are rejected (``UnsafePathError``).
* A file whose current sha256 differs from the recorded one is flagged
  "modified since install"; the uninstall aborts with
  ``ConfirmationRequiredError`` unless the caller confirms via
  ``confirm_modified=True`` (or passes a callable that inspects the
  modified files and returns a bool).
* Config files (loadlist / ploader.ini) are edited in place: only the
  exact lines the installer recorded for this mod are removed, as a
  multiset, so duplicate rows and every other mod's rows survive
  byte-for-byte.  Shared config files are never deleted, and a config
  file is left in place even when it becomes empty.
* Files also claimed by another installed mod are kept (shared).
* If any installed mod lists this one in ``requires``, the uninstall is
  refused and the dependents are listed (``ModStillRequiredError``).
* Every mutating uninstall runs as a journalled transaction under
  ``transaction_root``; files are moved to per-transaction trash
  (never unlinked outright) and config/state snapshots are kept, so
  ``rollback(token)`` can restore everything.  Rollback restores the
  pre-uninstall snapshots, so config edits made after the uninstall are
  reverted too.
"""

from __future__ import annotations

import hashlib
import json
import os
import posixpath
import re
import shutil
import time
import uuid
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Sequence, Tuple, Union

__all__ = [
    "UninstallError", "ManifestError", "UnsafePathError",
    "ModStillRequiredError", "ConfirmationRequiredError", "RollbackError",
    "FileOutcome", "UninstallPlan", "UninstallResult", "Uninstaller",
    "load_manifest", "sha256_file", "normalize_relpath", "safe_join",
]

STATUS_OK = "ok"
STATUS_MODIFIED = "modified"      # hash differs from the install record
STATUS_MISSING = "missing"        # already absent on disk
STATUS_SHARED = "shared"          # also claimed by another installed mod

_DRIVE_RE = re.compile(r"^[A-Za-z]:")


# --------------------------------------------------------------------------
# errors
# --------------------------------------------------------------------------

class UninstallError(Exception):
    """Base class for uninstall failures."""


class ManifestError(UninstallError):
    """The manifest is missing required fields or malformed."""


class UnsafePathError(UninstallError):
    """A manifest path would escape the game root."""


class ModStillRequiredError(UninstallError):
    """Another installed mod depends on the mod being uninstalled."""

    def __init__(self, mod_id: str, dependents: Sequence[str]):
        self.mod_id = mod_id
        self.dependents = sorted(dependents)
        super().__init__(
            "refusing to uninstall '%s': still required by %s "
            "(uninstall the dependents first)" % (mod_id, ", ".join(self.dependents))
        )


class ConfirmationRequiredError(UninstallError):
    """Files were modified since install; the caller must confirm removal."""

    def __init__(self, mod_id: str, modified_files: Sequence["FileOutcome"]):
        self.mod_id = mod_id
        self.modified_files = list(modified_files)
        paths = ", ".join(f.path for f in self.modified_files)
        super().__init__(
            "refusing to uninstall '%s': %d file(s) modified since install (%s); "
            "re-run with confirm_modified=True to remove them anyway"
            % (mod_id, len(self.modified_files), paths)
        )


class RollbackError(UninstallError):
    """A rollback transaction could not be found or was already applied."""


# --------------------------------------------------------------------------
# data classes
# --------------------------------------------------------------------------

@dataclass
class FileOutcome:
    path: str
    status: str
    recorded_sha256: Optional[str] = None
    current_sha256: Optional[str] = None
    note: str = ""


@dataclass
class UninstallPlan:
    mod_id: str
    dependents: List[str] = field(default_factory=list)
    files: List[FileOutcome] = field(default_factory=list)
    config_entries: Dict[str, List[str]] = field(default_factory=dict)

    @property
    def modified_files(self) -> List[FileOutcome]:
        return [f for f in self.files if f.status == STATUS_MODIFIED]

    @property
    def shared_files(self) -> List[FileOutcome]:
        return [f for f in self.files if f.status == STATUS_SHARED]

    @property
    def missing_files(self) -> List[FileOutcome]:
        return [f for f in self.files if f.status == STATUS_MISSING]


@dataclass
class UninstallResult:
    mod_id: str
    removed: List[str] = field(default_factory=list)
    removed_modified: List[str] = field(default_factory=list)
    missing: List[str] = field(default_factory=list)
    shared_kept: List[str] = field(default_factory=list)
    config_lines_removed: Dict[str, int] = field(default_factory=dict)
    pruned_dirs: List[str] = field(default_factory=list)
    state_path: str = ""
    rollback_token: Optional[str] = None
    dry_run: bool = False


# --------------------------------------------------------------------------
# helpers
# --------------------------------------------------------------------------

def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


def normalize_relpath(p: str) -> str:
    """Canonical form used for comparisons/journals (posix, no leading ./)."""
    norm = posixpath.normpath(str(p).replace("\\", "/").strip())
    return "" if norm == "." else norm


def load_manifest(source: Union[dict, str, os.PathLike]) -> dict:
    """Load and validate a manifest from a path or an already-parsed dict."""
    if isinstance(source, dict):
        data = source
    else:
        p = Path(source)
        try:
            data = json.loads(p.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as exc:
            raise ManifestError("cannot read manifest %s: %s" % (p, exc))
    if not isinstance(data, dict):
        raise ManifestError("manifest must be a JSON object")
    mod_id = data.get("mod_id")
    if not isinstance(mod_id, str) or not mod_id.strip():
        raise ManifestError("manifest missing non-empty 'mod_id'")
    files = data.get("files", [])
    if not isinstance(files, list):
        raise ManifestError("manifest 'files' must be a list")
    for entry in files:
        if not isinstance(entry, dict) or not isinstance(entry.get("path"), str):
            raise ManifestError("each manifest file entry needs a 'path' string")
    config_entries = data.get("config_entries") or {}
    if not isinstance(config_entries, dict):
        raise ManifestError("manifest 'config_entries' must be an object")
    for key, lines in config_entries.items():
        if not isinstance(lines, list) or not all(isinstance(x, str) for x in lines):
            raise ManifestError("config_entries[%r] must be a list of strings" % key)
    return data


def safe_join(root: Path, relpath: str) -> Path:
    """Join relpath under root, refusing anything that escapes the root."""
    raw = str(relpath).strip()
    if _DRIVE_RE.match(raw):
        raise UnsafePathError("absolute path not allowed: %r" % relpath)
    rel = Path(raw.replace("\\", "/"))
    if rel.is_absolute() or ".." in rel.parts:
        raise UnsafePathError("path escapes the game root: %r" % relpath)
    root_resolved = root.resolve()
    target = (root_resolved / rel).resolve()
    try:
        target.relative_to(root_resolved)
    except ValueError:
        raise UnsafePathError("path escapes the game root: %r" % relpath)
    return target


def _atomic_write(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.parent / (".%s.tmp-%s" % (path.name, uuid.uuid4().hex[:8]))
    tmp.write_bytes(data)
    os.replace(tmp, path)


def write_json_atomic(path: Path, obj: Any) -> None:
    _atomic_write(Path(path), (json.dumps(obj, indent=2) + "\n").encode("utf-8"))


def _remove_lines(text: str, entries: List[str]) -> Tuple[str, int]:
    """Multiset-remove the recorded entries from text, line-wise.

    Lines are compared after stripping whitespace; each recorded entry
    removes at most one matching line, so duplicate rows belonging to
    other mods survive.  Kept lines keep their original bytes/endings.
    """
    counts: Dict[str, int] = {}
    for entry in entries:
        key = entry.strip()
        if key:
            counts[key] = counts.get(key, 0) + 1
    kept: List[str] = []
    removed = 0
    for line in text.splitlines(keepends=True):
        key = line.rstrip("\r\n").strip()
        if key and counts.get(key, 0) > 0:
            counts[key] -= 1
            removed += 1
            continue
        kept.append(line)
    return "".join(kept), removed


# --------------------------------------------------------------------------
# uninstaller
# --------------------------------------------------------------------------

class Uninstaller:
    """Plan and execute host-side uninstalls with journalled rollback."""

    def __init__(self, game_root, state_path=None, transaction_root=None):
        self.game_root = Path(game_root).expanduser().resolve()
        if state_path is not None:
            self.state_path = Path(state_path).expanduser()
        else:
            self.state_path = self.game_root / "install_state.json"
        if transaction_root is not None:
            self.transaction_root = Path(transaction_root).expanduser()
        else:
            self.transaction_root = self.game_root / ".uninstall-transactions"

    # -- state -----------------------------------------------------------

    def _load_state(self) -> dict:
        try:
            return json.loads(self.state_path.read_text(encoding="utf-8"))
        except FileNotFoundError:
            raise UninstallError("install state not found: %s" % self.state_path)
        except json.JSONDecodeError as exc:
            raise UninstallError("install state is corrupt: %s" % exc)

    def _write_state(self, state: dict) -> None:
        write_json_atomic(self.state_path, state)

    # -- planning ----------------------------------------------------------

    def plan(self, manifest: Union[dict, str, os.PathLike]) -> UninstallPlan:
        """Classify every manifest file without touching anything."""
        mf = load_manifest(manifest)
        mod_id = mf["mod_id"]
        state = self._load_state()
        mods = state.get("mods") or {}
        if mod_id not in mods:
            raise UninstallError("mod '%s' is not present in the install state" % mod_id)

        dependents = sorted(
            mid for mid, info in mods.items()
            if mid != mod_id and mod_id in list((info or {}).get("requires") or [])
        )

        owners: Dict[str, List[str]] = {}
        for mid, info in mods.items():
            for f in (info or {}).get("files") or []:
                rel = f["path"] if isinstance(f, dict) else str(f)
                owners.setdefault(normalize_relpath(rel), []).append(mid)

        outcomes: List[FileOutcome] = []
        for f in mf.get("files") or []:
            rel = f["path"]
            norm = normalize_relpath(rel)
            recorded = f.get("sha256")
            target = safe_join(self.game_root, rel)
            others = sorted(m for m in owners.get(norm, []) if m != mod_id)
            if target.is_dir():
                outcomes.append(FileOutcome(rel, STATUS_MODIFIED, recorded, None,
                                            "path is a directory, not a file"))
                continue
            if not target.exists():
                outcomes.append(FileOutcome(rel, STATUS_MISSING, recorded, None,
                                            "already absent"))
                continue
            current = sha256_file(target)
            if others:
                outcomes.append(FileOutcome(rel, STATUS_SHARED, recorded, current,
                                            "also claimed by %s" % ", ".join(others)))
            elif recorded and current != recorded:
                outcomes.append(FileOutcome(rel, STATUS_MODIFIED, recorded, current,
                                            "modified since install"))
            else:
                outcomes.append(FileOutcome(rel, STATUS_OK, recorded, current, ""))

        config = {k: list(v) for k, v in (mf.get("config_entries") or {}).items()}
        return UninstallPlan(mod_id, dependents, outcomes, config)

    # -- execution ---------------------------------------------------------

    def uninstall(self,
                  manifest: Union[dict, str, os.PathLike],
                  confirm_modified: Union[bool, Callable[[List[FileOutcome]], bool]] = False,
                  dry_run: bool = False,
                  prune_empty_dirs: bool = True) -> UninstallResult:
        """Uninstall one mod.  Raises on hard refusals; returns a summary."""
        plan = self.plan(manifest)

        if plan.dependents:
            raise ModStillRequiredError(plan.mod_id, plan.dependents)

        modified = plan.modified_files
        if modified:
            if callable(confirm_modified):
                confirmed = bool(confirm_modified(modified))
            else:
                confirmed = bool(confirm_modified)
            if not confirmed:
                raise ConfirmationRequiredError(plan.mod_id, modified)

        to_remove = [f for f in plan.files if f.status in (STATUS_OK, STATUS_MODIFIED)]
        missing = [f.path for f in plan.missing_files]
        shared = [f.path for f in plan.shared_files]

        if dry_run:
            return UninstallResult(
                mod_id=plan.mod_id,
                removed=[f.path for f in to_remove if f.status == STATUS_OK],
                removed_modified=[f.path for f in to_remove if f.status == STATUS_MODIFIED],
                missing=missing,
                shared_kept=shared,
                config_lines_removed={k: 0 for k in plan.config_entries},
                pruned_dirs=[],
                state_path=str(self.state_path),
                rollback_token=None,
                dry_run=True,
            )

        token = "%s-%s-%s" % (time.strftime("%Y%m%d-%H%M%S"), plan.mod_id,
                              uuid.uuid4().hex[:8])
        txn_dir = self.transaction_root / token
        files_trash = txn_dir / "files"
        config_bak = txn_dir / "config"
        files_trash.mkdir(parents=True, exist_ok=True)

        journal: Dict[str, Any] = {
            "token": token,
            "mod_id": plan.mod_id,
            "created": time.strftime("%Y-%m-%dT%H:%M:%S"),
            "status": "in_progress",
            "rolled_back": False,
            "files": [],
            "config_backups": [],
            "state_backup": None,
            "pruned_dirs": [],
        }
        journal_path = txn_dir / "journal.json"
        write_json_atomic(journal_path, journal)

        removed: List[str] = []
        removed_modified: List[str] = []
        pruned: List[str] = []
        config_removed: Dict[str, int] = {}

        # 1. move installed files into the transaction trash (never unlink)
        for f in to_remove:
            src = safe_join(self.game_root, f.path)
            norm = normalize_relpath(f.path)
            dest = files_trash / norm
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.move(str(src), str(dest))
            journal["files"].append(norm)
            (removed_modified if f.status == STATUS_MODIFIED else removed).append(f.path)
            write_json_atomic(journal_path, journal)

        # 2. prune only this mod's config lines
        for rel, entries in plan.config_entries.items():
            cfg = safe_join(self.game_root, rel)
            if not cfg.is_file():
                config_removed[rel] = 0
                continue
            text = cfg.read_text(encoding="utf-8")
            new_text, count = _remove_lines(text, entries)
            if count == 0:
                config_removed[rel] = 0
                continue
            backup = config_bak / normalize_relpath(rel)
            backup.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(cfg, backup)
            journal["config_backups"].append(normalize_relpath(rel))
            write_json_atomic(journal_path, journal)
            _atomic_write(cfg, new_text.encode("utf-8"))
            config_removed[rel] = count

        # 3. update the install state (snapshot first for rollback)
        state = self._load_state()
        state.setdefault("mods", {}).pop(plan.mod_id, None)
        if self.state_path.exists():
            shutil.copy2(self.state_path, txn_dir / "state.json")
            journal["state_backup"] = "state.json"
        self._write_state(state)
        write_json_atomic(journal_path, journal)

        # 4. prune directories that became empty (rmdir can never touch files)
        if prune_empty_dirs:
            for f in to_remove:
                d = safe_join(self.game_root, f.path).parent
                while d != self.game_root:
                    try:
                        d.rmdir()
                    except OSError:
                        break
                    pruned.append(d.relative_to(self.game_root).as_posix())
                    d = d.parent

        journal["status"] = "completed"
        journal["pruned_dirs"] = pruned
        write_json_atomic(journal_path, journal)

        return UninstallResult(
            mod_id=plan.mod_id,
            removed=removed,
            removed_modified=removed_modified,
            missing=missing,
            shared_kept=shared,
            config_lines_removed=config_removed,
            pruned_dirs=pruned,
            state_path=str(self.state_path),
            rollback_token=token,
            dry_run=False,
        )

    # -- rollback ----------------------------------------------------------

    def _find_transaction(self, token: str) -> Path:
        if not re.fullmatch(r"[A-Za-z0-9._-]+", token or ""):
            raise RollbackError("invalid transaction token: %r" % token)
        txn_dir = self.transaction_root / token
        if not (txn_dir / "journal.json").is_file():
            raise RollbackError("unknown transaction: %r" % token)
        return txn_dir

    def rollback(self, token: str = "latest") -> dict:
        """Undo a completed uninstall (restore files, config and state)."""
        if token == "latest":
            candidates: List[Path] = []
            if self.transaction_root.is_dir():
                for d in sorted(self.transaction_root.iterdir()):
                    if not (d / "journal.json").is_file():
                        continue
                    try:
                        data = json.loads((d / "journal.json").read_text(encoding="utf-8"))
                    except (OSError, json.JSONDecodeError):
                        continue
                    if not data.get("rolled_back"):
                        candidates.append(d)
            if not candidates:
                raise RollbackError("no transactions available to roll back")
            txn_dir = candidates[-1]
            token = txn_dir.name
        else:
            txn_dir = self._find_transaction(token)

        journal = json.loads((txn_dir / "journal.json").read_text(encoding="utf-8"))
        if journal.get("rolled_back"):
            raise RollbackError("transaction %r was already rolled back" % token)

        files_restored: List[str] = []
        skipped_existing: List[str] = []
        for rel in journal.get("files") or []:
            src = txn_dir / "files" / rel
            dest = safe_join(self.game_root, rel)
            if not src.exists():
                continue
            if dest.exists():
                skipped_existing.append(rel)  # e.g. reinstalled meanwhile
                continue
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.move(str(src), str(dest))
            files_restored.append(rel)

        configs_restored: List[str] = []
        for rel in journal.get("config_backups") or []:
            src = txn_dir / "config" / rel
            dest = safe_join(self.game_root, rel)
            if src.exists():
                shutil.copy2(src, dest)
                configs_restored.append(rel)

        state_restored = False
        state_backup = journal.get("state_backup")
        if state_backup and (txn_dir / state_backup).is_file():
            shutil.copy2(txn_dir / state_backup, self.state_path)
            state_restored = True

        journal["rolled_back"] = True
        journal["status"] = "rolled_back"
        write_json_atomic(txn_dir / "journal.json", journal)

        return {
            "token": token,
            "mod_id": journal.get("mod_id"),
            "files_restored": files_restored,
            "skipped_existing": skipped_existing,
            "configs_restored": configs_restored,
            "state_restored": state_restored,
        }
