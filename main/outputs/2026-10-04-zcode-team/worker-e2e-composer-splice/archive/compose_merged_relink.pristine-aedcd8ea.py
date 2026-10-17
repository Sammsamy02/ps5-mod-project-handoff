#!/usr/bin/env python3
"""compose_console_bridge.py — worker-zip-bridge-console-link (PS5 mod-manager).

Controller-step worker: link the worker-zip-to-mfb F-R1 archive->bundle bridge
TU into the CONSOLE-flavor installer payload. worker-zip-to-mfb delivered the
bridge host-only ("the console link was NOT run here"); this worker closes
that step without touching any original file, by the same vendored-copy +
recorded exact-anchor patch chain discipline:

Chain (every step sha-pinned, applied to text, never to originals):
  1. worker-installer-composition/compose_installer.py (read-only root; sha
     cross-checked against worker-e2e-final e2e-receipts/script-patches.json
     vendored_script_sha256).
  2. worker-e2e-final/compose_e2e.py (sha cross-checked against
     worker-zip-to-mfb e2e-receipts/bridge-script-patches.json
     vendored_driver_sha256 — the exact input that produced the green 75/75
     delivery; NOTE: e2e-final's own older adapted_script_sha256 pin covers a
     pre-delivery revision and is intentionally not the pin used here).
  3. worker-zip-to-mfb's 4 recorded splices replayed VERBATIM; the
     intermediate adapted driver text must hash exactly to zip-to-mfb's
     adapted_driver_sha256 — a byte-exact replay proof.
  4. THIS worker's 2 corrective splices:
       - zb-console-inner-name: distinct inner script name in THIS directory
         (composition_zb_console_inner.py);
       - zb-console-target-bridge: add owned/modinst_zip_bridge.c to the
         CONSOLE target list. Recorded defect: the delivered chain adds the
         bridge TU to the HOST target list only, while the zb-dispatch-bridge
         modinst.c patch makes modinst_core.c reference mi_bridge_* on BOTH
         flavors — the delivered console link would fail with undefined
         mi_bridge_run/wants_auto/mod_arg. This splice is the minimal fix.
  5. vendor the delivered owned set from worker-zip-to-mfb/owned (11 files),
     every sha cross-checked against zip-to-mfb's owned-manifest.json, and
     the 9 pre-bridge files additionally against worker-e2e-final's
     owned-manifest.json.
  6. run the adapted inner script (subprocess, MODBRIDGE_ROOT pinned) with
     --flavor console (the deliverable payload) and/or --flavor host (the
     regression binary for the 75/75 bridge suite).

Run:
    python3 compose_console_bridge.py --flavor console   # payload step
    python3 compose_console_bridge.py --flavor host      # suite binary
    sh test/run_bridge_tests.sh                          # 75/75 regression

No network; no console; originals never modified; all writes under
outputs/2026-10-04-zcode-team/worker-zip-bridge-console-link/.
"""
import argparse
import hashlib
import json
import os
import pathlib
import shutil
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = pathlib.Path(os.environ.get("MODBRIDGE_ROOT", HERE.parents[2]))
TEAM = ROOT / "outputs/2026-10-04-zcode-team"

INSTALLER = TEAM / "worker-installer-composition"
E2E = TEAM / "worker-e2e-final"
ZB = TEAM / "worker-zip-to-mfb"

ORIG_SCRIPT = INSTALLER / "compose_installer.py"
E2E_SCRIPT = E2E / "compose_e2e.py"
DRIVER = HERE / "composition_zb_console_inner.py"
OWNED = HERE / "owned"
REC = HERE / "receipts"

# ---- zip-to-mfb's 4 recorded splices, replayed VERBATIM (compose_bridge.py) --
S_INNER = ("bridge-script-inner-name",
           'INNER = HERE / "composition_e2e_inner.py"',
           'INNER = HERE / "composition_bridge_inner.py"   # worker-zip-to-mfb')

