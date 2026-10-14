#!/usr/bin/env python3
"""modbridge integration adapter — the mod manager's single entry point.

Mac-only host tool (host-tested, stdlib only). It NEVER contacts a console:
every path it touches is a parameter. It wires together the four sibling
modules that previously could not interoperate:

    detector.py   -> classify the archive (refuse UE4SS-runtime / non-Lua)
    compat/checker.py -> static Lua-vs-catalog grade (optional catalog)
    conflicts.py  -> pre-install conflict check against installed mods
    installer.py  -> journalled install + verification
    uninstall.py  -> dependents check + journalled uninstall + rollback

THIS FILE CLOSES THE #22/#23 INTEGRATION GAP
--------------------------------------------
The installer's manifest contract (manifests/<Mod>.json, schema
modbridge-manifest/1: ABSOLUTE install_path values, structured
config_entries) and the uninstaller/conflicts contract (RELATIVE
files[].path lists + config_entries as relpath->line-list + a shared
install_state.json registry) are different formats, and nothing wired
conflict checking into the install path. The independent tester bridged
them with a test-only adapter (tests/src-tester/integration_tests.py);
this module is the PRODUCTION bridge.

UNIFIED STATE (one file feeds both uninstall.py and conflicts.py)
-----------------------------------------------------------------
After every successful install the adapter maintains a per-title registry
at  <state_dir>/install_state/<TITLE_ID>.json :

    {
      "schema": "modbridge-install-state/1",
      "game_version": "1.004.000" | null,
      "mods": {
        "<mod_dir>": {
          "manifest": "/abs/.../manifests/<mod_dir>.json",
          "requires": ["OtherMod", ...],
          "files": [{"path": "ue4ss/Mods/<mod_dir>/<rel>",
                     "sha256": "<archive sha256>"}],
          "ploader": {"name": "<prx>", "mode": "2",
                      "section": "<TITLE_ID>",
                      "section_created": false}
        }
      }
    }

uninstall.Uninstaller reads this file directly (state_path=) and
conflicts.InstalledRegistry reads the same file, so "what is installed"
has exactly one answer for both sides.

MANIFEST RECONCILIATION (installer -> uninstaller), see
installer_manifest_to_uninstall():
    game_root argument            == installer roots_base; the title tree
                                     is <game_root>/<TITLE_ID>/ and the
                                     uninstaller's game_root is THAT dir,
                                     so manifest install_rel values
                                     ("ue4ss/Mods/...") resolve as paths.
    files[].install_rel           -> uninstaller files[].path
    files[].original_sha256       -> uninstaller files[].sha256
    config_entries.loadlist_rows  -> config_entries["loadlist.txt"] lines
    config_entries.mods_txt_row   -> config_entries["ue4ss/Mods/mods.txt"]
    enabled_marker_rel (when
      enabled_flag_source ==
      "installer-created")        -> enabled_markers[] (existence-only,
                                     never hash-gated)
    ue4ss_overrides_path          -> extra files[] entry, sha256 null
                                     (runtime-owned config, no tamper gate)
    stale_files_left_in_place     -> extra files[] entries, sha256 null
                                     (upgrades must still uninstall clean)
    the installer manifest itself -> extra files[] entry, sha256 null
                                     (removed by the uninstaller, restored
                                     by its rollback)

ploader.ini lives at <plugins_dir>/ploader.ini, OUTSIDE the uninstaller's
game_root, so the uninstaller cannot touch it (the tester's bridge quietly
left the real line behind). The adapter removes it itself AFTER the
uninstall transaction commits: the exact "<prx>=<mode>" line in the
[TITLE_ID] section goes away only when no other installed mod still needs
the same line, and an installer-created section is dropped when it becomes
empty. A snapshot is kept inside the uninstall transaction directory so
rollback_uninstall() restores it together with everything else.

PIPELINE POLICIES (documented refusals, never silent):
    detect   ue4ss-runtime archives are refused outright (UE4SS itself is
             provided by the PS5 runtime layer); runnable_on_ps5 == "no"
             is refused; only ue4ss-lua (or mixed containing ue4ss-lua
             with runnable != "no") proceeds.
    compat   grade "unsupported" blocks; "partly-compatible" and
             "unknown" warn (unknown can be made blocking with
             block_on_unknown_compat=True).
    conflicts  error-severity conflicts (PATH_CONFLICT, VERSION_MISMATCH,
             MISSING_DEPENDENCY) block; warnings install with a warning.
    install/verify  installer.py semantics, unchanged.

The adapter is single-process by design (one mod manager at a time), like
the modules it wraps.
"""

from __future__ import annotations

import importlib.util
import json
import os
import sys
import tempfile
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, Tuple, Union

__all__ = [
    "InstallPipeline", "InstallResult", "UninstallResult", "CheckResult",
    "AdapterError", "CorruptStateError",
    "installer_manifest_to_uninstall", "SCHEMA_INSTALL_STATE",
    "SCHEMA_UNINSTALL_MANIFEST",
]

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

import detector as detector_mod            # noqa: E402  (same directory)
import installer as installer_mod          # noqa: E402  (same directory)
import uninstall as uninstall_mod          # noqa: E402  (same directory)
import conflicts as conflicts_mod          # noqa: E402  (same directory)

SCHEMA_INSTALL_STATE = "modbridge-install-state/1"
SCHEMA_UNINSTALL_MANIFEST = "modbridge-uninstall-manifest/1"

# compat/checker.py lives in the sibling compat/ directory; load it by
# file path so its generic module name never collides with anything else.
_COMPAT_CHECKER_PATH = os.path.abspath(
    os.path.join(_HERE, "..", "compat", "checker.py"))
