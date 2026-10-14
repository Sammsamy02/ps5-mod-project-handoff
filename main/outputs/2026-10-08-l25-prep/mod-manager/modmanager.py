#!/usr/bin/env python3
"""modmanager.py -- end-to-end Mac mod manager for the PS5 UE4SS port.

Agent GG (Claude round-5 Part 2), 2026-10-08-l25-prep/mod-manager.
Battery mode: Mac-local python/bash only. No PRX builds, no console contact
by default (dry-run is the default; --apply is refused while the console is
down, guarded by BOTH the --i-know-console-is-on flag AND a live
reachability probe).

WHAT IT OWNS
  install   stage a mod UNMODIFIED into the Mac mod-store, hash-pin it,
            FU-style host-harness pre-flight (io-shim mode), FV keymap
            regen, loadlist-row add with the exact byte count, [--enable].
            Mods carrying .pak/.utoc/.ucas assets additionally get an
            OVERLAY ADMISSION dry-run (overlay_builder.py subprocess vs
            the --game-dump-root mirror); the builder plan is captured
            into an install receipt (agent HK wiring).
  enable    row presence on (affects synthesis only, never the console)
  disable   row presence off
  uninstall row removal + store delete (never touches other mods' rows)
  deploy    synthesize the console loadlist.txt from ALL enabled mods (+
            any installed diag mods, e.g. zzdiag / a future ueselftest)
            with exact sizes, regenerate ue4ss/PS5Pad.ini, re-pin its MFA
            row, upload under the order law, verify readback of EVERY
            uploaded file. Dry-run by default. The dry-run prints ONE
            unified plan document (uploads + overlay journal + pad +
            loadlist + rollback refs; saved as deploy-plan.json).
            Package-ID conflicts from the overlay builder block deploy
            (--allow-overlay-conflict is the reviewed later-wins path),
            exactly like FV shared-key conflicts (--allow-cofire).
  keymap    regenerate + self-check staging/PS5Pad.ini alone
  audit     FV --check mirror + loadlist consistency (row byte counts vs
            staged files) + drift report (console file changed outside
            this manager, when the console is reachable) + overlay
            journal list/verify
  rollback  restore the previous loadlist + PS5Pad.ini receipt pair
  list      status table

LAWS ENFORCED (sources cited in DESIGN.md)
  NEVER-EDIT-MODS    staged copy sha256 == original source sha256, per
                     file, verified at install AND re-verified by audit;
                     the manager writes only: store copies (install),
                     generated PS5Pad.ini, generated loadlist.txt, and a
                     0-byte enabled.txt ADMISSION ARTIFACT that lives
                     outside the pristine tree when the mod ships none.
  SIZE-EXACT MFA     every loadlist row byte count == wc -c of the exact
                     uploaded bytes (modforge_activation.c size gate;
                     mismatch = mfa_activate_failed code=-8, the game
                     silently continues unmodded).
  SIDECAR LAW        deploy fetches the LIVE console loadlist first when
                     the console is on (offline: last known receipt) and
                     preserves every non-managed row verbatim; only rows
                     of manager-installed mods + the PS5Pad.ini row are
                     added/updated/removed.
  UPLOAD ORDER LAW   mod files first, then PS5Pad.ini, loadlist.txt LAST
                     (the manifest references the other files; MFA reads
                     it at module_start). UE4SS-settings.ini, the PRX and
                     ploader are NEVER touched by this tool.
  SCOPE              this tool never uploads anything whose remote path
                     is not  ue4ss/Mods/<managed-mod>/... ,
                     ue4ss/PS5Pad.ini , or loadlist.txt .

EXIT CODES  0 ok / 1 warnings / 2 refusal-violation / 3 usage
"""

import argparse
import datetime
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
import zipfile

VERSION = ("modmanager.py 1.1 (agent GG + HK end-to-end wiring, "
           "2026-10-08-l25-prep/mod-manager)")

HERE = os.path.dirname(os.path.abspath(__file__))
WHAT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))

# ---------------------------------------------------------------------------
# Defaults (overridable via <root>/config.json; tests use --root + a dead
# console address so nothing ever leaves the Mac)
# ---------------------------------------------------------------------------

DEFAULTS = {
    # console endpoints (harvest/lua_fast_deploy.sh + prep_modded_launch.py)
    "console_ip": "ps5-host-not-configured.invalid",
    "http_port": 8084,                      # pldmgr (reachability probe)
    "ftp_port": 2121,                       # ftpsrv
    "remote_root": "/data/modbridge/roots/PPSA33286",
    # frozen sibling tools
    "keymap_gen": os.path.join(WHAT, "outputs", "2026-10-08-l25-prep",
                               "keymap", "keymap-gen.py"),
    "harness_runner": os.path.join(WHAT, "outputs", "2026-10-08-l25-prep",
                                   "host-lua-harness", "runner.lua"),
    "lua_bin": "lua5.5",
    "shared_dir": os.path.join(
        WHAT, ".superpowers", "native-dependency-spike",
        "ue4ss-unreal-native-build-1", "source-dependencies", "ue4ss",
        "assets", "Mods", "shared"),
    # console-proven pinned pad profile (l23 harvest, 3718 B,
    # md5 5510caf11fb9307931fe26602245205d; MAPPING.md section 1)
    "pin_pad": os.path.join(
        WHAT, "outputs", "2026-10-08-l23-package",
        "console-test-20261008T171655Z-964d76", "harvest-20261008T174239Z-25929d",
        "PS5Pad.ini"),
    # resident baseline (keymap participant ONLY -- its loadlist rows come
    # from the live/base composition and are never managed here)
    "resident_mods": {
        "TownfallItemSpawner": os.path.join(
            WHAT, "outputs", "2026-10-08-l25-prep", "evidence",
            "townfall-main.lua.console-copy"),
    },
    # keymap priority (MAPPING.md section 4.6: spawner, zzdiag, MANIFEST RUN
    # order, LOAD-ONLY, SKIP; unlisted store mods sort after, alphabetical)
    "priority": ["TownfallItemSpawner", "zzdiag", "LineTraceMod",
                 "CheatManagerEnablerMod", "ConsoleEnablerMod",
                 "ConsoleCommandsMod", "SplitScreenMod", "TownfallMiniMap",
                 "TownfallModMenu", "Keybinds", "BPML_GenericFunctions",
                 "BPModLoaderMod", "ActorDumperMod", "jsbLuaProfilerMod"],
    "ladder": "pairs10",
    # ---- overlay (agent GK pak-overlay/overlay_builder.py; extended by HI).
    # The manager SUBPROCESS-CALLS the builder (JSON-out CLI contract,
    # pak-overlay/mod-manager/INTEGRATION.md); it never imports or edits it.
    "overlay_builder": os.path.join(WHAT, "outputs", "2026-10-08-l25-prep",
                                    "pak-overlay", "overlay_builder.py"),
    # local staging mirror of the game dump's paks directory. None until the
    # operator configures it (tests set it per-sandbox via config.json).
    "game_dump_root": None,
    # base container index name inside game_dump_root (a .utoc), e.g.
    # "pakchunk0-ps5.utoc". Required whenever enabled mods carry utoc/ucas.
    "overlay_index": None,
    # journal dir for manager-driven builder calls (keeps the dump mirror
    # free of manager state); default <root>/staging/overlay-state
    "overlay_state_dir": None,
    # optional pinned base facts for dumps whose .ucas is not local
    # (builder flags --base-ucas-size / --base-container-header passthrough)
    "overlay_base_ucas_size": None,
    "overlay_base_container_header": None,
}

# Documented base composition (TEST-PLAN.md "Base composition at start" and
# the l23 harvest loadlist receipt: 12 lines, 447 B). Offline synthesis base
# when no deploy receipt exists yet.
BASE_COMPOSITION = (
    "modforge-loadlist\t1\n"
    "title\tPPSA33286\n"
    "engine\tunreal\n"
    "framework\tue4ss-ps5\n"
    "mod\tTownfallItemSpawner\t1.0.0\n"
    "file\tconfig\tue4ss/UE4SS-settings.ini\t1947\n"
    "file\tconfig\tue4ss/MemberVariableLayout.ini\t588\n"
    "file\tconfig\tue4ss/VTableLayout.ini\t216\n"
    "file\tconfig\tue4ss/PS5Pad.ini\t3718\n"
    "file\tlua_mod\tue4ss/Mods/TownfallItemSpawner/Scripts/main.lua\t21415\n"
    "file\tlua_mod\tue4ss/Mods/TownfallItemSpawner/enabled.txt\t0\n"
    "file\tlua_mod\tue4ss/Mods/shared/UEHelpers/UEHelpers.lua\t9962\n"
)

PAD_ROW_KEY = "ue4ss/PS5Pad.ini"
LOADLIST_REMOTE_REL = "loadlist.txt"           # under remote_root
NAME_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_.-]*$")
# asset-class files: staged byte-exact under the never-edit law but NEVER
# uploaded under ue4ss/Mods and NEVER given loadlist rows -- they ship through
# the IoStore overlay (overlay_builder.py), a separate surface.
PAK_EXTS = (".pak", ".utoc", ".ucas")


def is_pak_rel(rel):
    return rel.lower().endswith(PAK_EXTS)


def now_utc():
    return datetime.datetime.now(datetime.timezone.utc).strftime(
        "%Y-%m-%dT%H:%M:%SZ")


def ts_slug():
    return datetime.datetime.now(datetime.timezone.utc).strftime(
        "%Y%m%dT%H%M%SZ")


def sha256_bytes(data):
    return hashlib.sha256(data).hexdigest()


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


def warn(msg):
    print("[warn] " + msg)


def info(msg):
    print("[mm] " + msg)


class Refusal(Exception):
    """Hard refusal (exit 2)."""


# ---------------------------------------------------------------------------
# Root state: <root>/{store/, staging/, receipts/, config.json}
# ---------------------------------------------------------------------------