S_OWNED_LIST = ("bridge-script-owned-authored-list",
                '    for name in ("modinst_download.c", "modinst_download.h", "mb_curl_stub_host.c"):',
                '    for name in ("modinst_download.c", "modinst_download.h", "mb_curl_stub_host.c",\n'
                '                 "modinst_zip_bridge.c", "modinst_zip_bridge.h"):')

S_TARGETS = ("bridge-script-host-target",
             "             '        (\"mb_curl_stub_host\", HERE / \"owned/mb_curl_stub_host.c\",\\n'\n"
             "             '         core_flags + [\"-I\" + str(VEND / \"download\")]),\\n'",
             "             '        (\"mb_curl_stub_host\", HERE / \"owned/mb_curl_stub_host.c\",\\n'\n"
             "             '         core_flags + [\"-I\" + str(VEND / \"download\")]),\\n'\n"
             "             '        (\"modinst_zip_bridge\", HERE / \"owned/modinst_zip_bridge.c\", core_flags),\\n'")

S_PATCH_APPEND = ("bridge-script-patches-append",
                  '    ("e2e-usage", C_USAGE_OLD, C_USAGE_NEW, 1),\n]',
                  '    ("e2e-usage", C_USAGE_OLD, C_USAGE_NEW, 1),\n]\n' + '''
# ---- worker-zip-to-mfb (F-R1 bridge) patch tuples, applied after the e2e chain
ZB_INC_OLD = """#ifdef MI_HAS_DOWNLOAD
#include "modinst_download.h"   /* worker-e2e-final: stage-1 owner transport */
#endif"""
ZB_INC_NEW = ZB_INC_OLD + '\\n#include "modinst_zip_bridge.h"   /* worker-zip-to-mfb: F-R1 quarantine->mfb bridge */'

ZB_FLAG_OLD = "        else if (!strcmp(argv[i], \\"--job\\") && i + 1 < argc) ++i;          /* download (owned command parses it) */"
ZB_FLAG_NEW = ZB_FLAG_OLD + '\\n        else if (!strcmp(argv[i], \\"--no-auto-bundle\\")) ;         /* value-less opt-out (worker-zip-to-mfb bridge parses it) */'

ZB_DISP_OLD = "              if (e2e_pr == 0) records_mark_archive_prepared(argc, argv);"
ZB_DISP_NEW = """              if (e2e_pr == 0 && mi_bridge_wants_auto(argc, argv)) {
                  /* worker-zip-to-mfb (F-R1): compose a kind-1 mfb from the
                   * quarantine BEFORE the records transition; on success the
                   * record's bundle becomes the bridge output so install
                   * resolves a real modforge bundle. A refusal keeps the
                   * record at downloaded and the quarantine retained. */
                  char bridge_bundle[LINE_MAX_ * 2];
                  e2e_pr = mi_bridge_run(argc, argv, g_state_dir,
                                         bridge_bundle, sizeof bridge_bundle);
                  if (e2e_pr == 0) {
                      modrec *bm = find_mod(mi_bridge_mod_arg(argc, argv));
                      if (bm) snprintf(bm->bundle, sizeof bm->bundle, "%s", bridge_bundle);
                  }
              }
              if (e2e_pr == 0) records_mark_archive_prepared(argc, argv);"""

ZB_USAGE_OLD = '    say("  prepare-archive --title T --archive F --quarantine D [--mod ID]   (stage 2a)");'
ZB_USAGE_NEW = ZB_USAGE_OLD + '\\n    say("          [--no-auto-bundle]  (F-R1 bridge: with --mod, default composes bundles/<id>.mfb from the quarantine)");'

EXTRA_PATCHES += [
    ("zb-inc-bridge", ZB_INC_OLD, ZB_INC_NEW, 1),
    ("zb-flag-no-auto-bundle", ZB_FLAG_OLD, ZB_FLAG_NEW, 1),
    ("zb-dispatch-bridge", ZB_DISP_OLD, ZB_DISP_NEW, 1),
    ("zb-usage-bridge", ZB_USAGE_OLD, ZB_USAGE_NEW, 1),
]
''')