_COMPAT_MODULE_CACHE: Any = None
_COMPAT_MODULE_LOADED = False


def _compat_module():
    """Load (once) the static Lua compatibility grader, or None if absent."""
    global _COMPAT_MODULE_CACHE, _COMPAT_MODULE_LOADED
    if not _COMPAT_MODULE_LOADED:
        _COMPAT_MODULE_LOADED = True
        if os.path.isfile(_COMPAT_CHECKER_PATH):
            spec = importlib.util.spec_from_file_location(
                "modbridge_compat_checker", _COMPAT_CHECKER_PATH)
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
            _COMPAT_MODULE_CACHE = module
    return _COMPAT_MODULE_CACHE


# --------------------------------------------------------------------------
# errors
# --------------------------------------------------------------------------

class AdapterError(Exception):
    """Base class for adapter-level failures."""


class CorruptStateError(AdapterError):
    """The unified install state exists but cannot be parsed."""


# --------------------------------------------------------------------------
# result objects
# --------------------------------------------------------------------------

@dataclass
class InstallResult:
    """Everything the manager needs after install_mod()/a refusal.

    stage is the last stage reached: detect / compat / conflicts / install
    / verify / state, or "done" on success.  A refusal before install sets
    `refusal`; a failure during/after install sets `error`+`error_kind`.
    Detection/compat/conflict sub-results are always carried along.
    """

    ok: bool
    mod_id: str = ""
    title_id: str = ""
    stage: str = ""
    refusal: str = ""
    error: str = ""
    error_kind: str = ""
    detection: Optional[Dict[str, Any]] = None
    compat: Optional[Dict[str, Any]] = None
    conflicts: Optional[Any] = None            # conflicts.ConflictReport
    conflict_blockers: List[str] = field(default_factory=list)
    conflict_warnings: List[str] = field(default_factory=list)
    manifest: Optional[Dict[str, Any]] = None
    manifest_path: str = ""
    verify: Optional[Dict[str, Any]] = None
    warnings: List[str] = field(default_factory=list)
    state_path: str = ""


@dataclass
class UninstallResult:
    """Adapter-level uninstall outcome (wraps uninstall.UninstallResult).

    stage: manifest / plan / dependents / confirm / uninstall / ploader /
    done.  `core` is uninstall.py's own UninstallResult (removed files,
    rollback token); `plan` is a plain-dict summary of the classified
    plan; `ploader` describes the plugins/ploader.ini cleanup.
    """

    ok: bool
    mod_id: str = ""
    title_id: str = ""
    stage: str = ""
    error: str = ""
    error_kind: str = ""
    plan: Optional[Dict[str, Any]] = None
    core: Optional[Any] = None                 # uninstall.UninstallResult
    modified_files: List[str] = field(default_factory=list)
    ploader: Optional[Dict[str, Any]] = None
    manifest_path: str = ""
    state_path: str = ""


@dataclass
class CheckResult:
    """Dry-run outcome of check_mod(): every pre-install stage, no writes."""

    ok: bool
    mod_id: str = ""
    title_id: str = ""
    stage: str = ""
    refusal: str = ""
    detection: Optional[Dict[str, Any]] = None
    compat: Optional[Dict[str, Any]] = None
    conflicts: Optional[Any] = None
    conflict_blockers: List[str] = field(default_factory=list)
    conflict_warnings: List[str] = field(default_factory=list)
    planned_files: List[str] = field(default_factory=list)
    planned_marker: str = ""
    warnings: List[str] = field(default_factory=list)


@dataclass
class _Pre:
    """Internal preflight outcome shared by install_mod()/check_mod()."""

    ok: bool
    stage: str
    refusal: str
    mod_id: str = ""
    detection: Optional[Dict[str, Any]] = None
    compat: Optional[Dict[str, Any]] = None
    conflicts: Optional[Any] = None
    planned_files: List[str] = field(default_factory=list)
    marker_rel: str = ""
    requires: List[str] = field(default_factory=list)


# --------------------------------------------------------------------------
# small helpers
# --------------------------------------------------------------------------

def _str_list(value: Any, field_name: str) -> Optional[List[str]]:
    """Validate an optional metadata field as a list of non-empty strings."""
    if value is None:
        return []
    if not isinstance(value, (list, tuple)) or not all(
            isinstance(x, str) and x.strip() for x in value):
        return None
    return [str(x) for x in value]


def _conflict_lines(report: Any) -> Tuple[List[str], List[str]]:
    blockers = ["[%s] %s" % (c.kind, c.message) for c in report.blockers]
    warnings = ["[%s] %s" % (c.kind, c.message) for c in report.warnings]
    return blockers, warnings


def _plan_summary(plan: Any) -> Dict[str, Any]:
    return {
        "mod_id": plan.mod_id,
        "dependents": list(plan.dependents),
        "files": [{"path": f.path, "status": f.status, "note": f.note}
                  for f in plan.files],
        "markers": [{"path": m.path, "status": m.status}
                    for m in plan.markers],
        "config_entries": {k: list(v) for k, v in plan.config_entries.items()},
    }