class Root:
    def __init__(self, path):
        self.path = os.path.abspath(path)
        self.store = os.path.join(self.path, "store")
        self.staging = os.path.join(self.path, "staging")
        self.receipts = os.path.join(self.path, "receipts")
        self.manifest_path = os.path.join(self.store, "manifest.json")
        for d in (self.store, self.staging, self.receipts):
            os.makedirs(d, exist_ok=True)
        self.config = dict(DEFAULTS)
        cfg = os.path.join(self.path, "config.json")
        if os.path.isfile(cfg):
            try:
                loaded = json.load(open(cfg, "r", encoding="utf-8"))
            except ValueError as e:
                raise Refusal("config.json is not valid JSON: %s" % e)
            self.config.update(loaded)
        self.manifest = self._load_manifest()

    def _load_manifest(self):
        if os.path.isfile(self.manifest_path):
            try:
                return json.load(open(self.manifest_path, "r",
                                      encoding="utf-8"))
            except ValueError as e:
                raise Refusal("store/manifest.json is not valid JSON: %s" % e)
        return {"schema": 1, "mods": {}}

    def save_manifest(self):
        tmp = self.manifest_path + ".tmp"
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump(self.manifest, f, indent=2, sort_keys=True)
            f.write("\n")
            f.flush()
            os.fsync(f.fileno())
        os.replace(tmp, self.manifest_path)

    def mod_dir(self, name):
        return os.path.join(self.store, name)

    def ordered_mods(self):
        """Installed mod names in keymap/deploy priority order."""
        names = list(self.manifest["mods"].keys())
        prio = list(self.config.get("priority", []))
        listed = [n for n in prio if n in names]
        rest = sorted(n for n in names if n not in prio)
        return listed + rest

    def enabled_mods(self):
        return [n for n in self.ordered_mods()
                if self.manifest["mods"][n].get("enabled")]


# ---------------------------------------------------------------------------
# Console plumbing (ONLY used under --apply / audit --console)
# ---------------------------------------------------------------------------

def run(argv, timeout=60):
    try:
        p = subprocess.run(argv, capture_output=True, text=True, timeout=timeout)
        return {"rc": p.returncode, "out": p.stdout or "",
                "err": p.stderr or "", "argv": argv}
    except (subprocess.TimeoutExpired, OSError) as e:
        return {"rc": -1, "out": "", "err": str(e), "argv": argv}


def probe_console(root, timeout=8):
    """Reachability probe: pldmgr HTTP processes_list (read-only GET)."""
    ip = root.config["console_ip"]
    port = int(root.config["http_port"])
    url = "http://%s:%d/processes_list" % (ip, port)
    r = run(["curl", "--silent", "--show-error", "--noproxy", "*",
             "--connect-timeout", "5", "--max-time", str(timeout), url],
            timeout + 10)
    return r["rc"] == 0, r


def guard_apply(root, args):
    """The console-off guard. Returns probe result dict when past the gates.

    --apply requires BOTH (1) the --i-know-console-is-on acknowledgment and
    (2) a successful reachability probe. The console is documented OFF in
    this session, so --apply is expected to refuse."""
    if not getattr(args, "i_know_console_is_on", False):
        raise Refusal(
            "--apply refused: pass --i-know-console-is-on to acknowledge the "
            "console is powered on and signed in. The console is documented "
            "OFF in this session; the default dry-run mode needs no flag.")
    up, r = probe_console(root)
    if not up:
        raise Refusal(
            "--apply refused: console %s:%d did not answer the reachability "
            "probe (curl rc=%s err=%s). While the console is down, run "
            "without --apply (dry-run). No writes were made."
            % (root.config["console_ip"], root.config["http_port"],
               r["rc"], (r["err"] or "unreachable").strip()[:120]))
    info("console probe OK (%s:%d answers processes_list)"
         % (root.config["console_ip"], root.config["http_port"]))
    return r


def ftp_url(root, rel):
    return "ftp://%s:%d%s/%s" % (root.config["console_ip"],
                                 int(root.config["ftp_port"]),
                                 root.config["remote_root"], rel)


def ftp_put(root, local, rel, max_time=60):
    """Text/binary upload, lua_fast_deploy.sh -T shape. No SITE SELF here:
    this tool only ever moves text-class files, never executables."""
    return run(["curl", "--silent", "--show-error", "--fail",
                "--connect-timeout", "5", "--max-time", str(max_time),
                "--upload-file", local, ftp_url(root, rel)], max_time + 15)


def ftp_get(root, rel, dest, max_time=30):
    return run(["curl", "--silent", "--show-error", "--fail",
                "--connect-timeout", "5", "--max-time", str(max_time),
                "-o", dest, ftp_url(root, rel)], max_time + 15)


# ---------------------------------------------------------------------------
# Mod staging (install) -- the never-edit-mods law lives here
# ---------------------------------------------------------------------------

def find_mod_root(src):
    """Locate the mod source. Returns (kind, dir) where kind is:
      'tree' -- a dir with Scripts/main.lua (standard mod layout; searches
                descendants too, for zip wrappers)
      'flat' -- a diag-style archive dir with main.lua directly inside
                (zzdiag-v3/v7/v11 shape); staged to Scripts/main.lua with
                BYTES UNTOUCHED (layout normalization only) and ONLY
                main.lua is staged (sibling receipts like LOADLIST-ROW.txt
                are evidence, not mod files)
      'assets' -- an IoStore ASSET mod: a dir with .utoc files (sibling
                .ucas required by the builder) and no lua at all. Staged
                whole like 'tree'; contributes NO loadlist rows (it ships
                through the overlay surface, never ue4ss/Mods).
    or (None, None) when neither shape is present."""
    if not os.path.isdir(src):
        return None, None
    if os.path.isfile(os.path.join(src, "Scripts", "main.lua")):
        return "tree", src
    if os.path.isfile(os.path.join(src, "main.lua")):
        return "flat", src
    for dirpath, dirnames, _ in os.walk(src):
        dirnames.sort()
        if os.path.isfile(os.path.join(dirpath, "Scripts", "main.lua")):
            return "tree", dirpath
    # no lua anywhere: accept an asset-only source (utoc+ucas files)
    for dirpath, dirnames, filenames in os.walk(src):
        dirnames.sort()
        utocs = [fn for fn in sorted(filenames) if fn.lower().endswith(".utoc")]
        for fn in utocs:
            if os.path.isfile(os.path.join(dirpath,
                                           fn[:-len(".utoc")] + ".ucas")):
                return "assets", dirpath
    return None, None


def collect_files(root_dir):
    """All files under the mod root, sorted, POSIX rel paths."""
    out = []
    for dirpath, dirnames, filenames in os.walk(root_dir):
        dirnames.sort()
        for fn in sorted(filenames):
            full = os.path.join(dirpath, fn)
            rel = os.path.relpath(full, root_dir).replace(os.sep, "/")
            if rel.split("/")[-1] == ".DS_Store":
                continue
            out.append((rel, full))
    out.sort()
    return out


def tree_digest(entries):
    """Stable digest over [(rel, size, sha256)] -- the provenance pin."""
    h = hashlib.sha256()
    for rel, size, sha in sorted(entries):
        h.update(("%s\0%d\0%s\n" % (rel, size, sha)).encode("utf-8"))
    return h.hexdigest()


def stage_mod(root, source, name):
    """Byte-exact copy source -> store/<name>/. Returns manifest file list.
    NEVER-EDIT-MODS: every staged file sha256 is compared to the ORIGINAL
    source sha right after the copy; any difference aborts the install."""
    src_root = None
    src_kind = None
    zip_sha = None
    tmpdir = None
    try:
        if os.path.isfile(source) and source.lower().endswith(".zip"):
            zip_sha = sha256_file(source)
            tmpdir = tempfile.mkdtemp(prefix="mm-zip-")
            try:
                with zipfile.ZipFile(source) as zf:
                    zf.extractall(tmpdir)
            except zipfile.BadZipFile as e:
                raise Refusal("not a valid zip: %s" % e)
            src_kind, src_root = find_mod_root(tmpdir)
            kind = "zip"
        else:
            src_kind, src_root = find_mod_root(source)
            kind = "dir"
        if src_root is None:
            raise Refusal(
                "%s is not a recognizable mod source: expected a directory "
                "with Scripts/main.lua (standard), a diag-style dir with "
                "main.lua directly inside (zzdiag shape), or a zip wrapping "
                "one of those." % source)
        if src_kind == "flat":
            mod_name = name or os.path.basename(os.path.abspath(src_root))
            if name is None:
                warn("flat (diag-style) source staged under its archive "
                     "dir name %r; the console mod dir will match. Pass "
                     "--name (e.g. --name zzdiag) to pin the console-side "
                     "name explicitly." % mod_name)
        else:
            mod_name = name or os.path.basename(
                os.path.abspath(src_root if kind == "dir" else source))
            if kind == "zip":
                mod_name = name or os.path.basename(src_root)
        if not NAME_RE.match(mod_name):
            raise Refusal("invalid mod name %r (allowed: [A-Za-z0-9._-])"
                          % mod_name)
        dest = root.mod_dir(mod_name)
        if os.path.exists(dest):
            raise Refusal("mod %r is already installed; uninstall first"
                          % mod_name)

        files = []
        staging_area = dest + ".staging"
        os.makedirs(staging_area)
        try:
            if src_kind == "flat":
                rels = [("Scripts/main.lua",
                         os.path.join(src_root, "main.lua"))]
            else:
                rels = collect_files(src_root)
            for rel, full in rels:
                if ".." in rel.split("/"):
                    raise Refusal("path escape in mod tree: %s" % rel)
                data = open(full, "rb").read()
                src_sha = sha256_bytes(data)
                out_path = os.path.join(staging_area, *rel.split("/"))
                os.makedirs(os.path.dirname(out_path), exist_ok=True)
                with open(out_path, "wb") as f:
                    f.write(data)
                    f.flush()
                    os.fsync(f.fileno())
                # byte-exact verify of the staged copy against the original
                staged_sha = sha256_file(out_path)
                if staged_sha != src_sha:
                    raise Refusal(
                        "never-edit-mods violation while staging %s: copy "
                        "sha %s != source sha %s -- aborting install"
                        % (rel, staged_sha, src_sha))
                files.append({"rel": rel, "size": len(data),
                              "sha256": src_sha})
            os.replace(staging_area, dest)
        finally:
            if os.path.exists(staging_area):
                shutil.rmtree(staging_area, ignore_errors=True)
        return mod_name, kind, files, zip_sha
    finally:
        if tmpdir:
            shutil.rmtree(tmpdir, ignore_errors=True)


# ---------------------------------------------------------------------------
# Pre-flight: FU host harness, io-shim mode (the mode that matches the
# composed console source per host-lua-harness/RESULTS.md)
# ---------------------------------------------------------------------------

