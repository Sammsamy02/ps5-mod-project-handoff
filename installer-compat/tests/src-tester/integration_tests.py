#!/usr/bin/env python3
"""END-TO-END integration tests across the sibling modules. Mac-only.

Wires the REAL module CLIs/libraries together (current consolidated layout,
paths resolved via framework.py; originally mod-types/, compat-checker/,
installer-core/, uninstall/ under /tmp/sideproject):
  installer/detector.py  -> classify
  compat/checker.py      -> grade (against a status-filled catalog)
  installer/installer.py -> install / verify / recover
  installer/conflicts.py -> pre-install conflict check
  installer/uninstall.py -> uninstall (+ its own state schema)

IMPEDANCE NOTE (a finding, not a workaround we hide): the installer's
manifest (manifests/<Mod>.json: install_path ABSOLUTE paths, loadlist rows)
and the uninstaller's record (install_state.json + files[].path RELATIVE +
config line lists) are DIFFERENT contracts. Nobody shipped an adapter, so
`adapt()` below converts installer output -> uninstaller input. Without it
the modules cannot interoperate at all.
"""

import json
import os
import subprocess
import sys
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from framework import (A, Sandbox, ZipKit,  # noqa: E402
                       DETECTOR, CHECKER, INSTALLER, CATALOG, INSTALLER_DIR)

sys.path.insert(0, INSTALLER_DIR)  # uninstall.py + conflicts.py live in installer/
import conflicts as C  # noqa: E402

TITLE = "PPSA01325"
ENV = dict(os.environ, PYTHONIOENCODING="utf-8")

CAT_STATUSES = None  # filled at import: map of name->status from sample cat


def sh(cmd, timeout=180):
    p = subprocess.run([sys.executable] + cmd, capture_output=True,
                       text=True, timeout=timeout, env=ENV)
    return p.returncode, p.stdout, p.stderr


def adapt_install_to_uninstall(sb, t, mod_dir_name, game_root):
    """Installer manifest -> uninstaller install_state.json + manifest.

    game_root for the uninstaller is the directory that contains the mod
    files RELATIVELY: we map <roots>/<TITLE>/ -> game_root so that
    'ue4ss/Mods/<mod>/...' paths work.
    """
    mpath = os.path.join(t.roots, TITLE, "manifests", mod_dir_name + ".json")
    mf = json.load(open(mpath))
    files = []
    for f in mf.get("files", []):
        rel = os.path.relpath(f["install_path"],
                              os.path.join(t.roots, TITLE))
        files.append({"path": rel, "sha256": f["original_sha256"]})
    cfg = {}
    ce = mf.get("config_entries", {})
    if ce.get("loadlist_rows"):
        cfg["loadlist.txt"] = list(ce["loadlist_rows"])
    ploader_lines = []
    pl = ce.get("ploader_prx_name")
    if pl:
        ploader_lines.append("%s=%s" % (pl, ce.get("ploader_prx_mode", "2")))
    if ploader_lines:
        cfg["ploader.ini"] = ploader_lines
    # PORT (2026-10-07): since the tester ran, the installer also creates a
    # UE4SS enable marker (Mods/<mod>/enabled.txt, recorded as
    # config_entries.enabled_marker_rel) and a ue4ss/Mods/mods.txt row
    # (mods_txt_row). The uninstaller manifest schema grew matching support:
    # "enabled_markers" (existence-only flags, moved to trash and included in
    # dir pruning) and mods.txt config-line removal. Translate both so the
    # uninstall is complete under the CURRENT contract; without the marker
    # the mod dir survives (enabled.txt left behind).
    markers = []
    if ce.get("enabled_marker_rel"):
        markers.append(ce["enabled_marker_rel"])
    if ce.get("mods_txt_row"):
        mt_rel = "ue4ss/Mods/mods.txt"
        if ce.get("mods_txt_path"):
            mt_rel = os.path.relpath(ce["mods_txt_path"],
                                     os.path.join(t.roots, TITLE))
        cfg.setdefault(mt_rel, []).append(ce["mods_txt_row"])
    state_path = os.path.join(sb.path, "install_state.json")
    state = {"game_version": "1.004.000", "mods": {}}
    if os.path.exists(state_path):
        state = json.load(open(state_path))
    state["mods"][mod_dir_name] = {
        "manifest": mpath, "requires": [], "files": files}
    with open(state_path, "w") as f:
        json.dump(state, f)
    um = {"mod_id": mod_dir_name, "name": mf.get("mod_name", mod_dir_name),
          "mod_version": mf.get("version", "1"),
          "requires": [], "files": files, "config_entries": cfg,
          "enabled_markers": markers}
    ump = os.path.join(sb.path, mod_dir_name + ".u.manifest.json")
    with open(ump, "w") as f:
        json.dump(um, f)
    return ump, state_path