def _planned_layout(archive_path: str, mod_dir_name: str) -> Dict[str, Any]:
    """Mirror Installer.install()'s archive planning WITHOUT writing anything.

    Uses the same SafeArchive validation the installer will use, so a plan
    that passes here is exactly what install() would write.  Returns the
    title-root-relative target paths, the Lua sources (for compat/conflict
    scanning) and the installer-created enable-marker path (None when the
    archive ships its own enabled.txt).
    """
    with installer_mod.SafeArchive(archive_path) as arc:
        mod_root = arc.find_mod_root()
        if mod_root is None:
            # same messages as Installer.install() (its helper is private;
            # reuse it so wording cannot drift)
            if installer_mod._looks_like_ue4ss_runtime(arc.entries):
                raise installer_mod.UnsafeArchiveError(
                    ["no Scripts/main.lua in archive; this zip is the "
                     "UE4SS runtime itself (UE4SS.dll / dwmapi.dll at the "
                     "archive root, no Mods/ tree) — UE4SS runtime "
                     "is provided by the PS5 runtime layer; do not "
                     "install as a mod"])
            lower = arc.lowercase_mainlua_candidates()
            hint = (" (found %r — UE4SS requires exact 'Scripts/main.lua' "
                    "case)" % lower[:3]) if lower else ""
            raise installer_mod.UnsafeArchiveError(
                ["no Scripts/main.lua in archive; not a UE4SS Lua mod%s"
                 % hint])
        files: List[str] = []
        lua_sources: Dict[str, str] = {}
        marker_shipped = False
        for e in arc.entries:
            if e.is_dir:
                continue
            if mod_root != "" and not e.norm.startswith(mod_root):
                continue
            rel = e.norm[len(mod_root):]
            if not rel:
                continue
            files.append("ue4ss/Mods/%s/%s" % (mod_dir_name, rel))
            if rel.lower().endswith(".lua"):
                lua_sources[rel] = arc.read(e.norm).decode(
                    "utf-8", errors="replace")
            if rel == installer_mod.ENABLED_MARKER_NAME:
                marker_shipped = True
        marker_rel = ""
        if not marker_shipped:
            marker_rel = "ue4ss/Mods/%s/%s" % (
                mod_dir_name, installer_mod.ENABLED_MARKER_NAME)
        return {"files": files, "lua_sources": lua_sources,
                "marker_rel": marker_rel, "mod_root": mod_root}


def _compat_scan(lua_sources: Dict[str, str], catalog_path: str) -> Dict:
    """Extract the candidate Lua into a temp dir and run compat checker."""
    checker = _compat_module()
    if checker is None:
        raise AdapterError("compat checker not available at %s"
                           % _COMPAT_CHECKER_PATH)
    with tempfile.TemporaryDirectory(prefix="modbridge-compat-") as td:
        for rel in sorted(lua_sources):
            dest = os.path.join(td, *rel.split("/"))
            os.makedirs(os.path.dirname(dest) or td, exist_ok=True)
            with open(dest, "w", encoding="utf-8") as f:
                f.write(lua_sources[rel])
        return checker.scan(td, catalog_path)


def installer_manifest_to_uninstall(
        imf: Dict[str, Any], manifest_relpath: Optional[str] = None
) -> Dict[str, Any]:
    """Convert an installer manifest (modbridge-manifest/1) into the
    uninstaller's manifest contract — the #22 reconciliation, as a pure
    function so the manager can inspect/use it directly.

    Paths become title-root-relative (the uninstaller's game_root is the
    title root), config entries become relpath -> line-list, the enable
    marker moves to enabled_markers[], and installer-owned artifacts
    (ue4ss-overrides ini, stale upgrade leftovers, the manifest itself)
    are appended as sha256-null file entries (removed without the
    modified-file gate, restorable by rollback).
    """
    mod_dir = imf.get("mod_dir") or ""
    files = [{"path": f["install_rel"], "sha256": f.get("original_sha256")}
             for f in imf.get("files", [])
             if isinstance(f, dict) and f.get("install_rel")]
    ce = imf.get("config_entries") or {}
    cfg: Dict[str, List[str]] = {}
    if ce.get("loadlist_rows"):
        cfg["loadlist.txt"] = list(ce["loadlist_rows"])
    if ce.get("mods_txt_row"):
        cfg["ue4ss/Mods/mods.txt"] = [ce["mods_txt_row"]]

    markers: List[str] = []
    if ce.get("enabled_flag_source") == "installer-created":
        rel = ce.get("enabled_marker_rel")
        if rel:
            markers.append(rel)

    extra: List[Dict[str, Any]] = []
    ov = ce.get("ue4ss_overrides_path")
    title_root = imf.get("title_root")
    if ov and title_root:
        rel = os.path.relpath(ov, title_root).replace(os.sep, "/")
        if not rel.startswith(".."):
            extra.append({"path": rel, "sha256": None})
    for stale in imf.get("stale_files_left_in_place") or []:
        extra.append({"path": "ue4ss/Mods/%s/%s" % (mod_dir, stale),
                      "sha256": None})
    if manifest_relpath is None:
        manifest_relpath = "manifests/%s.json" % mod_dir
    extra.append({"path": manifest_relpath, "sha256": None})

    return {
        "schema": SCHEMA_UNINSTALL_MANIFEST,
        "mod_id": mod_dir,
        "name": imf.get("mod_name", mod_dir),
        "mod_version": str(imf.get("version", "1")),
        # informational only: dependents come from the unified state
        "requires": [],
        "files": files + extra,
        "enabled_markers": markers,
        "config_entries": cfg,
    }


# --------------------------------------------------------------------------
# the pipeline
# --------------------------------------------------------------------------