def preflight(root, mod_name):
    runner = root.config["harness_runner"]
    lua = root.config["lua_bin"]
    main = os.path.join(root.mod_dir(mod_name), "Scripts", "main.lua")
    if not os.path.isfile(main):
        # asset-only mod (no lua): the host harness does not apply
        return {"tool": "host-lua-harness runner.lua (io-shim)",
                "verdict": "NOT-APPLICABLE",
                "detail": "no Scripts/main.lua (asset-only mod; overlays "
                          "carry no UE4SS lua)"}
    if not os.path.isfile(runner):
        return {"tool": "host-lua-harness runner.lua (io-shim)",
                "verdict": "UNAVAILABLE",
                "detail": "runner not found at %s" % runner}
    if shutil.which(lua) is None:
        return {"tool": "host-lua-harness runner.lua (io-shim)",
                "verdict": "UNAVAILABLE",
                "detail": "%s not on PATH" % lua}
    argv = [lua, runner, "--main", main, "--name", mod_name,
            "--shared", root.config["shared_dir"], "--with-io"]
    r = run(argv, timeout=120)
    verdict, detail = "UNAVAILABLE", (r["err"] or "").strip()[:200]
    m = re.search(r"^verdict:\s*(.+)$", r["out"] or "", re.M)
    if m:
        verdict, detail = m.group(1).strip(), (r["err"] or "").strip()[:200]
    elif r["rc"] != 0:
        verdict = "RUNNER-ERROR rc=%d" % r["rc"]
    return {"tool": "host-lua-harness runner.lua (io-shim)", "verdict": verdict,
            "detail": detail}


# ---------------------------------------------------------------------------
# FV keymap generator reuse (frozen tool, invoked as a subprocess)
# ---------------------------------------------------------------------------

def keymap_mod_args(root):
    """--mod NAME=PATH list for the enabled set, priority order:
    resident baseline(s) first, then enabled store mods."""
    args = []
    for name, path in sorted(root.config.get("resident_mods", {}).items()):
        if os.path.isfile(path):
            args.append((name, path))
        else:
            warn("resident mod %s main.lua missing at %s (skipped from "
                 "keymap set)" % (name, path))
    for name in root.enabled_mods():
        path = os.path.join(root.mod_dir(name), "Scripts", "main.lua")
        if not os.path.isfile(path):
            # asset-only mod (no lua): not a keymap participant
            continue
        args.append((name, path))
    return args


def regen_keymap(root, out_path=None):
    """Run keymap-gen.py over the enabled set. Returns a summary dict.
    The generator self-validates its output (reserved buttons, containment,
    16384 B law) and hard-fails on violation -- we propagate that."""
    out_path = out_path or os.path.join(root.staging, "PS5Pad.ini")
    mods = keymap_mod_args(root)
    if not mods:
        raise Refusal("keymap: no mods in the enabled set (resident mods "
                      "missing from config)")
    kg = root.config["keymap_gen"]
    if not os.path.isfile(kg):
        raise Refusal("keymap-gen.py not found at %s (config keymap_gen)"
                      % kg)
    argv = [sys.executable, kg]
    for name, path in mods:
        argv += ["--mod", "%s=%s" % (name, path)]
    argv += ["--pin-pad", root.config["pin_pad"],
             "--ladder", root.config.get("ladder", "pairs10"),
             "--out", out_path]
    if not os.path.isfile(root.config["pin_pad"]):
        raise Refusal("pinned pad profile not found at %s (config pin_pad)"
                      % root.config["pin_pad"])
    r = run(argv, 120)
    summary = {"rc": r["rc"], "stderr": (r["err"] or ""),
               "mods": [n for n, _ in mods]}
    if r["rc"] not in (0, 1):
        raise Refusal("keymap-gen.py failed (rc=%d): %s"
                      % (r["rc"], summary["stderr"].strip()[:400]))
    if not os.path.isfile(out_path):
        raise Refusal("keymap-gen.py did not write %s" % out_path)
    data = open(out_path, "rb").read()
    m = re.search(r"wrote [^\n]*: (\d+) bytes, (\d+) bindings \((\d+) pinned "
                  r"\+ (\d+) auto, (\d+) shared-key mods, (\d+) unmapped",
                  r["err"] or "")
    summary.update({"size": len(data), "sha256": sha256_bytes(data),
                    "bindings": int(m.group(2)) if m else None,
                    "pinned": int(m.group(3)) if m else None,
                    "auto": int(m.group(4)) if m else None,
                    "unmapped": int(m.group(6)) if m else None})
    return summary


def keymap_check(root, ini_path, expect_size, quiet=False):
    """Mirror of FV --check (violations/warnings counts)."""
    kg = root.config["keymap_gen"]
    argv = [sys.executable, kg, "--check", ini_path]
    for name, path in keymap_mod_args(root):
        argv += ["--mod", "%s=%s" % (name, path)]
    argv += ["--pin-pad", root.config["pin_pad"],
             "--expect-size", str(expect_size)]
    r = run(argv, 120)
    out = r["out"] or ""
    if not quiet:
        for line in out.splitlines():
            if line.startswith(("WARN ", "VIOLATION ", "shared: ")):
                print("    " + line)
    viol = len(re.findall(r"^VIOLATION ", out, re.M))
    warns = len(re.findall(r"^WARN ", out, re.M))
    return {"rc": r["rc"], "violations": viol, "warnings": warns,
            "output": out}


def detect_conflicts(root):
    """Same key wanted by 2+ enabled mods -> FV-style shared-key WARN.
    Implemented by importing FV's extractor (no duplicate parser logic)."""
    kg = root.config["keymap_gen"]
    if not os.path.isfile(kg):
        warn("keymap-gen.py unavailable -- conflict detection skipped")
        return []
    import importlib.util
    spec = importlib.util.spec_from_file_location("keymap_gen", kg)
    mod = importlib.util.module_from_spec(spec)
    try:
        spec.loader.exec_module(mod)
    except Exception as e:  # pragma: no cover - frozen tool import failure
        warn("cannot import keymap-gen.py (%s) -- conflict detection "
             "skipped" % e)
        return []
    conflicts = []
    for name, path in keymap_mod_args(root):
        try:
            binds = mod.extract_binds(path)
        except OSError:
            continue
        for lineno, key, mset in binds:
            conflicts.append({"key": mod.key_identity(key, mset),
                              "mod": name, "line": lineno})
    by_key = {}
    for c in conflicts:
        by_key.setdefault(c["key"], []).append(c["mod"])
    return sorted(
        ({"key": k, "mods": sorted(set(ms))} for k, ms in by_key.items()
         if len(set(ms)) > 1), key=lambda d: d["key"])


# ---------------------------------------------------------------------------
# IoStore overlay integration (agent GK's pak-overlay/overlay_builder.py,
# currently extended by HI). SUBPROCESS-ONLY coupling per the contract in
# pak-overlay/mod-manager/INTEGRATION.md: stdout is always ONE JSON document,
# exit 2 = hard refusal (code + detail) -- never retried. The manager never
# imports or edits the builder. Console shipping of overlay artifacts is
# Phase B (builder doc): this manager plans/verifies the LOCAL dump mirror.
# ---------------------------------------------------------------------------

def overlay_config(root):
    return {"builder": root.config.get("overlay_builder"),
            "paks_dir": root.config.get("game_dump_root"),
            "index": root.config.get("overlay_index"),
            "state_dir": (root.config.get("overlay_state_dir") or
                          os.path.join(root.staging, "overlay-state"))}


def run_overlay(root, command, mod_specs=None, remove_ids=None,
                dry_run=True, conflict="reject", timeout=300):
    """One overlay_builder.py subprocess call. Returns run() dict plus
    parsed 'json' (None when stdout is not a single JSON document)."""
    cfg = overlay_config(root)
    argv = [sys.executable, cfg["builder"],
            "--paks-dir", cfg["paks_dir"], "--index", cfg["index"],
            "--state-dir", cfg["state_dir"], "--conflict", conflict]
    if root.config.get("overlay_base_ucas_size") is not None:
        argv += ["--base-ucas-size",
                 str(int(root.config["overlay_base_ucas_size"]))]
    if root.config.get("overlay_base_container_header"):
        argv += ["--base-container-header",
                 root.config["overlay_base_container_header"]]
    for mid, path in (mod_specs or []):
        argv += ["--mod", "%s=%s" % (mid, path)]
    for rid in (remove_ids or []):
        argv += ["--remove-mod", rid]
    if dry_run:
        argv.append("--dry-run")
    argv.append(command)
    r = run(argv, timeout)
    parsed = None
    if (r["out"] or "").strip():
        try:
            parsed = json.loads(r["out"])
        except ValueError:
            parsed = None
    r["json"] = parsed
    return r


def overlay_mod_ids(store_dir, name, entry):
    """(overlay_mod_id, staged_utoc_path) pairs for a mod's .utoc files.
    The id is <manager mod name>--<utoc stem> so journal entries map back
    to the store. .pak-only mods yield no ids (the builder is IoStore
    utoc/ucas only; see pak-overlay/UNSUPPORTED.md)."""
    out = []
    for f in sorted(entry["files"], key=lambda f: f["rel"]):
        if f["rel"].lower().endswith(".utoc"):
            stem = os.path.splitext(os.path.basename(f["rel"]))[0]
            out.append(("%s--%s" % (name, stem),
                        os.path.join(store_dir, name, *f["rel"].split("/"))))
    return out


def overlay_desired(root):
    """Desired overlay state from the CURRENT enabled set."""
    desired = {}
    for name in root.enabled_mods():
        for mid, path in overlay_mod_ids(root.store, name,
                                         root.manifest["mods"][name]):
            desired[mid] = path
    return desired


