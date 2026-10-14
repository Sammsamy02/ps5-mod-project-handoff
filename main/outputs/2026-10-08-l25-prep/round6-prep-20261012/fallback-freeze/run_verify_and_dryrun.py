#!/usr/bin/env python3
"""run_verify_and_dryrun.py -- for each frozen fallback PRX, run (Mac-only):

  1. harvest/verify_build.sh <frozen prx> --manifest <its own manifest> <frozen dir>
     expected: exit 0, all N markers PASS, receipt MATCH from the frozen dir.
  2. NEGATIVE CONTROL: the same with today's 24-marker harvest manifest.
     expected: exit 1 and the missing markers are EXACTLY the later builds'
     markers (proves, from the binary, what the build lacks versus B4).
  3. harvest/prep_modded_launch.py --variant A --prx-path <frozen prx> --dry-run
     run inside `sandbox-exec` with network AND file writes denied, AND under
     audit_dryrun_wrapper.py (CPython audit hook that records every write-open,
     socket, process or fs-change ATTEMPT before it happens). Both are on top of
     the code read of dry_run(), prep_modded_launch.py:1106-1309.
     expected: exit 0, PRX exists/magic/sha lines PASS for the frozen PRX, and
     the audit reports events>0 and zero violations.
  0. POSITIVE CONTROLS first (r2): the same sandbox + wrapper around
     audit_probe_controls.py, which deliberately tries one file write (into a
     fresh empty probe-tmp-* dir inside this dir, removed afterwards) and one loopback TCP connect (127.0.0.1:9; nothing leaves
     the Mac). Expected: the wrapper records both attempts (exit 3) and the
     sandbox refuses both (PermissionError), and the temp file does not exist.
     This proves the two guards actually bite, so "zero violations" in step 3
     means something.

Logs are written into this directory only. verify_build.sh and
prep_modded_launch.py are only executed, never modified. Prints how many
checks ran; exits 1 if any failed or if zero ran.
"""
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

OUT = "/WORKSPACE/.codex/worktrees/translation-core/what/outputs"
PREP = os.path.join(OUT, "2026-10-08-l25-prep")
VERIFY = os.path.join(PREP, "harvest/verify_build.sh")
PREPSCRIPT = os.path.join(PREP, "harvest/prep_modded_launch.py")
FULL24 = os.path.join(PREP, "harvest/markers-declared.json")
FROZEN = os.path.join(OUT, "2026-10-08-l26-frozen")
HERE = os.path.dirname(os.path.abspath(__file__))
WRAPPER = os.path.join(HERE, "audit_dryrun_wrapper.py")
PROBE = os.path.join(HERE, "audit_probe_controls.py")
PRX = "modbridge_supplier_composition.prx"
SANDBOX = "(version 1)(allow default)(deny network*)(deny file-write*)"
LATER = ["b2-mem-line", "b2-lua-mem-note", "b3-foreachuobject", "b3-uenum-foreachname",
         "b4-shim-write-area-dir"]

BUILDS = [
    ("B3-iterators", "40b8972b58e313c61b84e2665d8c166887b49712753b000ff4f0cf919cfb9175", 23, LATER[4:]),
    ("B2b-error-wrapper-memlog", "74fab85a5165c17a4d169f8efdb25e0e647fa7b867ed7843a7b4869a5f8c58b2", 21, LATER[2:]),
    ("B1-lookup-nocache", "b9cc52485c51e5ebbaf1944e01f472f65d19136e28d3a0ef965756b1936aa909", 19, LATER[0:]),
]

n = 0
failed = []


def ok(cond, what):
    global n
    n += 1
    print("  [%s] %s" % ("PASS" if cond else "FAIL", what))
    if not cond:
        failed.append(what)
    return bool(cond)


def run(argv, log):
    p = subprocess.run(argv, cwd=PREP, capture_output=True, text=True, timeout=300)
    with open(log, "w") as f:
        f.write("# command: %s\n# cwd: %s\n# exit: %d\n" % (" ".join(argv), PREP, p.returncode))
        f.write(p.stdout)
        if p.stderr:
            f.write("\n# --- stderr ---\n" + p.stderr)
    return p.returncode, p.stdout, p.stderr


def controls(summary):
    print("== positive controls: sandbox + audit hook must both bite ==")
    # probe dir lives inside THIS dir (the only place this track may write);
    # the sandbox must refuse the write, so the dir stays empty and is removed.
    tmpd = tempfile.mkdtemp(prefix="probe-tmp-", dir=HERE)
    target = os.path.join(tmpd, "probe-must-not-exist.txt")
    try:
        log = os.path.join(HERE, "sandbox-audit-positive-control.log")
        rc, so, se = run(["sandbox-exec", "-p", SANDBOX, "python3", "-B", "-I", WRAPPER, PROBE, target], log)
        ok(rc == 3, "control: wrapper exit 3 (violations seen) for the deliberate write + connect (got %d)" % rc)
        ok("AUDIT VIOLATION write-open:" in se, "control: audit hook recorded the write-open attempt")
        ok("AUDIT VIOLATION network: socket.connect" in se, "control: audit hook recorded the socket.connect attempt")
        ok("PROBE write: PermissionError" in so, "control: sandbox refused the write (PermissionError)")
        ok("PROBE connect: PermissionError" in so, "control: sandbox refused the loopback connect (PermissionError)")
        ok(not os.path.exists(target), "control: the probe file does not exist afterwards")
        summary["positive_control"] = {"log": log, "exit": rc}
    finally:
        shutil.rmtree(tmpd, ignore_errors=True)