class InstallPipeline:
    """Single entry point: detect -> compat -> conflicts -> install ->
    verify (plus uninstall/check helpers).

    Constructor arguments are DEFAULTS for every call; each method also
    accepts per-call overrides.  plugins_dir defaults to
    <game_root>/plugins and state_dir to <game_root>/state when not given.
    game_version seeds the unified registry state (used by the
    VERSION_MISMATCH conflict check).
    """

    def __init__(self,
                 plugins_dir: Optional[str] = None,
                 state_dir: Optional[str] = None,
                 catalog_path: Optional[str] = None,
                 game_version: Optional[str] = None,
                 block_on_unknown_compat: bool = False) -> None:
        self.plugins_dir = os.path.abspath(plugins_dir) if plugins_dir else None
        self.state_dir = os.path.abspath(state_dir) if state_dir else None
        self.catalog_path = os.path.abspath(catalog_path) if catalog_path else None
        self.game_version = game_version
        self.block_on_unknown_compat = block_on_unknown_compat

    # ---- path derivation ------------------------------------------------

    def _paths(self, game_root: str) -> Tuple[str, str]:
        plugins = self.plugins_dir or os.path.join(game_root, "plugins")
        state_dir = self.state_dir or os.path.join(game_root, "state")
        return plugins, state_dir

    def _state_paths(self, game_root: str,
                     title_id: str) -> Tuple[str, str]:
        _plugins, state_dir = self._paths(game_root)
        return (os.path.join(state_dir, "install_state", title_id + ".json"),
                os.path.join(state_dir, "uninstall-transactions", title_id))

    def _load_state(self, game_root: str, title_id: str) -> Dict[str, Any]:
        path, _txn = self._state_paths(game_root, title_id)
        if not os.path.exists(path):
            return {"schema": SCHEMA_INSTALL_STATE,
                    "game_version": self.game_version, "mods": {}}
        try:
            with open(path, "r", encoding="utf-8") as f:
                state = json.load(f)
        except (OSError, ValueError) as e:
            raise CorruptStateError(
                "unified install state corrupt (%s): %s" % (path, e))
        if not isinstance(state, dict) or \
                not isinstance(state.get("mods", {}), dict):
            raise CorruptStateError(
                "unified install state malformed (%s): expected object with "
                "'mods' object" % path)
        return state

    def _write_state(self, game_root: str, title_id: str,
                     state: Dict[str, Any]) -> str:
        path, _txn = self._state_paths(game_root, title_id)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        installer_mod.atomic_write_bytes(
            path, json.dumps(state, indent=1, sort_keys=True).encode("utf-8"))
        return path

    def _register_mod(self, game_root: str, title_id: str,
                      manifest: Dict[str, Any],
                      requires: List[str]) -> str:
        """Record one installed mod into the unified registry state."""
        state = self._load_state(game_root, title_id)
        state["schema"] = SCHEMA_INSTALL_STATE
        if state.get("game_version") is None and self.game_version:
            state["game_version"] = self.game_version
        ce = manifest.get("config_entries") or {}
        state.setdefault("mods", {})[manifest["mod_dir"]] = {
            "manifest": installer_mod.manifest_path_of(manifest),
            "requires": list(requires or []),
            "files": [{"path": f["install_rel"],
                       "sha256": f.get("original_sha256")}
                      for f in manifest.get("files", [])],
            "ploader": {
                "name": ce.get("ploader_prx_name"),
                "mode": ce.get("ploader_prx_mode"),
                "section": ce.get("ploader_section"),
                "section_created": bool(ce.get("ploader_section_created")),
            },
        }
        return self._write_state(game_root, title_id, state)

    # ---- preflight (detect -> compat -> conflicts) ------------------------

    def _preflight(self, archive_path: str, metadata: Union[dict, str],
                   title_id: str, game_root: str, catalog: Optional[str],
                   warnings: List[str]) -> _Pre:
        """Run every pre-install stage; NO filesystem mutation happens here
        (state is read-only, the archive is only inspected)."""
        game_root = os.path.abspath(game_root)

        # ---- detect ----------------------------------------------------
        if isinstance(metadata, (str, os.PathLike)):
            metadata = installer_mod.load_metadata(str(metadata))
        if not isinstance(metadata, dict):
            return _Pre(False, "detect", "metadata must be a JSON object")
        try:
            md = installer_mod.validate_metadata(metadata)
        except installer_mod.MetadataError as e:
            return _Pre(False, "detect", "metadata: %s" % e)
        requires = _str_list(metadata.get("requires"), "requires")
        game_versions = _str_list(metadata.get("game_versions"),
                                  "game_versions")
        if requires is None or game_versions is None:
            return _Pre(False, "detect",
                        "metadata 'requires'/'game_versions' must be lists "
                        "of non-empty strings")
        mod_dir = installer_mod.sanitize_component(md["name"], 64, "name")
        if not isinstance(title_id, str) or \
                not installer_mod.TITLE_ID_RE.fullmatch(title_id):
            return _Pre(False, "detect",
                        "invalid title id %r (want e.g. PPSA33286)" % title_id)

        try:
            cls = detector_mod.classify_archive(archive_path)
        except detector_mod.ArchiveError as e:
            return _Pre(False, "detect", "archive: %s" % e, mod_id=mod_dir)
        det = cls.to_dict()
        label = cls.label.value if hasattr(cls.label, "value") else str(cls.label)
        types = [t.value if hasattr(t, "value") else str(t) for t in cls.types]
        if label == detector_mod.ModType.UE4SS_RUNTIME.value:
            return _Pre(
                False, "detect",
                "refused: archive is the UE4SS runtime itself — the PS5 "
                "runtime layer provides UE4SS; no part of it is installed "
                "as a mod (%s)" % "; ".join(cls.install_notes),
                mod_id=mod_dir, detection=det, requires=requires)
        if cls.runnable_on_ps5 == "no":
            return _Pre(
                False, "detect",
                "refused: detector label %r has runnable_on_ps5='no' — "
                "%s" % (label, "; ".join(cls.install_notes)),
                mod_id=mod_dir, detection=det, requires=requires)
        if label == detector_mod.ModType.UE4SS_LUA.value:
            pass
        elif label == detector_mod.ModType.MIXED.value and \
                detector_mod.ModType.UE4SS_LUA.value in types:
            warnings.append(
                "mixed archive (%s): only the UE4SS Lua mod tree is "
                "installed; other payload is out of scope" %
                ", ".join(t for t in types
                          if t != detector_mod.ModType.UE4SS_LUA.value))
        else:
            return _Pre(
                False, "detect",
                "refused: detector label %r (runnable_on_ps5=%r, "
                "required_runtime=%r) is not an installable UE4SS Lua mod "
                "— %s" % (label, cls.runnable_on_ps5, cls.required_runtime,
                          "; ".join(cls.install_notes)),
                mod_id=mod_dir, detection=det, requires=requires)

        # plan the install exactly like Installer.install would (validates
        # the zip: traversal/symlink/size/duplicate guards, mod root)
        try:
            planned = _planned_layout(archive_path, mod_dir)
        except installer_mod.InstallError as e:
            return _Pre(False, "detect", "archive: %s" % e,
                        mod_id=mod_dir, detection=det, requires=requires)

        # ---- compat (optional catalog) ---------------------------------
        compat_summary: Optional[Dict[str, Any]] = None
        if catalog:
            if _compat_module() is None:
                warnings.append(
                    "compat checker not available at %s; compat stage "
                    "skipped" % _COMPAT_CHECKER_PATH)
            elif not planned["lua_sources"]:
                warnings.append("no Lua sources found; compat stage skipped")
            else:
                try:
                    result = _compat_scan(planned["lua_sources"], catalog)
                except (OSError, ValueError, AdapterError) as e:
                    return _Pre(False, "compat", "compat check failed: %s" % e,
                                mod_id=mod_dir, detection=det,
                                requires=requires)
                compat_summary = {
                    "grade": result.get("grade"),
                    "grade_reason": result.get("grade_reason"),
                    "catalog": os.path.abspath(catalog),
                    "catalog_version": result.get("catalog_version"),
                    "total_calls": result.get("total_calls"),
                    "matched_calls": result.get("matched_calls"),
                    "missing_apis": [a.get("api")
                                     for a in result.get("missing", [])],
                    "behaves_differently_apis": [
                        a.get("api")
                        for a in result.get("behaves_differently", [])],
                }
                grade = result.get("grade")
                if grade == "unsupported":
                    return _Pre(
                        False, "compat",
                        "refused: compat grade 'unsupported' — %s"
                        % result.get("grade_reason"),
                        mod_id=mod_dir, detection=det, compat=compat_summary,
                        requires=requires)
                if grade == "unknown" and self.block_on_unknown_compat:
                    return _Pre(
                        False, "compat",
                        "refused: compat grade 'unknown' (strict mode) — %s"
                        % result.get("grade_reason"),
                        mod_id=mod_dir, detection=det, compat=compat_summary,
                        requires=requires)
                if grade == "partly-compatible":
                    warnings.append(
                        "compat grade 'partly-compatible': missing %s; "
                        "behaves-differently %s" % (
                            compat_summary["missing_apis"],
                            compat_summary["behaves_differently_apis"]))
                elif grade == "unknown":
                    warnings.append(
                        "compat grade 'unknown' (catalog incomplete): %s"
                        % result.get("grade_reason"))

        # ---- conflicts against all installed mods ----------------------
        try:
            state = self._load_state(game_root, title_id)
        except CorruptStateError as e:
            return _Pre(False, "conflicts", str(e), mod_id=mod_dir,
                        detection=det, compat=compat_summary,
                        requires=requires)
        title_root = os.path.join(game_root, title_id)
        spec_files = list(planned["files"])
        if planned["marker_rel"]:
            spec_files.append(planned["marker_rel"])
        spec = conflicts_mod.ModSpec(
            mod_id=mod_dir, files=tuple(spec_files),
            requires=tuple(requires),
            game_versions=tuple(game_versions),
            lua_sources=dict(planned["lua_sources"]))
        report = conflicts_mod.check_install(
            spec, state,
            game_version=self.game_version or state.get("game_version"),
            game_root=title_root)
        blockers, cwarns = _conflict_lines(report)
        warnings.extend(cwarns)
        if blockers:
            return _Pre(False, "conflicts",
                        "refused: conflict blockers: %s" % " | ".join(blockers),
                        mod_id=mod_dir, detection=det, compat=compat_summary,
                        conflicts=report, requires=requires)
        return _Pre(True, "conflicts", "", mod_id=mod_dir, detection=det,
                    compat=compat_summary, conflicts=report,
                    planned_files=planned["files"],
                    marker_rel=planned["marker_rel"], requires=requires)

    # ---- public API -------------------------------------------------------

    def install_mod(self, archive_path: str, metadata: Union[dict, str],
                    title_id: str, game_root: str,
                    catalog_path: Optional[str] = None) -> InstallResult:
        """detect -> compat -> conflicts -> install -> verify -> register.

        Returns InstallResult; refusals/failures are REPORTED, not raised
        (predictable domain errors are converted; unexpected exceptions
        propagate).  `metadata` is a dict (or path to a metadata JSON) as
        accepted by installer.py, plus optional lists 'requires' (mod ids)
        and 'game_versions' (constraints like ">=1.003") consumed by the
        conflict check.  `catalog_path` overrides the constructor default.
        """
        game_root = os.path.abspath(game_root)
        archive_path = os.path.abspath(archive_path)
        catalog = catalog_path or self.catalog_path
        warnings: List[str] = []
        pre = self._preflight(archive_path, metadata, title_id, game_root,
                              catalog, warnings)
        blockers: List[str] = []
        cwarns: List[str] = []
        if pre.conflicts is not None:
            blockers, cwarns = _conflict_lines(pre.conflicts)
        base = dict(
            mod_id=pre.mod_id, title_id=title_id, detection=pre.detection,
            compat=pre.compat, conflicts=pre.conflicts,
            conflict_blockers=blockers, conflict_warnings=cwarns,
            warnings=warnings)
        if not pre.ok:
            return InstallResult(ok=False, stage=pre.stage,
                                 refusal=pre.refusal, error=pre.refusal,
                                 error_kind="Refused", **base)

        plugins, state_dir = self._paths(game_root)
        try:
            inst = installer_mod.Installer(game_root, plugins, state_dir)
            manifest = inst.install(archive_path, metadata, title_id)
        except installer_mod.InstallError as e:
            return InstallResult(ok=False, stage="install", error=str(e),
                                 error_kind=type(e).__name__, **base)
        except OSError as e:
            return InstallResult(ok=False, stage="install",
                                 error="io: %s" % e,
                                 error_kind="OSError", **base)
        manifest_path = installer_mod.manifest_path_of(manifest)
        base.update(manifest=manifest, manifest_path=manifest_path)

        try:
            verify = inst.verify_installed(title_id, manifest["mod_name"])
        except installer_mod.InstallError as e:
            verify = {"ok": False, "mod": manifest["mod_dir"],
                      "title_id": title_id, "files_checked":
                          len(manifest.get("files", [])),
                      "problems": [str(e)]}
        base["verify"] = verify
        if not verify.get("ok"):
            return InstallResult(
                ok=False, stage="verify",
                error="install verification failed: %s"
                      % "; ".join(verify.get("problems", [])),
                error_kind="VerificationError", **base)

        state_path = self._register_mod(game_root, title_id, manifest,
                                        pre.requires)
        base["state_path"] = state_path
        return InstallResult(ok=True, stage="done", **base)

    def check_mod(self, archive_path: str, metadata: Union[dict, str],
                  title_id: str, game_root: str,
                  catalog_path: Optional[str] = None) -> CheckResult:
        """Dry-run install decision: every pre-install stage, zero writes."""
        game_root = os.path.abspath(game_root)
        catalog = catalog_path or self.catalog_path
        warnings: List[str] = []
        pre = self._preflight(archive_path, metadata, title_id, game_root,
                              catalog, warnings)
        blockers: List[str] = []
        cwarns: List[str] = []
        if pre.conflicts is not None:
            blockers, cwarns = _conflict_lines(pre.conflicts)
        return CheckResult(
            ok=pre.ok, mod_id=pre.mod_id, title_id=title_id,
            stage=pre.stage, refusal=pre.refusal, detection=pre.detection,
            compat=pre.compat, conflicts=pre.conflicts,
            conflict_blockers=blockers, conflict_warnings=cwarns,
            planned_files=list(pre.planned_files),
            planned_marker=pre.marker_rel, warnings=warnings)

    def uninstall_mod(self, manifest_path: str, game_root: str,
                      confirm_modified: Union[bool, Any] = False,
                      dry_run: bool = False) -> UninstallResult:
        """read manifest -> convert -> check dependents -> uninstall ->
        ploader cleanup.

        `manifest_path` is the INSTALLER manifest
        (<game_root>/<TITLE_ID>/manifests/<Mod>.json); `game_root` is the
        same roots base used for install_mod.  Tampered files refuse with
        stage='confirm' unless confirm_modified=True (or a callable
        deciding per modified file, as in uninstall.py).
        """
        game_root = os.path.abspath(game_root)
        mpath = os.path.abspath(manifest_path)
        try:
            with open(mpath, "r", encoding="utf-8") as f:
                imf = json.load(f)
        except (OSError, ValueError) as e:
            return UninstallResult(ok=False, stage="manifest",
                                   error="cannot read manifest: %s" % e,
                                   error_kind=type(e).__name__,
                                   manifest_path=mpath)
        if imf.get("schema") != installer_mod.SCHEMA_MANIFEST:
            return UninstallResult(
                ok=False, stage="manifest",
                error="not a %s installer manifest: %s"
                      % (installer_mod.SCHEMA_MANIFEST, mpath),
                error_kind="ManifestError", manifest_path=mpath)
        title_id = str(imf.get("title_id") or "")
        mod_dir = str(imf.get("mod_dir") or "")
        title_root = os.path.join(game_root, title_id)
        if os.path.abspath(str(imf.get("title_root") or "")) != title_root:
            return UninstallResult(
                ok=False, stage="manifest",
                error="manifest belongs to title root %s, not %s"
                      % (imf.get("title_root"), title_root),
                error_kind="ManifestError", mod_id=mod_dir,
                title_id=title_id, manifest_path=mpath)
        rel_manifest = os.path.relpath(mpath, title_root)
        if rel_manifest.startswith(".."):
            return UninstallResult(
                ok=False, stage="manifest",
                error="manifest lives outside the title root: %s" % mpath,
                error_kind="ManifestError", mod_id=mod_id,
                title_id=title_id, manifest_path=mpath)

        state_path, txn_root = self._state_paths(game_root, title_id)
        common = dict(mod_id=mod_dir, title_id=title_id,
                      manifest_path=mpath, state_path=state_path)
        um = installer_manifest_to_uninstall(imf,
                                             manifest_relpath=rel_manifest)
        un = uninstall_mod.Uninstaller(title_root, state_path=state_path,
                                       transaction_root=txn_root)
        try:
            plan = un.plan(um)
        except uninstall_mod.UninstallError as e:
            hint = ""
            if "install state not found" in str(e):
                hint = (" (the mod was installed without this pipeline; "
                        "register it first via register_existing())")
            return UninstallResult(
                ok=False, stage="plan", error=str(e) + hint,
                error_kind=type(e).__name__, **common)
        common["plan"] = _plan_summary(plan)
        if plan.dependents:
            return UninstallResult(
                ok=False, stage="dependents",
                error="refusing to uninstall '%s': still required by %s "
                      "(uninstall the dependents first)"
                      % (mod_dir, ", ".join(plan.dependents)),
                error_kind="ModStillRequiredError", **common)
        try:
            core = un.uninstall(um, confirm_modified=confirm_modified,
                                dry_run=dry_run)
        except uninstall_mod.ConfirmationRequiredError as e:
            return UninstallResult(
                ok=False, stage="confirm", error=str(e),
                error_kind="ConfirmationRequiredError",
                modified_files=[f.path for f in e.modified_files], **common)
        except uninstall_mod.UninstallError as e:
            return UninstallResult(ok=False, stage="uninstall",
                                   error=str(e),
                                   error_kind=type(e).__name__, **common)
        ploader = self._ploader_cleanup(game_root, title_id, imf,
                                        dry_run=dry_run,
                                        token=core.rollback_token,
                                        txn_root=txn_root)
        return UninstallResult(ok=True, stage="done", core=core,
                               ploader=ploader, **common)

    def verify_mod(self, mod_name: str, title_id: str,
                   game_root: str) -> Dict[str, Any]:
        """Re-run installer.py's post-install verification (hashes + config
        rows + enable marker) for one installed mod."""
        game_root = os.path.abspath(game_root)
        plugins, state_dir = self._paths(game_root)
        inst = installer_mod.Installer(game_root, plugins, state_dir)
        return inst.verify_installed(title_id, mod_name)

    def installed_mods(self, game_root: str,
                       title_id: str) -> Dict[str, Dict[str, Any]]:
        """The unified registry view of what is installed for a title."""
        return dict(self._load_state(os.path.abspath(game_root),
                                     title_id).get("mods") or {})

    def register_existing(self, manifest_path: str, game_root: str,
                          requires: Optional[List[str]] = None
                          ) -> str:
        """Adopt an install performed directly through installer.py (which
        knows nothing about the unified state) so conflict checks and
        uninstall_mod() work for it.  Returns the state file path."""
        game_root = os.path.abspath(game_root)
        mpath = os.path.abspath(manifest_path)
        with open(mpath, "r", encoding="utf-8") as f:
            imf = json.load(f)
        if imf.get("schema") != installer_mod.SCHEMA_MANIFEST:
            raise AdapterError("not a %s installer manifest: %s"
                               % (installer_mod.SCHEMA_MANIFEST, mpath))
        title_id = str(imf.get("title_id") or "")
        title_root = os.path.join(game_root, title_id)
        if os.path.abspath(str(imf.get("title_root") or "")) != title_root:
            raise AdapterError(
                "manifest belongs to title root %s, not %s"
                % (imf.get("title_root"), title_root))
        return self._register_mod(game_root, title_id, imf,
                                  list(requires or []))

    def rollback_uninstall(self, token: str, title_id: str,
                           game_root: str) -> Dict[str, Any]:
        """Undo a completed uninstall: files, config, unified state AND the
        adapter-managed ploader.ini snapshot (uninstall.py's rollback
        cannot see plugins/ploader.ini)."""
        game_root = os.path.abspath(game_root)
        title_root = os.path.join(game_root, title_id)
        state_path, txn_root = self._state_paths(game_root, title_id)
        un = uninstall_mod.Uninstaller(title_root, state_path=state_path,
                                       transaction_root=txn_root)
        try:
            core = un.rollback(token)
        except uninstall_mod.UninstallError as e:
            return {"ok": False, "error": str(e),
                    "error_kind": type(e).__name__}
        plugins, _state = self._paths(game_root)
        ploader_path = os.path.join(plugins, "ploader.ini")
        bak = os.path.join(txn_root, token, "ploader.ini.bak")
        ploader_restored = False
        if os.path.isfile(bak):
            with open(bak, "rb") as f:
                data = f.read()
            os.makedirs(os.path.dirname(ploader_path), exist_ok=True)
            installer_mod.atomic_write_bytes(ploader_path, data)
            ploader_restored = True
        return {"ok": True, "core": core,
                "ploader_restored": ploader_restored}

    # ---- ploader.ini cleanup (outside the uninstaller's game_root) ------

    def _ploader_cleanup(self, game_root: str, title_id: str,
                         imf: Dict[str, Any], dry_run: bool,
                         token: Optional[str],
                         txn_root: str) -> Dict[str, Any]:
        """Remove the mod's ploader.ini line after a committed uninstall.

        The line survives when another installed mod still needs the same
        prx/mode line (the default prx is shared by every mod); an
        installer-created [TITLE_ID] section is dropped when it becomes
        empty.  A pre-edit snapshot is stored inside the uninstall
        transaction dir for rollback_uninstall().
        """
        plugins, _state = self._paths(game_root)
        ploader_path = os.path.join(plugins, "ploader.ini")
        ce = imf.get("config_entries") or {}
        prx_name = ce.get("ploader_prx_name") or \
            installer_mod.DEFAULT_PLOADER_PRX
        prx_mode = str(ce.get("ploader_prx_mode") or
                       installer_mod.DEFAULT_PLOADER_MODE)
        section_created = bool(ce.get("ploader_section_created"))

        needed = False
        section_still_needed = False
        try:
            state = self._load_state(game_root, title_id)
            for info in (state.get("mods") or {}).values():
                pl = (info or {}).get("ploader") or {}
                if pl.get("name") == prx_name and \
                        str(pl.get("mode")) == prx_mode:
                    needed = True
                if pl.get("section") == title_id:
                    section_still_needed = True
        except CorruptStateError:
            needed = False  # nothing registered -> nothing can need it
            section_still_needed = False

        info: Dict[str, Any] = {
            "path": ploader_path, "prx": "%s=%s" % (prx_name, prx_mode),
            "line_removed": False, "section_removed": False,
            "line_kept_shared": needed, "dry_run": bool(dry_run)}
        if needed:
            info["note"] = ("prx line still needed by another installed "
                            "mod; kept")
            return info
        if not os.path.exists(ploader_path):
            info["note"] = "ploader.ini not present"
            return info
        with open(ploader_path, "rb") as f:
            raw = f.read()
        text = raw.decode("utf-8", errors="surrogateescape")
        lines = text.splitlines(keepends=True)

        sec_start = None
        sec_end = len(lines)
        for i, l in enumerate(lines):
            s = l.strip()
            if s.startswith("[") and s.endswith("]"):
                if sec_start is None:
                    if s[1:-1].strip() == title_id:
                        sec_start = i
                else:
                    sec_end = i
                    break
        if sec_start is None:
            info["note"] = ("[%s] section not found (already cleaned)"
                            % title_id)
            return info
        target = ("%s=%s" % (prx_name, prx_mode)).replace(" ", "")
        hit = None
        for i in range(sec_start + 1, sec_end):
            s = lines[i].rstrip("\r\n").strip()
            if s and not s.startswith((";", "#")) and \
                    s.replace(" ", "") == target:
                hit = i
                break
        if hit is None:
            info["note"] = "prx line not found (already cleaned)"
            return info
        if dry_run:
            info["note"] = "dry run: would remove the prx line"
            return info
        if token and txn_root:
            bak = os.path.join(txn_root, token, "ploader.ini.bak")
            try:
                os.makedirs(os.path.dirname(bak), exist_ok=True)
                with open(bak, "wb") as f:
                    f.write(raw)
            except OSError:
                pass  # snapshot is best-effort; the edit still proceeds
        del lines[hit]
        if hit < sec_end:
            sec_end -= 1
        info["line_removed"] = True
        body = [l for l in lines[sec_start + 1:sec_end]
                if l.strip() and not l.strip().startswith((";", "#"))]
        # Drop the section when it is empty AND either this install created
        # it or no remaining installed mod still references it.  The
        # creator flag alone is not enough when mods share the prx line:
        # the creator may be uninstalled first (its flag dies with its
        # manifest), leaving the last mod out to clean up the section.
        if not body and (section_created or not section_still_needed):
            del lines[sec_start]
            info["section_removed"] = True
        installer_mod.atomic_write_bytes(
            ploader_path,
            "".join(lines).encode("utf-8", errors="surrogateescape"))
        return info