def overlay_plan_for(root, desired, allow_conflict=False):
    """Compose the overlay for `desired` via builder DRY-RUNs. Returns a
    summary dict; status in none/unconfigured/would-clear/planned/refused/
    error."""
    cfg = overlay_config(root)
    summary = {"index": cfg["index"], "paks_dir": cfg["paks_dir"],
               "state_dir": cfg["state_dir"],
               "desired_mods": sorted(desired)}
    configured = bool(cfg["builder"] and cfg["paks_dir"] and cfg["index"])
    if not desired and not configured:
        summary["status"] = "none"
        summary["detail"] = ("no enabled overlay mods; overlay not "
                             "configured (nothing to plan)")
        return summary
    if not configured:
        missing = [k for k, v in (("overlay_builder", cfg["builder"]),
                                  ("game_dump_root", cfg["paks_dir"]),
                                  ("overlay_index", cfg["index"])) if not v]
        if cfg["builder"] and not os.path.isfile(cfg["builder"]):
            missing.append("overlay_builder file not found at %s"
                           % cfg["builder"])
        if cfg["paks_dir"] and not os.path.isdir(cfg["paks_dir"]):
            missing.append("game_dump_root is not a directory: %s"
                           % cfg["paks_dir"])
        summary["status"] = "unconfigured"
        summary["detail"] = ("overlay config incomplete (%s) -- set "
                             "game_dump_root + overlay_index (and "
                             "overlay_builder) in config.json"
                             % "; ".join(missing))
        return summary
    # journal snapshot: current mods (drives --remove-mod + rollback refs)
    lst = run_overlay(root, "list")
    journal_ids = []
    if lst["rc"] == 0 and isinstance(lst["json"], dict) and \
            lst["json"].get("result") == "listed":
        journal_ids = [m.get("mod_id") for m in lst["json"].get("mods", [])
                       if m.get("mod_id")]
        summary["journal"] = {"base": lst["json"].get("base"),
                              "derived": lst["json"].get("derived")}
    summary["journal_mods"] = journal_ids
    if not desired and not journal_ids:
        summary["status"] = "none"
        return summary
    if not desired and journal_ids:
        r = run_overlay(root, "uninstall", dry_run=True)
        summary["status"] = "would-clear"
        summary["builder"] = (r["json"] if isinstance(r["json"], dict)
                              else {"rc": r["rc"],
                                    "stdout": (r["out"] or "")[:400],
                                    "stderr": (r["err"] or "")[:400]})
        return summary
    removes = [i for i in journal_ids if i not in desired]
    conflict = "later-wins" if allow_conflict else "reject"
    r = run_overlay(root, "install", mod_specs=sorted(desired.items()),
                    remove_ids=removes, dry_run=True, conflict=conflict)
    # exit 2 + result=refused is the CONTRACT refusal shape (never retried);
    # any other nonzero rc or unparseable stdout is a hard error
    if isinstance(r["json"], dict) and r["json"].get("result") == "refused":
        summary["status"] = "refused"
        summary["code"] = r["json"].get("code")
        summary["detail"] = r["json"].get("detail")
        summary["builder"] = r["json"]
        return summary
    if r["rc"] != 0 or not isinstance(r["json"], dict):
        summary["status"] = "error"
        summary["detail"] = ("builder rc=%s stdout=%s stderr=%s"
                             % (r["rc"], (r["out"] or "")[:200],
                                (r["err"] or "")[:200]))
        return summary
    summary["builder"] = r["json"]
    ver = run_overlay(root, "verify")
    if ver["rc"] == 0 and isinstance(ver["json"], dict):
        summary["verify"] = ver["json"].get("verdict")
        summary["verify_checks"] = ver["json"].get("checks")
    summary["status"] = "planned"
    summary["plan"] = r["json"].get("plan")
    summary["counts"] = r["json"].get("counts")
    return summary


def gate_overlay(root, args):
    """Deploy gate 3b: plan the overlay for the enabled set, surface
    package-ID/chunk/virtual-path conflicts (builder refusal) and journal
    drift as hard refusals with actionable remedies."""
    desired = overlay_desired(root)
    plan = overlay_plan_for(
        root, desired, getattr(args, "allow_overlay_conflict", False))
    st = plan["status"]
    if st == "none":
        return plan
    if st == "unconfigured":
        raise Refusal(
            "deploy refused: enabled mod(s) carry pak/utoc/ucas assets but "
            "the overlay is not configured (%s). Set game_dump_root + "
            "overlay_index (and overlay_builder) in config.json, or disable "
            "the asset mod(s)." % plan.get("detail"))
    if st == "refused":
        raise Refusal(
            "deploy refused: the overlay builder rejected the composition "
            "(code=%s): %s -- enabled asset mods override the same package "
            "id / chunk / virtual path. Disable one of them, or pass "
            "--allow-overlay-conflict to accept the builder's later-wins "
            "policy as a reviewed decision."
            % (plan.get("code"), str(plan.get("detail"))[:400]))
    if st == "error":
        raise Refusal("deploy refused: overlay builder error: %s"
                      % plan.get("detail"))
    verdict = plan.get("verify")
    if verdict in ("STALE", "DRIFT"):
        raise Refusal(
            "deploy refused: overlay journal verify=%s -- the derived index "
            "was replaced (game update / foreign write) or previously "
            "published overlay state drifted. Reconcile with "
            "overlay_builder.py verify against %s before deploying."
            % (verdict, plan.get("state_dir")))
    return plan


def overlay_execute_apply(root, plan, args):
    """--apply only: publish the overlay LOCALLY against the game dump
    mirror (real builder install/uninstall + verify), after the console
    surfaces uploaded cleanly. Determinism law: the real publish's shas
    must equal the dry-run plan's. Console shipping of the overlay trio is
    Phase B (not this tool)."""
    desired = overlay_desired(root)
    conflict = ("later-wins"
                if getattr(args, "allow_overlay_conflict", False)
                else "reject")
    if plan["status"] == "would-clear":
        r = run_overlay(root, "uninstall", dry_run=False)
        if r["rc"] != 0 or not isinstance(r["json"], dict):
            raise Refusal(
                "LOCAL overlay clear failed (rc=%s): %s -- console "
                "surfaces are already uploaded and verified; reconcile the "
                "overlay mirror before the next launch."
                % (r["rc"], (r["out"] or r["err"] or "")[:300]))
        return {"applied": "cleared", "builder": r["json"]}
    if plan["status"] != "planned":
        return {"applied": "skipped", "status": plan["status"]}
    removes = [i for i in (plan.get("journal_mods") or [])
               if i not in desired]
    r = run_overlay(root, "install", mod_specs=sorted(desired.items()),
                    remove_ids=removes, dry_run=False, conflict=conflict)
    got = (r["json"] or {}) if isinstance(r["json"], dict) else {}
    if r["rc"] != 0 or got.get("result") != "installed":
        raise Refusal(
            "LOCAL overlay publish failed (result=%s code=%s rc=%s): %s -- "
            "console surfaces are already uploaded and verified; the "
            "overlay mirror was NOT fully published. Reconcile with "
            "overlay_builder.py verify before the next launch."
            % (got.get("result", "?"), got.get("code", "-"), r["rc"],
               (r["out"] or r["err"] or "")[:300]))
    want = plan.get("plan") or {}
    if got.get("plan", {}).get("derived_index_sha256") != \
            want.get("derived_index_sha256") or \
            got.get("plan", {}).get("sidecar_sha256") != \
            want.get("sidecar_sha256"):
        raise Refusal(
            "LOCAL overlay publish shas differ from the dry-run plan "
            "(derived %s vs %s, sidecar %s vs %s) -- determinism law "
            "violation; reconcile before the next launch."
            % (got.get("plan", {}).get("derived_index_sha256"),
               want.get("derived_index_sha256"),
               got.get("plan", {}).get("sidecar_sha256"),
               want.get("sidecar_sha256")))
    ver = run_overlay(root, "verify")
    verdict = ((ver["json"] or {}).get("verdict")
               if ver["rc"] == 0 and isinstance(ver["json"], dict) else None)
    if verdict != "OK":
        raise Refusal("LOCAL overlay publish verify=%s (expected OK) -- "
                      "reconcile before the next launch." % verdict)
    info("overlay published LOCALLY against %s (console shipping of the "
         "trio is Phase B, not this tool)" % plan.get("paks_dir"))
    return {"applied": "installed",
            "derived_index_sha256":
                got.get("plan", {}).get("derived_index_sha256"),
            "sidecar_sha256": got.get("plan", {}).get("sidecar_sha256"),
            "journal": got.get("journal"), "verify": verdict}


# ---------------------------------------------------------------------------
# Loadlist model + synthesis
# ---------------------------------------------------------------------------

def parse_loadlist(text):
    rows = []
    for raw in text.split("\n"):
        if raw == "":
            continue
        f = raw.split("\t")
        rows.append({"raw": raw, "fields": f,
                     "is_file": f[0] == "file" and len(f) == 4,
                     "key": f[2] if f[0] == "file" and len(f) >= 3 else None})
    return rows


def render_loadlist(rows):
    return "\n".join(r["raw"] for r in rows) + "\n"


def mod_row_keys(entry):
    """Remote rel keys this installed mod owns (uploads + rows). Overlay-
    owned asset files (.pak/.utoc/.ucas) NEVER appear here; asset-only mods
    (no Scripts/main.lua) own no ue4ss surface at all."""
    if not any(f["rel"] == "Scripts/main.lua" for f in entry["files"]):
        return []
    keys = ["ue4ss/Mods/%s/%s" % (entry["_name"], f["rel"])
            for f in entry["files"] if not is_pak_rel(f["rel"])]
    keys.append("ue4ss/Mods/%s/enabled.txt" % entry["_name"])
    return keys


def mod_row_build(entry):
    """Ordered (key, size, local_path) triples for synthesis + upload.
    Row order: Scripts/main.lua first, remaining files sorted,
    enabled.txt last (mirrors the CANDIDATE-SHEETS row sets).
    Overlay-owned asset files are excluded (they ship through the overlay
    surface, never ue4ss/Mods); asset-only mods contribute no rows."""
    if not any(f["rel"] == "Scripts/main.lua" for f in entry["files"]):
        return []
    triples = []
    files = [f for f in sorted(entry["files"], key=lambda f: f["rel"])
             if not is_pak_rel(f["rel"])]
    main_first = [f for f in files if f["rel"] == "Scripts/main.lua"]
    enabled_shipped = [f for f in files if f["rel"] == "enabled.txt"]
    rest = [f for f in files
            if f not in main_first and f not in enabled_shipped]
    for f in main_first + rest:
        triples.append(("ue4ss/Mods/%s/%s" % (entry["_name"], f["rel"]),
                        f["size"],
                        os.path.join(root_mod_dir_global, entry["_name"],
                                     *f["rel"].split("/"))))
    if enabled_shipped:
        f = enabled_shipped[0]
        triples.append(("ue4ss/Mods/%s/enabled.txt" % entry["_name"],
                        f["size"],
                        os.path.join(root_mod_dir_global, entry["_name"],
                                     "enabled.txt")))
    else:
        # 0-byte admission artifact, manager-owned, OUTSIDE the pristine
        # staged tree (never-edit-mods law).
        art_dir = os.path.join(root_staging_global, "artifacts",
                               entry["_name"])
        os.makedirs(art_dir, exist_ok=True)
        art = os.path.join(art_dir, "enabled.txt")
        if not os.path.exists(art):
            open(art, "wb").close()
        triples.append(("ue4ss/Mods/%s/enabled.txt" % entry["_name"], 0, art))
    return triples


# globals set by commands that build rows (single-process tool; keeps the
# helpers above signature-stable)
root_mod_dir_global = None
root_staging_global = None