def register(h):
    # ================================================== L1: full lifecycle
    @h.add("INT-L1-detect-compat-install-verify-uninstall", "integration")
    def _(sb=None):
        t = type("T", (), {})()
        t.roots = os.path.join(sb.path, "roots")
        t.plugins = os.path.join(sb.path, "plugins")
        t.state = os.path.join(sb.path, "state")

        z = ZipKit(sb.path)
        mod_zip = z.normal("lifecycle.zip", {
            "LifeMod/Scripts/main.lua":
                "local it = FindFirstOf('Item')\n"
                "while it do it = FindNextOf(it) end\n",
            "LifeMod/config/overrides.ini": "[x]\ny=1\n",
        })
        # 1. detect
        rc, out, err = sh([DETECTOR, "--compact", mod_zip])
        A.eq(rc, 0, f"detector rc={rc} {err[:150]}")
        det = json.loads(out)[0]
        A.eq(det["label"], "ue4ss-lua", f"label={det['label']}")
        # 2. compat (catalog-backed)
        rc, out, err = sh([CHECKER, mod_zip, CATALOG, "--format", "json"])
        # checker wants .lua files or dir; feed the extracted main.lua
        src_dir = os.path.join(sb.path, "src")
        with zipfile.ZipFile(mod_zip) as zf:
            zf.extractall(src_dir)
        rc, out, err = sh([CHECKER,
                           os.path.join(src_dir, "LifeMod", "Scripts"),
                           CATALOG, "--format", "json"])
        A.eq(rc, 0, f"checker rc={rc} {err[:150]}")
        grade = json.loads(out)["grade"]
        A.ok(grade in ("compatible", "partly-compatible", "unknown"),
             f"grade={grade}")
        # 3. install
        mdp = os.path.join(sb.path, "meta.json")
        json.dump({"name": "LifeMod", "version": "1.0.0"},
                  open(mdp, "w"))
        rc, out, err = sh([INSTALLER, "install", "--archive", mod_zip,
                           "--metadata", mdp, "--title", TITLE,
                           "--roots", t.roots, "--plugins", t.plugins,
                           "--state", t.state])
        A.eq(rc, 0, f"install rc={rc} {err[:200]}")
        # 4. verify
        rc, out, err = sh([INSTALLER, "verify", "--title", TITLE,
                           "--mod", "LifeMod", "--roots", t.roots,
                           "--plugins", t.plugins, "--state", t.state])
        A.eq(rc, 0, f"verify rc={rc} {out[:150]} {err[:150]}")
        # 5. uninstall (via adapter)
        from uninstall import Uninstaller
        ump, state_path = adapt_install_to_uninstall(
            sb, t, "LifeMod", os.path.join(t.roots, TITLE))
        u = Uninstaller(os.path.join(t.roots, TITLE),
                        state_path=state_path,
                        transaction_root=os.path.join(sb.path, "txn"))
        res = u.uninstall(ump)
        # 6. verify clean
        mods_dir = os.path.join(t.roots, TITLE, "ue4ss", "Mods", "LifeMod")
        A.ok(not os.path.exists(mods_dir), "mod dir left after uninstall")
        ll = os.path.join(t.roots, TITLE, "loadlist.txt")
        ll_txt = open(ll).read()
        A.ok("mod\tLifeMod" not in ll_txt,
             "loadlist row left after uninstall")
        pl = open(os.path.join(t.plugins, "ploader.ini")).read()
        A.ok("LifeMod" not in pl or
             "modbridge_supplier_composition.prx" not in pl,
             "ploader entry left after uninstall")
        # installer-side verify must now FAIL (mod gone)
        rc, out, err = sh([INSTALLER, "verify", "--title", TITLE,
                           "--mod", "LifeMod", "--roots", t.roots,
                           "--plugins", t.plugins, "--state", t.state])
        A.ok(rc != 0, "installer verify still ok after uninstall")

    # ================================================== L2: conflict pair
    @h.add("INT-L2-conflicting-mods", "integration")
    def _(sb=None):
        t = type("T", (), {})()
        t.roots = os.path.join(sb.path, "roots")
        t.plugins = os.path.join(sb.path, "plugins")
        t.state = os.path.join(sb.path, "state")
        z = ZipKit(sb.path)
        shared = "ue4ss/Mods/_shared/overlay.lua"
        za = z.normal("modA.zip", {"ModA/Scripts/main.lua": "-- A\n",
                                   "ModA/overrides/shared.lua": "-- A\n"})
        zb = z.normal("modB.zip", {"ModB/Scripts/main.lua": "-- B\n",
                                   "ModB/overrides/shared.lua": "-- B\n"})
        # model the CONFLICT at the conflicts-module level: both mods claim
        # the same target path 'ue4ss/Mods/<mod>/overrides/shared.lua' is
        # per-mod unique — so model a true shared claim via same rel path:
        specA = C.ModSpec(mod_id="ModA",
                          files=("ue4ss/Mods/_shared/overlay.lua",))
        specB = C.ModSpec(mod_id="ModB",
                          files=("ue4ss/Mods/_shared/overlay.lua",))
        # install A first (real installer), build state, then check B
        mdp = os.path.join(sb.path, "mA.json")
        json.dump({"name": "ModA", "version": "1.0"}, open(mdp, "w"))
        rc, out, err = sh([INSTALLER, "install", "--archive", za,
                           "--metadata", mdp, "--title", TITLE,
                           "--roots", t.roots, "--plugins", t.plugins,
                           "--state", t.state])
        A.eq(rc, 0, f"install A rc={rc} {err[:150]}")
        # adapter-state for conflicts module (registry view of installed A)
        state_path = os.path.join(sb.path, "install_state.json")
        json.dump({"game_version": "1.004.000", "mods": {"ModA": {
            "requires": [],
            "files": [{"path": "ue4ss/Mods/_shared/overlay.lua"}]}}},
            open(state_path, "w"))
        rep = C.check_install(specB, state_path)
        A.ok(not rep.ok, "conflict for B NOT detected while A installed")
        A.eq(rep.blockers[0].kind, C.PATH_CONFLICT)
        # uninstall A through the uninstall module, then B must be clean
        from uninstall import Uninstaller
        ump = os.path.join(sb.path, "a.u.manifest.json")
        json.dump({"mod_id": "ModA", "requires": [],
                   "files": [{"path": "ue4ss/Mods/_shared/overlay.lua",
                              "sha256": None}],
                   "config_entries": {}}, open(ump, "w"))
        u = Uninstaller(os.path.join(t.roots, TITLE),
                        state_path=state_path,
                        transaction_root=os.path.join(sb.path, "txn"))
        # NOTE: A's real install put files under ue4ss/Mods/ModA/, not the
        # shared path — the uninstaller removes what the manifest claims.
        # For L2 the shared-path claim is the conflicts-model view.
        res = u.uninstall(ump)
        rep2 = C.check_install(specB, state_path)
        A.ok(rep2.ok, f"conflict persisted after uninstalling A: {rep2}")
        # install B for real
        mdb = os.path.join(sb.path, "mB.json")
        json.dump({"name": "ModB", "version": "1.0"}, open(mdb, "w"))
        rc, out, err = sh([INSTALLER, "install", "--archive", zb,
                           "--metadata", mdb, "--title", TITLE,
                           "--roots", t.roots, "--plugins", t.plugins,
                           "--state", t.state])
        A.eq(rc, 0, f"install B rc={rc} {err[:150]}")

    # ================================================== L3: tampered install
    @h.add("INT-L3-tampered-install", "integration")
    def _(sb=None):
        t = type("T", (), {})()
        t.roots = os.path.join(sb.path, "roots")
        t.plugins = os.path.join(sb.path, "plugins")
        t.state = os.path.join(sb.path, "state")
        z = ZipKit(sb.path)
        mz = z.normal("tamper.zip", {
            "TamperMod/Scripts/main.lua": "FindFirstOf('Item')\n",
            "TamperMod/assets/data.bin": b"\x00\x01payload-ish",
        })
        mdp = os.path.join(sb.path, "meta.json")
        json.dump({"name": "TamperMod", "version": "1.0"}, open(mdp, "w"))
        rc, out, err = sh([INSTALLER, "install", "--archive", mz,
                           "--metadata", mdp, "--title", TITLE,
                           "--roots", t.roots, "--plugins", t.plugins,
                           "--state", t.state])
        A.eq(rc, 0, f"install rc={rc} {err[:150]}")
        # tamper: flip a byte in main.lua
        mainp = os.path.join(t.roots, TITLE, "ue4ss", "Mods", "TamperMod",
                             "Scripts", "main.lua")
        data = bytearray(open(mainp, "rb").read())
        data[0] ^= 0xFF
        open(mainp, "wb").write(bytes(data))
        # installer verify catches it
        rc, out, err = sh([INSTALLER, "verify", "--title", TITLE,
                           "--mod", "TamperMod", "--roots", t.roots,
                           "--plugins", t.plugins, "--state", t.state])
        A.eq(rc, 3, f"verify rc={rc}")
        # uninstall must survive tampering: refuses without confirm, works
        # with confirm
        from uninstall import (Uninstaller, ConfirmationRequiredError)
        ump, state_path = adapt_install_to_uninstall(
            sb, t, "TamperMod", os.path.join(t.roots, TITLE))
        u = Uninstaller(os.path.join(t.roots, TITLE),
                        state_path=state_path,
                        transaction_root=os.path.join(sb.path, "txn"))
        try:
            u.uninstall(ump)
            A.ok(False, "tampered file removed without confirmation")
        except ConfirmationRequiredError:
            pass
        res = u.uninstall(ump, confirm_modified=True)
        A.ok(not os.path.exists(mainp), "tampered file not removed")
        A.ok(not os.path.exists(os.path.dirname(mainp)),
             "mod dir left behind after confirmed uninstall")