ZB_SPLICES = [S_INNER, S_OWNED_LIST, S_TARGETS, S_PATCH_APPEND]

# ---- THIS worker's corrective splices (applied AFTER the byte-exact replay) --
# The console target list does not exist in driver text (it is produced at
# inner-generation time), so the console fix extends the driver's splice
# sequence with one more exact-anchor tuple over the vendored
# compose_installer.py text — the same mechanism the delivered chain uses.
MY_SPLICES = [
    ("zb-console-inner-name",
     'INNER = HERE / "composition_bridge_inner.py"   # worker-zip-to-mfb',
     'INNER = HERE / "composition_zb_console_inner.py"   # worker-zip-bridge-console-link'),
    ("zb-console-targets-seq",
     '    for pid, old, new in (S_DL_DEF, S_MKDIR, S_VENDOR, S_COREFLAGS, S_TARGETS):',
     '    for pid, old, new in (S_DL_DEF, S_MKDIR, S_VENDOR, S_COREFLAGS, S_TARGETS,\n'
     '                          S_CONSOLE_TARGETS):'),
    ("zb-console-target-def",
     '             \'        obj = out / (name + ".o")\')\n'
     'PATCHES_TAIL_HEAD =',
     '             \'        obj = out / (name + ".o")\')\n'
     "S_CONSOLE_TARGETS = (\"zb-console-target-bridge\",\n"
     "                     '        (\"posix_compat\", VEND / \"archive/private/posix_compat.c\", closure_flags),\\n'\n"
     "                     '    ]\\n'\n"
     "                     '    commands = []\\n'\n"
     "                     '    objects = []\\n'\n"
     "                     '    for name, src, flags in targets:\\n'\n"
     "                     '        obj = out / (name + \".ps5.o\")',\n"
     "                     '        (\"posix_compat\", VEND / \"archive/private/posix_compat.c\", closure_flags),\\n'\n"
     "                     '        (\"modinst_zip_bridge\", HERE / \"owned/modinst_zip_bridge.c\", core_flags),\\n'\n"
     "                     '    ]\\n'\n"
     "                     '    commands = []\\n'\n"
     "                     '    objects = []\\n'\n"
     "                     '    for name, src, flags in targets:\\n'\n"
     "                     '        obj = out / (name + \".ps5.o\")')\n"
     'PATCHES_TAIL_HEAD ='),
    # ---- worker-d1-chain-relink merged-chain splices (D1 fix FIRST,
    # F-R2 fix LAST; canonical 51-tuple order per worker-risk-e2e/
    # D1-PROPAGATION.md section 4). Block texts verified verbatim
    # against both fix-origin patches.json pins by the builder. ----
    ("d1-uf-block-append",
     '    ("zb-usage-bridge", ZB_USAGE_OLD, ZB_USAGE_NEW, 1),\n]',
     '    ("zb-usage-bridge", ZB_USAGE_OLD, ZB_USAGE_NEW, 1),\n]\n# ---- worker-uninstall-fault-expansion patch tuples, applied after the bridge\n# Defect D1 (found by adversarial window analysis + d1_window_probe.sh on the\n# baseline): cmd_install deleted the journal BEFORE committing records, so a\n# death in between left "journal durably gone + records stale" -- an end state\n# no recovery can repair (no journal == "nothing in flight"), and records\n# claimed installed while files were already gone. Fix: make records durable\n# FIRST in all three commit sites (cmd_install + both recover_title commit\n# paths). The residual window (records durable, journal present) is repaired\n# idempotently by recover_title via the staged load-list hash.\nUF_COMMIT_A_OLD = """    maybe_crash("prevdel");\n    journal_delete(title);\n\n    /* ---- commit records ---- */"""\nUF_COMMIT_A_NEW = """    maybe_crash("prevdel");\n\n    /* ---- commit records ----\n     * worker-uninstall-fault-expansion (uf-order-commit): records commit\n     * BEFORE journal_delete. Old order had a crash window leaving durable\n     * journal-delete + stale records: no journal means "nothing in flight",\n     * so recovery never repaired records that claimed installed while files\n     * were already gone. New order leaves records durable while the journal\n     * is still present, which recover_title repairs idempotently via the\n     * staged load-list hash. */"""\n\nUF_COMMIT_B_OLD = """        records_save();\n        say("installed title=%s generation=%ld mods=%s loadlist=%s", title, newgen, mods_out, sha);\n    }\n}"""\nUF_COMMIT_B_NEW = """        records_save();\n        say("installed title=%s generation=%ld mods=%s loadlist=%s", title, newgen, mods_out, sha);\n        maybe_crash("pre-journal-del"); /* uf probe: records durable, journal still present */\n    }\n    journal_delete(title); /* uf-order-commit: journal released only after records are durable */\n}"""\n\nUF_REC_FWD_OLD = """                rm_rf(prev);\n                journal_delete(title);\n                fsync_dir(g_roots);\n                records_commit_from_root(title, &j);\n                say("recover title=%s finished swap forward (new set active, generation %ld)", title, j.generation);"""\nUF_REC_FWD_NEW = """                rm_rf(prev);\n                records_commit_from_root(title, &j); /* uf-order-commit: records first */\n                journal_delete(title);\n                fsync_dir(g_roots);\n                say("recover title=%s finished swap forward (new set active, generation %ld)", title, j.generation);"""\n\nUF_REC_CMT_OLD = """                rm_rf(next);\n                rm_rf(prev);\n                journal_delete(title);\n                fsync_dir(g_roots);\n                records_commit_from_root(title, &j);\n                say("recover title=%s committed (new set already active, generation %ld)", title, j.generation);"""\nUF_REC_CMT_NEW = """                rm_rf(next);\n                rm_rf(prev);\n                records_commit_from_root(title, &j); /* uf-order-commit: records first */\n                journal_delete(title);\n                fsync_dir(g_roots);\n                say("recover title=%s committed (new set already active, generation %ld)", title, j.generation);"""\n\nEXTRA_PATCHES += [\n    ("uf-order-commit-a", UF_COMMIT_A_OLD, UF_COMMIT_A_NEW, 1),\n    ("uf-order-commit-b", UF_COMMIT_B_OLD, UF_COMMIT_B_NEW, 1),\n    ("uf-recover-forward", UF_REC_FWD_OLD, UF_REC_FWD_NEW, 1),\n    ("uf-recover-commit", UF_REC_CMT_OLD, UF_REC_CMT_NEW, 1),\n]\n'),
    ("fr2-block-append",
     '    ("uf-recover-commit", UF_REC_CMT_OLD, UF_REC_CMT_NEW, 1),\n]',
     '    ("uf-recover-commit", UF_REC_CMT_OLD, UF_REC_CMT_NEW, 1),\n]\n# ---- worker-f-r2-fix (F-R2) patch tuples, applied after the zb chain\nR2_SHIM_OLD = \'#include "../vendored/modforge_activation.h"\'\nR2_SHIM_NEW = R2_SHIM_OLD + """\n\n/* worker-f-r2-fix (F-R2, worker-e2e-rehearsal REPORT.md section 3): route\n * every controlled exit() of the composed core through the status end path,\n * so a refused command (e.g. install rejecting a non-bundle archive) cannot\n * leave a dangling begin line in e2e-status.jsonl. Exit codes and all say()\n * output are unchanged; _exit(86) crash probes and real signals still leave\n * no end line, so "missing end == command died" honesty is preserved. This\n * sits after the LAST include so no header text is seen through the macro. */\ntypedef void (*r2_real_exit_fn)(int);\nstatic r2_real_exit_fn r2_real_exit = exit;\n#define exit r2_e2e_exit\nstatic void r2_e2e_exit(int code) {\n    e2e_status_end(code);\n    r2_real_exit(code);\n}"""\nR2_STATUS_OLD = \'    say("installed title=%s generation=%ld mods=%s loadlist=%s", title, newgen, mods_out, sha);\'\nR2_STATUS_NEW = R2_STATUS_OLD + \'\\n    if (mods_out[0]) e2e_status_event("stage-3-install", "ok", mods_out);   /* worker-f-r2-fix (F-R2): terminal install status event; installs only — the remove commit shares this line with an empty mods list */\'\n\nEXTRA_PATCHES += [\n    ("r2-exit-shim", R2_SHIM_OLD, R2_SHIM_NEW, 1),\n    ("r2-install-status-ok", R2_STATUS_OLD, R2_STATUS_NEW, 1),\n]\n'),
]

