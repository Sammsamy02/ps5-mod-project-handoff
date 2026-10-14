#!/usr/bin/env python3
"""compose_fault.py — worker-uninstall-fault-expansion chain driver.

Origin: worker-uninstall-fault-expansion, 2026-10-04 (ZCode team). Host-side
only; no network; no original file modified.

Extends the delivered F-R1 bridge composition (worker-zip-to-mfb
compose_bridge.py, which extends worker-e2e-final compose_e2e.py, which
extends worker-installer-composition compose_installer.py) by the same
vendored-copy + recorded exact-anchor patch discipline:

  1. vendors worker-zip-to-mfb/compose_bridge.py (sha-logged) and applies 2
     recorded text splices into an adapted driver (compose_fault_e2e.py):
       - INNER output renamed to composition_fault_inner.py (same content:
         the bridge inner, which already carries the full 49-patch chain);
       - appends 4 modinst.c patch tuples AFTER the bridge tuples (order in
         EXTRA_PATCHES is the apply order; anchors exist only after the
         earlier chain produced the text they anchor on):
           uf-order-commit-a/b  (defect D1 fix: records commit BEFORE
                                journal_delete in cmd_install, plus a new
                                "pre-journal-del" crash probe in the new
                                window),
           uf-recover-forward, uf-recover-commit (same reorder in the two
                                recover_title commit paths).
  2. runs the adapted driver (subprocess) with MODBRIDGE_ROOT pinned; its
     receipts land in ./e2e-receipts and ./composition/receipts.

Rebuild + expanded regression:
    python3 compose_fault.py --flavor host
    sh test/run_fault_tests.sh
"""
import argparse
import hashlib
import json
import os
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = pathlib.Path(os.environ.get("MODBRIDGE_ROOT", HERE.parents[2]))
TEAM = ROOT / "outputs/2026-10-04-zcode-team"
BRIDGE = TEAM / "worker-zip-to-mfb"
ORIG_SCRIPT = BRIDGE / "compose_bridge.py"
DRIVER = HERE / "compose_fault_e2e.py"
REC = HERE / "e2e-receipts"