def main():
    summary = {"schema": "fallback-verify-dryrun/2", "mac_only": True, "console_contact": False, "builds": []}
    controls(summary)
    for label, sha, count, later in BUILDS:
        d = os.path.join(FROZEN, "%s-%s" % (label, sha[:8]))
        prx = os.path.join(d, PRX)
        man = os.path.join(HERE, "markers-%s-%d.json" % (label, count))
        print("== %s ==" % label)
        ok(os.path.isfile(prx) and os.path.isfile(man), "%s: frozen PRX and its manifest exist" % label)
        row = {"label": label, "sha256": sha, "frozen_dir": d, "manifest": man, "markers": count}

        # 1. verify with the build's own manifest
        log = os.path.join(HERE, "verify-%s-%dmarkers.log" % (label, count))
        rc, so, _ = run(["bash", VERIFY, prx, "--manifest", man, d], log)
        ok(rc == 0, "%s: verify_build.sh exit 0 (got %d)" % (label, rc))
        ok("SUMMARY: declared=%d pass=%d miss=0" % (count, count) in so, "%s: %d/%d declared markers PASS" % (label, count, count))
        ok("receipt: %s" % os.path.join(d, "FINAL-RECEIPT-VARIANT-A.json") in so and "RESULT: MATCH" in so,
           "%s: receipt from the frozen dir, RESULT: MATCH" % label)
        ok("magic=4f153d1d" in so and ("sha256=%s" % sha) in so, "%s: identity line magic 4f153d1d + sha" % label)
        ok("VERDICT: PASS" in so, "%s: VERDICT: PASS" % label)
        row["verify"] = {"log": log, "exit": rc, "verdict": "PASS" if rc == 0 and "VERDICT: PASS" in so else "FAIL"}

        # 2. negative control with the full 24-marker manifest
        log2 = os.path.join(HERE, "negative-control-24markers-%s.log" % label)
        rc2, so2, _ = run(["bash", VERIFY, prx, "--manifest", FULL24, d], log2)
        m = re.search(r"MISSING DECLARED MARKERS \(fix the build, not the gate\): (.*)", so2)
        missing = [x.strip() for x in m.group(1).split(",")] if m else []
        ok(rc2 == 1, "%s: negative control exits 1 as expected (got %d)" % (label, rc2))
        ok("SUMMARY: declared=24 pass=%d miss=%d" % (24 - len(later), len(later)) in so2,
           "%s: negative control 24 declared, %d expected misses" % (label, len(later)))
        ok(missing == later, "%s: missing markers are exactly the later builds' markers %s" % (label, later))
        row["negative_control_24"] = {"log": log2, "exit": rc2, "missing": missing}

        # 3. prep_modded_launch.py --dry-run, network + writes denied by the sandbox
        log3 = os.path.join(HERE, "dryrun-%s.log" % label)
        argv = ["python3", PREPSCRIPT, "--variant", "A", "--prx-path", prx, "--dry-run"]
        guarded = ["sandbox-exec", "-p", SANDBOX, "python3", "-B", "-I", WRAPPER] + argv[1:]
        rc3, so3, se3 = run(guarded, log3)
        ok(rc3 == 0, "%s: dry-run exit 0 (got %d)" % (label, rc3))
        ok(so3.startswith("=== DRY-RUN (no network"), "%s: dry-run banner present" % label)
        ok(("file exists: %s -> PASS" % prx) in so3, "%s: dry-run STEP 2 file exists PASS" % label)
        ok("magic: '4f153d1d'" in so3 and "-> PASS (signed SELF)" in so3, "%s: dry-run STEP 2 magic PASS" % label)
        ok(("sha256 (recorded for the STEP 3 readback match): %s" % sha) in so3, "%s: dry-run records the frozen sha" % label)
        ok(("--upload-file %s " % prx) in so3, "%s: dry-run upload line names the frozen PRX" % label)
        ok(so3.count("local verify -> PASS") == 2, "%s: dry-run settings + ploader.ini local checks PASS" % label)
        am = re.search(r"AUDIT events_observed=(\d+) read_opens=(\d+) violations=(\d+) script_exit=(-?\d+)", se3)
        ok(am is not None and int(am.group(1)) > 0,
           "%s: audit hook observed %s events (must be > 0)" % (label, am.group(1) if am else "no"))
        ok(am is not None and int(am.group(3)) == 0 and "AUDIT RESULT: no write-open, network, process or fs-change attempt" in se3,
           "%s: audit saw zero write-open / network / process / fs-change attempts" % label)
        ok("Operation not permitted" not in (so3 + se3) and "PermissionError" not in (so3 + se3),
           "%s: no permission error surfaced in the dry-run output (weak check; the audit line above is the real one)" % label)
        row["dryrun"] = {"log": log3, "exit": rc3, "command": " ".join(argv),
                         "guarded_command": " ".join(guarded),
                         "audit": am.group(0) if am else None,
                         "sandbox_wrapper": "sandbox-exec -p '%s'" % SANDBOX}
        summary["builds"].append(row)

    with open(os.path.join(HERE, "VERIFY-DRYRUN-SUMMARY.json"), "w") as f:
        json.dump(summary, f, indent=1)
    print("SUMMARY verify+dryrun: checks=%d pass=%d fail=%d" % (n, n - len(failed), len(failed)))
    if n == 0:
        print("FAIL: zero checks ran")
        return 1
    if failed:
        print("FAIL: " + "; ".join(failed))
        return 1
    print("RESULT verify+dryrun: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