def synthesize_loadlist(root, base_text, pad_local, pad_size):
    """Sidecar-law synthesis: preserve every non-managed row verbatim (and
    in order); replace managed mod rows in place; drop rows of installed
    but disabled/uninstalled mods; append new managed rows at the end; the
    PS5Pad.ini row is always re-pinned to the regenerated size."""
    global root_mod_dir_global, root_staging_global
    root_mod_dir_global = root.store
    root_staging_global = root.staging
    installed = dict(root.manifest["mods"])
    for name, entry in installed.items():
        entry["_name"] = name
    managed = {}          # key -> (size, local_path) for ENABLED mods
    owned_keys = set()    # keys of installed mods (enabled or not)
    for name in root.ordered_mods():
        entry = installed[name]
        for key in mod_row_keys(entry):
            owned_keys.add(key)
        if entry.get("enabled"):
            for key, size, local in mod_row_build(entry):
                managed[key] = (size, local)
    managed[PAD_ROW_KEY] = (pad_size, pad_local)
    owned_keys.add(PAD_ROW_KEY)

    rows = parse_loadlist(base_text)
    out, seen = [], set()
    for row in rows:
        key = row["key"]
        if key is not None and key in owned_keys:
            if key in managed and key not in seen:
                size, _ = managed[key]
                f = list(row["fields"])
                f[3] = str(size)
                out.append({"raw": "\t".join(f), "fields": f,
                            "is_file": True, "key": key})
                seen.add(key)
            # disabled/uninstalled mod row, duplicate row, or already
            # rendered: dropped
            continue
        out.append(row)
    for key, (size, _) in managed.items():
        if key not in seen:
            out.append({"raw": "file\t%s\t%s\t%d" % (
                "config" if key == PAD_ROW_KEY else "lua_mod", key, size),
                "fields": ["file",
                           "config" if key == PAD_ROW_KEY else "lua_mod",
                           key, str(size)],
                "is_file": True, "key": key})
    return render_loadlist(out), managed


def load_base_loadlist(root):
    """Offline base: the last known console receipt, else the documented
    base composition. (Online deploys fetch live instead.)"""
    last = os.path.join(root.receipts, "last", "loadlist.txt")
    if os.path.isfile(last):
        info("offline base: last known console loadlist receipt (%s)" % last)
        return open(last, "r", encoding="utf-8").read(), "last-receipt"
    info("offline base: documented BASE_COMPOSITION (12 lines / 447 B, "
         "TEST-PLAN.md; no deploy receipt exists yet)")
    return BASE_COMPOSITION, "base-composition"


def write_atomic(path, data):
    if isinstance(data, str):
        data = data.encode("utf-8")
    tmp = path + ".tmp"
    with open(tmp, "wb") as f:
        f.write(data)
        f.flush()
        os.fsync(f.fileno())
    os.replace(tmp, path)


# ---------------------------------------------------------------------------
# Gates shared by deploy/audit
# ---------------------------------------------------------------------------

def gate_preflight(root):
    bad = [n for n in root.enabled_mods()
           if root.manifest["mods"][n].get("preflight", {}).get("verdict",
                                                                "") not in
           ("LOADS-CLEAN", "UNAVAILABLE", "NOT-APPLICABLE")]
    return bad


def gate_conflicts(root):
    conflicts = detect_conflicts(root)
    for c in conflicts:
        warn("FV WARN shared key: %s fires for %s (UE4SS binds are "
             "non-exclusive; every callback fires)" % (c["key"],
                                                       ", ".join(c["mods"])))
    return conflicts


# ---------------------------------------------------------------------------
# Receipts
# ---------------------------------------------------------------------------

def write_receipt(root, kind, ledger, loadlist_text, pad_path,
                  prev_loadlist=None, prev_pad=None, applied=False,
                  extra_files=None):
    d = None
    for _ in range(50):  # unique even when two receipts land in one second
        cand = os.path.join(root.receipts, "%s-%s" % (kind, ts_slug()))
        if not os.path.exists(cand):
            d = cand
            break
        time.sleep(1.0)
    if d is None:
        d = os.path.join(root.receipts, "%s-%s-%d"
                         % (kind, ts_slug(), os.getpid()))
    os.makedirs(d, exist_ok=True)
    write_atomic(os.path.join(d, "loadlist.txt"), loadlist_text)
    if pad_path and os.path.isfile(pad_path):
        shutil.copyfile(pad_path, os.path.join(d, "PS5Pad.ini"))
    if prev_loadlist is not None:
        write_atomic(os.path.join(d, "prev-loadlist.txt"), prev_loadlist)
    if prev_pad is not None:
        write_atomic(os.path.join(d, "prev-PS5Pad.ini"), prev_pad)
    for nm, content in (extra_files or {}).items():
        write_atomic(os.path.join(d, nm), content)
    ledger.update({"kind": kind, "applied": applied, "utc": now_utc(),
                   "version": VERSION})
    write_atomic(os.path.join(d, "ledger.json"), json.dumps(
        ledger, indent=2, sort_keys=True) + "\n")
    if applied:  # receipts/last tracks the CONSOLE state, apply only
        last = os.path.join(root.receipts, "last")
        os.makedirs(last, exist_ok=True)
        shutil.copyfile(os.path.join(d, "loadlist.txt"),
                        os.path.join(last, "loadlist.txt"))
        shutil.copyfile(os.path.join(d, "PS5Pad.ini"),
                        os.path.join(last, "PS5Pad.ini"))
        for nm in ("prev-loadlist.txt", "prev-PS5Pad.ini"):
            src = os.path.join(d, nm)
            if os.path.isfile(src):
                shutil.copyfile(src, os.path.join(last, nm))
        shutil.copyfile(os.path.join(d, "ledger.json"),
                        os.path.join(last, "ledger.json"))
    info("receipt: %s (applied=%s)" % (d, applied))
    return d


# ---------------------------------------------------------------------------
# Commands
# ---------------------------------------------------------------------------

def cmd_install(root, args):
    source = args.source
    if not os.path.exists(source):
        raise Refusal("install source does not exist: %s" % source)
    name, kind, files, zip_sha = stage_mod(root, source, args.name)
    entry = {
        "name": name,
        "source": os.path.abspath(source),
        "source_kind": kind,
        "source_zip_sha256": zip_sha,
        "source_tree_sha256": tree_digest(
            [(f["rel"], f["size"], f["sha256"]) for f in files]),
        "installed_utc": now_utc(),
        "enabled": False,
        "files": files,
        "enabled_txt_shipped": any(f["rel"] == "enabled.txt" for f in files),
        "preflight": preflight(root, name),
        "bytes_total": sum(f["size"] for f in files),
    }
    root.manifest["mods"][name] = entry
    root.save_manifest()
    info("installed %s (%s, %d files, %d B, tree %s)"
         % (name, kind, len(files), entry["bytes_total"],
            entry["source_tree_sha256"][:16] + "..."))
    info("preflight [%s]: %s %s" % (entry["preflight"]["tool"],
                                    entry["preflight"]["verdict"],
                                    entry["preflight"]["detail"]))
    if args.enable:
        _enable(root, name)
    _post_state_change(root, "install")
    # ---- pak-track steps (agent HK): mods carrying .pak/.utoc/.ucas get
    # an overlay admission dry-run against the game dump mirror; the
    # builder's plan/ledger is captured verbatim into an install receipt.
    pak_files = [f for f in files if is_pak_rel(f["rel"])]
    overlay_summary = None
    if pak_files:
        desired = overlay_desired(root)
        for mid, path in overlay_mod_ids(root.store, name, entry):
            # admission-check the new trio even when installed disabled
            desired.setdefault(mid, path)
        overlay_summary = overlay_plan_for(root, desired)
        verdict = {"planned": "PLANNED", "refused": "REFUSED",
                   "unconfigured": "UNAVAILABLE", "would-clear": "WOULD-CLEAR",
                   "none": "NONE", "error": "ERROR"}.get(
                       overlay_summary["status"],
                       overlay_summary["status"].upper())
        ov_entry = {"verdict": verdict, "status": overlay_summary["status"],
                    "mod_ids": [mid for mid, _ in
                                overlay_mod_ids(root.store, name, entry)]}
        if overlay_summary["status"] in ("refused", "unconfigured", "error"):
            ov_entry["code"] = overlay_summary.get("code")
            ov_entry["detail"] = str(overlay_summary.get("detail"))[:2000]
        root.manifest["mods"][name]["overlay"] = ov_entry
        root.save_manifest()
        if verdict == "PLANNED":
            p = overlay_summary.get("plan") or {}
            info("overlay admission dry-run OK vs %s: derived index %s... "
                 "(%s B), sidecar %s... (%s B), mods %s"
                 % (overlay_summary.get("paks_dir"),
                    (p.get("derived_index_sha256") or "?")[:16],
                    p.get("derived_index_bytes"),
                    (p.get("sidecar_sha256") or "?")[:16],
                    p.get("sidecar_bytes"),
                    ",".join(overlay_summary.get("desired_mods") or [])))
        elif verdict == "REFUSED":
            warn("overlay builder REFUSED this composition (code=%s): %s "
                 "-- recorded in the manifest; deploy will refuse while the "
                 "conflicting set is enabled."
                 % (overlay_summary.get("code"),
                    str(overlay_summary.get("detail"))[:300]))
        else:
            warn("overlay admission not completed: %s (%s)"
                 % (verdict, overlay_summary.get("detail")))
        if not ov_entry["mod_ids"]:
            warn("mod carries .pak files but no .utoc/.ucas trio -- the "
                 "overlay builder is IoStore-only (see pak-overlay/"
                 "UNSUPPORTED.md); this mod cannot be shipped by the "
                 "current overlay path")
        staging_ll = os.path.join(root.staging, "loadlist.txt")
        ll_text = (open(staging_ll, "r", encoding="utf-8").read()
                   if os.path.isfile(staging_ll) else "")
        write_receipt(
            root, "install",
            {"mod": name, "pak_files": [f["rel"] for f in pak_files],
             "overlay": {k: v for k, v in overlay_summary.items()
                         if k != "builder"},
             "overlay_builder_json": overlay_summary.get("builder")},
            ll_text, os.path.join(root.staging, "PS5Pad.ini"), applied=False,
            extra_files={"overlay-plan.json": json.dumps(
                overlay_summary, indent=2, sort_keys=True) + "\n"})
    conflicts = gate_conflicts(root)
    if conflicts:
        root.manifest["mods"][name]["conflicts_last_seen"] = conflicts
        root.save_manifest()
    if entry["preflight"]["verdict"] not in ("LOADS-CLEAN", "UNAVAILABLE"):
        warn("preflight did not pass for %s -- deploy will refuse while it "
             "is enabled (mods are never edited; disable it or deploy with "
             "an explicit --allow-preflight-fail after review)" % name)
    return 0