# --------------------------------------------------------------------------
# CLI (thin, mirrors installer.py conventions)
# --------------------------------------------------------------------------

def main(argv: Optional[List[str]] = None) -> int:
    import argparse
    ap = argparse.ArgumentParser(
        description="modbridge integration adapter (Mac-only host tool)")
    sub = ap.add_subparsers(dest="cmd", required=True)

    def common(p):
        p.add_argument("--plugins", default=None)
        p.add_argument("--state", default=None)
        p.add_argument("--catalog", default=None)
        p.add_argument("--game-version", default=None)

    p_i = sub.add_parser("install", help="full pipeline install")
    p_i.add_argument("--archive", required=True)
    p_i.add_argument("--metadata", required=True)
    p_i.add_argument("--title", required=True)
    p_i.add_argument("--game-root", required=True)
    common(p_i)

    p_c = sub.add_parser("check", help="dry-run every pre-install stage")
    p_c.add_argument("--archive", required=True)
    p_c.add_argument("--metadata", required=True)
    p_c.add_argument("--title", required=True)
    p_c.add_argument("--game-root", required=True)
    common(p_c)

    p_u = sub.add_parser("uninstall", help="pipeline uninstall")
    p_u.add_argument("--manifest", required=True)
    p_u.add_argument("--game-root", required=True)
    p_u.add_argument("--confirm-modified", action="store_true")
    p_u.add_argument("--dry-run", action="store_true")
    common(p_u)

    p_v = sub.add_parser("verify", help="verify an installed mod")
    p_v.add_argument("--mod", required=True)
    p_v.add_argument("--title", required=True)
    p_v.add_argument("--game-root", required=True)
    common(p_v)

    args = ap.parse_args(argv)
    pipe = InstallPipeline(plugins_dir=args.plugins, state_dir=args.state,
                           catalog_path=args.catalog,
                           game_version=args.game_version)
    if args.cmd in ("install", "check"):
        md = installer_mod.load_metadata(args.metadata)
        fn = pipe.install_mod if args.cmd == "install" else pipe.check_mod
        res = fn(args.archive, md, args.title, args.game_root)
        print(json.dumps(res.__dict__, indent=1, default=str))
        return 0 if res.ok else 2
    if args.cmd == "uninstall":
        res = pipe.uninstall_mod(args.manifest, args.game_root,
                                 confirm_modified=args.confirm_modified,
                                 dry_run=args.dry_run)
        print(json.dumps(res.__dict__, indent=1, default=str))
        return 0 if res.ok else 2
    if args.cmd == "verify":
        report = pipe.verify_mod(args.mod, args.title, args.game_root)
        print(json.dumps(report, indent=1))
        return 0 if report.get("ok") else 3
    return 2


if __name__ == "__main__":
    sys.exit(main())
