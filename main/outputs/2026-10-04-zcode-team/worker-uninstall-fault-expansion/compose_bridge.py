#!/usr/bin/env python3
"""compose_bridge.py — worker-zip-to-mfb F-R1 bridge wiring driver (PS5 mod-manager).

Origin: worker-zip-to-mfb, 2026-10-04 (ZCode team). Host-side only.

Extends the delivered E2E composition (worker-e2e-final compose_e2e.py, which
itself extends worker-installer-composition compose_installer.py) WITHOUT
touching any original file, by the same vendored-copy + recorded exact-anchor
patch chain discipline:

  1. vendors worker-e2e-final/compose_e2e.py (sha cross-checked against its
     e2e-receipts/final-provenance.json pin) and applies recorded text splices
     into an adapted driver (compose_bridge_e2e.py) that builds into THIS dir:
       - vendors worker-e2e-final's authored owned adapters (download TU, host
         curl stub) into this owned/ first, sha-verified against its
         e2e-receipts/owned-manifest.json;
       - adds THIS worker's owned/modinst_zip_bridge.c/.h to the owned
         manifest and the host target list (core_flags, same includes as the
         other owned TUs);
       - appends 4 modinst.c patch tuples AFTER the e2e chain (order in the
         PATCHES list is the apply order): bridge include, --no-auto-bundle
         flag-loop skip, the prepare-archive auto-bundle dispatch step (runs
         BEFORE records_mark_archive_prepared; repoints the record's bundle
         to the composed mfb on success), usage line.
  2. runs the adapted driver (subprocess) with MODBRIDGE_ROOT pinned; its
     receipts land in ./e2e-receipts and ./composition/receipts.

Rebuild + host regression:
    python3 compose_bridge.py --flavor host
    sh test/run_bridge_tests.sh

Design: BRIDGE-DESIGN.md. The mfb kind-1 bundle remains the ONLY bundle
format; the bridge composes one from the intake's quarantine manifest.
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
E2E = TEAM / "worker-e2e-final"
ORIG_SCRIPT = E2E / "compose_e2e.py"
DRIVER = HERE / "compose_bridge_e2e.py"
OWNED = HERE / "owned"
REC = HERE / "e2e-receipts"

# e2e-final authored adapters the adapted driver's targets reference from
# HERE/owned; vendored here first (sha-checked against e2e-final receipts).
VENDOR_OWNED = ["modinst_download.c", "modinst_download.h", "mb_curl_stub_host.c"]


def digest(path):
    h = hashlib.sha256()
    with pathlib.Path(path).open("rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def die(msg):
    print("compose_bridge: FATAL: " + msg, file=sys.stderr)
    sys.exit(2)


# ---------------------------------------------------------------------------
# recorded splices into the vendored compose_e2e.py source text
# ---------------------------------------------------------------------------
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

# The 4 modinst.c patch tuples, appended after the e2e chain (list order =
# apply order; anchors exist only after the earlier patches produced them).
ZB_BLOCK = '''
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

ZB_USAGE_OLD = \'    say("  prepare-archive --title T --archive F --quarantine D [--mod ID]   (stage 2a)");\'
ZB_USAGE_NEW = ZB_USAGE_OLD + \'\\n    say("          [--no-auto-bundle]  (F-R1 bridge: with --mod, default composes bundles/<id>.mfb from the quarantine)");\'

EXTRA_PATCHES += [
    ("zb-inc-bridge", ZB_INC_OLD, ZB_INC_NEW, 1),
    ("zb-flag-no-auto-bundle", ZB_FLAG_OLD, ZB_FLAG_NEW, 1),
    ("zb-dispatch-bridge", ZB_DISP_OLD, ZB_DISP_NEW, 1),
    ("zb-usage-bridge", ZB_USAGE_OLD, ZB_USAGE_NEW, 1),
]
'''

S_PATCH_APPEND = ("bridge-script-patches-append",
                  '    ("e2e-usage", C_USAGE_OLD, C_USAGE_NEW, 1),\n]',
                  '    ("e2e-usage", C_USAGE_OLD, C_USAGE_NEW, 1),\n]\n' + ZB_BLOCK)

SPLICES = [S_INNER, S_OWNED_LIST, S_TARGETS, S_PATCH_APPEND]


def stage_e2e_owned():
    """Vendor worker-e2e-final's authored owned adapters into this owned/,
    sha-verified against its e2e-receipts/owned-manifest.json."""
    man_path = E2E / "e2e-receipts" / "owned-manifest.json"
    if not man_path.is_file():
        die("worker-e2e-final owned manifest receipt missing: " + str(man_path))
    man = json.loads(man_path.read_text())
    OWNED.mkdir(parents=True, exist_ok=True)
    log = {}
    for name in VENDOR_OWNED:
        src = E2E / "owned" / name
        if not src.is_file():
            die("vendoring input missing: " + str(src))
        got = digest(src)
        want = man.get(name, {}).get("sha256")
        if want and got != want:
            die("owned adapter %s sha diverges from e2e-final receipt (%s != %s)" % (name, got, want))
        shutil.copyfile(src, OWNED / name)
        log[name] = {"source": str(src), "sha256": digest(OWNED / name),
                     "receipt_verified": bool(want)}
    (REC / "vendored-e2e-owned.json").write_text(json.dumps(log, indent=2, sort_keys=True))
    return log


def adapt_driver():
    src = ORIG_SCRIPT.read_text()
    original_sha = digest(ORIG_SCRIPT)
    # cross-check the vendored chain against e2e-final's own provenance pin
    prov = E2E / "e2e-receipts" / "final-provenance.json"
    if prov.is_file():
        pins = json.loads(prov.read_text())
        pin = pins.get("compose_e2e.py") or pins.get("files", {}).get("compose_e2e.py")
        if isinstance(pin, str) and pin != original_sha:
            die("compose_e2e.py sha diverges from final-provenance pin (%s)" % pin)
    log = []
    for pid, old, new in SPLICES:
        n = src.count(old)
        if n != 1:
            die("driver splice %s anchor found %d times (want 1)" % (pid, n))
        src = src.replace(old, new)
        log.append({"id": pid,
                    "old_sha256": hashlib.sha256(old.encode()).hexdigest(),
                    "new_sha256": hashlib.sha256(new.encode()).hexdigest()})
    DRIVER.write_text(src)
    (REC / "bridge-script-patches.json").write_text(json.dumps({
        "vendored_driver": str(ORIG_SCRIPT),
        "vendored_driver_sha256": original_sha,
        "adapted_driver": str(DRIVER),
        "adapted_driver_sha256": digest(DRIVER),
        "splices": log}, indent=2))
    print("adapted bridge driver: %s (from %s)" % (DRIVER, original_sha[:12]))


def main():
    ap = argparse.ArgumentParser(description="worker-zip-to-mfb bridge wiring driver")
    ap.add_argument("--flavor", choices=["host", "console", "all"], default="host")
    args = ap.parse_args()
    REC.mkdir(parents=True, exist_ok=True)
    for p in (ORIG_SCRIPT, HERE / "owned" / "modinst_zip_bridge.c",
              HERE / "owned" / "modinst_zip_bridge.h"):
        if not p.is_file():
            die("vendoring input missing: " + str(p))
    stage_e2e_owned()
    adapt_driver()
    env = os.environ.copy()
    env["MODBRIDGE_ROOT"] = str(ROOT)
    r = subprocess.run([sys.executable, str(DRIVER), "--flavor", args.flavor],
                       env=env, cwd=str(HERE))
    if r.returncode:
        die("bridge composition failed (see composition/receipts/)")
    print("BRIDGE COMPOSITION OK (flavor=%s)" % args.flavor)
    return 0


if __name__ == "__main__":
    sys.exit(main())