def _enable(root, name):
    if name not in root.manifest["mods"]:
        raise Refusal("no installed mod named %r (see `list`)" % name)
    root.manifest["mods"][name]["enabled"] = True
    root.save_manifest()
    info("enabled %s (loadlist rows will be present in the next synthesis)"
         % name)


def _disable(root, name):
    if name not in root.manifest["mods"]:
        raise Refusal("no installed mod named %r (see `list`)" % name)
    root.manifest["mods"][name]["enabled"] = False
    root.save_manifest()
    info("disabled %s (its rows drop out of synthesis; other rows "
         "untouched)" % name)


def _post_state_change(root, why):
    """After any install/enable/disable/uninstall: regenerate the keymap,
    run FV --check (the same chain deploy gates on), and re-synthesize the
    loadlist so staging always matches state. Any pad size drift re-pins
    the MFA pad row here (synthesize_loadlist pins wc -c of the fresh
    pad)."""
    km = regen_keymap(root)
    pad = os.path.join(root.staging, "PS5Pad.ini")
    chk = keymap_check(root, pad, km["size"], quiet=True)
    info("FV --check after %s: %d violation(s), %d warning(s)"
         % (why, chk["violations"], chk["warnings"]))
    if chk["violations"]:
        raise Refusal("state change (%s) left %d keymap violation(s) -- "
                      "staging is NOT consistent; run `modmanager.py keymap` "
                      "for the full --check output and resolve before any "
                      "deploy" % (why, chk["violations"]))
    base_text, base_src = load_base_loadlist(root)
    loadlist, _ = synthesize_loadlist(root, base_text, pad, km["size"])
    write_atomic(os.path.join(root.staging, "loadlist.txt"), loadlist)
    info("staging refreshed (%s): PS5Pad.ini %d B sha %s...; loadlist %d B "
         "(base=%s)" % (why, km["size"], km["sha256"][:16], len(loadlist),
                        base_src))
    row = [l for l in loadlist.split("\n")
           if l.startswith("file\tconfig\tue4ss/PS5Pad.ini\t")]
    if row:
        info("MFA pad row now: %s" % row[0])
    return km


def cmd_enable(root, args):
    _enable(root, args.name)
    _post_state_change(root, "enable " + args.name)
    gate_conflicts(root)
    return 0


def cmd_disable(root, args):
    _disable(root, args.name)
    _post_state_change(root, "disable " + args.name)
    return 0


def cmd_uninstall(root, args):
    name = args.name
    if name not in root.manifest["mods"]:
        raise Refusal("no installed mod named %r" % name)
    if not args.yes and root.manifest["mods"][name].get("enabled"):
        warn("%s is still ENABLED; uninstalling removes its rows from "
             "synthesis (never other mods' rows)" % name)
    d = root.mod_dir(name)
    if os.path.isdir(d):
        shutil.rmtree(d)
    had_overlay = bool(root.manifest["mods"][name].get("overlay", {})
                       .get("mod_ids"))
    del root.manifest["mods"][name]
    root.save_manifest()
    info("uninstalled %s (store dir deleted; only its own rows disappear "
         "from synthesis)" % name)
    if had_overlay:
        info("note: %s had overlay mod ids -- the next deploy re-derives "
             "the overlay without them (builder --remove-mod path); until "
             "then the journal references a deleted source and audit will "
             "flag it" % name)
    _post_state_change(root, "uninstall " + name)
    return 0


def cmd_keymap(root, args):
    km = regen_keymap(root)
    pad = os.path.join(root.staging, "PS5Pad.ini")
    chk = keymap_check(root, pad, km["size"])
    info("PS5Pad.ini: %d B, %s bindings (%s pinned + %s auto, %s unmapped)"
         % (km["size"], km["bindings"], km["pinned"], km["auto"],
            km["unmapped"]))
    info("FV --check: %d violations, %d warnings" % (chk["violations"],
                                                     chk["warnings"]))
    if chk["violations"]:
        return 2
    return 1 if chk["warnings"] else 0


def _deploy_common(root, args):
    """Shared deploy pipeline; returns (km, loadlist_text, managed, ledger,
    prev_loadlist, overlay_plan) after all gates."""
    # gate 1: preflight
    bad = gate_preflight(root)
    if bad and not args.allow_preflight_fail:
        raise Refusal("deploy refused: enabled mod(s) with failed host "
                      "harness pre-flight: %s. Mods are never edited -- "
                      "disable them, or pass --allow-preflight-fail after "
                      "review." % ", ".join(bad))
    if bad:
        warn("preflight failures overridden by --allow-preflight-fail: %s"
             % ", ".join(bad))
    # gate 2: keymap regen (self-validating)
    km = regen_keymap(root)
    pad = os.path.join(root.staging, "PS5Pad.ini")
    if km.get("unmapped") and not args.allow_unmapped:
        raise Refusal(
            "deploy refused: %d keybind(s) UNMAPPED (free-slot pool "
            "exhausted; the binds would be unreachable on console). "
            "Disable a mod, raise the ladder, or pass --allow-unmapped to "
            "accept the documented WARN block." % km["unmapped"])
    if km.get("unmapped"):
        warn("unmapped keys accepted via --allow-unmapped: %d"
             % km["unmapped"])
    # gate 3: conflicts
    conflicts = gate_conflicts(root)
    if conflicts and not args.allow_cofire:
        raise Refusal(
            "deploy refused: shared-key conflict(s) above. UE4SS binds are "
            "non-exclusive, so both mods' callbacks would fire on one "
            "button. Resolve by disabling one mod, or pass --allow-cofire "
            "to accept the co-fire semantics explicitly.")
    # gate 3b: overlay composition (package-ID conflicts, journal drift)
    overlay_plan = gate_overlay(root, args)
    if overlay_plan["status"] == "planned":
        p = overlay_plan.get("plan") or {}
        info("overlay plan OK: %d mod(s) %s -> derived index %s... (%s B), "
             "sidecar %s... (%s B), journal verify=%s"
             % (len(overlay_plan.get("desired_mods") or []),
                ",".join(overlay_plan.get("desired_mods") or []),
                (p.get("derived_index_sha256") or "?")[:16],
                p.get("derived_index_bytes"),
                (p.get("sidecar_sha256") or "?")[:16],
                p.get("sidecar_bytes"), overlay_plan.get("verify")))
    elif overlay_plan["status"] == "would-clear":
        info("overlay plan: journal would be cleared (no enabled overlay "
             "mods remain; builder restores the pristine index)")
    # gate 4: FV --check with the size we are about to pin (MFA chain)
    chk = keymap_check(root, pad, km["size"])
    if chk["violations"]:
        raise Refusal("deploy refused: keymap --check reported %d "
                      "violation(s) (see above)" % chk["violations"])
    # base + synthesis
    prev_loadlist = None
    if args.apply:
        dest = os.path.join(root.staging, "live-loadlist.fetch.txt")
        r = ftp_get(root, LOADLIST_REMOTE_REL, dest)
        if r["rc"] != 0 or not os.path.isfile(dest):
            raise Refusal("deploy refused: cannot fetch the LIVE loadlist "
                          "(sidecar law; curl rc=%s err=%s) -- never "
                          "blind-write the manifest."
                          % (r["rc"], (r["err"] or "")[:120]))
        prev_loadlist = open(dest, "r", encoding="utf-8").read()
        base_src = "live-fetch"
        base_text = prev_loadlist
    else:
        base_text, base_src = load_base_loadlist(root)
    loadlist, managed = synthesize_loadlist(root, base_text, pad, km["size"])
    write_atomic(os.path.join(root.staging, "loadlist.txt"), loadlist)
    ledger = {"base": base_src, "pad": {"size": km["size"],
                                        "sha256": km["sha256"],
                                        "bindings": km["bindings"]},
              "mods_enabled": root.enabled_mods(),
              "overlay": {k: v for k, v in overlay_plan.items()
                          if k != "builder"},
              "uploads": [], "checks": []}
    info("synthesized loadlist: %d B from base=%s (%d managed rows)"
         % (len(loadlist.encode()), base_src, len(managed)))
    return km, loadlist, managed, ledger, prev_loadlist, overlay_plan


def build_deploy_plan_document(root, mode, uploads, km, loadlist, base_src,
                               overlay_plan):
    """ONE plan document composing every prep-owned surface: console
    uploads (order law), the overlay journal plan, the pad, the loadlist,
    and rollback references. Never PRX/settings/ploader."""
    ll_bytes = loadlist.encode("utf-8")
    pad_local = os.path.join(root.staging, "PS5Pad.ini")
    last = os.path.join(root.receipts, "last")
    pair = [p for p in (os.path.join(last, "prev-loadlist.txt"),
                        os.path.join(last, "prev-PS5Pad.ini"))
            if os.path.isfile(p)]
    ov_backup = None
    ov_plan = (overlay_plan or {}).get("plan") or {}
    if ov_plan.get("paths", {}).get("backup"):
        ov_backup = ov_plan["paths"]["backup"]
    doc = {
        "kind": "deploy-plan", "version": 1, "utc": now_utc(), "mode": mode,
        "surfaces": ("prep-owned only: ue4ss/Mods/<managed>/... + "
                     "ue4ss/PS5Pad.ini + loadlist.txt + LOCAL overlay "
                     "journal; NEVER PRX/settings/ploader"),
        "uploads": [{"order": i + 1, "rel": rel,
                     "bytes": os.path.getsize(local),
                     "sha256": sha256_file(local)}
                    for i, (rel, local) in enumerate(uploads)],
        "pad": {"rel": PAD_ROW_KEY, "bytes": km["size"],
                "sha256": km["sha256"],
                "mfa_row": "file\tconfig\t%s\t%d" % (PAD_ROW_KEY,
                                                     km["size"]),
                "local": pad_local},
        "loadlist": {"rel": LOADLIST_REMOTE_REL, "bytes": len(ll_bytes),
                     "sha256": sha256_bytes(ll_bytes), "base": base_src,
                     "uploaded_last": True},
        "overlay": {k: v for k, v in (overlay_plan or {}).items()
                    if k not in ("builder", "verify_checks")},
        "rollback": {
            "console_pair": pair or ("none yet (first --apply in this "
                                     "store)"),
            "console_note": ("rollback restores the receipts/last prev pair "
                             "(pad first, loadlist LAST, readback-cmp both)"),
            "overlay_backup": ov_backup or "n/a (no overlay plan)",
            "overlay_note": ("the journal's backup path is the overlay "
                             "rollback ref; overlay_builder uninstall "
                             "restores the pristine index"),
        },
    }
    return doc