# owned files vendored from the delivered worker-zip-to-mfb/owned set
OWNED_ALL = ["ps5_io_dir.c", "ps5_io_dir.h", "e2e_status.c", "e2e_status.h",
             "modinst_archive_prepare.c", "modinst_archive_prepare.h",
             "modinst_download.c", "modinst_download.h", "mb_curl_stub_host.c",
             "modinst_zip_bridge.c", "modinst_zip_bridge.h"]
OWNED_PREBRIDGE = set(OWNED_ALL) - {"modinst_zip_bridge.c", "modinst_zip_bridge.h"}


def digest(path):
    h = hashlib.sha256()
    with pathlib.Path(path).open("rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def die(msg):
    print("compose_console_bridge: FATAL: " + msg, file=sys.stderr)
    sys.exit(2)


def check_pins():
    """Chain-root integrity: the files I vendor must be the exact bytes the
    delivered receipts pinned."""
    e2e_sp = json.loads((E2E / "e2e-receipts/script-patches.json").read_text())
    if digest(ORIG_SCRIPT) != e2e_sp["vendored_script_sha256"]:
        die("compose_installer.py diverges from the e2e-final chain-root pin")
    zb_bp = json.loads((ZB / "e2e-receipts/bridge-script-patches.json").read_text())
    if digest(E2E_SCRIPT) != zb_bp["vendored_driver_sha256"]:
        die("compose_e2e.py diverges from the zip-to-mfb vendored-driver pin")
    return e2e_sp, zb_bp


def stage_owned():
    OWNED.mkdir(parents=True, exist_ok=True)
    zb_pins = json.loads((ZB / "e2e-receipts/owned-manifest.json").read_text())
    e2e_pins = json.loads((E2E / "e2e-receipts/owned-manifest.json").read_text())
    log = {}
    for name in OWNED_ALL:
        src = ZB / "owned" / name
        if not src.is_file():
            die("owned vendoring input missing: " + str(src))
        got = digest(src)
        want = zb_pins.get(name, {}).get("sha256")
        if want and got != want:
            die("owned %s diverges from zip-to-mfb owned-manifest pin" % name)
        if name in OWNED_PREBRIDGE:
            want2 = e2e_pins.get(name, {}).get("sha256")
            if want2 and got != want2:
                die("owned %s diverges from e2e-final owned-manifest pin" % name)
        shutil.copyfile(src, OWNED / name)
        log[name] = {"source": str(src), "sha256": digest(OWNED / name),
                     "zb_pin_verified": bool(want),
                     "e2e_pin_verified": name in OWNED_PREBRIDGE}
    (REC / "vendored-owned.json").write_text(json.dumps(log, indent=2, sort_keys=True))
    return log


def adapt_driver():
    src = E2E_SCRIPT.read_text()
    original_sha = digest(E2E_SCRIPT)
    log = []
    for pid, old, new in ZB_SPLICES:
        n = src.count(old)
        if n != 1:
            die("replayed splice %s anchor found %d times (want 1)" % (pid, n))
        src = src.replace(old, new)
        log.append({"id": pid, "origin": "worker-zip-to-mfb (verbatim replay)",
                    "old_sha256": hashlib.sha256(old.encode()).hexdigest(),
                    "new_sha256": hashlib.sha256(new.encode()).hexdigest()})
    replay_sha = hashlib.sha256(src.encode()).hexdigest()
    zb_bp = json.loads((ZB / "e2e-receipts/bridge-script-patches.json").read_text())
    if replay_sha != zb_bp["adapted_driver_sha256"]:
        die("verbatim replay diverges: intermediate sha %s != delivered "
            "adapted_driver_sha256 %s" % (replay_sha, zb_bp["adapted_driver_sha256"]))
    log.append({"id": "REPLAY-PROOF",
                "intermediate_sha256": replay_sha,
                "matches_delivered": True})
    for pid, old, new in MY_SPLICES:
        n = src.count(old)
        if n != 1:
            die("console splice %s anchor found %d times (want 1)" % (pid, n))
        src = src.replace(old, new)
        log.append({"id": pid, "origin": "worker-zip-bridge-console-link (correction)",
                    "old_sha256": hashlib.sha256(old.encode()).hexdigest(),
                    "new_sha256": hashlib.sha256(new.encode()).hexdigest()})
    DRIVER.write_text(src)
    (REC / "console-bridge-script-patches.json").write_text(json.dumps({
        "chain_root": str(ORIG_SCRIPT),
        "chain_root_sha256": digest(ORIG_SCRIPT),
        "vendored_driver": str(E2E_SCRIPT),
        "vendored_driver_sha256": original_sha,
        "replay_proof": {"intermediate_sha256": replay_sha,
                         "delivered_pin": zb_bp["adapted_driver_sha256"],
                         "byte_exact": True},
        "adapted_driver": str(DRIVER),
        "adapted_driver_sha256": digest(DRIVER),
        "splices": log,
        "corrections": [
            {"id": "zb-console-target-bridge",
             "finding": "delivered chain wires modinst_zip_bridge into the "
                        "host target list only, while zb-dispatch-bridge "
                        "makes the patched modinst_core.c reference "
                        "mi_bridge_* on both flavors — the delivered console "
                        "link would abort on undefined mi_bridge_run/"
                        "mi_bridge_wants_auto/mi_bridge_mod_arg",
             "fix": "add (\"modinst_zip_bridge\", HERE / \"owned/"
                    "modinst_zip_bridge.c\", core_flags) to the console "
                    "target list (same core_flags as the other owned TUs)"}]},
        indent=2, sort_keys=True))
    print("adapted console-bridge driver: %s (replay proof ok, from %s)"
          % (DRIVER, original_sha[:12]))


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--flavor", choices=["host", "console", "all"], default="console")
    args = ap.parse_args()
    REC.mkdir(parents=True, exist_ok=True)
    for p in (ORIG_SCRIPT, E2E_SCRIPT, ZB / "owned/modinst_zip_bridge.c"):
        if not p.is_file():
            die("vendoring input missing: " + str(p))
    check_pins()
    stage_owned()
    adapt_driver()
    env = os.environ.copy()
    env["MODBRIDGE_ROOT"] = str(ROOT)
    r = subprocess.run([sys.executable, str(DRIVER), "--flavor", args.flavor],
                       env=env, cwd=str(HERE))
    if r.returncode:
        die("inner composition failed (see composition/receipts/)")
    print("CONSOLE-BRIDGE COMPOSITION OK (flavor=%s)" % args.flavor)
    return 0


if __name__ == "__main__":
    sys.exit(main())