def digest(path):
    h = hashlib.sha256()
    with pathlib.Path(path).open("rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def die(msg):
    print("compose_fault: FATAL: " + msg, file=sys.stderr)
    sys.exit(2)


# ---------------------------------------------------------------------------
# recorded splices into the vendored compose_bridge.py source text
# ---------------------------------------------------------------------------
S_INNER = ("uf-script-inner-name",
           'INNER = HERE / "composition_bridge_inner.py"   # worker-zip-to-mfb',
           'INNER = HERE / "composition_fault_inner.py"   # worker-uninstall-fault-expansion')

# The 4 modinst.c patch tuples, appended after the bridge tuples (list order =
# apply order; every anchor is verified to occur exactly once post-chain).
UF_BLOCK = '''
# ---- worker-uninstall-fault-expansion patch tuples, applied after the bridge
# Defect D1 (found by adversarial window analysis + d1_window_probe.sh on the
# baseline): cmd_install deleted the journal BEFORE committing records, so a
# death in between left "journal durably gone + records stale" -- an end state
# no recovery can repair (no journal == "nothing in flight"), and records
# claimed installed while files were already gone. Fix: make records durable
# FIRST in all three commit sites (cmd_install + both recover_title commit
# paths). The residual window (records durable, journal present) is repaired
# idempotently by recover_title via the staged load-list hash.
UF_COMMIT_A_OLD = """    maybe_crash("prevdel");
    journal_delete(title);

    /* ---- commit records ---- */"""
UF_COMMIT_A_NEW = """    maybe_crash("prevdel");

    /* ---- commit records ----
     * worker-uninstall-fault-expansion (uf-order-commit): records commit
     * BEFORE journal_delete. Old order had a crash window leaving durable
     * journal-delete + stale records: no journal means "nothing in flight",
     * so recovery never repaired records that claimed installed while files
     * were already gone. New order leaves records durable while the journal
     * is still present, which recover_title repairs idempotently via the
     * staged load-list hash. */"""

UF_COMMIT_B_OLD = """        records_save();
        say("installed title=%s generation=%ld mods=%s loadlist=%s", title, newgen, mods_out, sha);
    }
}"""
UF_COMMIT_B_NEW = """        records_save();
        say("installed title=%s generation=%ld mods=%s loadlist=%s", title, newgen, mods_out, sha);
        maybe_crash("pre-journal-del"); /* uf probe: records durable, journal still present */
    }
    journal_delete(title); /* uf-order-commit: journal released only after records are durable */
}"""

UF_REC_FWD_OLD = """                rm_rf(prev);
                journal_delete(title);
                fsync_dir(g_roots);
                records_commit_from_root(title, &j);
                say("recover title=%s finished swap forward (new set active, generation %ld)", title, j.generation);"""
UF_REC_FWD_NEW = """                rm_rf(prev);
                records_commit_from_root(title, &j); /* uf-order-commit: records first */
                journal_delete(title);
                fsync_dir(g_roots);
                say("recover title=%s finished swap forward (new set active, generation %ld)", title, j.generation);"""

UF_REC_CMT_OLD = """                rm_rf(next);
                rm_rf(prev);
                journal_delete(title);
                fsync_dir(g_roots);
                records_commit_from_root(title, &j);
                say("recover title=%s committed (new set already active, generation %ld)", title, j.generation);"""
UF_REC_CMT_NEW = """                rm_rf(next);
                rm_rf(prev);
                records_commit_from_root(title, &j); /* uf-order-commit: records first */
                journal_delete(title);
                fsync_dir(g_roots);
                say("recover title=%s committed (new set already active, generation %ld)", title, j.generation);"""

EXTRA_PATCHES += [
    ("uf-order-commit-a", UF_COMMIT_A_OLD, UF_COMMIT_A_NEW, 1),
    ("uf-order-commit-b", UF_COMMIT_B_OLD, UF_COMMIT_B_NEW, 1),
    ("uf-recover-forward", UF_REC_FWD_OLD, UF_REC_FWD_NEW, 1),
    ("uf-recover-commit", UF_REC_CMT_OLD, UF_REC_CMT_NEW, 1),
]
'''

SPLICES = [S_INNER,
           ("uf-script-patches-append",
            '    ("zb-usage-bridge", ZB_USAGE_OLD, ZB_USAGE_NEW, 1),\n]',
            '    ("zb-usage-bridge", ZB_USAGE_OLD, ZB_USAGE_NEW, 1),\n]' + UF_BLOCK)]


def adapt_driver():
    src = ORIG_SCRIPT.read_text()
    original_sha = digest(ORIG_SCRIPT)
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
    (REC / "fault-script-patches.json").write_text(json.dumps({
        "vendored_driver": str(ORIG_SCRIPT),
        "vendored_driver_sha256": original_sha,
        "adapted_driver": str(DRIVER),
        "adapted_driver_sha256": digest(DRIVER),
        "splices": log}, indent=2))
    print("adapted fault driver: %s (from %s)" % (DRIVER, original_sha[:12]))


def main():
    ap = argparse.ArgumentParser(description="worker-uninstall-fault-expansion chain driver")
    ap.add_argument("--flavor", choices=["host", "console", "all"], default="host")
    args = ap.parse_args()
    REC.mkdir(parents=True, exist_ok=True)
    if not ORIG_SCRIPT.is_file():
        die("vendoring input missing: " + str(ORIG_SCRIPT))
    adapt_driver()
    env = os.environ.copy()
    env["MODBRIDGE_ROOT"] = str(ROOT)
    r = subprocess.run([sys.executable, str(DRIVER), "--flavor", args.flavor],
                       env=env, cwd=str(HERE))
    if r.returncode:
        die("fault composition failed (see composition/receipts/)")
    print("FAULT COMPOSITION OK (flavor=%s)" % args.flavor)
    return 0


if __name__ == "__main__":
    sys.exit(main())