def print_deploy_plan_document(doc):
    print("\n=== unified deploy plan (one document; saved to the receipt as "
          "deploy-plan.json) ===")
    print("  mode        %s" % doc["mode"])
    print("  surfaces    %s" % doc["surfaces"])
    tot = sum(u["bytes"] for u in doc["uploads"])
    print("  uploads     %d file(s), %d B total (loadlist.txt LAST)"
          % (len(doc["uploads"]), tot))
    ov = doc["overlay"]
    if ov.get("status") == "planned":
        p = ov.get("plan") or {}
        print("  overlay     status=planned index=%s mods=%s derived=%s... "
              "sidecar=%s... verify=%s"
              % (ov.get("index"), ",".join(ov.get("desired_mods") or []),
                 (p.get("derived_index_sha256") or "?")[:16],
                 (p.get("sidecar_sha256") or "?")[:16], ov.get("verify")))
    else:
        print("  overlay     status=%s%s" % (
            ov.get("status"),
            (" " + str(ov.get("detail"))[:120]) if ov.get("detail") else ""))
    print("  pad         %d B sha256 %s... (MFA row re-pinned: %s)"
          % (doc["pad"]["bytes"], doc["pad"]["sha256"][:16],
             doc["pad"]["mfa_row"]))
    print("  loadlist    %d B sha256 %s... (base=%s)"
          % (doc["loadlist"]["bytes"], doc["loadlist"]["sha256"][:16],
             doc["loadlist"]["base"]))
    rb = doc["rollback"]
    print("  rollback    console pair: %s; overlay backup: %s"
          % (", ".join(rb["console_pair"]) if isinstance(
              rb["console_pair"], list) else rb["console_pair"],
             rb["overlay_backup"]))


def cmd_deploy(root, args):
    if args.apply:
        guard_apply(root, args)
    km, loadlist, managed, ledger, prev_loadlist, overlay_plan = \
        _deploy_common(root, args)

    # Upload plan: UPLOAD ORDER LAW -- mod files, then PS5Pad.ini, then
    # loadlist.txt LAST (the manifest references the rest; MFA reads it at
    # module_start).
    plan = [(rel, local)
            for rel, (size, local) in sorted(managed.items())
            if rel != PAD_ROW_KEY and rel != LOADLIST_REMOTE_REL]
    plan.append((PAD_ROW_KEY, os.path.join(root.staging, "PS5Pad.ini")))
    plan.append((LOADLIST_REMOTE_REL,
                 os.path.join(root.staging, "loadlist.txt")))

    print("\n=== upload plan (order law: mod files -> PS5Pad.ini -> "
          "loadlist.txt last) ===")
    for rel, local in plan:
        print("  %-70s %8d B  sha256 %s" % (rel, os.path.getsize(local),
                                            sha256_file(local)[:16] + "..."))
    scope_ok = all(rel == PAD_ROW_KEY or rel == LOADLIST_REMOTE_REL or
                   rel.startswith("ue4ss/Mods/") for rel, _ in plan)
    if not scope_ok:
        raise Refusal("internal scope violation: plan contains a path "
                      "outside ue4ss/Mods/ + PS5Pad.ini + loadlist.txt")

    doc = build_deploy_plan_document(
        root, "apply" if args.apply else "dry-run", plan, km, loadlist,
        ledger["base"], overlay_plan)
    print_deploy_plan_document(doc)

    if not args.apply:
        print("\nDRY-RUN: no console writes. Re-run with --apply "
              "(requires --i-know-console-is-on and a live console) to "
              "execute.")
        write_receipt(root, "deploy-dry", ledger, loadlist,
                      os.path.join(root.staging, "PS5Pad.ini"),
                      prev_loadlist=prev_loadlist, applied=False,
                      extra_files={"deploy-plan.json": json.dumps(
                          doc, indent=2, sort_keys=True) + "\n"})
        return 0

    # ---- apply: upload + readback-verify EVERY file ----
    prev_pad_path = os.path.join(root.staging, "prev-PS5Pad.ini")
    r = ftp_get(root, PAD_ROW_KEY, prev_pad_path)
    prev_pad = None
    if r["rc"] == 0 and os.path.isfile(prev_pad_path):
        prev_pad = open(prev_pad_path, "rb").read()
    for rel, local in plan:
        up = ftp_put(root, local, rel)
        ledger["uploads"].append({"rel": rel, "rc": up["rc"],
                                  "bytes": os.path.getsize(local)})
        if up["rc"] != 0:
            raise Refusal("upload FAILED for %s (curl rc=%s err=%s) -- "
                          "console state partially updated; reconcile "
                          "before any launch (loadlist is uploaded LAST, "
                          "so admission still references the previous "
                          "state)" % (rel, up["rc"],
                                      (up["err"] or "")[:120]))
        rb = os.path.join(root.staging, "readback",
                          rel.replace("/", "__"))
        os.makedirs(os.path.dirname(rb), exist_ok=True)
        g = ftp_get(root, rel, rb)
        ok = g["rc"] == 0 and os.path.isfile(rb) and \
            open(rb, "rb").read() == open(local, "rb").read()
        ledger["checks"].append({"rel": rel, "readback_ok": ok})
        if not ok:
            raise Refusal("READBACK MISMATCH for %s -- verify + reconcile "
                          "before any launch" % rel)
        info("uploaded + readback verified: %s (%d B)" % (rel,
                                                          os.path.getsize(
                                                              local)))
    # local overlay publish (console shipping of the overlay trio is
    # Phase B; the console surfaces above are already verified)
    ledger["overlay_applied"] = overlay_execute_apply(root, overlay_plan,
                                                      args)
    write_receipt(root, "deploy", ledger, loadlist,
                  os.path.join(root.staging, "PS5Pad.ini"),
                  prev_loadlist=prev_loadlist,
                  prev_pad=prev_pad, applied=True,
                  extra_files={"deploy-plan.json": json.dumps(
                      doc, indent=2, sort_keys=True) + "\n"})
    info("deploy APPLIED and verified. Reminder: settings/PRX/ploader were "
         "not touched by this tool.")
    return 0


def cmd_audit(root, args):
    problems, warnings = [], []
    # 1. store integrity (never-edit-mods law re-verification)
    for name in sorted(root.manifest["mods"]):
        entry = root.manifest["mods"][name]
        for f in entry["files"]:
            p = os.path.join(root.mod_dir(name), *f["rel"].split("/"))
            if not os.path.isfile(p):
                problems.append("store: %s/%s missing from store" % (name,
                                                                     f["rel"]))
                continue
            size, sha = os.path.getsize(p), sha256_file(p)
            if sha != f["sha256"] or size != f["size"]:
                problems.append(
                    "store: %s/%s DRIFTED (size %d/%d, sha %s../%s..) -- "
                    "never-edit-mods violation; reinstall the mod from its "
                    "original source" % (name, f["rel"], size, f["size"],
                                         sha[:12], f["sha256"][:12]))
        td = tree_digest([(f["rel"], f["size"], f["sha256"])
                          for f in entry["files"]])
        if td != entry["source_tree_sha256"]:
            problems.append("store: %s tree digest mismatch" % name)
        pf = entry.get("preflight", {}).get("verdict")
        if entry.get("enabled") and pf not in ("LOADS-CLEAN", "UNAVAILABLE",
                                               "NOT-APPLICABLE"):
            warnings.append("%s enabled but preflight verdict is %s"
                            % (name, pf))
        ov = entry.get("overlay")
        if ov and ov.get("verdict") == "REFUSED" and entry.get("enabled"):
            warnings.append("%s overlay admission REFUSED (code=%s) -- "
                            "deploy will refuse while it is enabled"
                            % (name, ov.get("code")))
    # 2. keymap + loadlist consistency
    km = regen_keymap(root)
    pad = os.path.join(root.staging, "PS5Pad.ini")
    base_text, base_src = load_base_loadlist(root)
    loadlist, managed = synthesize_loadlist(root, base_text, pad, km["size"])
    write_atomic(os.path.join(root.staging, "loadlist.txt"), loadlist)
    print("audit base=%s; staged PS5Pad.ini %d B; synthesized loadlist %d B"
          % (base_src, km["size"], len(loadlist.encode())))
    rows = parse_loadlist(loadlist)
    staged = {rel: size for rel, (size, _l) in managed.items()}
    for row in rows:
        if not row["is_file"]:
            continue
        key, declared = row["key"], int(row["fields"][3])
        if key in staged:
            if declared != staged[key]:
                problems.append("loadlist: row %s pins %d B but staged "
                                "file is %d B (MFA would fail code=-8)"
                                % (key, declared, staged[key]))
        elif key.startswith("ue4ss/Mods/"):
            modname = key.split("/")[2]
            if modname in root.manifest["mods"]:
                problems.append("loadlist: row %s belongs to installed mod "
                                "%s but is not in the managed row set"
                                % (key, modname))
    pad_row = [r for r in rows if r["key"] == PAD_ROW_KEY]
    expect = pad_row[0]["fields"][3] if pad_row else None
    if expect is None:
        problems.append("loadlist: no PS5Pad.ini row after synthesis")
    else:
        if int(expect) != km["size"]:
            problems.append("loadlist: pad row %s != regenerated pad %d B"
                            % (expect, km["size"]))
        chk = keymap_check(root, pad, int(expect), quiet=not args.verbose)
        print("FV --check (expect-size %s): %d violations, %d warnings"
              % (expect, chk["violations"], chk["warnings"]))
        problems += ["keymap: " + l for l in chk["output"].splitlines()
                     if l.startswith("VIOLATION ")]
        warnings += ["keymap: " + l for l in chk["output"].splitlines()
                     if l.startswith("WARN ")]
    # 3. drift vs last known console receipt
    last = os.path.join(root.receipts, "last", "loadlist.txt")
    if args.console:
        up, _ = probe_console(root)
        if up:
            dest = os.path.join(root.staging, "audit-live-loadlist.txt")
            r = ftp_get(root, LOADLIST_REMOTE_REL, dest)
            if r["rc"] == 0 and os.path.isfile(dest):
                live = open(dest, "r", encoding="utf-8").read()
                _drift_report(live, loadlist, problems, warnings,
                              label="live console")
        else:
            warnings.append("--console: console unreachable; drift check "
                            "against live skipped")
    elif os.path.isfile(last):
        _drift_report(open(last, "r", encoding="utf-8").read(), loadlist,
                      problems, warnings, label="last receipt")
    else:
        info("no last receipt yet -- drift check limited to store/synthesis")
    # 4. overlay journal state (builder list + verify; Mac-local mirror)
    cfg = overlay_config(root)
    if cfg["builder"] and cfg["paks_dir"] and cfg["index"] and \
            os.path.isfile(cfg["builder"]):
        desired = overlay_desired(root)
        lst = run_overlay(root, "list")
        ver = run_overlay(root, "verify")
        if lst["rc"] == 0 and isinstance(lst["json"], dict) and \
                lst["json"].get("result") == "listed":
            for mid in lst["json"].get("mods", []):
                mod_id = mid.get("mod_id", "")
                mgr_name = mod_id.split("--")[0]
                if mod_id not in desired:
                    warnings.append(
                        "overlay: journal mod %s is not in the enabled set "
                        "(manager mod %r is disabled or uninstalled) -- the "
                        "next deploy re-derives the overlay without it"
                        % (mod_id, mgr_name))
            missing_desired = [m for m in desired
                               if m not in [x.get("mod_id") for x in
                                            lst["json"].get("mods", [])]]
            if missing_desired:
                info("overlay: %d enabled overlay mod(s) not yet in the "
                     "journal (first deploy publishes them): %s"
                     % (len(missing_desired), ",".join(missing_desired)))
        if ver["rc"] == 0 and isinstance(ver["json"], dict):
            verdict = ver["json"].get("verdict")
            print("overlay verify: %s (journal %s)" % (verdict, cfg["state_dir"]))
            if verdict in ("STALE", "DRIFT"):
                warnings.append("overlay: journal verify=%s -- reconcile "
                                "with overlay_builder.py verify before the "
                                "next deploy" % verdict)
        else:
            warnings.append("overlay: verify call failed (rc=%s)"
                            % ver["rc"])
    print("\naudit: %d violation(s), %d warning(s)" % (len(problems),
                                                       len(warnings)))
    for w in warnings:
        print("WARN " + w)
    for p in problems:
        print("VIOLATION " + p)
    if problems:
        return 2
    return 1 if warnings else 0


def _drift_report(known_text, synthesized, problems, warnings, label):
    """Rows changed outside the manager = drift. Compares only NON-managed
    rows (managed rows are supposed to differ after state changes)."""
    known = {r["key"]: r["raw"] for r in parse_loadlist(known_text)
             if r["is_file"]}
    new = {r["key"]: r["raw"] for r in parse_loadlist(synthesized)
           if r["is_file"]}
    for key, raw in sorted(known.items()):
        if key in new and new[key] != raw and \
                key != PAD_ROW_KEY and \
                not _key_of_installed_mod(key):
            warnings.append("drift vs %s: %s changed %r -> %r outside the "
                            "manager" % (label, key, raw, new[key]))
    for key in sorted(set(known) - set(new)):
        if key != PAD_ROW_KEY and not _key_of_installed_mod(key):
            warnings.append("drift vs %s: row %s disappeared" % (label, key))


def _key_of_installed_mod(key):
    parts = key.split("/")
    return len(parts) > 3 and parts[2] in _installed_names_global


_installed_names_global = set()


def cmd_rollback(root, args):
    last = os.path.join(root.receipts, "last")
    prev_ll = os.path.join(last, "prev-loadlist.txt")
    prev_pad = os.path.join(last, "prev-PS5Pad.ini")
    if not (os.path.isfile(prev_ll) and os.path.isfile(prev_pad)):
        raise Refusal("nothing to roll back: %s lacks the previous "
                      "loadlist/PS5Pad pair (no successful --apply deploy "
                      "has happened in this store)" % last)
    if args.apply:
        guard_apply(root, args)
    prev_ll_text = open(prev_ll, "r", encoding="utf-8").read()
    print("rollback plan: restore the previous receipt pair")
    print("  loadlist.txt   %6d B  sha256 %s..."
          % (len(prev_ll_text.encode()),
             sha256_bytes(prev_ll_text.encode())[:16]))
    print("  ue4ss/PS5Pad.ini %4d B  sha256 %s..."
          % (os.path.getsize(prev_pad), sha256_file(prev_pad)[:16]))
    print("  (upload order: PS5Pad.ini first, loadlist.txt last; readback "
          "cmp both)")
    if not args.apply:
        print("\nDRY-RUN: no console writes. Add --apply (requires "
              "--i-know-console-is-on and a live console) to execute.")
        write_receipt(root, "rollback-dry", {"plan": "restore prev pair"},
                      prev_ll_text, prev_pad, applied=False)
        return 0
    for rel, local in ((PAD_ROW_KEY, prev_pad),
                       (LOADLIST_REMOTE_REL, prev_ll)):
        up = ftp_put(root, local, rel)
        if up["rc"] != 0:
            raise Refusal("rollback upload FAILED for %s (rc=%s err=%s) -- "
                          "reconcile before any launch"
                          % (rel, up["rc"], (up["err"] or "")[:120]))
        rb = os.path.join(root.staging, "readback-rollback",
                          rel.replace("/", "__"))
        os.makedirs(os.path.dirname(rb), exist_ok=True)
        g = ftp_get(root, rel, rb)
        if not (g["rc"] == 0 and os.path.isfile(rb) and
                open(rb, "rb").read() == open(local, "rb").read()):
            raise Refusal("rollback READBACK MISMATCH for %s" % rel)
        info("restored + verified: %s" % rel)
    write_receipt(root, "rollback", {"restored": "prev pair"},
                  prev_ll_text, prev_pad, applied=True)
    info("rollback APPLIED. Note: manager state (enabled mods) is "
         "unchanged; the next deploy will re-apply it.")
    return 0


def cmd_list(root, args):
    mods = root.manifest["mods"]
    if not mods:
        print("store is empty (no mods installed)")
        return 0
    print("%-24s %-7s %-6s %-9s %-14s %-9s %s"
          % ("mod", "enabled", "files", "bytes", "preflight", "overlay",
             "source"))
    for name in root.ordered_mods():
        e = mods[name]
        ov = e.get("overlay", {}).get("verdict", "-")
        print("%-24s %-7s %-6d %-9d %-14s %-9s %s"
              % (name, "yes" if e.get("enabled") else "no", len(e["files"]),
                 e.get("bytes_total", 0),
                 e.get("preflight", {}).get("verdict", "-")[:14],
                 ov[:9],
                 e.get("source", "")[:60]))
    print("\npriority order: %s" % ", ".join(root.ordered_mods()))
    km_note = ("staging/PS5Pad.ini and staging/loadlist.txt reflect the "
               "CURRENT state; run `deploy` to plan a console update.")
    print(km_note)
    return 0


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def add_apply_flags(p):
    p.add_argument("--apply", action="store_true",
                   help="perform the FTP writes (default: dry-run, zero "
                        "console contact)")
    p.add_argument("--i-know-console-is-on", action="store_true",
                   help="required with --apply: acknowledge the console is "
                        "powered on and signed in")
    p.add_argument("--console-ip", default=None,
                   help="override console IP for this invocation")
    p.add_argument("--http-port", type=int, default=None)


def main(argv):
    ap = argparse.ArgumentParser(
        prog="modmanager.py", description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", default=HERE,
                    help="manager root (store/staging/receipts/config.json);"
                         " default: the mod-manager directory")
    ap.add_argument("--game-dump-root", default=None,
                    help="override config game_dump_root: local mirror of "
                         "the game dump paks dir (overlay planning target)")
    ap.add_argument("--overlay-index", default=None,
                    help="override config overlay_index: base container "
                         "index name, e.g. pakchunk0-ps5.utoc")
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("install", help="stage a mod UNMODIFIED + preflight "
                                       "+ keymap + loadlist row add")
    p.add_argument("source", help="mod dir (contains Scripts/main.lua) or "
                                  "zip of one")
    p.add_argument("--name", default=None,
                   help="override mod name (default: source dir name)")
    p.add_argument("--enable", action="store_true",
                   help="enable immediately (adds its rows to synthesis)")
    p.set_defaults(fn=cmd_install)

    p = sub.add_parser("enable", help="row presence on")
    p.add_argument("name")
    p.set_defaults(fn=cmd_enable)

    p = sub.add_parser("disable", help="row presence off")
    p.add_argument("name")
    p.set_defaults(fn=cmd_disable)

    p = sub.add_parser("uninstall", help="row removal + store delete")
    p.add_argument("name")
    p.add_argument("--yes", action="store_true",
                   help="silence the still-enabled notice")
    p.set_defaults(fn=cmd_uninstall)

    p = sub.add_parser("deploy", help="synthesize + (apply) upload "
                                      "loadlist/pad/mod files")
    add_apply_flags(p)
    p.add_argument("--allow-unmapped", action="store_true",
                   help="accept unmapped keybinds (documented WARN block)")
    p.add_argument("--allow-cofire", action="store_true",
                   help="accept shared-key co-fire conflicts explicitly")
    p.add_argument("--allow-overlay-conflict", action="store_true",
                   help="pass the overlay builder's later-wins policy for "
                        "package-id/chunk/virtual-path conflicts (default: "
                        "reject and refuse the deploy)")
    p.add_argument("--allow-preflight-fail", action="store_true",
                   help="deploy enabled mods whose host-harness preflight "
                          "did not pass, after review")
    p.set_defaults(fn=cmd_deploy)

    p = sub.add_parser("keymap", help="regenerate + check PS5Pad.ini only")
    p.set_defaults(fn=cmd_keymap)

    p = sub.add_parser("audit", help="store integrity + loadlist "
                                     "consistency + drift report")
    p.add_argument("--console", action="store_true",
                   help="also probe the console and diff the LIVE loadlist "
                        "(read-only)")
    p.add_argument("--verbose", action="store_true",
                   help="print every WARN/VIOLATION line from FV --check")
    p.set_defaults(fn=cmd_audit)

    p = sub.add_parser("rollback", help="restore the previous receipt pair")
    add_apply_flags(p)
    p.set_defaults(fn=cmd_rollback)

    p = sub.add_parser("list", help="status table")
    p.set_defaults(fn=cmd_list)

    args = ap.parse_args(argv)
    root = Root(args.root)
    if getattr(args, "console_ip", None):
        root.config["console_ip"] = args.console_ip
    if getattr(args, "http_port", None):
        root.config["http_port"] = args.http_port
    if getattr(args, "game_dump_root", None):
        root.config["game_dump_root"] = args.game_dump_root
    if getattr(args, "overlay_index", None):
        root.config["overlay_index"] = args.overlay_index
    global _installed_names_global
    _installed_names_global = set(root.manifest["mods"])
    print(VERSION)
    try:
        return args.fn(root, args)
    except Refusal as e:
        print("[mm] REFUSED: %s" % e)
        return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
